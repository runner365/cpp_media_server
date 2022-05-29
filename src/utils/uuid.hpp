#ifndef UUID_H
#define UUID_H
#include "byte_crypto.hpp"
#include <cstring>

inline std::string make_uuid() {
    char uuid_sz[128];
    int len = 0;

    for (size_t i = 0; i < 12; i++) {
        uint32_t data = byte_crypto::get_random_uint(0, 255);
        len += snprintf(uuid_sz + len, sizeof(uuid_sz), "%02x", data);
    }
    
    return std::string((char*)uuid_sz, strlen(uuid_sz));
}

#endif//UUID_H
