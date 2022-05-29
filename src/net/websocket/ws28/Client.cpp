#include "Client.h"
#include "Server.h"
#include "base64.h"
#include <string>
#include <sstream>
#include <cassert>

#include <openssl/sha.h>
#include "logger.hpp"

namespace ws28 {
	
namespace detail {
	bool equalsi(std::string_view a, std::string_view b){
		if(a.size() != b.size()) return false;
		for(;;){
			if(tolower(a.front()) != tolower(b.front())) return false;
			
			a.remove_prefix(1);
			b.remove_prefix(1);
			if(a.empty()) return true;
		}
	}
	
	bool equalsi(std::string_view a, std::string_view b, size_t n){
		while(n--){
			if(a.empty()) return b.empty();
			else if(b.empty()) return false;
			
			if(tolower(a.front()) != tolower(b.front())) return false;
			
			a.remove_prefix(1);
			b.remove_prefix(1);
		}
		
		return true;
	}
	
	bool HeaderContains(std::string_view header, std::string_view substring){
		bool hasMatch = false;
		
		while(!header.empty()){
			if(header.front() == ' ' || header.front() == '\t'){
				header.remove_prefix(1);
				continue;
			}
			
			if(hasMatch){
				if(header.front() == ',') return true;
				hasMatch = false;
				header.remove_prefix(1);
				
				// Skip to comma or end of string
				while(!header.empty() && header.front() != ',') header.remove_prefix(1);
				if(header.empty()) return false;
				
				// Skip comma
				assert(header.front() == ',');
				header.remove_prefix(1);
			}else{
				if(detail::equalsi(header, substring, substring.size())){
					// We have a match... if the header ends here, or has a comma
					hasMatch = true;
					header.remove_prefix(substring.size());
				}else{
					header.remove_prefix(1);
					
					// Skip to comma or end of string
					while(!header.empty() && header.front() != ',') header.remove_prefix(1);
					if(header.empty()) return false;
					
					// Skip comma
					assert(header.front() == ',');
					header.remove_prefix(1);
				}
			}
		}
		
		return hasMatch;
	}
	
	
	struct Corker {
		Client &client;
		
		Corker(Client &client) : client(client) { client.Cork(true); }
		~Corker(){ client.Cork(false); }
	};
}

struct DataFrameHeader {
	char *data;
	
	DataFrameHeader(char *data) : data(data){
		
	}
	
	void reset(){
		data[0] = 0;
		data[1] = 0;
	}

	void fin(bool v) { data[0] &= ~(1 << 7); data[0] |= v << 7; }
	void rsv1(bool v) { data[0] &= ~(1 << 6); data[0] |= v << 6; }
	void rsv2(bool v) { data[0] &= ~(1 << 5); data[0] |= v << 5; }
	void rsv3(bool v) { data[0] &= ~(1 << 4); data[0] |= v << 4; }
	void mask(bool v) { data[1] &= ~(1 << 7); data[1] |= v << 7; }
	void opcode(uint8_t v) {
		data[0] &= ~0x0F;
		data[0] |= v & 0x0F;
	}

	void len(uint8_t v) {
		data[1] &= ~0x7F;
		data[1] |= v & 0x7F;
	}

	bool fin() { return (data[0] >> 7) & 1; }
	bool rsv1() { return (data[0] >> 6) & 1; }
	bool rsv2() { return (data[0] >> 5) & 1; }
	bool rsv3() { return (data[0] >> 4) & 1; }
	bool mask() { return (data[1] >> 7) & 1; }

	uint8_t opcode() {
		return data[0] & 0x0F;
	}

