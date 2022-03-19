#ifndef RTC_DTLS_HPP
#define RTC_DTLS_HPP
#include "webrtc_pub.hpp"
#include "srtp_session.hpp"
#include "utils/timer.hpp"
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#define SSL_READ_BUFFER_SIZE 65536

typedef enum
{
    DTLS_NEW = 1,
    DTLS_CONNECTING,
    DTLS_CONNECTED,
    DTLS_FAILED,
    DTLS_CLOSED
} DTLS_STATE;

typedef enum
{
    ROLE_NONE = 0,
    ROLE_AUTO = 1,
    ROLE_CLIENT,
    ROLE_SERVER
} DTLS_ROLE;

typedef enum
{
    NONE                    = 0,
    AES_CM_128_HMAC_SHA1_80 = 1,
    AES_CM_128_HMAC_SHA1_32,
    AEAD_AES_256_GCM,
    AEAD_AES_128_GCM
} crypto_suite_enum;

typedef enum
{
    FINGER_NONE = 0,
    FINGER_SHA1 = 1,
    FINGER_SHA224,
    FINGER_SHA256,
    FINGER_SHA384,
    FINGER_SHA512
}finger_print_algorithm_enum;

typedef struct srtp_crypto_suite_map_s
{
    crypto_suite_enum crypto_suite;
    std::string name;
} srtp_crypto_suite_map;

typedef struct finger_print_info_s
{
    finger_print_algorithm_enum algorithm = FINGER_NONE;
    std::string value;
} finger_print_info;

extern std::vector<srtp_crypto_suite_map> srtp_crypto_suite_vec;
extern std::unordered_map<std::string, finger_print_algorithm_enum> string2finger_print_algorithm;
extern std::map<finger_print_algorithm_enum, std::string> finger_print_algorithm2String;

class webrtc_session;

class rtc_dtls : public timer_interface
{
public:
    rtc_dtls(webrtc_session* session, boost::asio::io_context& io_ctx);
    ~rtc_dtls();

public:
    void start(DTLS_ROLE role_mode);
    void handle_dtls_data(const uint8_t* data, size_t data_len);
    void on_ssl_info(int type, int value);

private:
    bool check_status(int ret_code);
    void send_pending_dtls_data();
    bool process_handshake();
    bool check_remote_fingerprint();
    CRYPTO_SUITE_ENUM get_srtp_crypto_suite();
    void extract_srtp_keys(CRYPTO_SUITE_ENUM srtp_suite);

protected:
    virtual void on_timer() override;
    
public:
    static void dtls_init(const std::string& key_file, const std::string& cert_file);
    static int on_ssl_certificate_verify(int, X509_STORE_CTX*);
    static bool is_dtls(const uint8_t* data, size_t len);

public:
    static std::vector<finger_print_info> s_local_fingerprint_vec;
    
    static X509* s_certificate;
    static EVP_PKEY* s_privatekey;
    static SSL_CTX* s_ssl_ctx;

public:
    DTLS_STATE state;
    DTLS_ROLE role;
    finger_print_info remote_finger_print;
    std::string remote_cert;

private://for dtls
    SSL* ssl_ = nullptr;
    BIO* ssl_bio_read_  = nullptr; // The BIO from which ssl reads.
    BIO* ssl_bio_write_ = nullptr; // The BIO in which ssl writes.
    uint8_t* ssl_read_buffer_ = nullptr;
    bool handshake_done_ = false;

    webrtc_session* session_ = nullptr;
};


#endif//RTC_DTLS_HPP
