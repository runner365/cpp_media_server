#ifndef RTMP_HANDSHAKE_HPP
#define RTMP_HANDSHAKE_HPP
#include "rtmp_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "timeex.hpp"

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/dh.h>

#define HASH_SIZE 512
#define RTMP_HANDSHAKE_VERSION 0x03

#define RFC2409_PRIME_1024 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
    "FFFFFFFFFFFFFFFF"

enum HANDSHAKE_SCHEMA {
    SCHEMA_INIT = -1,
    SCHEMA0,
    SCHEMA1
};

//SIZE = 62
static uint8_t GENUINE_FLASH_PLAYER_KEY[] = {
    0x47, 0x65, 0x6E, 0x75, 0x69, 0x6E, 0x65, 0x20,
    0x41, 0x64, 0x6F, 0x62, 0x65, 0x20, 0x46, 0x6C,
    0x61, 0x73, 0x68, 0x20, 0x50, 0x6C, 0x61, 0x79,
    0x65, 0x72, 0x20, 0x30, 0x30, 0x31,
    // "Genuine Adobe Flash Player 001"
    0xF0, 0xEE, 0xC2, 0x4A, 0x80, 0x68, 0xBE, 0xE8,
    0x2E, 0x00, 0xD0, 0xD1, 0x02, 0x9E, 0x7E, 0x57,
    0x6E, 0xEC, 0x5D, 0x2D, 0x29, 0x80, 0x6F, 0xAB,
    0x93, 0xB8, 0xE6, 0x36, 0xCF, 0xEB, 0x31, 0xAE
};

inline void rtmp_random_generate(char* bytes, int size)
{   
    for (int i = 0; i < size; i++) {
        // the common value in [0x0f, 0xf0]
        bytes[i] = 0x0f + (random() % (256 - 0x0f - 0x0f));
    }
}
#if OPENSSL_VERSION_NUMBER < 0x10100000L
static void DH_get0_key(const DH *dh, const BIGNUM **pub_key, const BIGNUM **priv_key)
{
    if (pub_key != NULL) {
        *pub_key = dh->pub_key;
    }
    if (priv_key != NULL) {
        *priv_key = dh->priv_key;
    }
}

static int DH_set0_pqg(DH *dh, BIGNUM *p, BIGNUM *q, BIGNUM *g)
{
    /* If the fields p and g in d are NULL, the corresponding input
     * parameters MUST be non-NULL.  q may remain NULL.
     */
    if ((dh->p == NULL && p == NULL)
        || (dh->g == NULL && g == NULL))
        return 0;
    
    if (p != NULL) {
        BN_free(dh->p);
        dh->p = p;
    }
    if (q != NULL) {
        BN_free(dh->q);
        dh->q = q;
    }
    if (g != NULL) {
        BN_free(dh->g);
        dh->g = g;
    }
    
    if (q != NULL) {
        dh->length = BN_num_bits(q);
    }
    
    return 1;
}

static int DH_set_length(DH *dh, long length)
{
    dh->length = length;
    return 1;
}
#endif

inline int hmac_sha256(const char* key, int key_size, const char* data, int data_size, char* digest)
{   
    unsigned int digest_size = 0;
    
    uint8_t* temp_key = (uint8_t*)key;
    uint8_t* temp_digest = (uint8_t*)digest;

#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    HMAC_CTX ctx;

    HMAC_CTX_init(&ctx);

    if (HMAC_Init_ex(&ctx, temp_key, key_size, EVP_sha256(), NULL) < 0) {
        return -1;
    }

    if (HMAC_Update(&ctx, (uint8_t*)data, data_size) < 0) {
        return -1;
    }
    
    if (HMAC_Final(&ctx, temp_digest, &digest_size) < 0) {
        return -1;
    }

    HMAC_CTX_cleanup(&ctx);
#else
    HMAC_CTX *ctx = HMAC_CTX_new();
    if (ctx == nullptr) {
        log_errorf("hmac new error...");
        return -1;
    }

    if (HMAC_Init_ex(ctx, temp_key, key_size, EVP_sha256(), NULL) < 0) {
        HMAC_CTX_free(ctx);
        return -1;
    }

    if (HMAC_Update(ctx, (uint8_t*)data, data_size) < 0) {
        HMAC_CTX_free(ctx);
        return -1;
    }
    
    if (HMAC_Final(ctx, temp_digest, &digest_size) < 0) {
        HMAC_CTX_free(ctx);
        return -1;
    }
    HMAC_CTX_free(ctx);
#endif
    if (digest_size != 32) {
        return -1;
    }
    
    return 0;
}