	uint8_t len() {
		return data[1] & 0x7F;
	}
};

Client::Client(Server *server, SocketHandle socket) : m_pServer(server), m_Socket(std::move(socket)){
	m_Socket->data = this;
	
	// Default to true since that's what most people want
	uv_tcp_nodelay(m_Socket.get(), true);
	uv_tcp_keepalive(m_Socket.get(), true, 10000);
	
	{ // Put IP in m_IP
		m_IP[0] = '\0';
		struct sockaddr_storage addr;
		int addrLen = sizeof(addr);
		uv_tcp_getpeername(m_Socket.get(), (sockaddr*) &addr, &addrLen);
		
		if(addr.ss_family == AF_INET){
			int r = uv_ip4_name((sockaddr_in*) &addr, m_IP, sizeof(m_IP));
			(void) r;
			assert(r == 0);
		}else if(addr.ss_family == AF_INET6){
			int r = uv_ip6_name((sockaddr_in6*) &addr, m_IP, sizeof(m_IP));
			(void) r;
			assert(r == 0);
			
			// Remove this prefix if it exists, it means that we actually have a ipv4
			static const char *ipv4Prefix = "::ffff:";
			if(strncmp(m_IP, ipv4Prefix, strlen(ipv4Prefix)) == 0){
				memmove(m_IP, m_IP + strlen(ipv4Prefix), strlen(m_IP) - strlen(ipv4Prefix) + 1);
			}
		}else{
			// Server::OnConnection will destroy us
		}
	}
	
	uv_read_start((uv_stream_t*) m_Socket.get(), [](uv_handle_t*, size_t suggested_size, uv_buf_t *buf){
		buf->base = new char[suggested_size];
		buf->len = suggested_size;
	}, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
		auto client = (Client*) stream->data;
		
		if(client != nullptr){
			if(nread < 0){
				client->Destroy();
			}else if(nread > 0){
				client->OnRawSocketData(buf->base, (size_t) nread);
			}
		}
		
		if(buf != nullptr) delete[] buf->base;
	});
}

Client::~Client(){
	assert(!m_Socket);
}

void Client::Destroy(){
	if(!m_Socket) return;
	
	Cork(false);
	
	m_Socket->data = nullptr;
	
	auto myself = m_pServer->NotifyClientPreDestroyed(this);
	
	struct ShutdownRequest : uv_shutdown_t {
		SocketHandle socket;
		std::unique_ptr<Client> client;
		Server::ClientDisconnectedFn cb;
	};
	
	auto req = new ShutdownRequest();
	req->socket = std::move(m_Socket);
	req->client = std::move(myself);
	req->cb = m_pServer->m_fnClientDisconnected;
	
	m_pServer = nullptr;
	
	static auto cb = [](uv_shutdown_t* reqq, int){
		auto req = (ShutdownRequest*) reqq;
		
		if(req->cb && req->client->m_bHasCompletedHandshake){
			req->cb(req->client.get(), req->client->GetServer());
		}
		
		delete req;
	};
	
	if(uv_shutdown(req, (uv_stream_t*) req->socket.get(), cb) != 0){
		// Shutdown failed, but we have to delay the destruction to the next event loop
		auto timer = new uv_timer_t;
		uv_timer_init(req->socket->loop, timer);
		timer->data = req;
		uv_timer_start(timer, [](uv_timer_t *timer){
			auto req = (ShutdownRequest*) timer->data;
			cb(req, 0);
			uv_close((uv_handle_t*) timer, [](uv_handle_t *h){ delete (uv_timer_t*) h;});
		}, 0, 0);
	}
}



template<size_t N>
void Client::WriteRaw(uv_buf_t bufs[N]){
	if(!m_Socket) return;
	
	// Try to write without allocating memory first, if that doesn't work, we call WriteRawQueue
	int written = uv_try_write((uv_stream_t*) m_Socket.get(), bufs, N);
	if(written == UV_EAGAIN) written = 0;
		
	if(written >= 0){
		size_t totalLength = 0;
		
		for(size_t i = 0; i < N; ++i){
			auto &buf = bufs[i];
			totalLength += buf.len;
		}
		
		size_t skipping = (size_t) written;
		if(skipping == totalLength) return; // Complete write
		
		// Partial write
		// Copy the remainder into a buffer to send to WriteRawQueue
		
		auto cpy = std::make_unique<char[]>(totalLength);
		size_t offset = 0;
		
		for(size_t i = 0; i < N; ++i){
			auto &buf = bufs[i];
			if(skipping >= buf.len){
				skipping -= buf.len;
				continue;
			}
			
			memcpy(cpy.get() + offset, buf.base + skipping, buf.len - skipping);
			offset += buf.len - skipping;
			skipping = 0;
		}
		
		WriteRawQueue(std::move(cpy), offset);
	}else{
		// Write error
		Destroy();
		return;
	}
}

