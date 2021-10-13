#include "rtc_publisher.hpp"
#include "rtc_session_pub.hpp"
#include "utils/timer.hpp"
#include "utils/timeex.hpp"
#include "utils/logger.hpp"
#include <sstream>
#include <cstring>

extern boost::asio::io_context& get_global_io_context();

rtc_publisher::rtc_publisher(room_callback_interface* room, rtc_base_session* session, const MEDIA_RTC_INFO& media_info):timer_interface(get_global_io_context(), 2000)
        , room_(room)
        , session_(session)
        , media_info_(media_info) {
    memset(&ntp_, 0, sizeof(NTP_TIMESTAMP));

    start_timer();
    log_infof("rtc_publisher construct media type:%s, ssrcs:%s",
        this->get_media_type().c_str(), this->get_ssrcs().c_str());
}

rtc_publisher::~rtc_publisher() {
    log_infof("rtc_publisher destruct media type:%s, ssrcs:%s",
        this->get_media_type().c_str(), this->get_ssrcs().c_str());
}

std::string rtc_publisher::get_media_type() {
    return media_info_.media_type;
}

void rtc_publisher::get_ssrcs(std::vector<uint32_t> ssrcs) {
    for (auto info : media_info_.ssrc_infos) {
        ssrcs.push_back(info.ssrc);
    }
    return;
}

std::string rtc_publisher::get_ssrcs() {
    std::stringstream ss;
    for (auto info : media_info_.ssrc_infos) {
        ss << info.ssrc << " ";
    }

    return ss.str().c_str();
}

int rtc_publisher::get_clockrate() {
    return media_info_.rtp_encodings[0].clock_rate;
}

void rtc_publisher::on_handle_rtppacket(rtp_packet* pkt) {
    statics_.update(pkt->get_data_length(), pkt->get_local_ms());

    room_->rtppacket_publisher2room(session_, this, pkt);
}

void rtc_publisher::on_handle_rtcp_sr(rtcp_sr_packet* sr_pkt) {
    sr_ssrc_       = sr_pkt->get_ssrc();
    ntp_.ntp_sec   = sr_pkt->get_ntp_sec();
    ntp_.ntp_frac  = sr_pkt->get_ntp_frac();
    rtp_timestamp_ = (int64_t)sr_pkt->get_rtp_timestamp();
    sr_local_ms_   = now_millisec();
    pkt_count_     = sr_pkt->get_pkt_count();
    bytes_count_   = sr_pkt->get_bytes_count();
}

void rtc_publisher::on_timer() {
    size_t fps;
    size_t speed = statics_.bytes_per_second((int64_t)now_millisec(), fps);

    log_infof("rtc publish media:%s, speed(bytes/s):%lu, fps:%lu",
        get_media_type().c_str(), speed, fps);
}