class hmac_sha256_handler
{
public:
    hmac_sha256_handler() {
        #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        HMAC_CTX_init(&ctx_);
        #else
        ctx_ = HMAC_CTX_new();
        #endif
    }
    ~hmac_sha256_handler() {
        #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        HMAC_CTX_cleanup(&ctx_);
        #else
        if (ctx_) {
            HMAC_CTX_free(ctx_);
        }
        #endif
    }

public:
    int init(uint8_t* key, int key_len) {
        #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        if (HMAC_Init_ex(&ctx_, key, key_len, EVP_sha256(), NULL) < 0) {
            return -1;
        }
        #else
        if (HMAC_Init_ex(ctx_, key, key_len, EVP_sha256(), NULL) < 0) {
            return -1;
        }
        #endif
        return 0;
    }

    int update(uint8_t* data, size_t data_len) {
        #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        if (HMAC_Update(&ctx_, data, data_len) < 0) {
            return -1;
        }
        #else
        if (HMAC_Update(ctx_, data, data_len) < 0) {
            return -1;
        }
        #endif
        return 0;
    }

    int get_final(uint8_t* digest, size_t& digest_size) {
        #if OPENSSL_VERSION_NUMBER < 0x1010000fL
        if (HMAC_Final(&ctx_, digest, (unsigned int*)&digest_size) < 0) {
            return -1;
        }
        #else
        if (HMAC_Final(ctx_, digest, (unsigned int*)&digest_size) < 0) {
            return -1;
        }
        #endif
        return 0;
    }

private:
#if OPENSSL_VERSION_NUMBER < 0x1010000fL
    HMAC_CTX ctx_;
#else
    HMAC_CTX *ctx_ = nullptr;
#endif
};

class DH_gen_key
{
private:
    DH* pdh_ = nullptr;

public:
    DH_gen_key()
    {
    }
    ~DH_gen_key()
    {
        close();
    }

public:
    int init(bool public_128bytes_key = false) {
        int ret;

        while(true) {
            ret = do_init();
            if (ret != 0) {
                return ret;
            }

            if (public_128bytes_key) {
                const BIGNUM *pub_key = NULL;
                DH_get0_key(pdh_, &pub_key, NULL);
                int32_t key_size = BN_num_bytes(pub_key);
                if (key_size != 128) {
                    log_warnf("regenerate 128 bytes key, current=%d bytes", key_size);
                    continue;
                }
                log_warnf("get right key size:%d", key_size);
            }
            break;
        }
        return 0;
    }

    int copy_shared_key(const char* ppkey, uint32_t ppkey_size, char* skey, uint32_t& skey_size) {
        int ret = 0;
        BIGNUM* ppk = NULL;

        if ((ppk = BN_bin2bn((const unsigned char*)ppkey, ppkey_size, 0)) == NULL) {
            log_errorf("BN_bin2bn error...");
            return -1;
        }

        int32_t key_size = DH_compute_key((unsigned char*)skey, ppk, pdh_);
        
        if (key_size < (int32_t)ppkey_size) {
            log_warnf("shared key size=%d, ppk_size=%d", key_size, ppkey_size);
        }
        
        if (key_size < 0 || key_size > (int32_t)skey_size) {
            ret = -1;
        } else {
            skey_size = key_size;
        }
        
        if (ppk) {
            BN_free(ppk);
        }
        
        return ret;
    }

private:
    int do_init() {
        long length = 1024;

        close();
        //Create the DH
        if ((pdh_ = DH_new()) == NULL) {
            log_errorf("DH_new error...");
            return -1;
        }
        
        //Create the internal p and g
        BIGNUM *p, *g;
        if ((p = BN_new()) == NULL) {
            log_errorf("BN_new error...");
            return -1;
        }
        if ((g = BN_new()) == NULL) {
            BN_free(p);
            log_errorf("BN_new error...");
            return -1;
        }
        DH_set0_pqg(pdh_, p, NULL, g);
        
        //initialize p and g
        if (!BN_hex2bn(&p, RFC2409_PRIME_1024)) {
            log_errorf("BN_hex2bn error...");
            return -1;
        }
        
        if (!BN_set_word(g, 2)) {
            log_errorf("BN_set_word error...");
            return -1;
        }
        
        //Set the key length
        DH_set_length(pdh_, length);
        
        //Generate private and public key
        if (!DH_generate_key(pdh_)) {
            log_errorf("DH_generate_key error...");
            return -1;
        }
        return 0;
    }