void Client::WriteRawQueue(std::unique_ptr<char[]> data, size_t len){
	if(!m_Socket) return;
	
	struct CustomWriteRequest {
		uv_write_t req;
		Client *client;
		std::unique_ptr<char[]> data;
	};
	
	uv_buf_t buf;
	buf.base = data.get();
	buf.len = len;

	auto request = new CustomWriteRequest();
	request->client = this;
	request->data = std::move(data);
	
	if(uv_write(&request->req, (uv_stream_t*) m_Socket.get(), &buf, 1, [](uv_write_t* req, int status){
		auto request = (CustomWriteRequest*) req;

		if(status < 0){
			request->client->Destroy();
		}
		
		delete request;
	}) != 0){
		delete request;
		Destroy();
	}
}

template<size_t N>
void Client::Write(uv_buf_t bufs[N]){
	if(!m_Socket) return;
	if(IsSecure()){
		for(size_t i = 0; i < N; ++i){
			if(!m_pTLS->Write(bufs[i].base, bufs[i].len)) return Destroy();
		}
		FlushTLS();
	}else{
		WriteRaw<N>(bufs);
	}
}

void Client::Write(const char *data, size_t len){
	uv_buf_t bufs[1];
	bufs[0].base = (char*) data;
	bufs[0].len = len;
	Write<1>(bufs);
}

void Client::Write(const char *data){
	Write(data, strlen(data));
}

void Client::WriteDataFrameHeader(uint8_t opcode, size_t len, char *headerStart){
	DataFrameHeader header{ headerStart };
	
	header.reset();
	header.fin(true);
	header.opcode(opcode);
	header.mask(false);
	header.rsv1(false);
	header.rsv2(false);
	header.rsv3(false);
	if(len >= 126){
		if(len > UINT16_MAX){
			header.len(127);
			*(uint8_t*)(headerStart + 2) = (len >> 56) & 0xFF;
			*(uint8_t*)(headerStart + 3) = (len >> 48) & 0xFF;
			*(uint8_t*)(headerStart + 4) = (len >> 40) & 0xFF;
			*(uint8_t*)(headerStart + 5) = (len >> 32) & 0xFF;
			*(uint8_t*)(headerStart + 6) = (len >> 24) & 0xFF;
			*(uint8_t*)(headerStart + 7) = (len >> 16) & 0xFF;
			*(uint8_t*)(headerStart + 8) = (len >> 8) & 0xFF;
			*(uint8_t*)(headerStart + 9) = (len >> 0) & 0xFF;
		}else{
			header.len(126);
			*(uint8_t*)(headerStart + 2) = (len >> 8) & 0xFF;
			*(uint8_t*)(headerStart + 3) = (len >> 0) & 0xFF;
		}
	}else{
		header.len(len);
	}
}


size_t Client::GetDataFrameHeaderSize(size_t len){
	if(len >= 126){
		if(len > UINT16_MAX){
			return 10;
		}else{
			return 4;
		}
	}else{
		return 2;
	}
}

void Client::OnRawSocketData(char *data, size_t len){
	if(len == 0) return;
	if(!m_Socket) return;
	
	if(m_bWaitingForFirstPacket){
		m_bWaitingForFirstPacket = false;
		
		assert(!IsSecure());
		
		if(m_pServer->GetSSLContext() != nullptr && (data[0] == 0x16 || uint8_t(data[0]) == 0x80)){
			if(m_pServer->m_fnCheckTCPConnection && !m_pServer->m_fnCheckTCPConnection(GetIP(), true)){
				return Destroy();
			}
			
			InitSecure();
		}else{
			if(m_pServer->m_fnCheckTCPConnection && !m_pServer->m_fnCheckTCPConnection(GetIP(), false)){
				return Destroy();
			}
		}
	}
	
	if(IsSecure()){
		if(!m_pTLS->ReceivedData(data, len, [&](char *data, size_t len){
			OnSocketData(data, len);
		})){
			return Destroy();
		}
		
		FlushTLS();
	}else{
		OnSocketData(data, len);
	}
}

