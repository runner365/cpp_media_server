#ifndef SRTP_SESSION_HPP
#define SRTP_SESSION_HPP
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <srtp.h>

typedef enum
{
    CRYPTO_SUITE_NONE                    = 0,
    CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80 = 1,
    CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32,
    CRYPTO_SUITE_AEAD_AES_256_GCM,
    CRYPTO_SUITE_AEAD_AES_128_GCM
} CRYPTO_SUITE_ENUM;

typedef struct SRTP_CRYPTO_SUITE_ENTRY_S
{
    CRYPTO_SUITE_ENUM crypto_suite;
    const char* name;
} SRTP_CRYPTO_SUITE_ENTRY;

typedef enum
{
    SRTP_SESSION_IN_TYPE = 0,
    SRTP_SESSION_OUT_TYPE = 1
} SRTP_SESSION_TYPE;

#define SRTP_ENCRYPT_BUFFER_SIZE (10*1024)

class srtp_session
{
public:
    srtp_session(SRTP_SESSION_TYPE session_type, CRYPTO_SUITE_ENUM suite, uint8_t* key, size_t keyLen);
    ~srtp_session();

public:
    static void init();
    static void on_srtp_event(srtp_event_data_t* data);

public:
    bool encrypt_rtp(const uint8_t** data, size_t* len);
    bool decrypt_srtp(uint8_t* data, size_t* len);
    bool encrypt_rtcp(const uint8_t** data, size_t* len);
    bool decrypt_srtcp(uint8_t* data, size_t* len);
    void remove_stream(uint32_t ssrc);

private:
    static std::vector<const char*> errors;
    srtp_t session_ = nullptr;

private:
    uint8_t encrypt_buffer_[SRTP_ENCRYPT_BUFFER_SIZE];
};

#endif