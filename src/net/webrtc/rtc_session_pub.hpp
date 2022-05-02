#ifndef RTC_SESSION_PUB_HPP
#define RTC_SESSION_PUB_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>
#include "media_packet.hpp"

#define RTC_DIRECTION_SEND 1
#define RTC_DIRECTION_RECV 2

typedef enum
{
    PUBLISH_WEBRTC,
    PUBLISH_LIVE,
} PUBLISH_TYPE;

class rtp_packet;
class rtc_publisher;
class rtc_base_session;

typedef struct publisher_info_s
{
    std::string media_type;
    uint32_t ssrc;
    int mid;
    std::string pid;
    PUBLISH_TYPE publish_type;
} publisher_info;

class room_callback_interface
{
public:
    virtual void on_rtppacket_publisher2room(rtc_publisher* publisher, rtp_packet* pkt) = 0;
    virtual void on_rtppacket_publisher2room(const std::string& publisher_id, const std::string& media_type, rtp_packet* pkt) = 0;
    virtual void on_request_keyframe(const std::string& pid, const std::string& sid, uint32_t media_ssrc) = 0;
    virtual void on_unpublish(const std::string& pid) = 0;
    virtual void on_unsubscribe(const std::string& pid, const std::string& sid) = 0;
    virtual void on_update_alive(const std::string& roomId, const std::string& uid, int64_t now_ms) = 0;
    virtual void on_rtmp_callback(const std::string& roomId, const std::string& uid,
                                const std::string& stream_type, MEDIA_PACKET_PTR pkt_ptr) = 0;
};

#endif