#ifndef AV_OUTPUT_HPP
#define AV_OUTPUT_HPP
#include "media_packet.hpp"
#include "av_format_interface.hpp"

class av_outputer : public av_format_callback
{
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;
};

#endif