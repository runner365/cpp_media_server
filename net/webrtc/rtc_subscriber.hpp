#ifndef RTC_SUBSCRIBER_HPP
#define RTC_SUBSCRIBER_HPP
#include "utils/uuid.hpp"
#include "rtc_media_info.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>

class rtc_base_session;
class rtc_subscriber
{
public:
    rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
            , rtc_base_session* session, const MEDIA_RTC_INFO& media_info);
    ~rtc_subscriber();

public:
    std::string get_roomid() {return roomId_;}
    std::string get_uid() {return uid_;}
    std::string get_remote_uid() {return remote_uid_;}
    std::string get_publisher_id() {return pid_;}
    std::string get_subscirber_id() {return sid_;}

public:
    void send_rtp_packet(const std::string& roomId, const std::string& media_type,
                    const std::string& publish_id, rtp_packet* pkt);

private:
    std::string roomId_;
    std::string uid_;
    std::string remote_uid_;
    std::string pid_;
    std::string sid_;
    rtc_base_session* session_ = nullptr;
    MEDIA_RTC_INFO media_info_;
};



#endif