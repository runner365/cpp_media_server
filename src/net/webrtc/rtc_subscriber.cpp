#include "rtc_subscriber.hpp"
#include "rtc_base_session.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "utils/byte_crypto.hpp"
#include "logger.hpp"

extern boost::asio::io_context& get_global_io_context();

rtc_subscriber::rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
    , rtc_base_session* session, const MEDIA_RTC_INFO& media_info, room_callback_interface* room_cb):timer_interface(get_global_io_context(), 50)
            , roomId_(roomId)
            , uid_(uid)
            , remote_uid_(remote_uid)
            , pid_(pid)
            , session_(session)
            , room_cb_(room_cb)
{
    sid_ = make_uuid();

    media_info_ = media_info;
    media_type_ = media_info_.media_type;
    mid_        = media_info_.mid;
    src_mid_    = media_info_.src_mid;
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
    stop_timer();
    log_infof("rtc_subscriber destruct media type:%s, rtp ssrc:%u, rtx ssrc:%u, clock rate:%d, \
payload:%d, has rtx:%d, rtx payload:%d, mid:%d, id:%s",
        media_type_.c_str(), rtp_ssrc_, rtx_ssrc_, clock_rate_, payloadtype_,
        has_rtx_, rtx_payloadtype_, mid_, sid_.c_str());
}

void rtc_subscriber::send_rtp_packet(const std::string& roomId, const std::string& media_type,
                    const std::string& publish_id, rtp_packet* pkt) {
    if (pkt->has_extension()) {
        pkt->update_mid(this->get_mid());
    }

    uint32_t origin_ssrc   = pkt->get_ssrc();
    uint8_t origin_payload_type = pkt->get_payload_type();

    pkt->set_ssrc(rtp_ssrc_);
    pkt->set_payload_type(payloadtype_);

    //update timestamp only for rtmp2webrtc
    if (stream_type_ == LIVE_STREAM_TYPE) {
        double rtp_ts = pkt->get_timestamp();
        rtp_ts = rtp_ts * clock_rate_ / 1000.0;
        pkt->set_timestamp((uint32_t)rtp_ts);
    }
    //update payload&ssrc in subscriber
    stream_ptr_->on_send_rtp_packet(pkt);
    session_->send_rtp_data_in_dtls(pkt->get_data(), pkt->get_data_length());

    pkt->set_ssrc(origin_ssrc);
    pkt->set_payload_type(origin_payload_type);
    return;
}

void rtc_subscriber::handle_fb_rtp_nack(rtcp_fb_nack* nack_pkt) {
    stream_ptr_->handle_fb_rtp_nack(nack_pkt);
}

void rtc_subscriber::handle_rtcp_rr(rtcp_rr_packet* rr_pkt) {
    stream_ptr_->handle_rtcp_rr(rr_pkt);
}

void rtc_subscriber::on_timer() {
    int64_t now_ms = (int64_t)now_millisec();
    stream_ptr_->on_timer(now_ms);
}

void rtc_subscriber::stream_send_rtcp(uint8_t* data, size_t len) {
    session_->send_rtcp_data_in_dtls(data, len);
}

void rtc_subscriber::stream_send_rtp(uint8_t* data, size_t len) {
    //TODO: need to replace mid by this->src_mid;
    session_->send_rtp_data_in_dtls(data, len);
}

void rtc_subscriber::request_keyframe() {
    room_cb_->on_request_keyframe(pid_, sid_, rtp_ssrc_);
}
