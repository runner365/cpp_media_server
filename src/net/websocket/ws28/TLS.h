#ifndef H_3078F0E9644347BD9F28E4C56F162FC8
#define H_3078F0E9644347BD9F28E4C56F162FC8

#include <vector>
#include <mutex>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

namespace ws28 {

// Ported from https://github.com/darrenjs/openssl_examples
// MIT licensed
class TLS {
	enum SSLStatus {
		SSLSTATUS_OK, SSLSTATUS_WANT_IO, SSLSTATUS_FAIL
	};
	
public:
	
	TLS(SSL_CTX *ctx, bool server = true, const char *hostname = nullptr){
		m_ReadBIO = BIO_new(BIO_s_mem());
		m_WriteBIO = BIO_new(BIO_s_mem());
		m_SSL = SSL_new(ctx);
		
		if(server){
			SSL_set_accept_state(m_SSL);
		}else{
			SSL_set_connect_state(m_SSL);
		}
		
		if(!server && hostname) SSL_set_tlsext_host_name(m_SSL, hostname);
		SSL_set_bio(m_SSL, m_ReadBIO, m_WriteBIO);
		
		if(!server) DoSSLHandhake();
	}
	
	~TLS(){
		SSL_free(m_SSL);
	}
	
	TLS(const TLS &other) = delete;
	TLS& operator=(const TLS &other) = delete;
	
	// Helper to setup SSL, you still need to create the context
	static void InitSSL(){
		static std::once_flag f;
		std::call_once(f, [](){
			SSL_library_init();
			OpenSSL_add_all_algorithms();
			SSL_load_error_strings();
			ERR_load_BIO_strings();
			ERR_load_crypto_strings();
		});
	}
	
	// Writes unencrypted bytes to be encrypted and sent out
	// If this returns false, the connection must be closed
	bool Write(const char *buf, size_t len){
		m_EncryptBuf.insert(m_EncryptBuf.end(), buf, buf + len);
		return DoEncrypt();
	}
	
	// Process raw bytes received from the other side
	// If this returns false, the connection must be closed
	template<typename F>
	bool ReceivedData(const char *src, size_t len, const F &f){
		int n;
		while(len > 0){
			n = BIO_write(m_ReadBIO, src, len);
			
			// Assume bio write failure is unrecoverable
			if(n <= 0) return false;
			
			src += n;
			len -= n;
			
			if(!SSL_is_init_finished(m_SSL)){
				if(DoSSLHandhake() == SSLSTATUS_FAIL) return false;
				if(!SSL_is_init_finished(m_SSL)) return true;
			}
			
			ERR_clear_error();
			do {
				char buf[4096];
				n = SSL_read(m_SSL, buf, sizeof buf);
				if(n > 0){
					f(buf, (size_t) n);
				}
			}while(n > 0);
			
			auto status = GetSSLStatus(n);
			if(status == SSLSTATUS_WANT_IO){
				do {
					char buf[4096];
					n = BIO_read(m_WriteBIO, buf, sizeof(buf));
					if(n > 0){
						QueueEncrypted(buf, n);
					}else if(!BIO_should_retry(m_WriteBIO)){
						return false;
					}
				}while(n > 0);
			}else if(status == SSLSTATUS_FAIL){
				return false;
			}
		}
		
		return true;
	}
	
	template<typename F>
	void ForEachPendingWrite(const F &f){
		// If the callback does something crazy like calling Write inside of it
		// We need to handle this carefully, thus the swap.
		for(;;){
			if(m_WriteBuf.empty()) return;
			
			std::vector<char> buf;
			std::swap(buf, m_WriteBuf);
			
			f(buf.data(), buf.size());
		}
	}
	
	bool IsHandshakeFinished(){
		return SSL_is_init_finished(m_SSL);
	}
	
private:
	SSLStatus GetSSLStatus(int n){
		switch(SSL_get_error(m_SSL, n)){
			case SSL_ERROR_NONE:
				return SSLSTATUS_OK;
				
			case SSL_ERROR_WANT_WRITE:
			case SSL_ERROR_WANT_READ:
				return SSLSTATUS_WANT_IO;
				
			case SSL_ERROR_ZERO_RETURN:
			case SSL_ERROR_SYSCALL:
			default:
				return SSLSTATUS_FAIL;
		}
	}
	
	void QueueEncrypted(const char *buf, size_t len){
		m_WriteBuf.insert(m_WriteBuf.end(), buf, buf + len);
	}
	
	bool DoEncrypt(){
		if(!SSL_is_init_finished(m_SSL)) return true;
		
		int n;
		
		while(!m_EncryptBuf.empty()){
			ERR_clear_error();
			n = SSL_write(m_SSL, m_EncryptBuf.data(), (int) m_EncryptBuf.size());
			
			if(GetSSLStatus(n) == SSLSTATUS_FAIL) return false;
			
			if(n > 0){
				// Consume bytes
				m_EncryptBuf.erase(m_EncryptBuf.begin(), m_EncryptBuf.begin() + n);
				
				// Write them out
				do {
					char buf[4096];
					n = BIO_read(m_WriteBIO, buf, sizeof buf);
					if(n > 0){
						QueueEncrypted(buf, n);
					}else if(!BIO_should_retry(m_WriteBIO)){
						return false;
					}
				}while(n > 0);
			}
		}
		
		return true;
	}
	
	SSLStatus DoSSLHandhake(){
		ERR_clear_error();
		SSLStatus status = GetSSLStatus(SSL_do_handshake(m_SSL));
		
		// Did SSL request to write bytes?
		if(status == SSLSTATUS_WANT_IO){
			int n;
			do {
				char buf[4096];
				n = BIO_read(m_WriteBIO, buf, sizeof buf);
				
				if(n > 0){
					QueueEncrypted(buf, n);
				}else if(!BIO_should_retry(m_WriteBIO)){
					return SSLSTATUS_FAIL;
				}
				
			} while(n > 0);
		}
		
		return status;
	}
	
	
	
	std::vector<char> m_EncryptBuf; // Bytes waiting to be encrypted
	std::vector<char> m_WriteBuf; // Bytes waiting to be written to the socket
	
	SSL *m_SSL;
	BIO *m_ReadBIO;
	BIO *m_WriteBIO;
};

}

#endif