    void close() {
        if (pdh_ != NULL) {
            DH_free(pdh_);
            pdh_ = NULL;
        }
    }
};

class c2s2_handle
{
public:
    c2s2_handle() {
        rtmp_random_generate(random_, sizeof(random_));

        rtmp_random_generate(digest_, 32);
    }
    ~c2s2_handle() {

    }

public:
    void generate(char* body) {
        memcpy(body, random_, 1504);
        memcpy(body + 1504, digest_, 32);
    }

    void parse(char* body)
    {   
        memcpy(random_, body, sizeof(random_));
        memcpy(digest_, body + 1504, sizeof(digest_));
        return;
    }

    int create_by_digest(char* c1_digest) {
        int ret = 0;
        char temp_key[HASH_SIZE];

        ret = hmac_sha256((char*)GENUINE_FLASH_PLAYER_KEY, 68, c1_digest, 32, temp_key);
        if (ret != 0) {
            log_errorf("hmac sha256 error:%d", ret);
            return ret;
        }

        char temp_digest[HASH_SIZE];
        ret = hmac_sha256(temp_key, 32, random_, 1504, temp_digest);
        if (ret != 0) {
            log_errorf("hmac sha256 error:%d", ret);
            return ret;
        }
        memcpy(digest_, temp_digest, 32);

        return 0;
    }

    bool validate_s2(char* c1_digest) {
        int ret;
        char temp_key[HASH_SIZE];

        ret = hmac_sha256((char*)GENUINE_FLASH_PLAYER_KEY, 68, c1_digest, 32, temp_key);
        if (ret != 0) {
            log_errorf("hmac sha256 error:%d", ret);
            return ret;
        }

        char temp_digest[HASH_SIZE];
        ret = hmac_sha256(temp_key, 32, random_, 1504, temp_digest);
        if (ret != 0) {
            log_errorf("hmac sha256 error:%d", ret);
            return ret;
        }

        bool is_equal = bytes_is_equal(digest_, temp_digest, sizeof(digest_));
    
        return is_equal;
    }
private:
    char random_[1504];
    char digest_[32];
};

class c1s1_handle
{
public:
    c1s1_handle() {
    }
    ~c1s1_handle() {
        if (key_random0_) {
            delete[] key_random0_;
        }
        if (key_random1_) {
            delete[] key_random1_;
        }
        if (digest_random0_) {
            delete[] digest_random0_;
        }
        if (digest_random1_) {
            delete[] digest_random1_;
        }
    }

public:
    int parse_c1(char* c1, size_t len) {
        int ret = 0;
        uint8_t* p = (uint8_t*)c1;

        memcpy(c1_data_, c1, sizeof(c1_data_));

        c1_time_ = read_4bytes(p);
        p += 4;
        c1_version_ = read_4bytes(p);
        p += 4;

        //schema1 first which make ffmpeg happy
        ret = try_schema1(p);
        if (ret != 0) {
            return ret;
        }

        bool is_valid = check_digest_valid(SCHEMA1);
        if (!is_valid) {
            log_errorf("check schema0 digest valid error...");
            log_infof("try rtmp handshae schema1...");
            ret = try_schema0(p);
            if (ret != 0) {
                return ret;
            }
            is_valid = check_digest_valid(SCHEMA0);
            if (!is_valid) {
                log_errorf("check schema0 digest valid error...");
                return -1;
            }
            schema_ = SCHEMA1;
            log_infof("check schema1 digest valid ok...");
        } else {
            schema_ = SCHEMA0;
            log_infof("check schema0 digest valid ok...");
        }

        return ret;
    }

    int make_s1(char* s1_data) {
        int ret;
        s1_time_sec_ = get_c1_time();//use c1 time
        s1_version_  = 0x01000504;

        DH_gen_key dh;
        ret = dh.init(true);
        if (ret != 0) {
            log_errorf("dh init error...");
            return ret;
        }

        uint32_t key_size = 128;
        ret = dh.copy_shared_key(c1_key_data_, sizeof(c1_key_data_), s1_key_data_, key_size);
        if (ret != 0) {
            log_errorf("dh copy shared key error...");
            return ret;
        }
        char* s1_digest = nullptr;
        ret = make_s1_digest(s1_digest);
        if (ret != 0) {
            log_errorf("make s1 digest error:%d", ret);
            return ret;
        }
        memcpy(s1_digest_data_, s1_digest, sizeof(s1_digest_data_));
        delete[] s1_digest;

        uint8_t* p = (uint8_t*)s1_data;
        write_4bytes(p, s1_time_sec_);
        p += 4;
        write_4bytes(p, s1_version_);
        p += 4;

        if (schema_ == SCHEMA0) {
            make_schema0(p);
        } else if (schema_ == SCHEMA1) {
            make_schema1(p);
        } else {
            log_errorf("schema not init:%d", (int)schema_);
            return -1;
        }
        return 0;
    }

