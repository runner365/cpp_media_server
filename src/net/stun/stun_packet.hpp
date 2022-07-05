#ifndef STUN_PACKET_HPP
#define STUN_PACKET_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>
#include "ipaddress.hpp"

#define STUN_HEADER_SIZE 20

/*
rfc: https://datatracker.ietf.org/doc/html/rfc5389
All STUN messages MUST start with a 20-byte header followed by zero
or more Attributes.  The STUN header contains a STUN message type,
magic cookie, transaction ID, and message length.
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |0 0|     STUN Message Type     |         Message Length        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                         Magic Cookie                          |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                                                               |
   |                     Transaction ID (96 bits)                  |
   |                                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
typedef enum
{
    STUN_REQUEST          = 0,
    STUN_INDICATION       = 1,
    STUN_SUCCESS_RESPONSE = 2,
    STUN_ERROR_RESPONSE   = 3
} STUN_CLASS_ENUM;

typedef enum
{
    BINDING = 1
} STUN_METHOD_ENUM;


typedef enum
{
    STUN_MAPPED_ADDRESS     = 0x0001,
    STUN_USERNAME           = 0x0006,
    STUN_MESSAGE_INTEGRITY  = 0x0008,
    STUN_ERROR_CODE         = 0x0009,
    STUN_UNKNOWN_ATTRIBUTES = 0x000A,
    STUN_REALM              = 0x0014,
    STUN_NONCE              = 0x0015,
    STUN_XOR_MAPPED_ADDRESS = 0x0020,
    STUN_PRIORITY           = 0x0024,
    STUN_USE_CANDIDATE      = 0x0025,
    STUN_SOFTWARE           = 0x8022,
    STUN_ALTERNATE_SERVER   = 0x8023,
    STUN_FINGERPRINT        = 0x8028,
    STUN_ICE_CONTROLLED     = 0x8029,
    STUN_ICE_CONTROLLING    = 0x802A
} STUN_ATTRIBUTE_ENUM;

typedef enum
{
    OK           = 0,
    UNAUTHORIZED = 1,
    BAD_REQUEST  = 2
} STUN_AUTHENTICATION;

class stun_packet
{
public:
    stun_packet();
    stun_packet(const uint8_t* data, size_t len);
    ~stun_packet();

public:
    std::string dump();
    void serialize();//may throw exception
    STUN_AUTHENTICATION check_auth(const std::string& local_username, const std::string& local_pwd);
    stun_packet* create_success_response();
    stun_packet* create_error_response(uint16_t err_code);

public:
    static bool is_stun(const uint8_t* data, size_t len);
    static stun_packet* parse(const uint8_t* data, size_t len);

public:
    static const uint8_t magic_cookie[];

public:
    STUN_CLASS_ENUM  stun_class;
    STUN_METHOD_ENUM stun_method;
    uint8_t transaction_id[12];

public://attribute
    std::string user_name;
    std::string password;
    uint32_t priority        = 0;
    uint64_t ice_controlling = 0;
    uint64_t ice_controlled  = 0;
    uint32_t fingerprint     = 0;
    uint16_t error_code      = 0;
    const uint8_t* message_integrity = nullptr;
    const struct sockaddr* xor_address = nullptr; // 8 or 20 bytes.
    bool has_use_candidate   = false;
    bool has_fingerprint     = false;

public:
    uint8_t* origin_data = nullptr;
    uint8_t data[1500];
    size_t  data_len = 0;
    
};

#endif