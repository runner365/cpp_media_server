#ifndef USER_INFO_HPP
#define USER_INFO_HPP
#include "net/websocket/wsimple/protoo_pub.hpp"
#include "sdp_analyze.hpp"
#include "udp/udp_server.hpp"
#include <string>

class user_info
{
public:
    user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb);
    ~user_info();

public:
    std::string uid() {return uid_;}
    std::string roomId() {return roomId_;}
    protoo_request_interface* feedback() {return feedback_;}

private:
    std::string uid_;
    std::string roomId_;
    protoo_request_interface* feedback_ = nullptr;

private:
    sdp_analyze remove_sdp_analyze_;
    sdp_analyze local_sdp_analyze_;
};

#endif