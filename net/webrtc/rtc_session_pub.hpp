#ifndef RTC_SESSION_PUB_HPP
#define RTC_SESSION_PUB_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>

#define RTC_DIRECTION_SEND 1
#define RTC_DIRECTION_RECV 2

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

#endif