void Client::OnSocketData(char *data, size_t len){
	if(m_pServer == nullptr) return;
	
	// This gives us an extra byte just in case
	if(m_Buffer.size() + len + 1 >= m_pServer->m_iMaxMessageSize){
		if(m_bHasCompletedHandshake){
			Close(1009, "Message too large");
		}
		
		Destroy();
		return;
	}
	
	// If we don't have anything stored in our class-level buffer (m_Buffer),
	// we use the buffer we received in the function arguments so we don't have to
	// perform any copying. The Bail function needs to be called before we leave this
	// function (unless we're destroying the client), to copy the unused part of the buffer
	// back to our class-level buffer
	std::string_view buffer;
	bool usingLocalBuffer;
	
	if(m_Buffer.empty()){
		usingLocalBuffer = true;
		buffer = std::string_view(data, len);
	}else{
		usingLocalBuffer = false;
		
		m_Buffer.insert(m_Buffer.end(), data, data + len);
		buffer = std::string_view(m_Buffer.data(), m_Buffer.size());
	}
	
	auto Consume = [&](size_t amount){
		assert(buffer.size() >= amount);
		buffer.remove_prefix(amount);
	};
	
	auto Bail = [&](){
		// Copy partial HTTP headers to our buffer
		if(usingLocalBuffer){
			if(!buffer.empty()){
				assert(m_Buffer.empty());
				m_Buffer.insert(m_Buffer.end(), buffer.data(), buffer.data() + buffer.size());
			}
		}else{
			if(buffer.empty()){
				m_Buffer.clear();
			}else if(buffer.size() != m_Buffer.size()){
				memmove(m_Buffer.data(), buffer.data(), buffer.size());
				m_Buffer.resize(buffer.size());
			}
		}
	};
	
	if(!m_bHasCompletedHandshake && m_pServer->GetAllowAlternativeProtocol() && buffer[0] == 0x00){
		m_bHasCompletedHandshake = true;
		m_bUsingAlternativeProtocol = true;
		Consume(1);
		
		if(m_pServer->m_fnCheckAlternativeConnection && !m_pServer->m_fnCheckAlternativeConnection(this)){
			Destroy();
			return;
		}
		
		RequestHeaders headers;
		HTTPRequest req{
			m_pServer,
			"GET",
			"/",
			m_IP,
			headers,
		};
		m_pServer->NotifyClientInit(this, req);
	}else if(!m_bHasCompletedHandshake){
		// HTTP headers not done yet, wait
		auto endOfHeaders = buffer.find("\r\n\r\n");
		if(endOfHeaders == std::string_view::npos) return Bail();
		
		auto MalformedRequest = [&](){
			Write("HTTP/1.1 400 Bad Request\r\n\r\n");
			Destroy();
		};
		
		auto headersBuffer = buffer.substr(0, endOfHeaders+4); // Include \r\n\r\n
		
		std::string_view method;
		std::string_view path;
		
		{
			auto methodEnd = headersBuffer.find(' ');
			
			auto endOfLine = headersBuffer.find("\r\n");
			assert(endOfLine != std::string_view::npos); // Can't fail because of a check above
			
			if(methodEnd == std::string_view::npos || methodEnd > endOfLine) return MalformedRequest();
			
			method = headersBuffer.substr(0, methodEnd);
			
			// Uppercase method
			std::transform(method.begin(), method.end(), (char*) method.data(), [](char v) -> char{
				if(v < 0 || v >= 127) return v;
				return toupper(v);
			});
			
			auto pathStart = methodEnd + 1;
			auto pathEnd = headersBuffer.find(' ', pathStart);
			
			if(pathEnd == std::string_view::npos || pathEnd > endOfLine) return MalformedRequest();
			
			path = headersBuffer.substr(pathStart, pathEnd - pathStart);
			
			// Skip line
			headersBuffer = headersBuffer.substr(endOfLine + 2);
		}
		
		RequestHeaders headers;
		
		for(;;) {
			auto nextLine = headersBuffer.find("\r\n");
			
			// This means that we have finished parsing the headers
			if(nextLine == 0) {
				break;
			}
			
			// This can't happen... right?
			if(nextLine == std::string_view::npos) return MalformedRequest();
			
			auto colonPos = headersBuffer.find(':');
			if(colonPos == std::string_view::npos || colonPos > nextLine) return MalformedRequest();
			
			auto key = headersBuffer.substr(0, colonPos);
			
			// Key to lower case
			std::transform(key.begin(), key.end(), (char*) key.data(), [](char v) -> char {
				if(v < 0 || v >= 127) return v;
				return tolower(v);
			});
			
			auto value = headersBuffer.substr(colonPos + 1, nextLine - (colonPos + 1));
			
			// Trim spaces
			while(!key.empty() && key.front() == ' ') key.remove_prefix(1);
			while(!key.empty() && key.back() == ' ') key.remove_suffix(1);
			while(!value.empty() && value.front() == ' ') value.remove_prefix(1);
			while(!value.empty() && value.back() == ' ') value.remove_suffix(1);
			
			headers.Set(key, value);
			
			headersBuffer = headersBuffer.substr(nextLine+2);
		}
		
		HTTPRequest req{
			m_pServer,
			method,
			path,
			m_IP,
			headers,
		};
		
		{
			if(auto upgrade = headers.Get("upgrade")){
				if(!detail::equalsi(*upgrade, "websocket")){
					return MalformedRequest();
				}
			}else{
				
				HTTPResponse res;
				
				if(m_pServer->m_fnHTTPRequest) m_pServer->m_fnHTTPRequest(req, res);
				
				if(res.statusCode == 0) res.statusCode = 404;
				if(res.statusCode < 200 || res.statusCode >= 1000) res.statusCode = 500;
				
				
				const char *statusCodeText = "WS28"; // Too lazy to create a table of those
				
				std::stringstream ss;
				ss << "HTTP/1.1 " << res.statusCode << " " << statusCodeText << "\r\n";
				ss << "Connection: close\r\n";
				ss << "Content-Length: " << res.body.size() << "\r\n";
				
				for(auto &p : res.headers){
					ss << p.first << ": " << p.second << "\r\n";
				}
				
				ss << "\r\n";
				
				ss << res.body;
				
				std::string str = ss.str();
				Write(str.data(), str.size());
				
				Destroy();
				return;
			}
		}
		
		// WebSocket upgrades must be GET
		if(method != "GET") return MalformedRequest();
		
		auto connectionHeader = headers.Get("connection");
		if(!connectionHeader) {
			return MalformedRequest();
		}
		
		// Hackish, ideally we should check it's surrounded by commas (or start/end of string)
		if(!detail::HeaderContains(*connectionHeader, "upgrade")) {
			return MalformedRequest();
		}
		
		bool sendMyVersion = false;
		
		auto websocketVersion = headers.Get("sec-websocket-version");
		if(!websocketVersion) {
			return MalformedRequest();
		}
		if(!detail::equalsi(*websocketVersion, "13")){
			sendMyVersion = true;
		}
		
		auto websocketKey = headers.Get("sec-websocket-key");
		if(!websocketKey) {
			return MalformedRequest();
		}
		
		std::string securityKey = std::string(*websocketKey);
		
		if(m_pServer->m_fnCheckConnection && !m_pServer->m_fnCheckConnection(this, req)){
			Write("HTTP/1.1 403 Forbidden\r\n\r\n");
			Destroy();
			return;
		}
		
		
		securityKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		unsigned char hash[20];
        SHA_CTX sha1;
        SHA1_Init(&sha1);
        SHA1_Update(&sha1, securityKey.data(), securityKey.size());
        SHA1_Final(hash, &sha1);
		
		auto solvedHash = base64_encode(hash, sizeof(hash));
		
		char buf[256]; // We can use up to 101 + 27 + 28 + 1 characters, and we round up just because
		int bufLen = snprintf(buf, sizeof(buf),
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"%s"
			"Sec-WebSocket-Accept: %s\r\n\r\n",
			
			sendMyVersion ? "Sec-WebSocket-Version: 13\r\n" : "",
			solvedHash.c_str()
		);
		
		assert(bufLen >= 0 && (size_t) bufLen < sizeof(buf));
		
		Write(buf, bufLen);
		
		m_bHasCompletedHandshake = true;
		
		m_Buffer.clear();
		
		m_pServer->NotifyClientInit(this, req);
		
		return;
	}
	
	detail::Corker corker{*this};
	
	for(;;){
		if(!m_Socket) return; // No need to destroy even
		
		if(m_bUsingAlternativeProtocol){
			if(buffer.size() < 4) return Bail();
			uint32_t frameLength = ((uint32_t)(uint8_t) buffer[0]) | ((uint32_t)(uint8_t) buffer[1] << 8) | ((uint32_t)(uint8_t) buffer[2] << 16) | ((uint32_t)(uint8_t) buffer[3] << 24);
			if(frameLength > m_pServer->m_iMaxMessageSize) return Close(1002, "Too large");
			if(buffer.size() < 4 + frameLength) return Bail();
			
			ProcessDataFrame(2, (char*)buffer.data() + 4, frameLength);
			Consume(4 + frameLength);
		}else{ // Websockets
			// Not enough to read the header
			if(buffer.size() < 2) return Bail();
			
			DataFrameHeader header((char*) buffer.data());
			
			if(header.rsv1() || header.rsv2() || header.rsv3()) return Close(1002, "Reserved bit used");
			
			// Clients MUST mask their headers
			if(!header.mask()) return Close(1002, "Clients must mask their payload");
			assert(header.mask());
			
			char *curPosition = (char*) buffer.data() + 2;

			size_t frameLength = header.len();
			if(frameLength == 126){
				if(buffer.size() < 4) return Bail();
				frameLength = (*(uint8_t*)(curPosition) << 8) | (*(uint8_t*)(curPosition + 1));
				curPosition += 2;
			}else if(frameLength == 127){
				if(buffer.size() < 10) return Bail();

				frameLength = ((uint64_t)*(uint8_t*)(curPosition) << 56) | ((uint64_t)*(uint8_t*)(curPosition + 1) << 48)
					| ((uint64_t)*(uint8_t*)(curPosition + 2) << 40) | ((uint64_t)*(uint8_t*)(curPosition + 3) << 32)
					| (*(uint8_t*)(curPosition + 4) << 24) | (*(uint8_t*)(curPosition + 5) << 16)
					| (*(uint8_t*)(curPosition + 6) << 8) | (*(uint8_t*)(curPosition + 7) << 0);

				curPosition += 8;
			}

			auto amountLeft = buffer.size() - (curPosition - buffer.data());
			const char *maskKey = nullptr;
			
			{ // Read mask
				if(amountLeft < 4) return Bail();
				maskKey = curPosition;
				curPosition += 4;
				amountLeft -= 4;
			}
			
			if(frameLength > amountLeft) return Bail();
			
			{ // Unmask
				for(size_t i = 0; i < (frameLength & ~3); i += 4){
					curPosition[i + 0] ^= maskKey[0];
					curPosition[i + 1] ^= maskKey[1];
					curPosition[i + 2] ^= maskKey[2];
					curPosition[i + 3] ^= maskKey[3];
				}
				
				for(size_t i = frameLength & ~3; i < frameLength; ++i){
					curPosition[i] ^= maskKey[i % 4];
				} 
			}
			
			if(header.opcode() >= 0x08){
				if(!header.fin()) return Close(1002, "Control op codes can't be fragmented");
				if(frameLength > 125) return Close(1002, "Control op codes can't be more than 125 bytes");
				
				
				ProcessDataFrame(header.opcode(), curPosition, frameLength);
			}else if(!IsBuildingFrames() && header.fin()){
				// Fast path, we received a whole frame and we don't need to combine it with anything
				ProcessDataFrame(header.opcode(), curPosition, frameLength);
			}else{
				if(IsBuildingFrames()){
					if(header.opcode() != 0) return Close(1002, "Expected continuation frame");
				}else{
					if(header.opcode() == 0) return Close(1002, "Unexpected continuation frame");
					m_iFrameOpcode = header.opcode();
				}
				
				if(m_FrameBuffer.size() + frameLength >= m_pServer->m_iMaxMessageSize) return Close(1009, "Message too large");
				
				m_FrameBuffer.insert(m_FrameBuffer.end(), curPosition, curPosition + frameLength);
				
				if(header.fin()){
					// Assemble frame
					
					ProcessDataFrame(m_iFrameOpcode, m_FrameBuffer.data(), m_FrameBuffer.size());
					
					m_iFrameOpcode = 0;
					m_FrameBuffer.clear();
				}
				
			}
			
			Consume((curPosition - buffer.data()) + frameLength);
		}
	}
	
	// Unreachable
}


