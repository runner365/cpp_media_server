#include "rtp_send_stream.hpp"
#include "rtc_stream_pub.hpp"
#include "utils/stream_statics.hpp"
#include "utils/timeex.hpp"
#include <stddef.h>

static const size_t RTP_VIDEO_BUFFER_MAX = 800;
static const size_t RTP_AUDIO_BUFFER_MAX = 100;

rtp_send_stream::rtp_send_stream(const std::string& media_type, bool nack_enable,
                        rtc_stream_callback* cb):media_type_(media_type)
                                                , nack_enable_(nack_enable)
                                                , cb_(cb)
{
}

rtp_send_stream::~rtp_send_stream() {
    clear_buffer();
}

void rtp_send_stream::save_buffer(rtp_packet* input_pkt) {
    uint16_t seq = input_pkt->get_seq();
    size_t buffer_max = (media_type_ == "video") ? RTP_VIDEO_BUFFER_MAX : RTP_AUDIO_BUFFER_MAX;
    NACK_PACKET nack_pkt;

    rtp_packet* pkt = input_pkt->clone();

    nack_pkt.last_sent_timestamp = 0;
    nack_pkt.sent_count          = 0;
    nack_pkt.packet              = pkt;

    auto iter = rtp_buffer_map_.find(seq);
    if (iter != rtp_buffer_map_.end()) {
        delete iter->second.packet;
        rtp_buffer_map_.erase(iter);
    }

    rtp_buffer_map_[seq] = nack_pkt;
    if (first_seq_ == 0) {
        first_seq_ = seq;
    }

    while(rtp_buffer_map_.size() > buffer_max) {
        auto iter = rtp_buffer_map_.find(first_seq_);
        delete iter->second.packet;
        auto next_iter = rtp_buffer_map_.erase(iter);
        if (next_iter == rtp_buffer_map_.end()) {
            next_iter = rtp_buffer_map_.begin();
        }
        first_seq_ = next_iter->first;
    }
}

void rtp_send_stream::on_send_rtp_packet(rtp_packet* pkt) {
    send_statics_.update(pkt->get_data_length(),  pkt->get_local_ms());

    save_buffer(pkt);
}

void rtp_send_stream::clear_buffer() {
    while (!rtp_buffer_map_.empty())
    {
        auto iter = rtp_buffer_map_.begin();
        delete iter->second.packet;
        rtp_buffer_map_.erase(iter);
    }
}

void rtp_send_stream::handle_fb_rtp_nack(rtcp_fb_nack* nack_pkt) {
    std::vector<uint16_t> lost_seqs = nack_pkt->get_lost_seqs();
    int64_t now_ms = now_millisec();

    for (auto seq : lost_seqs) {
        auto pkt_iter = rtp_buffer_map_.find(seq);
        if (pkt_iter == rtp_buffer_map_.end()) {
            log_errorf("the lost sequence(%d) is missed", seq);
            continue;
        }
        if (pkt_iter->second.last_sent_timestamp == 0) {
            pkt_iter->second.last_sent_timestamp = now_ms;
            pkt_iter->second.sent_count = 1;
        } else {
            int64_t diff_t = now_ms - pkt_iter->second.last_sent_timestamp;
            if (diff_t < (rtt_ - 5)) {
                continue;
            }
            
            pkt_iter->second.sent_count++;
            if (pkt_iter->second.sent_count > RETRANSMIT_MAX_COUNT) {
                log_errorf("the lost sequence(%d) has been retransmited over times(%d)",
                        seq, pkt_iter->second.sent_count);
                continue;
            }
            pkt_iter->second.last_sent_timestamp = now_ms;
        }
        log_infof("retransmit rtp packet seq:%d, retransmit count:%d",
            pkt_iter->second.packet->get_seq(), pkt_iter->second.sent_count);
        cb_->stream_send_rtp(pkt_iter->second.packet->get_data(),
                            pkt_iter->second.packet->get_data_length());
    }
}

