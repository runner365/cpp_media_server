#ifndef URL_ANALYZE_HPP
#define URL_ANALYZE_HPP
#include "stringex.hpp"
#include "logger.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <vector>

inline int get_scheme_by_url(const std::string& url, std::string& scheme) {
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        return -1;
    }

    scheme = url.substr(0, pos);
    return 0;
}

inline int get_hostport_by_url(const std::string& url, std::string& host, uint16_t& port,
                    std::string& subpath, std::map<std::string, std::string> parameters) {
    size_t pos = url.find("://");
    if (pos == std::string::npos) {
        return -1;
    }

    std::string host_port = url.substr(pos + 3);
    std::string left_str;
    std::string param_str;

    pos = host_port.find("/");
    if (pos != std::string::npos) {
        if ((pos + 1) < host_port.length()) {
            left_str = host_port.substr(pos+1);
        }
        host_port = host_port.substr(0, pos);
    }

    pos = host_port.find(":");
    if (pos != std::string::npos) {
        host = host_port.substr(0, pos);
        port = atoi(host_port.substr(pos+1).c_str());
    } else {
        host = host_port;
    }

    if (left_str.empty()) {
        return 0;
    }

    pos = left_str.find("?");
    if (pos != std::string::npos) {
        subpath   = left_str.substr(0, pos);
        param_str = left_str.substr(pos+1);

        std::vector<std::string> params_vec;
        
        string_split(param_str, "&", params_vec);
        for (std::string& param_item : params_vec) {
            pos = param_item.find("=");
            if (pos != std::string::npos) {
                std::string key   = param_item.substr(0, pos);
                std::string value = param_item.substr(pos+1);

                parameters[key] = value;
            }
        }
    } else {
        subpath = left_str;
    }

    return 0;
}

#endif