    char* get_c1_digest() {
        return digest_data_;
    }

    uint32_t get_c1_time() {
        return c1_time_;
    }

    char* get_c1_data() {
        return c1_data_;
    }

private:
    int parse_key(uint8_t* data) {
        uint8_t* p = data;
        //key: 764bytes
        //---- random-data: (offset)bytes
        //---- key-data: 128bytes
        //---- random-data: (764-offset-128-4)bytes
        //---- offset: 4bytes

        uint8_t* offset_p = p + 764 - sizeof(uint32_t);
        c1_key_offset_ = read_4bytes(offset_p);

        uint32_t real_offset = calc_valid_key_offset(c1_key_offset_);
        assert(real_offset < (764 - sizeof(uint32_t)));
        key_random0_size_ = real_offset;
        assert(key_random0_size_ < (764 - sizeof(uint32_t)));
        key_random0_ = new char[key_random0_size_];
        memcpy(key_random0_, p, key_random0_size_);
        p += key_random0_size_;

        memcpy(c1_key_data_, p, sizeof(c1_key_data_));//128bytes key
        p += sizeof(c1_key_data_);

        key_random1_size_ = 764 - real_offset - 128 - 4;
        assert(key_random1_size_ < (764 - sizeof(uint32_t)));
        key_random1_ = new char[key_random1_size_];
        memcpy(key_random1_, p, key_random1_size_);
        p += key_random1_size_ + 4;

        assert(p == (data + 764));

        return 0;
    }

    int parse_digest(uint8_t* data) {
        uint8_t* p = data;

        //digest: 764bytes
        //---- offset: 4bytes
        //---- random-data: (offset)bytes
        //---- digest-data: 32bytes
        //---- random-data: (764-4-offset-32)bytes
        c1_digest_offset_ = read_4bytes(p);
        p += 4;

        uint32_t real_offset = calc_valid_digest_offset(c1_digest_offset_);
        digest_random0_size_ = real_offset;
        assert(digest_random0_size_ < (764 - sizeof(uint32_t)));
        digest_random0_ = new char[digest_random0_size_];
        memcpy(digest_random0_, p, digest_random0_size_);
        p += digest_random0_size_;

        memcpy(digest_data_, p, sizeof(digest_data_));
        p += sizeof(digest_data_);

        digest_random1_size_ = 764 - 4 - real_offset - 32;
        assert(digest_random1_size_ < (764 - sizeof(uint32_t)));
        digest_random1_ = new char[digest_random1_size_];
        memcpy(digest_random1_, p, digest_random1_size_);
    
        assert((p + digest_random1_size_) == (data + 764));
        return 0;
    }

    bool check_digest_valid(enum HANDSHAKE_SCHEMA schema) {
        size_t first_part_len = 0;
        size_t second_part_len = 0;
        char c1_digest[32];
        size_t digest_len = 32;

        hmac_sha256_handler hmac;

        int ret = hmac.init(GENUINE_FLASH_PLAYER_KEY, 30);
        if (ret != 0) {
            log_errorf("hmac init error.");
            return ret;
        }

        if (schema == SCHEMA0) {
            // time + ver + key part(764) + digest offset(4bytes) + rand0 part
            first_part_len  = 8 + 764 + 4 + digest_random0_size_;
            second_part_len = sizeof(c1_data_) - first_part_len - 32;
        } else if (schema == SCHEMA1) {
            // time + ver + digest offset(4bytes) + rand0 part
            first_part_len  = 8 + 4 + digest_random0_size_;
            second_part_len = sizeof(c1_data_) - first_part_len - 32;
        } else {
            log_errorf("unkown handshake schema:%d", (int)schema);
            return -1;
        }
        
        ret = hmac.update((uint8_t*)c1_data_, first_part_len);
        if (ret != 0) {
            log_errorf("hmac update error.");
            return ret;
        }
        ret = hmac.update((uint8_t*)c1_data_ + first_part_len + 32, second_part_len);
        if (ret != 0) {
            log_errorf("hmac update error.");
            return ret;
        }
        ret = hmac.get_final((uint8_t*)c1_digest, digest_len);
        if (ret != 0) {
            log_errorf("hmac final error.");
            return ret;
        }
        assert(digest_len == 32);

        bool is_equal = bytes_is_equal(digest_data_, c1_digest, sizeof(digest_data_));

        if (!is_equal) {
            log_info_data((uint8_t*)digest_data_, sizeof(digest_data_), "digest_data");
            log_info_data((uint8_t*)c1_digest, sizeof(digest_data_), "make c1 digest data");
        }

        return is_equal;
    }

