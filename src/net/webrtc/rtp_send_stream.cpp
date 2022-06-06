#include "rtp_send_stream.hpp"
#include "rtc_stream_pub.hpp"
#include "utils/stream_statics.hpp"
#include "utils/timeex.hpp"
#include "utils/byte_crypto.hpp"
#include <stddef.h>
#include "json.hpp"
#include <sstream>

using json = nlohmann::json;

static const size_t RTP_VIDEO_BUFFER_MAX = 800;
static const size_t RTP_AUDIO_BUFFER_MAX = 100;
static const size_t STORAGE_MAX_SIZE     = 2000;

rtp_send_stream::rtp_send_stream(const std::string& media_type, bool nack_enable,
                        rtc_stream_callback* cb):rtp_packet_array(65536, nullptr)
                                                , media_type_(media_type)
                                                , nack_enable_(nack_enable)
                                                , cb_(cb)
{
    storages_.resize(STORAGE_MAX_SIZE);
}

rtp_send_stream::~rtp_send_stream() {
    clear_buffer();
}

void rtp_send_stream::get_statics(json& json_data) {
    size_t fps;
    size_t speed = send_statics_.bytes_per_second((int64_t)now_millisec(), fps);

    json_data["bps"] = speed * 8;
    json_data["fps"] = fps;
    json_data["count"] = send_statics_.get_count();
    json_data["bytes"] = send_statics_.get_bytes();

    json_data["rtt"] = avg_rtt_;
    json_data["lostrate"] = lost_rate_;
    json_data["losttotal"] = lost_total_;
    json_data["jitter"] = jitter_;

    return;
}

void rtp_send_stream::save_buffer(rtp_packet* input_pkt) {
    uint16_t seq = input_pkt->get_seq();
    size_t buffer_max = (media_type_ == "video") ? RTP_VIDEO_BUFFER_MAX : RTP_AUDIO_BUFFER_MAX;

    size_t storage_index = seq % STORAGE_MAX_SIZE;

    if (storages_[storage_index].packet != nullptr) {
        if (storages_[storage_index].packet->get_timestamp() == input_pkt->get_timestamp()) {
            return;
        }
        assert(false == storages_[storage_index].packet->get_need_delete());
        delete storages_[storage_index].packet;

        storages_[storage_index].packet              = nullptr;
        storages_[storage_index].last_sent_timestamp = 0;
        storages_[storage_index].sent_count          = 0;
    }

    if (first_seq_ < 0) {
        first_seq_ = seq;
    }

    storages_[storage_index].packet              = input_pkt->clone(storages_[storage_index].data);
    storages_[storage_index].last_sent_timestamp = 0;
    storages_[storage_index].sent_count          = 0;

    rtp_packet_array[seq] = &(storages_[storage_index]);

    pkt_count_++;

    int repeat_count = 0;
    while (pkt_count_ > (int)buffer_max) {
        if (repeat_count++ > 5000) {
            break;
        }
        size_t index = first_seq_ % STORAGE_MAX_SIZE;

        rtp_packet_array[first_seq_] = nullptr;
        if (storages_[index].packet == nullptr) {
            first_seq_ = (first_seq_ + 1) % 65536;
            storages_[index].last_sent_timestamp = 0;
            storages_[index].sent_count          = 0;
        } else {
            assert(false == storages_[index].packet->get_need_delete());
            delete storages_[index].packet;

            storages_[index].packet              = nullptr;
            storages_[index].last_sent_timestamp = 0;
            storages_[index].sent_count          = 0;
            
            first_seq_ = (first_seq_ + 1) % 65536;
            if (pkt_count_ > 0) {
                pkt_count_--;
            }
        }
    }
}

void rtp_send_stream::on_send_rtp_packet(rtp_packet* pkt) {
    send_statics_.update(pkt->get_data_length(),  pkt->get_local_ms());

    pkt->set_payload_type(rtp_payload_type_);
    pkt->set_ssrc(rtp_ssrc_);

    if (nack_enable_) {
        save_buffer(pkt);
    }
    return;
}

void rtp_send_stream::clear_buffer() {
    for (size_t i = 0; i < STORAGE_MAX_SIZE; i++) {
        if (storages_[i].packet) {
            assert(false == storages_[i].packet->get_need_delete());
            delete storages_[i].packet;
        }
        storages_[i].packet              = nullptr;
        storages_[i].last_sent_timestamp = 0;
        storages_[i].sent_count          = 0;
    }
    pkt_count_ = 0;
}

