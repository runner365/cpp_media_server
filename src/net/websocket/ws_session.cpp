#include "ws_session.hpp"
#include "ws_server.hpp"
#include "utils/stringex.hpp"
#include "utils/base64.h"
#include "utils/logger.hpp"
#include <iostream>
#include <openssl/sha.h>

websocket_session::websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                websocket_server* server,
                websocket_server_callbackI* cb):server_(server)
                                            , cb_(cb)
{
    close_ = false;
    session_ptr_ = std::make_unique<tcp_session>(loop, handle, this);
    uuid_ = make_uuid();

    session_ptr_->async_read();
    log_infof("websocket session construct uuid:%s, ssl disable", uuid_.c_str());
}

websocket_session::websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                websocket_server* server, websocket_server_callbackI* cb,
                const std::string& key_file, const std::string& cert_file):server_(server)
                                                                        , cb_(cb)
{
    close_ = false;
    session_ptr_ = std::make_unique<tcp_session>(loop, handle, this, key_file, cert_file);
    uuid_ = make_uuid();

    session_ptr_->async_read();
    log_infof("websocket session construct uuid:%s, ssl enable", uuid_.c_str());
}

websocket_session::~websocket_session()
{
    log_infof("websocket session destruct");
}

std::string websocket_session::get_uuid() {
    return uuid_;
}

void websocket_session::on_write(int ret_code, size_t sent_size) {

}

void websocket_session::on_read(int ret_code, const char* data, size_t data_size) {
    if (ret_code != 0) {
        return;
    }

    recv_buffer_.append_data(data, data_size);
    
    int ret;

    if (!http_request_ready_) {
        try {
            ret = on_handle_http_request();
            if (ret == WS_RET_READ_MORE) {
                session_ptr_->async_read();
            } else if (ret == WS_RET_OK) {
                ret = send_http_response();
                if (ret != WS_RET_OK) {
                    if (server_ != nullptr) {
                        server_->session_close(this);
                    }
                    return;
                }
                recv_buffer_.reset();
                session_ptr_->async_read();
            } else {
                send_error_response();
                if (server_ != nullptr) {
                    server_->session_close(this);
                }
            }
        } catch(const MediaServerError& e) {
            send_error_response();
            log_errorf("exception:%s", e.what());
            if (server_ != nullptr) {
                server_->session_close(this);
            }
        }
        return;
    }


    log_infof("recv data size:%lu", data_size);
    on_handle_frame();
}

void websocket_session::on_handle_frame() {
    int ret = 0;

    log_info_data((uint8_t*)recv_buffer_.data(), recv_buffer_.data_len(), "ws frame");
    ret = frame_.parse((uint8_t*)recv_buffer_.data(), recv_buffer_.data_len());
    if (ret != 0) {
        session_ptr_->async_read();
        return;
    }

    if (!frame_.payload_is_ready()) {
        return;
    }

    log_infof("ws frame is ready, opcode:%d", frame_.get_oper_code());
    if (frame_.get_oper_code() == WS_OP_PING_TYPE) {
        handle_ws_ping();
    } else if (frame_.get_oper_code() == WS_OP_PONG_TYPE) {
        log_infof("receive ws pong");
        return;
    } else if (frame_.get_oper_code() == WS_OP_CLOSE_TYPE) {
        handle_ws_close(frame_.get_payload_data(), frame_.get_payload_len());
    } else if (frame_.get_oper_code() == WS_OP_CONTINUE_TYPE) {

    } else if (frame_.get_oper_code() == WS_OP_TEXT_TYPE) {
        handle_ws_text(frame_.get_payload_data(), frame_.get_payload_len());
    } else if (frame_.get_oper_code() == WS_OP_BIN_TYPE) {

    } else {
        log_errorf("websocket opcode:%d not handle", frame_.get_oper_code());
    }


    recv_buffer_.consume_data(frame_.get_payload_len() + frame_.get_payload_start());
    frame_.reset();
    log_infof("after consume recv buffer, buffer len:%lu", recv_buffer_.data_len());
}

void websocket_session::handle_ws_ping() {
    send_ws_frame(frame_.get_payload_data(), frame_.get_payload_len(), WS_OP_PONG_TYPE);
}

