#ifndef BYTE_CRYTO_HPP
#define BYTE_CRYTO_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <random>
#include <openssl/hmac.h>
#include <openssl/ssl.h>

#define SHA1_BUFFER_SIZE 20

class byte_crypto
{
public:
    static void init();
    static void deinit();
    static uint32_t get_random_uint(uint32_t min, uint32_t max);
    static uint32_t get_crc32(const uint8_t* data, size_t size);
    static uint8_t* get_hmac_sha1(const std::string& key, const uint8_t* data, size_t len);

public:
    static std::default_random_engine random;
    static HMAC_CTX* hmac_sha1_ctx;
    static uint8_t hmac_sha1_buffer[20];
    static const uint32_t crc32_table[256];
};


#endif