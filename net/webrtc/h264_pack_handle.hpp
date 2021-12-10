#ifndef H264_PACK_HANDLE_HPP
#define H264_PACK_HANDLE_HPP
#include "pack_handle_pub.hpp"
#include <map>

class h264_pack_handle
{
public:
    h264_pack_handle();
    virtual ~h264_pack_handle();

public:
    virtual void input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) override;
    virtual void rtp_packet_reset(std::shared_ptr<rtp_packet_info> pkt_ptr) override;
    virtual void media_packet_output(std::shared_ptr<MEDIA_PACKET> pkt_ptr) override;

private:
    std::map<int64_t, std::shared_ptr<rtp_packet_info>> packets_map_;
};

#endif