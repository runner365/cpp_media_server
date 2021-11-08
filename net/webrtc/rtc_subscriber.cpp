#include "rtc_subscriber.hpp"
#include "rtc_base_session.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "logger.hpp"

extern boost::asio::io_context& get_global_io_context();

rtc_subscriber::rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
    , rtc_base_session* session, const MEDIA_RTC_INFO& media_info):timer_interface(get_global_io_context(), 500)
            , roomId_(roomId)
            , uid_(uid)
            , remote_uid_(remote_uid)
            , pid_(pid)
            , session_(session)
{
    sid_ = make_uuid();

    media_info_ = media_info;
    media_type_ = media_info_.media_type;
    mid_        = media_info_.mid;
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
            rtx_ssrc_ = 0;
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
    
    stream_ptr_ = std::make_shared<rtp_send_stream>(media_type_, (rtx_ssrc_ != 0), this);
    stream_ptr_->set_rtp_payload_type(payloadtype_);
    stream_ptr_->set_rtx_payload_type(rtx_payloadtype_);
    stream_ptr_->set_clock(clock_rate_);
    stream_ptr_->set_rtp_ssrc(rtp_ssrc_);
    stream_ptr_->set_rtx_ssrc(rtx_ssrc_);

    start_timer();

    log_infof("rtc_subscriber construct media type:%s, rtp ssrc:%u, rtx ssrc:%u, clock rate:%d, \
payload:%d, has rtx:%d, rtx payload:%d, mid:%d, id:%s",
        media_type_.c_str(), rtp_ssrc_, rtx_ssrc_, clock_rate_, payloadtype_,
        has_rtx_, rtx_payloadtype_, mid_, sid_.c_str());
}

rtc_subscriber::~rtc_subscriber() {
    log_infof("rtc_subscriber destruct media type:%s, rtp ssrc:%u, rtx ssrc:%u, clock rate:%d, \
payload:%d, has rtx:%d, rtx payload:%d, mid:%d, id:%s",
        media_type_.c_str(), rtp_ssrc_, rtx_ssrc_, clock_rate_, payloadtype_,
        has_rtx_, rtx_payloadtype_, mid_, sid_.c_str());
}

void rtc_subscriber::send_rtp_packet(const std::string& roomId, const std::string& media_type,
                    const std::string& publish_id, rtp_packet* pkt) {
    //log_debugf("subscriber receive rtp packet roomid:%s, media type:%s, publisher:%s, pkt dump:\r\n%s",
    //    roomId.c_str(), media_type.c_str(), publish_id.c_str(), pkt->dump().c_str());

    stream_ptr_->on_send_rtp_packet(pkt);
    session_->send_rtp_data_in_dtls(pkt->get_data(), pkt->get_data_length());

    return;
}

void rtc_subscriber::handle_fb_rtp_nack(rtcp_fb_nack* nack_pkt) {
    stream_ptr_->handle_fb_rtp_nack(nack_pkt);
}

void rtc_subscriber::handle_rtcp_rr(rtcp_rr_packet* rr_pkt) {
    stream_ptr_->handle_rtcp_rr(rr_pkt);
}

void rtc_subscriber::on_timer() {
    stream_ptr_->on_timer();
}

void rtc_subscriber::stream_send_rtcp(uint8_t* data, size_t len) {
    session_->send_rtcp_data_in_dtls(data, len);
}

void rtc_subscriber::stream_send_rtp(uint8_t* data, size_t len) {
    session_->send_rtp_data_in_dtls(data, len);
}
