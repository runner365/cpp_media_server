#include "jitterbuffer.hpp"
#include "timeex.hpp"

jitterbuffer::jitterbuffer(jitterbuffer_callbackI* cb, boost::asio::io_context& io_ctx):timer_interface(io_ctx, 100)
                    , cb_(cb) {

}

jitterbuffer::~jitterbuffer() {

}

void jitterbuffer::input_rtp_packet(const std::string& roomId, const std::string& uid,
                            const std::string& media_type, const std::string& stream_type,
                            int clock_rate, rtp_packet* input_pkt) {
    int64_t extend_seq = 0;
    bool reset = false;
    bool first_pkt = false;

    if (!init_flag_) {
        init_flag_ = true;
        init_seq(input_pkt);
        
        reset = true;
        first_pkt = true;
        extend_seq = input_pkt->get_seq();
    } else {
        bool bad_pkt = update_seq(input_pkt, extend_seq, reset);
        if (!bad_pkt) {
            return;
        }
        if (reset) {
            init_flag_ = false;
        }
    }

    std::shared_ptr<rtp_packet_info> pkt_info_ptr = std::make_shared<rtp_packet_info>(roomId,
                                                                                uid,
                                                                                media_type,
                                                                                stream_type,
                                                                                clock_rate,
                                                                                input_pkt,
                                                                                extend_seq);
    if (reset) {
        //if the rtc client is reset, call the reset callback which send pli
        report_lost(pkt_info_ptr);
    }

    //if it's the first packet, output the packet
    if (first_pkt || reset) {
        output_packet(pkt_info_ptr);
        return;
    }

    //if the seq is continued, output the packet
    if ((output_seq_ + 1) == extend_seq) {
        output_packet(pkt_info_ptr);

        //check the packet in map
        for (auto iter = rtp_packets_map_.begin();
            iter != rtp_packets_map_.end();
            ) {
            int64_t pkt_extend_seq = iter->first;
            if ((output_seq_ + 1) == pkt_extend_seq) {
                if (iter->second->media_type_ == "video") {
                    log_debugf("jitter buffer media type:%s, output seq(%d) in buffer queue",
                        iter->second->media_type_.c_str(), pkt_extend_seq);
                }
                output_packet(iter->second);
                iter = rtp_packets_map_.erase(iter);
                continue;
            }
            break;
        }
        return;
    } else if (extend_seq <= output_seq_) {
        log_infof("receive old seq:%d media type:%s", extend_seq, pkt_info_ptr->media_type_.c_str());
        return;
    }
    rtp_packets_map_[extend_seq] = pkt_info_ptr;
    if (pkt_info_ptr->media_type_ == "video") {
        log_debugf("jitterbuffer media type:%s, packets queue len:%lu, pkt seq:%d, last output seq:%d",
            pkt_info_ptr->media_type_.c_str(), rtp_packets_map_.size(), pkt_info_ptr->extend_seq_, output_seq_);
    }

    check_timeout();

    return;
}

void jitterbuffer::on_timer() {
    check_timeout();
}

void jitterbuffer::check_timeout() {
    if (rtp_packets_map_.empty()) {
        return;
    }
    int64_t now_ms = now_millisec();
    for(auto iter = rtp_packets_map_.begin();
        iter != rtp_packets_map_.end();) {
        std::shared_ptr<rtp_packet_info> pkt_info_ptr = iter->second;
        int64_t diff_t = now_ms - pkt_info_ptr->pkt->get_local_ms();

        if (diff_t > JITTER_BUFFER_TIMEOUT) {
            if (pkt_info_ptr->media_type_ == "video") {
                log_infof("timeout output type:%s, seq:%d",
                    pkt_info_ptr->media_type_.c_str(), pkt_info_ptr->extend_seq_);
            }

            output_packet(pkt_info_ptr);
            iter = rtp_packets_map_.erase(iter);
            report_lost(pkt_info_ptr);
            continue;
        }
        if ((output_seq_ + 1) == pkt_info_ptr->extend_seq_) {
            output_packet(iter->second);
            iter = rtp_packets_map_.erase(iter);
            continue;
        }
        iter++;
    }

    return;
}

void jitterbuffer::report_lost(std::shared_ptr<rtp_packet_info> pkt_ptr) {
    int64_t now_ms = now_millisec();

    if (now_ms - report_lost_ts_ > 500) {
        report_lost_ts_ = now_ms;
        cb_->rtp_packet_reset(pkt_ptr);
    }
    
}

void jitterbuffer::output_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) {
    output_seq_ = pkt_ptr->extend_seq_;
    cb_->rtp_packet_output(pkt_ptr);
    return;
}

void jitterbuffer::init_seq(rtp_packet* input_pkt) {
    base_seq_ = input_pkt->get_seq();
    max_seq_  = input_pkt->get_seq();
    bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    cycles_   = 0;
}

bool jitterbuffer::update_seq(rtp_packet* input_pkt, int64_t& extend_seq, bool& reset) {
    const int MAX_DROPOUT    = 3000;
    const int MAX_MISORDER   = 100;
    uint16_t seq = input_pkt->get_seq();
    //const int MIN_SEQUENTIAL = 2;

    uint16_t udelta = seq - max_seq_;

    reset = false;

    if (udelta < MAX_DROPOUT) {
        /* in order, with permissible gap */
        if (seq < max_seq_) {
            /*
             * Sequence number wrapped - count another 64K cycle.
             */
            cycles_ += RTP_SEQ_MOD;
        }
        max_seq_ = seq;
    } else if (udelta <= RTP_SEQ_MOD - MAX_MISORDER) {
            /* the sequence number made a very large jump */
            if (seq == bad_seq_) {
                /*
                 * Two sequential packets -- assume that the other side
                 * restarted without telling us so just re-sync
                 * (i.e., pretend this was the first packet).
                 */
                init_seq(input_pkt);
                reset = true;
                extend_seq = cycles_ + seq;
                return true;
            } else {
                bad_seq_= (seq + 1) & (RTP_SEQ_MOD-1);
                return false;
            }
    } else {
        /* duplicate or reordered packet */
    }
    extend_seq = cycles_ + seq;
    return true;
}