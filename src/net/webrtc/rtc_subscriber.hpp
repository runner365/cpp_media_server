#ifndef RTC_SUBSCRIBER_HPP
#define RTC_SUBSCRIBER_HPP
#include "utils/uuid.hpp"
#include "utils/timer.hpp"
#include "rtc_media_info.hpp"
#include "rtp_send_stream.hpp"
#include "rtc_stream_pub.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "net/rtprtcp/rtcpfb_nack.hpp"
#include "net/rtprtcp/rtcp_rr.hpp"
#include "json.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <memory>

typedef enum
{
    RTC_STREAM_TYPE,
    LIVE_STREAM_TYPE
} SOURCE_STREAM_TYPE;

using json = nlohmann::json;

class rtc_base_session;
class room_callback_interface;

class rtc_subscriber : public timer_interface, public rtc_stream_callback
{
public:
    rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
            , rtc_base_session* session, const MEDIA_RTC_INFO& media_info, room_callback_interface* room_cb);
    ~rtc_subscriber();

public:
    std::string get_roomid() {return roomId_;}
    std::string get_uid() {return uid_;}
    std::string get_remote_uid() {return remote_uid_;}
    std::string get_publisher_id() {return pid_;}
    std::string get_subscirber_id() {return sid_;}
    uint32_t get_rtp_ssrc() { return rtp_ssrc_; }
    uint32_t get_rtx_ssrc() { return rtx_ssrc_; }
    uint8_t get_mid() { return media_info_.mid; }
    uint8_t get_payload() { return payloadtype_; }
    uint8_t get_rtx_payload() { return rtx_payloadtype_; }
    int get_clockrate() { return clock_rate_; }
    void set_stream_type(SOURCE_STREAM_TYPE stream_type) { stream_type_ = stream_type; }
    SOURCE_STREAM_TYPE get_stream_type() { return stream_type_; }
    int64_t get_remb_bitrate() { return remb_bitrate_; }
    void set_remb_bitrate(int64_t bitrate);
    void get_statics(json& json_data);

public:
    void send_rtp_packet(const std::string& roomId, const std::string& media_type,
                    const std::string& publish_id, rtp_packet* pkt);
    void handle_fb_rtp_nack(rtcp_fb_nack* nack_pkt);
    void handle_rtcp_rr(rtcp_rr_packet* rr_pkt);
    void request_keyframe();

public://implement timer_interface
    virtual void on_timer() override;

public://implement rtc_stream_callback
    virtual void stream_send_rtcp(uint8_t* data, size_t len) override;
    virtual void stream_send_rtp(uint8_t* data, size_t len) override;

private:
    std::string roomId_;
    std::string uid_;
    std::string remote_uid_;
    std::string pid_;
    std::string sid_;
    SOURCE_STREAM_TYPE stream_type_ = RTC_STREAM_TYPE;
    rtc_base_session* session_ = nullptr;
    std::shared_ptr<rtp_send_stream> stream_ptr_;

private:
    MEDIA_RTC_INFO media_info_;
    std::string media_type_;
    int      mid_            = 0;
    int      src_mid_        = 0;
    uint32_t rtp_ssrc_       = 0;
    uint32_t rtx_ssrc_       = 0;
    int clock_rate_          = 0;
    uint8_t payloadtype_     = 0;
    uint8_t rtx_payloadtype_ = 0;
    bool has_rtx_            = false;

private:
    room_callback_interface* room_cb_ = nullptr;

private:
    int64_t remb_bitrate_ = 0;
};



#endif