rtcp_sr_packet* rtp_send_stream::get_rtcp_sr(int64_t now_ms) {
    rtcp_sr_packet* sr_pkt = new rtcp_sr_packet();

    last_sr_ntp_ts_ = millisec_to_ntp(now_ms);
    last_sr_rtp_ts_ = (uint32_t)(now_ms / 1000 * clock_);

    sr_pkt->set_ssrc(rtp_ssrc_);
    sr_pkt->set_ntp(last_sr_ntp_ts_.ntp_sec, last_sr_ntp_ts_.ntp_frac);
    sr_pkt->set_rtp_timestamp(last_sr_rtp_ts_);
    sr_pkt->set_pkt_count(send_statics_.get_count());
    sr_pkt->set_bytes_count(send_statics_.get_bytes());

    return sr_pkt;
}

void rtp_send_stream::handle_rtcp_rr(rtcp_rr_packet* rr_pkt) {
    lost_total_ = rr_pkt->get_cumulative_lost();
    uint8_t frac_lost = rr_pkt->get_fraclost();
    lost_rate_ = (float)(frac_lost/256.0);
    jitter_ = rr_pkt->get_jitter();

    //RTT= RTP发送方本地时间 - RR中LSR - RR中DLSR
    uint32_t lsr = rr_pkt->get_lsr();
    uint32_t dlsr = rr_pkt->get_dlsr();

    if (lsr == 0) {
        rtt_ = RTT_DEFAULT;
        return;
    }
    int64_t now_ms = (int64_t)now_millisec();
    NTP_TIMESTAMP lsr_ntp = {
        .ntp_sec = (lsr & 0xffff0000) >> 16,
        .ntp_frac = (lsr & 0xffff) << 16
    };
    int64_t lsr_ms = ntp_to_millisec(lsr_ntp);
    int64_t dlsr_ms = dlsr * 1000 / 65536;

    NTP_TIMESTAMP now_ntp = millisec_to_ntp(now_ms);
    now_ntp.ntp_sec  = now_ntp.ntp_sec & 0x0000ffff;
    now_ntp.ntp_frac = now_ntp.ntp_frac & 0xffff0000;
    uint32_t now_uint32_ms = ntp_to_millisec(now_ntp);

    rtt_ = now_uint32_ms - lsr_ms - dlsr_ms;

    log_infof("handle rtcp rr media(%s), ssrc:%u, lost total:%u, lost rate:%.03f, jitter:%u, rtt_:%d",
        media_type_.c_str(), rtp_ssrc_, lost_total_, lost_rate_, jitter_, rtt_);
    log_infof("handle rtcp rr now_uint32_ms:%u, lsr ms:%ld, dlsr ms:%ld",
        now_uint32_ms, lsr_ms, dlsr_ms);
    if (rtt_ < 0) {
        rtt_ = 1;
    }
    if (rtt_ > 150) {
        rtt_ = 150;
    }
}

void rtp_send_stream::on_timer() {
    int64_t now_ms = (int64_t)now_millisec();
    int64_t ret = ++statics_count_;

    if ((ret%12) == 0) {
        size_t fps;
        size_t speed = send_statics_.bytes_per_second(now_ms, fps);
    
        log_infof("rtc send mediatype:%s, ssrc:%u, payloadtype:%d, speed(bytes/s):%lu, fps:%lu",
            media_type_.c_str(), rtp_ssrc_, rtp_payload_type_, speed, fps);
    }

    if ((ret%2) == 0) {
        rtcp_sr_packet* sr_pkt = get_rtcp_sr(now_ms);

        log_infof("rtcp sr ssrc:%u, ntp sec:%u, ntp frac:%u, rtp ts:%u, pkt count:%u, bytes count:%u, data len:%lu, data:%p",
            sr_pkt->get_ssrc(), sr_pkt->get_ntp_sec(), sr_pkt->get_ntp_frac(),
            sr_pkt->get_rtp_timestamp(), sr_pkt->get_pkt_count(), sr_pkt->get_bytes_count(),
            sr_pkt->get_data_len(), sr_pkt->get_data());
        log_info_data(sr_pkt->get_data(), sr_pkt->get_data_len(), "rtcp sr data");
        cb_->stream_send_rtcp(sr_pkt->get_data(), sr_pkt->get_data_len());
        delete sr_pkt;
    }
}