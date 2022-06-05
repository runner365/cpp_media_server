#ifndef SSL_SERVER_HPP
#define SSL_SERVER_HPP
#include "logger.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <iostream>
#include <stdio.h>
#include <queue>
#include <sstream>
#include <openssl/ssl.h>
#include <assert.h>

#define SSL_DEF_RECV_BUFFER_SIZE (10*1024)

typedef enum {
    TLS_SSL_ZERO,
    TLS_SSL_INIT_DONE      = 1,
    TLS_SSL_HELLO_DONE     = 2,
    TLS_KEY_EXCHANGE_DONE  = 3,
    TLS_DATA_RECV_STATE    = 4
} TLS_STATE;

class ssl_server_callbackI
{
public:
    virtual void plaintext_data_send(const char* data, size_t len) = 0;
    virtual void plaintext_data_recv(const char* data, size_t len) = 0;
    virtual void encrypted_data_send(const char* data, size_t len) = 0;
};

class ssl_server
{
public:
    ssl_server(const std::string& key_file,
            const std::string& cert_file,
            ssl_server_callbackI* cb):key_file_(key_file)
                                , cert_file_(cert_file)
                                , cb_(cb)
    {
        plaintext_data_ = new uint8_t[plaintext_data_len_];
        log_infof("ssl_server construct ...");
    }
    ~ssl_server() {
        if (ssl_) {
            SSL_free(ssl_);
            ssl_ = NULL;
        }
    
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = NULL;
        }
        if (plaintext_data_) {
            delete[] plaintext_data_;
            plaintext_data_ = nullptr;
        }
    }

public:
    TLS_STATE get_state() {
        return tls_state_;
    }

    int handshake(char* buf, size_t nn) {
        int ret = 0;

        if (tls_state_ >= TLS_KEY_EXCHANGE_DONE) {
            return 0;
        }

        ret = ssl_init();
        if (ret != 0) {
            return ret;
        }
        switch (tls_state_)
        {
            case TLS_SSL_INIT_DONE:
            {
                ret = handle_tls_hello(buf, nn);
                if (ret != 0) {
                    return ret;
                }
                tls_state_ = TLS_SSL_HELLO_DONE;
                break;
            }
            case TLS_SSL_HELLO_DONE:
            {
                ret = handle_key_exchange(buf, nn);
                if (ret != 0) {
                    return ret;
                }
                break;
            }
            default:
                break;
        }
        return 0;
    }


    int handle_ssl_data_recv(uint8_t* data, size_t len) {
        if (tls_state_ < TLS_KEY_EXCHANGE_DONE) {
            return 0;
        }

        if (tls_state_ == TLS_KEY_EXCHANGE_DONE) {
            tls_state_ = TLS_DATA_RECV_STATE;
            return 0;
        }

        int r0 = BIO_write(bio_in_, data, len);
        if (r0 <= 0) {
            log_errorf("BIO_write error:%d", r0);
            return -1;
        }
        //while(true) {
            r0 = SSL_read(ssl_, plaintext_data_, plaintext_data_len_);
            int r1 = SSL_get_error(ssl_, r0);
            int r2 = BIO_ctrl_pending(bio_in_);
            int r3 = SSL_is_init_finished(ssl_);

            // OK, got data.
            if (r0 > 0) {
                cb_->plaintext_data_recv((char*)plaintext_data_, r0);
            } else {
                log_debugf("SSL_read error, r0:%d, r1:%d, r2:%d, r3:%d",
                        r0, r1, r2, r3);
                //break;
            }
        //}
        return 0;
    }

    int ssl_write(uint8_t* plain_text_data, size_t len) {
        int writen_len = 0;

        for (char* p = (char*)plain_text_data; p < (char*)plain_text_data + len;) {
            int left = (int)len - (p - (char*)plain_text_data);
            int r0 = SSL_write(ssl_, (const void*)p, left);
            int r1 = SSL_get_error(ssl_, r0);
            if (r0 <= 0) {
                log_errorf("ssl write data=%p, size=%d, r0=%d, r1=%d",
                        p, left, r0, r1);
                return -1;
            }
    
            // Move p to the next writing position.
            p += r0;
            writen_len += (ssize_t)r0;
    
            uint8_t* data = NULL;
            int size = BIO_get_mem_data(bio_out_, &data);
            cb_->encrypted_data_send((char*)data, size);

            if ((r0 = BIO_reset(bio_out_)) != 1) {
                log_errorf("BIO_reset r0=%d", r0);
                return -1;
            }
        }

        return writen_len;
    }

