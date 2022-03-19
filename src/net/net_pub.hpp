#ifndef NET_PUB_HPP
#define NET_PUB_HPP
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>

inline void make_endpoint_string(boost::asio::ip::tcp::endpoint endpoint, std::string& info) {
    info = endpoint.address().to_string();
    info += ":";
    info += std::to_string(endpoint.port());
}

#endif