void Client::ProcessDataFrame(uint8_t opcode, char *data, size_t len){
	switch(opcode){
	case 9: // Ping
		if(m_bIsClosing) return;
		Send(data, len, 10); // Send Pong
	break;
	
	case 10: break; // Pong
	
	case 8: // Close
		if(m_bIsClosing){
			Destroy();
		}else{
			
			if(len == 1){
				Close(1002, "Incomplete close code");
				return;
			}else if(len >= 2){
				bool invalid = false;
				uint16_t code = (uint8_t(data[0]) << 8) | uint8_t(data[1]);
				if(code < 1000 || code >= 5000) invalid = true;
				
				switch(code){
				case 1004:
				case 1005:
				case 1006:
				case 1015:
					invalid = true;
				default:;
				}
				
				if(invalid){
					Close(1002, "Invalid close code");
					return;
				}
				
				if(len > 2 && !IsValidUTF8(data + 2, len - 2)){
					Close(1002, "Close reason is not UTF-8");
					return;
				}
			}
			
			// Copy close message
			m_bIsClosing = true;
			
			char header[MAX_HEADER_SIZE];
			WriteDataFrameHeader(8, len, header);
			
			uv_buf_t bufs[2];
			bufs[0].base = header;
			bufs[0].len = GetDataFrameHeaderSize(len);
			bufs[1].base = (char*) data;
			bufs[1].len = len;
			
			Write<2>(bufs);
			
			// We always close the tcp connection on our side, as allowed in 7.1.1
			Destroy();
		}
	break;
	
	case 1: // Text
	case 2: // Binary
		if(m_bIsClosing) return;
		if(opcode == 1 && !IsValidUTF8(data, len)) return Close(1007, "Invalid UTF-8 in text frame");
		
		m_pServer->NotifyClientData(this, data, len, opcode);
	break;
	
	default:
		return Close(1002, "Unknown op code");
	}
}