void websocket_session::handle_ws_text(uint8_t* data, size_t len) {
    if (cb_) {
        cb_->on_read(this, (char*)data, len);
    }
}

void websocket_session::handle_ws_bin(uint8_t* data, size_t len) {

}

void websocket_session::handle_ws_close(uint8_t* data, size_t len) {
    if (close_) {
        return;
    }

    if (len <= 1) {
        send_close(1002, "Incomplete close code");
    } else {
		bool invalid = false;
		uint16_t code = (uint8_t(data[0]) << 8) | uint8_t(data[1]);
		if(code < 1000 || code >= 5000) {
            invalid = true;
        }
		
		switch(code){
		    case 1004:
		    case 1005:
		    case 1006:
		    case 1015:
		    	invalid = true;
		    default:;
		}
		
		if(invalid){
			send_close(1002, "Invalid close code");
			return;
		}

        send_ws_frame(data, len, WS_OP_CLOSE_TYPE);
    }

    close_ = true;
    session_ptr_->close();
    log_infof("tcp sesseion closed");
}

void websocket_session::send_close(uint16_t code, const char *reason, size_t reason_len) {
    if(reason_len == 0) {
        reason_len = strlen(reason);
    }
    
    send_ws_frame((uint8_t*)reason, reason_len, WS_OP_CLOSE_TYPE);
}

void websocket_session::send_data_text(const char* data, size_t len) {
    send_ws_frame((uint8_t*)data, len, WS_OP_TEXT_TYPE);
}

void websocket_session::send_ws_frame(uint8_t* data, size_t len, uint8_t op_code) {
    const size_t MAX_HEADER_LEN = 10;
    WS_PACKET_HEADER* ws_header;
    uint8_t* header_start = new uint8_t[MAX_HEADER_LEN + len];
    size_t header_len = 2;

    ws_header = (WS_PACKET_HEADER*)header_start;
    memset(header_start, 0, MAX_HEADER_LEN);
    ws_header->fin = 1;
    ws_header->opcode = op_code;

    if (len >= 126) {
		if(len > UINT16_MAX){
			ws_header->payload_len = 127;
			*(uint8_t*)(header_start + 2) = (len >> 56) & 0xFF;
			*(uint8_t*)(header_start + 3) = (len >> 48) & 0xFF;
			*(uint8_t*)(header_start + 4) = (len >> 40) & 0xFF;
			*(uint8_t*)(header_start + 5) = (len >> 32) & 0xFF;
			*(uint8_t*)(header_start + 6) = (len >> 24) & 0xFF;
			*(uint8_t*)(header_start + 7) = (len >> 16) & 0xFF;
			*(uint8_t*)(header_start + 8) = (len >> 8) & 0xFF;
			*(uint8_t*)(header_start + 9) = (len >> 0) & 0xFF;
            header_len = MAX_HEADER_LEN;
		} else{
			ws_header->payload_len = 126;
			*(uint8_t*)(header_start + 2) = (len >> 8) & 0xFF;
			*(uint8_t*)(header_start + 3) = (len >> 0) & 0xFF;
            header_len = 4;
		}
    } else {
        ws_header->payload_len = len;
        header_len = 2;
    }

    memcpy(header_start +  header_len, data, len);
    log_info_data(header_start, header_len + len, "send header data");
    session_ptr_->async_write((char*)header_start, header_len + len);

    delete[] header_start;
}

void websocket_session::send_error_response() {
    std::string resp_msg = "HTTP/1.1 400 Bad Request\r\n\r\n";

    log_infof("send error message:%s", resp_msg.c_str());
    session_ptr_->async_write(resp_msg.c_str(), resp_msg.length());
}

int websocket_session::send_http_response() {
    std::stringstream ss;

    gen_hashcode();

    ss << "HTTP/1.1 101 Switching Protocols" << "\r\n";
    ss << "Sec-WebSocket-Version: 13" << "\r\n";
    ss << "Upgrade: websocket" << "\r\n";
    ss << "Connection: Upgrade" << "\r\n";
    ss << "Sec-WebSocket-Accept: " << hash_code_ << "\r\n";
    /*
    if (sec_ws_protocol_.empty()) {
        ss << "Sec-WebSocket-Protocol: "  << "binary" << "\r\n";
    } else {
        ss << "Sec-WebSocket-Protocol: "  << sec_ws_protocol_ << "\r\n";
    }
    */
    ss << "\r\n";

    log_infof("send response:%s", ss.str().c_str());
    session_ptr_->async_write(ss.str().c_str(), ss.str().length() + 1);
    return WS_RET_OK;
}

