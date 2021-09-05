#ifndef MEDIA_PACKET_HPP
#define MEDIA_PACKET_HPP
#include "data_buffer.hpp"
#include <stdint.h>
#include <string>
#include <memory>

typedef enum {
    MEDIA_UNKOWN_TYPE = 0,
    MEDIA_VIDEO_TYPE = 1,
    MEDIA_AUDIO_TYPE,
    MEDIA_METADATA_TYPE
} MEDIA_PKT_TYPE;

typedef enum {
    MEDIA_CODEC_UNKOWN = 0,
    MEDIA_CODEC_H264 = 1,
    MEDIA_CODEC_H265,
    MEDIA_CODEC_VP8,
    MEDIA_CODEC_VP9,
    MEDIA_CODEC_AAC = 100,
    MEDIA_CODEC_OPUS,
    MEDIA_CODEC_MP3
} MEDIA_CODEC_TYPE;

typedef enum {
    MEDIA_FORMAT_UNKOWN = 0,
    MEDIA_FORMAT_RAW,
    MEDIA_FORMAT_FLV
} MEDIA_FORMAT_TYPE;

class MEDIA_PACKET
{
public:
    MEDIA_PACKET()
    {
        buffer_ptr_ = std::make_shared<data_buffer>();
    }
    ~MEDIA_PACKET()
    {
    }

//av common info: 
//    av type;
//    codec type;
//    timestamp;
//    is key frame;
//    is seq hdr;
//    media data in bytes;
public:
    MEDIA_PKT_TYPE av_type_      = MEDIA_UNKOWN_TYPE;
    MEDIA_CODEC_TYPE codec_type_ = MEDIA_CODEC_UNKOWN;
    MEDIA_FORMAT_TYPE fmt_type_  = MEDIA_FORMAT_UNKOWN;
    int64_t dts_ = 0;
    int64_t pts_ = 0;
    bool is_key_frame_ = false;
    bool is_seq_hdr_   = false;
    std::shared_ptr<data_buffer> buffer_ptr_;

//rtmp info:
public:
    std::string key_;//vhost(option)_appname_streamname
    std::string vhost_;
    std::string app_;
    std::string streamname_;
    uint32_t streamid_ = 0;
    uint8_t typeid_ = 0;
};

typedef std::shared_ptr<MEDIA_PACKET> MEDIA_PACKET_PTR;

class av_writer_base
{
public:
    virtual int write_packet(MEDIA_PACKET_PTR) = 0;
    virtual std::string get_key() = 0;
    virtual std::string get_writerid() = 0;
    virtual void close_writer() = 0;
    virtual bool is_inited() = 0;
    virtual void set_init_flag(bool flag) = 0;
};

#endif//MEDIA_PACKET_HPP