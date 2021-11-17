#ifndef RTC_PUBLISH_HPP
#define RTC_PUBLISH_HPP
#include "rtc_media_info.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "net/rtprtcp/rtcp_sr.hpp"
#include "utils/timer.hpp"
#include "utils/timeex.hpp"
#include "rtp_recv_stream.hpp"
#include "rtc_stream_pub.hpp"
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string>

class rtc_base_session;
class room_callback_interface;

class rtc_publisher : public timer_interface, public rtc_stream_callback
{
public:
    rtc_publisher(const std::string& roomId, const std::string& uid,
            room_callback_interface* room, rtc_base_session* session, const MEDIA_RTC_INFO& media_info);
    virtual ~rtc_publisher();

public:
    void on_handle_rtppacket(rtp_packet* pkt);
    void on_handle_rtcp_sr(rtcp_sr_packet* sr_pkt);

public:
    MEDIA_RTC_INFO get_media_info() {return media_info_;}
    std::string get_publisher_id() {return pid_;}
    std::string get_media_type();
    uint32_t get_rtp_ssrc() {return rtp_ssrc_;}
    uint32_t get_rtx_ssrc() {return rtx_ssrc_;}
    int get_mid() {return media_info_.mid;}
    int get_clockrate();
    uint8_t get_rtp_payloadtype();
    uint8_t get_rtx_payloadtype();
    bool has_rtx();

    void request_keyframe(uint32_t media_ssrc);

public://implement timer_interface
    virtual void on_timer() override;

public://implement rtc_stream_callback
    virtual void stream_send_rtcp(uint8_t* data, size_t len) override;
    virtual void stream_send_rtp(uint8_t* data, size_t len) override;

private:
    std::string roomId_;
    std::string uid_;
    room_callback_interface* room_ = nullptr;
    rtc_base_session* session_ = nullptr;

private:
    std::string pid_;
    MEDIA_RTC_INFO media_info_;
    std::string media_type_;
    uint32_t rtp_ssrc_        = 0;
    uint32_t rtx_ssrc_        = 0;
    int clock_rate_           = 0;
    uint8_t payloadtype_      = 0;
    uint8_t rtx_payloadtype_  = 0;
    bool has_rtx_             = false;
    int mid_extension_id_     = 0;
    int abs_time_extension_id_ = 0;

private:
    rtp_recv_stream* rtp_handler_ = nullptr;
};

#endif