private:
    int ssl_init() {
        if (tls_state_ >= TLS_SSL_INIT_DONE) {
            return 0;
        }

    #if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
        ssl_ctx_ = SSL_CTX_new(TLS_method());
    #else
        ssl_ctx_ = SSL_CTX_new(TLSv1_2_method());
    #endif
        SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, NULL);
        assert(SSL_CTX_set_cipher_list(ssl_ctx_, "ALL") == 1);

        if ((ssl_ = SSL_new(ssl_ctx_)) == NULL) {
            log_errorf("ssl new error");
            return -1;
        }
    
        if ((bio_in_ = BIO_new(BIO_s_mem())) == NULL) {
            log_errorf("BIO_new in error");
            return -1;
        }
    
        if ((bio_out_ = BIO_new(BIO_s_mem())) == NULL) {
            log_errorf("BIO_new in error");
            BIO_free(bio_in_);
            return -1;
        }
    
        SSL_set_bio(ssl_, bio_in_, bio_out_);
    
        // SSL setup active, as server role.
        SSL_set_accept_state(ssl_);
        SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE);
    
        int r0;
    
        // Setup the key and cert file for server.
        if ((r0 = SSL_use_certificate_file(ssl_, cert_file_.c_str(), SSL_FILETYPE_PEM)) != 1) {
            log_errorf("SSL_use_certificate_file error, cert file:%s, return %d", cert_file_.c_str(), r0);
            return -1;
        }
    
        if ((r0 = SSL_use_RSAPrivateKey_file(ssl_, key_file_.c_str(), SSL_FILETYPE_PEM)) != 1) {
            log_errorf("SSL_use_RSAPrivateKey_file error, key file:%s, return %d", key_file_.c_str(), r0);
            return -1;
        }
    
        if ((r0 = SSL_check_private_key(ssl_)) != 1) {
            log_errorf("SSL_check_private_key error, return %d", r0);
            return -1;
        }
        tls_state_ = TLS_SSL_INIT_DONE;

        log_infof("ssl init done ....");
        
        return 0;
    }


    int handle_tls_hello(char* buf, size_t nn) {
        int r0 = 0;
        int r1 = 0;
        size_t size = 0;
        uint8_t* data = nullptr;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            log_errorf("client hello BIO_write error:%d", r0);
            return -1;
        }
    
        r0 = SSL_do_handshake(ssl_);
        r1 = SSL_get_error(ssl_, r0);
        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            log_errorf("client hello BIO_write error r0:%d, r1:%d", r0, r1);
            return -1;
        }
    
        if ((size = BIO_get_mem_data(bio_out_, &data)) > 0) {
            if ((r0 = BIO_reset(bio_in_)) != 1) {
                log_infof("BIO_reset error:%d", r0);
                return -1;
            }
        }

        size = BIO_get_mem_data(bio_out_, &data);
        if (!data || size <= 0) {
            log_errorf("BIO_get_mem_data error");
            return -1;
        }

        cb_->plaintext_data_send((char*)data, size);

        if ((r0 = BIO_reset(bio_out_)) != 1) {
            log_errorf("BIO_reset error:%d", r0);
            return -1;
        }

        return 0;
    }

    int handle_key_exchange(char* buf, size_t nn) {
        int r0 = 0;
        int r1 = 0;
        size_t size = 0;
        char* data  = nullptr;

        if ((r0 = BIO_write(bio_in_, buf, nn)) <= 0) {
            log_errorf("BIO_write error:%d", r0);
            return -1;
        }

        r0 = SSL_do_handshake(ssl_);
        r1 = SSL_get_error(ssl_, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            size = BIO_get_mem_data(bio_out_, &data);
            if (!data || size <= 0) {
                log_errorf("BIO_get_mem_data error");
                return -1;
            }
        } else {
            if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
                log_errorf("SSL_do_handshake error, r0:%d, r1:%d", r0, r1);
                return -1;
            }
    
            if ((size = BIO_get_mem_data(bio_out_, &data)) > 0) {
                if ((r0 = BIO_reset(bio_in_)) != 1) {
                    log_errorf("BIO_reset error");
                    return -1;
                }
            }
        }

        // Send New Session Ticket, Change Cipher Spec, Encrypted Handshake Message
        cb_->plaintext_data_send((char*)data, size);

        if ((r0 = BIO_reset(bio_out_)) != 1) {
            log_errorf("BIO_reset error");
            return -1;
        }
        tls_state_ = TLS_KEY_EXCHANGE_DONE;
        return 0;
    }

private:
    std::string key_file_;
    std::string cert_file_;
    ssl_server_callbackI* cb_ = nullptr;

private:
    SSL_CTX* ssl_ctx_ = nullptr;
    SSL* ssl_         = nullptr;
    BIO* bio_in_      = nullptr;
    BIO* bio_out_     = nullptr;
    uint8_t* plaintext_data_ = nullptr;
    size_t plaintext_data_len_ = SSL_DEF_RECV_BUFFER_SIZE;

private:
    TLS_STATE tls_state_ = TLS_SSL_ZERO;
};

#endif //SSL_SERVER_HPP