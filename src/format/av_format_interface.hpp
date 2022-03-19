#ifndef AV_FORMAT_INTERFACE_HPP
#define AV_FORMAT_INTERFACE_HPP
#include "av/media_packet.hpp"
#include <memory>

class av_format_callback
{
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) = 0;
};

#endif

