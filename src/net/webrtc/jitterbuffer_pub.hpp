#ifndef JITTER_BUFFER_PUB_HPP
#define JITTER_BUFFER_PUB_HPP

#include "rtp_packet.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <memory>

#define JITTER_BUFFER_TIMEOUT 600 //ms

class rtp_packet_info
{
public:
    rtp_packet_info(const std::string& roomId, const std::string& uid,
            const std::string& media_type, const std::string& stream_type,
            int clock_rate, rtp_packet* input_pkt, int64_t extend_seq):roomId_(roomId)
                                , uid_(uid)
                                , media_type_(media_type)
                                , stream_type_(stream_type)
                                , extend_seq_(extend_seq)
                                , clock_rate_(clock_rate)
    {
        this->pkt = input_pkt->clone();
    }

    ~rtp_packet_info()
    {
        delete this->pkt;
        this->pkt = nullptr;
    }

public:
    std::string roomId_;
    std::string uid_;
    std::string media_type_;//"video" or "audio"
    std::string stream_type_;//"camera" or "screen"
    rtp_packet* pkt = nullptr;
    int64_t extend_seq_ = 0;
    int clock_rate_     = 0;
};

class jitterbuffer_callbackI
{
public:
    virtual void rtp_packet_reset(std::shared_ptr<rtp_packet_info> pkt_ptr) = 0;
    virtual void rtp_packet_output(std::shared_ptr<rtp_packet_info> pkt_ptr) = 0;
};

#endif