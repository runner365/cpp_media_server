#include "nack_generator.hpp"
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "logger.hpp"
#include "timeex.hpp"
#include <algorithm>
#include <sstream>

extern uv_loop_t* get_global_io_context();

nack_generator::nack_generator(nack_generator_callback_interface* cb):timer_interface(get_global_io_context(), NACK_DEFAULT_TIMEOUT)
    , cb_(cb)
{
    start_timer();
}

nack_generator::~nack_generator()
{
    stop_timer();
}

void nack_generator::update_rtt(int64_t rtt) {
    rtt_ = rtt;
}

void nack_generator::update_nacklist(rtp_packet* pkt) {
    uint16_t seq = pkt->get_seq();

    if (!init_flag_) {
        init_flag_ = true;
        last_seq_  = seq;
        return;
    }

    //receive repeated sequence
    if (seq == last_seq_) {
        return;
    }

    if (seq_lower_than(seq, last_seq_)) {
        auto iter = nack_map_.find(seq);

        //the seq has been in the nack list, remove it.
        if (iter != nack_map_.end()) {
            nack_map_.erase(iter);
            log_debugf("remove from nack map, ssrc:%u, seq:%d, last seq:%d, payloadtype:%d",
                pkt->get_ssrc(), seq, last_seq_, pkt->get_payload_type());
            return;
        }
        std::stringstream ss;

        ss << "[";
        for (auto item : nack_map_) {
            ss << " " << item.first;
        }
        ss << " ]";
        log_debugf("receive the old packet which is not in nack list, ssrc:%u, seq:%d, last seq:%d, payloadtype:%d, nack list:%s",
            pkt->get_ssrc(), seq, last_seq_, pkt->get_payload_type(), ss.str().c_str());
        return;
    }

    if ((last_seq_ + 1) == seq) {
        last_seq_ = seq;
        return;
    }

    uint16_t seq_start = last_seq_;
    uint16_t seq_end   = seq;
    
    last_seq_ = seq;

    //add seqs in nack list
    for (uint16_t key_seq = seq_start + 1; key_seq < seq_end; key_seq++) {
        auto iter = nack_map_.find(key_seq);
        if (iter == nack_map_.end()) {
            nack_map_.insert(std::make_pair(key_seq, NACK_INFO(key_seq, 0, 0)));
        }
    }
}

void nack_generator::on_timer() {
    if (nack_map_.empty()) {
        return;
    }

    int64_t now_ms = now_millisec();
    std::vector<uint16_t> lost_seq_list;
    auto iter = nack_map_.begin();

    while(iter != nack_map_.end()) {
        if (iter->second.retry > NACK_RETRY_MAX) {
            iter = nack_map_.erase(iter);
            continue;
        }
        if (now_ms - iter->second.sent_ms < rtt_) {
            iter++;
            continue;
        }
        iter->second.sent_ms = now_ms;
        iter->second.retry++;

        lost_seq_list.push_back(iter->first);

        iter++;
    }

    if (!lost_seq_list.empty()) {
        std::sort(lost_seq_list.begin(), lost_seq_list.end());
        cb_->generate_nacklist(lost_seq_list);
    }

    if (nack_map_.size() > NACK_LIST_MAX) {
        log_warnf("the nack list is overflow(%lu) and the list threshold is %d",
            nack_map_.size(), NACK_LIST_MAX);
    }

    while (nack_map_.size() > NACK_LIST_MAX) {
        nack_map_.erase(nack_map_.begin());
    }
}