    uint32_t calc_valid_key_offset(uint32_t offset)
    {
        const int MAX_OFFSET = 764 - 128 - 4;
        
        uint32_t valid_offset = 0;
        uint8_t* p = (uint8_t*)&offset;
        valid_offset += *p++;
        valid_offset += *p++;
        valid_offset += *p++;
        valid_offset += *p++;
        
        return valid_offset % MAX_OFFSET;
    }

    uint32_t calc_valid_digest_offset(uint32_t offset)
    {
        const int MAX_OFFSET = 764 - 32 - 4;
        
        uint32_t valid_offset = 0;
        uint8_t* p = (uint8_t*)&offset;
        valid_offset += *p++;
        valid_offset += *p++;
        valid_offset += *p++;
        valid_offset += *p++;
        
        return valid_offset % MAX_OFFSET;
    }

    int try_schema0(uint8_t* data) {
        uint8_t* p = data;

        //schema0: key first + digest second
        parse_key(p);
        p += 764;
        parse_digest(p);

        return 0;
    }

    int try_schema1(uint8_t* data) {
        uint8_t* p = data;

        //schema1: digest first + key second
        parse_digest(p);
        p += 764;
        parse_key(p);

        return 0;
    }

    int make_c1_digest(char*& c1_digest) {
        /*
         * c1s1 is splited by digest:
         *     c1s1-part1: n bytes (time, version, key and digest-part1).
         *     digest-data: 32bytes
         *     c1s1-part2: (1536-n-32)bytes (digest-part2)
         */
        char* c1s1_joined_data = new char[1536 -32];
        uint8_t* p = (uint8_t*)c1s1_joined_data;

        /* ++++++ c1s1-part1: (time, version, key and digest-part1)++++++ */
        write_4bytes(p, c1_time_);
        p += 4;
        write_4bytes(p, c1_version_);
        p += 4;

        /* +++++ key ++++ */
        memcpy(p, key_random0_, key_random0_size_);
        p += key_random0_size_;

        memcpy(p, c1_key_data_, sizeof(c1_key_data_));
        p += sizeof(c1_key_data_);

        memcpy(p, key_random1_, key_random1_size_);
        p += key_random1_size_;

        write_4bytes(p, c1_key_offset_);
        p += 4;

        /* +++++ digest-part1 +++++ */
        write_4bytes(p, c1_digest_offset_);
        p += 4;

        memcpy(p, digest_random0_, digest_random0_size_);
        p += digest_random0_size_;

        //skip digest-data: 32bytes

        /* +++++ digest-part2 +++++ */
        memcpy(p, digest_random1_, digest_random1_size_);
        p += digest_random1_size_;
        assert(p == ((uint8_t*)c1s1_joined_data + 1536 -32));

        c1_digest = new char[HASH_SIZE];

        int ret = hmac_sha256((const char*)GENUINE_FLASH_PLAYER_KEY, 30,
                        c1s1_joined_data, 1536 - 32, c1_digest);
        if (ret != 0) {
            delete[] c1s1_joined_data;
            return ret;
        }

        delete[] c1s1_joined_data;
        return ret;
    }

