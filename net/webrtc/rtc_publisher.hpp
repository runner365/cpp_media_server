#ifndef RTC_PUBLISH_HPP
#define RTC_PUBLISH_HPP
#include "rtc_media_info.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "net/rtprtcp/rtcp_sr.hpp"
#include "utils/stream_statics.hpp"
#include "utils/timer.hpp"
#include "utils/timeex.hpp"
#include <vector>
#include <stdint.h>
#include <stddef.h>
#include <string>

class rtc_base_session;
class room_callback_interface;

class rtc_publisher : public timer_interface
{
public:
    rtc_publisher(room_callback_interface* room, rtc_base_session* session, const MEDIA_RTC_INFO& media_info);
    virtual ~rtc_publisher();

public:
    void on_handle_rtppacket(rtp_packet* pkt);
    void on_handle_rtcp_sr(rtcp_sr_packet* sr_pkt);

public:
    std::string get_media_type();
    void get_ssrcs(std::vector<uint32_t> ssrcs);
    std::string get_ssrcs();
    int get_clockrate();

public://implement timer_interface
    virtual void on_timer() override;

private:
    room_callback_interface* room_ = nullptr;
    rtc_base_session* session_ = nullptr;

    MEDIA_RTC_INFO media_info_;

private:
    stream_statics statics_;

private://for rtcp sr
    NTP_TIMESTAMP ntp_;//from rtcp sr
    uint32_t sr_ssrc_      = 0;
    int64_t rtp_timestamp_ = 0;
    int64_t sr_local_ms_   = 0;
    uint32_t pkt_count_    = 0;
    uint32_t bytes_count_  = 0;
};

#endif