void rtp_send_stream::handle_fb_rtp_nack(rtcp_fb_nack* nack_pkt) {
    std::vector<uint16_t> lost_seqs = nack_pkt->get_lost_seqs();
    int64_t now_ms = now_millisec();

    //std::stringstream ss;
    //ss << "[";
    //for (auto seq : lost_seqs) {
    //    ss << " " << seq;
    //}
    //ss << " ]";
    //log_infof("media ssrc:%u, nack lost seqs:%s, avg rtt:%.02f",
    //    nack_pkt->get_media_ssrc(), ss.str().c_str(), avg_rtt_);

    for (auto seq : lost_seqs) {
        if ((rtp_packet_array[seq] == nullptr) || (rtp_packet_array[seq]->packet == nullptr)) {
            log_warnf("nack seq[%d] can't be found", seq);
            continue;
        }
        
        if (rtp_packet_array[seq]->last_sent_timestamp == 0) {
            rtp_packet_array[seq]->last_sent_timestamp = now_ms;
            rtp_packet_array[seq]->sent_count = 1;
        } else {
            int64_t diff_t = now_ms - rtp_packet_array[seq]->last_sent_timestamp;
            if ((diff_t < avg_rtt_) && (avg_rtt_ <= 150)) {
                log_warnf("resend is too often, seq:%d, diff:%ld",
                    rtp_packet_array[seq]->packet->get_seq(), diff_t);
                continue;
            }
            
            rtp_packet_array[seq]->sent_count++;
            if (rtp_packet_array[seq]->sent_count > RETRANSMIT_MAX_COUNT) {
                log_errorf("the lost sequence(%d) has been retransmited over times(%d), avg rtt:%.02f",
                        seq, rtp_packet_array[seq]->sent_count, avg_rtt_);
                continue;
            }
            rtp_packet_array[seq]->last_sent_timestamp = now_ms;
        }
        if (rtp_packet_array[seq]->sent_count > 3) {
            for (int i = 0; i < 2; i++) {
                cb_->stream_send_rtp(rtp_packet_array[seq]->packet->get_data(),
                                rtp_packet_array[seq]->packet->get_data_length());
            }
        } else {
            cb_->stream_send_rtp(rtp_packet_array[seq]->packet->get_data(),
                            rtp_packet_array[seq]->packet->get_data_length());
        }
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

    NTP_TIMESTAMP now_ntp = millisec_to_ntp(now_ms);
    uint32_t compact_ntp = (now_ntp.ntp_sec & 0x0000FFFF) << 16;

    compact_ntp |= (now_ntp.ntp_frac & 0xFFFF0000) >> 16;

    uint32_t rtt = 0;

    // If no Sender Report was received by the remote endpoint yet, ignore lastSr
    // and dlsr values in the Receiver Report.
    if (lsr && dlsr && (compact_ntp > dlsr + lsr)){
        rtt = compact_ntp - dlsr - lsr;
    }
    rtt_ = static_cast<float>(rtt >> 16) * 1000;
    rtt_ += (static_cast<float>(rtt & 0x0000FFFF) / 65536) * 1000;

    if (avg_rtt_ == 0.0) {
        avg_rtt_ = rtt_;
    } else {
        avg_rtt_ += (rtt_ - avg_rtt_) / 4.0;
    }
    log_debugf("handle rtcp rr media(%s), ssrc:%u, lost total:%u, lost rate:%.03f, jitter:%u, rtt_:%.02f, avg rtt:%.02f",
        media_type_.c_str(), rtp_ssrc_, lost_total_, lost_rate_, jitter_, rtt_, avg_rtt_);
}

void rtp_send_stream::on_timer(int64_t now_ms) {
    const int64_t STATICS_TIMEOUT    = 2500;
    const int64_t VIDEO_RTCP_TIMEOUT = 800;
    const int64_t AUDIO_RTCP_TIMEOUT = 4000;

    if (last_statics_ms_ == 0) {
        last_statics_ms_ = now_ms;
    } else {
        if ((now_ms - last_statics_ms_) > STATICS_TIMEOUT) {
            size_t fps;
            size_t speed = send_statics_.bytes_per_second(now_ms, fps);

            last_statics_ms_ = now_ms;
        
            log_debugf("rtc send mediatype:%s, ssrc:%u, payloadtype:%d, speed(bytes/s):%lu, fps:%lu",
                media_type_.c_str(), rtp_ssrc_, rtp_payload_type_, speed, fps);
        }
    }

    if (last_send_rtcp_ts_ == 0) {
        last_send_rtcp_ts_ = now_ms;
    } else {
        bool send_sr_flag = false;
        uint32_t rand_number = byte_crypto::get_random_uint(5, 15);
        float ratio = rand_number / 10.0;

        if (media_type_ == "video") {
            send_sr_flag = ((now_ms - last_send_rtcp_ts_) * ratio > VIDEO_RTCP_TIMEOUT);
        } else {
            send_sr_flag = ((now_ms - last_send_rtcp_ts_) * ratio > AUDIO_RTCP_TIMEOUT);
        }
        if (send_sr_flag) {
            last_send_rtcp_ts_ = now_ms;

            rtcp_sr_packet* sr_pkt = get_rtcp_sr(now_ms);
    
            log_debugf("rtcp sr ssrc:%u, ntp sec:%u, ntp frac:%u, rtp ts:%u",
                sr_pkt->get_ssrc(), sr_pkt->get_ntp_sec(), sr_pkt->get_ntp_frac(),
                sr_pkt->get_rtp_timestamp());
            cb_->stream_send_rtcp(sr_pkt->get_data(), sr_pkt->get_data_len());
            delete sr_pkt;
        }
    }

}
