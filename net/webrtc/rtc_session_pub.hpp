#ifndef RTC_SESSION_PUB_HPP
#define RTC_SESSION_PUB_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>

#define RTC_DIRECTION_SEND 1
#define RTC_DIRECTION_RECV 2

class rtp_packet;
class rtc_publisher;
class rtc_base_session;

typedef struct publisher_info_s
{
    std::string media_type;
    uint32_t ssrc;
    int mid;
    std::string pid;
} publisher_info;

class room_callback_interface
{
public:
    virtual void on_rtppacket_publisher2room(rtc_base_session* session, rtc_publisher* publisher, rtp_packet* pkt) = 0;
    virtual void on_request_keyframe(const std::string& pid, const std::string& sid, uint32_t media_ssrc) = 0;
};

#endif