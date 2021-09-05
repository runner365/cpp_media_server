#ifndef FLV_MUX_HPP
#define FLV_MUX_HPP
#include "data_buffer.hpp"
#include "format/av_format_interface.hpp"

class flv_muxer
{
public:
    flv_muxer(bool has_video, bool has_audio, av_format_callback* cb);
    ~flv_muxer();

public:
    int input_packet(MEDIA_PACKET_PTR pkt_ptr);
    static int add_flv_media_header(MEDIA_PACKET_PTR pkt_ptr);

private:
    int mux_flv_header(MEDIA_PACKET_PTR pkt_ptr);

private:
    bool has_video_;
    bool has_audio_;
    av_format_callback* cb_;

private:
    bool header_ready_ = false;
};

#endif