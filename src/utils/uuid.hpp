#ifndef UUID_H
#define UUID_H
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

inline std::string make_uuid() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    char uuid_sz[128];
    int len = 0;

    for (size_t i = 0; i < sizeof(uuid.data); i++) {
        len += snprintf(uuid_sz + len, sizeof(uuid_sz), "%02x", uuid.data[i]);
    }
    
    return std::string((char*)uuid_sz, strlen(uuid_sz));
}

#endif//UUID_H