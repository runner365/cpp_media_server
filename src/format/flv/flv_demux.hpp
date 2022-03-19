#ifndef FLV_DEMUXER_HPP
#define FLV_DEMUXER_HPP
#include "data_buffer.hpp"
#include "format/av_format_interface.hpp"

#define FLV_RET_NEED_MORE    1

#define FLV_HEADER_LEN     9
#define FLV_TAG_PRE_SIZE   4
#define FLV_TAG_HEADER_LEN 11

class flv_demuxer
{
public:
    flv_demuxer(av_format_callback* cb);
    ~flv_demuxer();

public:
    int input_packet(MEDIA_PACKET_PTR pkt_ptr);
    bool has_video() {return has_video_;}
    bool has_audio() {return has_audio_;}

private:
    int handle_packet();

private:
    av_format_callback* callback_ = nullptr;
    data_buffer buffer_;
    std::string key_;
    bool has_video_ = false;
    bool has_audio_ = false;

private:
    bool flv_header_ready_ = false;
    bool tag_header_ready_ = false;

private:
    uint8_t tag_type_ = 0;
    uint32_t tag_data_size_ = 0;
    uint32_t tag_timestamp_ = 0;
};

#endif //FLV_DEMUXER_HPP