#ifndef H_0AC5AB22DD724A3F8FE93E27C178D633
#define H_0AC5AB22DD724A3F8FE93E27C178D633

#include <cstdint>
#include <memory>
#include <uv.h>
#include <algorithm>
#include <map>

#include "Headers.h"
#include "TLS.h"

namespace ws28 {
	namespace detail {
		struct Corker;
		struct SocketDeleter {
			void operator()(uv_tcp_t *socket) const {
				if(socket == nullptr) return;
				uv_close((uv_handle_t*) socket, [](uv_handle_t *h){
					delete (uv_tcp_t*) h;
				});
			}
		};
	}
	
	typedef std::unique_ptr<uv_tcp_t, detail::SocketDeleter> SocketHandle;
	
	class Server;
	class Client {
		enum { MAX_HEADER_SIZE = 10 };
		enum : unsigned char { NO_FRAMES = 0 };
	public:
		~Client();
		
		// If reasonLen is -1, it'll use strlen
		void Close(uint16_t code, const char *reason = nullptr, size_t reasonLen = -1);
		void Destroy();
		void Send(const char *data, size_t len, uint8_t opCode = 2);
		
		inline void SetUserData(void *v){ m_pUserData = v; }
		inline void* GetUserData(){ return m_pUserData; }
		
		inline bool IsSecure(){ return m_pTLS != nullptr; }
		inline bool IsUsingAlternativeProtocol(){ return m_bUsingAlternativeProtocol; }
		
		inline Server* GetServer(){ return m_pServer; }
		
		inline const char* GetIP(){ return m_IP; }
		
		void SetCloseFlag(bool flag);

	private:
		
		struct DataFrame {
			uint8_t opcode;
			std::unique_ptr<char[]> data;
			size_t len;
		};
		
		Client(Server *server, SocketHandle socket);
		
		Client(const Client &other) = delete;
		Client& operator=(Client &other) = delete;
		
		size_t GetDataFrameHeaderSize(size_t len);
		void WriteDataFrameHeader(uint8_t opcode, size_t len, char *out);
		void EncryptAndWrite(const char *data, size_t len);
		
		void OnRawSocketData(char *data, size_t len);
		void OnSocketData(char *data, size_t len);
		void ProcessDataFrame(uint8_t opcode, char *data, size_t len);
		
		void InitSecure();
		void FlushTLS();
		
		void Write(const char *data);
		void Write(const char *data, size_t len);
		
		template<size_t N>
		void Write(uv_buf_t bufs[N]);
		
		template<size_t N>
		void WriteRaw(uv_buf_t bufs[N]);
		
		void WriteRawQueue(std::unique_ptr<char[]> data, size_t len);
		
		void Cork(bool v);
		
		// Stub, maybe some day
		inline bool IsValidUTF8(const char *str, size_t len){ return true; }
		
		inline bool IsBuildingFrames(){ return m_iFrameOpcode != NO_FRAMES; }
		
		Server *m_pServer;
		SocketHandle m_Socket;
		void *m_pUserData = nullptr;
		bool m_bWaitingForFirstPacket = true;
		bool m_bHasCompletedHandshake = false;
		bool m_bIsClosing = false;
		bool m_bUsingAlternativeProtocol = false;
		char m_IP[46];
		
		std::unique_ptr<TLS> m_pTLS;
		
		std::vector<char> m_Buffer;
		
		uint8_t m_iFrameOpcode = NO_FRAMES;
		std::vector<char> m_FrameBuffer;
		
		friend class Server;
		friend struct detail::Corker;
		friend class std::unique_ptr<Client>;
	};
	
}

#endif