int websocket_session::on_handle_http_request() {
    std::string content(recv_buffer_.data(), recv_buffer_.data_len());

    size_t pos = content.find("\r\n\r\n");
    if (pos == content.npos) {
        return WS_RET_READ_MORE;
    }
    std::vector<std::string> lines;

    http_request_ready_ = true;
    content = content.substr(0, pos);

    int ret = string_split(content, "\r\n", lines);
    if (ret <= 0 || lines.empty()) {
        throw MediaServerError("websocket http header error");
    }

    //headers_
    std::vector<std::string> http_items;
    string_split(lines[0], " ", http_items);
    if (http_items.size() != 3) {
        log_infof("http header error:%s", lines[0].c_str());
        throw MediaServerError("websocket http header error");
    }
    method_ = http_items[0];
    uri_ = http_items[1];

    log_infof("http method:%s", method_.c_str());
    log_infof("http uri:%s", uri_.c_str());
    
    string2lower(method_);
    
    int index = 0;
    for (auto& line : lines) {
        if (index++ == 0) {
            continue;
        }

        size_t pos = line.find(" ");
        if (pos == line.npos) {
            continue;
        }
        std::string key = line.substr(0, pos - 1);//remove ':'
        std::string value = line.substr(pos + 1);

        string2lower(key);
        headers_[key] = value;
        log_infof("http header:%s %s", key.c_str(), value.c_str());
    }

    auto connection_iter = headers_.find("connection");
    if (connection_iter == headers_.end()) {
        throw MediaServerError("websocket http header error: Connection not exist");
    }
    string2lower(connection_iter->second);
    if (connection_iter->second != "upgrade") {
        log_errorf("http header error:%s %s",
                connection_iter->first.c_str(),
                connection_iter->second.c_str());
        throw MediaServerError("websocket http header error: Connection is not upgrade");
    }

    auto upgrade_iter = headers_.find("upgrade");
    if (upgrade_iter == headers_.end()) {
        throw MediaServerError("websocket http header error: Upgrade not exist");
    }
    string2lower(upgrade_iter->second);
    if (upgrade_iter->second != "websocket") {
        log_errorf("http header error:%s %s",
                connection_iter->first.c_str(),
                connection_iter->second.c_str());
        throw MediaServerError("websocket http header error: upgrade is not websocket");
    }

    auto ver_iter = headers_.find("sec-websocket-version");
    if (ver_iter != headers_.end()) {
        sec_ws_ver_ = atoi(ver_iter->second.c_str());
    } else {
        sec_ws_ver_ = 13;
    }

    auto key_iter = headers_.find("sec-websocket-key");
    if (key_iter != headers_.end()) {
        sec_ws_key_ = key_iter->second;
    } else {
        throw MediaServerError("websocket http header error: Sec-WebSocket-Key not exist");
    }

    auto protocal_iter = headers_.find("sec-webSocket-protocol");
    if (protocal_iter != headers_.end()) {
        sec_ws_protocol_ = protocal_iter->second;
    }

    return WS_RET_OK;
}

void websocket_session::gen_hashcode() {
    std::string sec_key = sec_ws_key_;
	sec_key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	unsigned char hash[20];
    SHA_CTX sha1;

    SHA1_Init(&sha1);
    SHA1_Update(&sha1, sec_key.data(), sec_key.size());
    SHA1_Final(hash, &sha1);
	
	hash_code_ = base64_encode(hash, sizeof(hash));
    return;
}

std::string websocket_session::method() {
    return method_;
}

std::string websocket_session::remote_address() {
    return session_ptr_->get_remote_endpoint();
}

websocket_server* websocket_session::get_server() {
    return server_;
}

void websocket_session::set_close(bool flag) {
    close_ = flag;
}

bool websocket_session::is_close() {
    return close_;
}

std::string websocket_session::get_uri() {
    return uri_;
}
