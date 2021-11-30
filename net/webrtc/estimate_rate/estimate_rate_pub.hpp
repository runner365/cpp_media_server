#ifndef ESTIMATE_RATE_PUB_HPP
#define ESTIMATE_RATE_PUB_HPP
#include "net/rtprtcp/rtp_packet.hpp"

class estimate_rate_base
{
public:
    virtual void input_rtp_packet(int64_t now_ms, rtp_packet* pkt) = 0;
    virtual int64_t get_bitrate() = 0;
};

#endif