void Client::SetCloseFlag(bool flag) {
	log_infof("update close flag:%d", flag);
    m_bIsClosing = flag;
}

void Client::Close(uint16_t code, const char *reason, size_t reasonLen){
	if(m_bIsClosing) return;
	
	m_bIsClosing = true;
	
	log_infof("websocket client close...");
	if(!m_bUsingAlternativeProtocol){
		char coded[2];
		coded[0] = (code >> 8) & 0xFF;
		coded[1] = (code >> 0) & 0xFF;
		
		if(reason == nullptr){
			Send(coded, sizeof(coded), 8);
		}else{
			if(reasonLen == (size_t) -1) reasonLen = strlen(reason);
			
			char header[MAX_HEADER_SIZE];
			WriteDataFrameHeader(8, 2 + reasonLen, header);
			
			uv_buf_t bufs[2];
			bufs[0].base = header;
			bufs[0].len = GetDataFrameHeaderSize(2 + reasonLen);
			bufs[1].base = (char*) reason;
			bufs[1].len = reasonLen;
			
			Write<2>(bufs);
		}
	}
	
	// We always close the tcp connection on our side, as allowed in 7.1.1
	Destroy();
}


void Client::Send(const char *data, size_t len, uint8_t opcode){
	if(!m_Socket) return;
	if(m_bIsClosing) return;
	
	if(m_bUsingAlternativeProtocol){
		uint32_t len32 = (uint32_t) len;
		uint8_t header[4];
		header[0] = (len32 >>  0) & 0xFF;
		header[1] = (len32 >>  8) & 0xFF;
		header[2] = (len32 >> 16) & 0xFF;
		header[3] = (len32 >> 24) & 0xFF;
		
		uv_buf_t bufs[2];
		bufs[0].base = (char*) header;
		bufs[0].len = 4;
		bufs[1].base = (char*) data;
		bufs[1].len = len;
		
		Write<2>(bufs);
	}else{
		char header[MAX_HEADER_SIZE];
		WriteDataFrameHeader(opcode, len, header);
		
		uv_buf_t bufs[2];
		bufs[0].base = header;
		bufs[0].len = GetDataFrameHeaderSize(len);
		bufs[1].base = (char*) data;
		bufs[1].len = len;
		
		Write<2>(bufs);
	}
}

void Client::InitSecure(){
	m_pTLS = std::make_unique<TLS>(m_pServer->GetSSLContext());
}

void Client::FlushTLS(){
	assert(m_pTLS != nullptr);
	m_pTLS->ForEachPendingWrite([&](const char *data, size_t len){
		uv_buf_t bufs[1];
		bufs[0].base = (char*) data;
		bufs[0].len = len;
		WriteRaw<1>(bufs);
	});
}

void Client::Cork(bool v){
	if(!m_Socket) return;
	
#if defined(TCP_CORK) || defined(TCP_NOPUSH)
	
	int enable = v;
	uv_os_fd_t fd;
	uv_fileno((uv_handle_t*) m_Socket.get(), &fd);
	
	// Shamelessly copied from uWebSockets
#if defined(TCP_CORK)
	// Linux
	setsockopt(fd, IPPROTO_TCP, TCP_CORK, &enable, sizeof(int));
#elif defined(TCP_NOPUSH)
	// Mac OS X & FreeBSD
	setsockopt(fd, IPPROTO_TCP, TCP_NOPUSH, &enable, sizeof(int));
	
	// MacOS needs this to flush the messages out
	if(!enable){
		::send(fd, "", 0, 0);
	}
#endif
	
#endif
}


}
