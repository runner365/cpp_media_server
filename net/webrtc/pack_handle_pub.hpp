#ifndef PACK_HANDLE_PUB_HPP
#define PACK_HANDLE_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <memory>
#include "jitterbuffer_pub.hpp"
#include "utils/av/media_packet.hpp"

#define PACK_BUFFER_TIMEOUT 400 //ms

class pack_callbackI
{
public:
    virtual void pack_handle_reset(std::shared_ptr<rtp_packet_info> pkt_ptr) = 0;
    virtual void media_packet_output(std::shared_ptr<MEDIA_PACKET> pkt_ptr) = 0;
};

class pack_handle_base
{
public:
    pack_handle_base()
    {
    }
    virtual ~pack_handle_base()
    {
    }
    
public:
    virtual void input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) = 0;
};


#endif
