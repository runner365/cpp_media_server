#ifndef IP_ADDRESS_HPP
#define IP_ADDRESS_HPP

#ifdef WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#else
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()
#include <netinet/in.h> // sockaddr_in, sockaddr_in6
#include <sys/socket.h> // struct sockaddr, struct sockaddr_storage, AF_INET, AF_INET6
#endif

#include <string>
#include <stdint.h>
#include <stddef.h>
#include <cstring>

inline std::string get_ip_str(const struct sockaddr *sa, uint16_t& port) {
    const socklen_t maxlen = 64;
    char s[maxlen];

    if (!sa) {
        return "";
    }
    
    std::memset(s, 0, maxlen);
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
            port = ((struct sockaddr_in *)sa)->sin_port;
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    s, maxlen);
            port = ((struct sockaddr_in *)sa)->sin_port;
            break;
    }

    return std::string(s);
}

inline void get_ipv4_sockaddr(const std::string& ip, uint16_t port, struct sockaddr* addr) {
    struct sockaddr_in* sa = (struct sockaddr_in*)addr;

    inet_pton(AF_INET, ip.c_str(), &(sa->sin_addr));
    sa->sin_port = port;
    sa->sin_family = AF_INET;
    return;
}

#endif