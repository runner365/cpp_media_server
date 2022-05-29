#ifndef JITTER_BUFFER_HPP
#define JITTER_BUFFER_HPP

#include "rtp_packet.hpp"
#include "jitterbuffer_pub.hpp"
#include "timer.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <map>
#include <memory>
#include <uv.h>

class jitterbuffer : public timer_interface
{
public:
    jitterbuffer(jitterbuffer_callbackI* cb, uv_loop_t* loop);
    ~jitterbuffer();

public:
    void input_rtp_packet(const std::string& roomId, const std::string& uid,
            const std::string& media_type, const std::string& stream_type,
            int clock_rate, rtp_packet* input_pkt);

public:
    virtual void on_timer() override;

private:
    void init_seq(rtp_packet* input_pkt);
    bool update_seq(rtp_packet* input_pkt, int64_t& extend_seq, bool& reset);
    void output_packet(std::shared_ptr<rtp_packet_info>);
    void check_timeout();
    void report_lost(std::shared_ptr<rtp_packet_info> pkt_ptr);

private:
    jitterbuffer_callbackI* cb_ = nullptr;
    bool init_flag_ = false;
    uint16_t base_seq_ = 0;
    uint16_t max_seq_  = 0;
    uint32_t bad_seq_  = RTP_SEQ_MOD + 1;   /* so seq == bad_seq is false */
    uint32_t cycles_   = 0;
    std::map<int64_t, std::shared_ptr<rtp_packet_info>> rtp_packets_map_;//key: extend_seq, value: rtp packet info

private:
    int64_t output_seq_ = 0;
    int64_t report_lost_ts_ = -1;
};

#endif