    int make_s1_digest(char*& s1_digest) {
        const int JOINED_BYTES_SIZE = 1536 - 32;
        int ret = 0;
        char joined_bytes[JOINED_BYTES_SIZE];
        uint8_t* p = (uint8_t*)joined_bytes;

        write_4bytes(p, s1_time_sec_);
        p += 4;
        write_4bytes(p, s1_version_);
        p += 4;

        /* ++++++ key part ++++++ */
        memcpy(p, key_random0_, key_random0_size_);
        p += key_random0_size_;
        
        memcpy(p, c1_key_data_, sizeof(c1_key_data_));
        p += sizeof(c1_key_data_);

        memcpy(p, key_random1_, key_random1_size_);
        p += key_random1_size_;

        write_4bytes(p, c1_key_offset_);
        p += 4;

        /* ++++++ digest part(no digest inside) ++++++ */
        write_4bytes(p, c1_digest_offset_);
        p += 4;

        memcpy(p, digest_random0_, digest_random0_size_);
        p += digest_random0_size_;

        //no digest inside: less 32 bytes
    
        memcpy(p, digest_random1_, digest_random1_size_);
        p += digest_random1_size_;

        s1_digest = new char[HASH_SIZE];
        ret = hmac_sha256((char*)GENUINE_FLASH_PLAYER_KEY, 36, joined_bytes, JOINED_BYTES_SIZE, s1_digest);
        if (ret != 0) {
            log_errorf("hmac_sha256 error:%d", ret);
            return ret;
        }

        return ret;
    }

    int make_key(uint8_t* data) {
        uint8_t* p = data;
        //key: 764bytes
        //---- random-data: (offset)bytes
        //---- key-data: 128bytes
        //---- random-data: (764-offset-128-4)bytes
        //---- offset: 4bytes
        memcpy(p, key_random0_, key_random0_size_);
        p += key_random0_size_;

        memcpy(p, s1_key_data_, sizeof(s1_key_data_));
        p += sizeof(s1_key_data_);

        memcpy(p, key_random1_, key_random1_size_);
        p += key_random1_size_;

        write_4bytes(p, c1_key_offset_);

        return 0;
    }

    int make_digest(uint8_t* data) {
        uint8_t* p = data;

        //digest: 764bytes
        //---- offset: 4bytes
        //---- random-data: (offset)bytes
        //---- digest-data: 32bytes
        //---- random-data: (764-4-offset-32)bytes
        write_4bytes(p, c1_digest_offset_);
        p += 4;

        memcpy(p, digest_random0_, digest_random0_size_);
        p += digest_random0_size_;

        memcpy(p, s1_digest_data_, sizeof(s1_digest_data_));
        p += sizeof(s1_digest_data_);

        memcpy(p, digest_random1_, digest_random1_size_);
        p += digest_random1_size_;
    
        return 0;
    }

    int make_schema0(uint8_t* data) {
        uint8_t* p = data;

        //schema0: key first + digest second
        make_key(p);
        p += 764;
        make_digest(p);

        return 0;
    }

    int make_schema1(uint8_t* data) {
        uint8_t* p = data;

        //schema1: digest first + key second
        make_digest(p);
        p += 764;
        make_key(p);
        return 0;
    }

private:
    enum HANDSHAKE_SCHEMA schema_ = SCHEMA_INIT;
    char c1_data_[1536];
    uint32_t c1_time_;
    uint32_t c1_version_;

    char* key_random0_ = nullptr;
    char* key_random1_ = nullptr;
    uint32_t key_random0_size_;
    uint32_t key_random1_size_;

    uint32_t c1_key_offset_;
    char c1_key_data_[128];

    uint32_t c1_digest_offset_;
    char* digest_random0_ = nullptr;
    char* digest_random1_ = nullptr;
    uint32_t digest_random0_size_;
    uint32_t digest_random1_size_;
    char digest_data_[32];

private:
    uint32_t s1_time_sec_;
    uint32_t s1_version_;
    char s1_key_data_[128];
    char s1_digest_data_[32];
};

class rtmp_server_session;
class rtmp_server_handshake
{
public:
    rtmp_server_handshake(rtmp_server_session* session);
    ~rtmp_server_handshake();

public:
    int handle_c0c1();
    int handle_c2();
    int send_s0s1s2();

private:
    int parse_c0c1(char* c0c1);
    char* make_s1_data(int& s1_len);
    char* make_s2_data(int& s2_len);
    uint32_t get_c1_time();
    char* get_c1_data();

private:
    uint8_t c0_version_;
    c1s1_handle c1s1_;
    c2s2_handle c2s2_;
    char s1_body_[1536];
    char s2_body_[1536];
    rtmp_server_session* session_ = nullptr;
};

class rtmp_client_session;
class rtmp_client_handshake
{
public:
    rtmp_client_handshake(rtmp_client_session* session);
    ~rtmp_client_handshake();

    int send_c0c1();

public:
    static size_t s0s1s2_size;

private:
    rtmp_client_session* session_;
    uint8_t random_[(1 + 1536*2) * 2];//c0c1c2
};
#endif //RTMP_HANDSHAKE_HPP