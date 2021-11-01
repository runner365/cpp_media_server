#include "rtc_publisher.hpp"
#include "rtc_session_pub.hpp"
#include "rtc_base_session.hpp"
#include "utils/timer.hpp"
#include "utils/timeex.hpp"
#include "utils/logger.hpp"
#include "utils/uuid.hpp"
#include <sstream>
#include <cstring>

extern boost::asio::io_context& get_global_io_context();

rtc_publisher::rtc_publisher(const std::string& roomId, const std::string& uid,
        room_callback_interface* room, rtc_base_session* session, const MEDIA_RTC_INFO& media_info):timer_interface(get_global_io_context(), 500)
        , roomId_(roomId)
        , uid_(uid)
        , room_(room)
        , session_(session)
        , media_info_(media_info) {
    pid_ = make_uuid();
    memset(&ntp_, 0, sizeof(NTP_TIMESTAMP));
    media_type_ = media_info_.media_type;

    clock_rate_ = media_info_.rtp_encodings[0].clock_rate;

    has_rtx_ = false;
    for (auto enc_item : media_info_.rtp_encodings) {
        if (enc_item.codec == "rtx") {
            has_rtx_ = true;
            rtx_payloadtype_ = (uint8_t)enc_item.payload;
        } else {
            payloadtype_ = (uint8_t)enc_item.payload;
        }
    }

    if (has_rtx_ && !media_info_.ssrc_groups.empty()) {
        SSRC_GROUPS group = media_info_.ssrc_groups[0];
        if (group.ssrcs.size() >= 2) {
            rtp_ssrc_ = group.ssrcs[0];
            rtx_ssrc_ = group.ssrcs[1];
        } else {
            rtp_ssrc_ = group.ssrcs[0];
            log_warnf("the rtc publisher has only rtp ssrc:%u, but has no rtx ssrc", rtp_ssrc_);
        }
    } else if (!has_rtx_ && !media_info_.ssrc_groups.empty()) {
        SSRC_GROUPS group = media_info_.ssrc_groups.at(0);
        rtp_ssrc_ = group.ssrcs.at(0);
    } else {
        size_t index = 0;
        for (auto info : media_info_.ssrc_infos) {
            if (index == 0) {
                rtp_ssrc_ = info.ssrc;
            } else {
                rtx_ssrc_ = info.ssrc;
            }
        }
    }

    start_timer();
    log_infof("rtc_publisher construct media type:%s, rtp ssrc:%u, rtx ssrc:%u, clock rate:%d, \
payload:%d, has rtx:%d, rtx payload:%d, mid:%d, id:%s",
        this->get_media_type().c_str(), rtp_ssrc_, rtx_ssrc_, clock_rate_, payloadtype_,
        has_rtx_, rtx_payloadtype_, get_mid(), pid_.c_str());
}

rtc_publisher::~rtc_publisher() {
    log_infof("rtc_publisher destruct media type:%s, rtp ssrc:%u, rtx ssrc:%u, mid:%d",
        this->get_media_type().c_str(), rtp_ssrc_, rtx_ssrc_, get_mid());
    stop_timer();
    if (rtp_handler_) {
        delete rtp_handler_;
    }
}

std::string rtc_publisher::get_media_type() {
    return media_type_;
}

int rtc_publisher::get_clockrate() {
    return clock_rate_;
}

uint8_t rtc_publisher::get_rtp_payloadtype() {
    return payloadtype_;
}

uint8_t rtc_publisher::get_rtx_payloadtype() {
    return rtx_payloadtype_;
}

bool rtc_publisher::has_rtx() {
    return has_rtx_;
}

void rtc_publisher::on_handle_rtppacket(rtp_packet* pkt) {
    if ((pkt->get_ssrc() == rtp_ssrc_) && (pkt->get_payload_type() == payloadtype_)) {
        if (!rtp_handler_) {
            rtp_handler_ = new rtp_stream(this, media_type_, pkt->get_ssrc(), payloadtype_, false, get_clockrate());
            if (has_rtx()) {
                rtp_handler_->set_rtx_ssrc(rtx_ssrc_);
                rtp_handler_->set_rtx_payloadtype(rtx_payloadtype_);
            }
        }
        rtp_handler_->on_handle_rtp(pkt);
    } else if (has_rtx() && (pkt->get_ssrc() == rtx_ssrc_) && (pkt->get_payload_type() == rtx_payloadtype_)) {
        if (rtp_handler_) {
            rtp_handler_->on_handle_rtx_packet(pkt);
        } else {
            log_warnf("rtp(%s) handler is not ready for rtx, rtx_ssrc:%u, rtx_payload_type:%d",
                media_type_.c_str(), rtx_ssrc_, rtx_payloadtype_);
        }
    } else {
        log_errorf("unkown packet payload:%d, packet ssrc:%u, media type:%s, has rtx:%d, rtp ssrc:%u, rtx ssrc:%u",
            pkt->get_payload(), pkt->get_ssrc(), media_type_.c_str(), has_rtx_, rtp_ssrc_, rtx_ssrc_);
        return;
    }
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

    if ((sr_ssrc_ == rtp_ssrc_) && rtp_handler_) {
        rtp_handler_->update_lsr(ntp_.ntp_sec, ntp_.ntp_frac);
    } else {
        log_errorf("unkown rtcp sr ssrc:%u", sr_ssrc_);
    }
}

void rtc_publisher::stream_send_rtcp(uint8_t* data, size_t len) {
    session_->send_rtcp_data_in_dtls(data, len);
}

void rtc_publisher::stream_send_rtp(uint8_t* data, size_t len) {
    session_->send_rtp_data_in_dtls(data, len);
}

void rtc_publisher::on_timer() {
    if (rtp_handler_) {
        rtp_handler_->on_timer();
    }
}
