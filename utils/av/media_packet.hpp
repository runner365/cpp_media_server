#ifndef MEDIA_PACKET_HPP
#define MEDIA_PACKET_HPP
#include "av.hpp"
#include "data_buffer.hpp"

#include <stdint.h>
#include <string>
#include <memory>
#include <sstream>

class MEDIA_PACKET
{
public:
    MEDIA_PACKET()
    {
        buffer_ptr_ = std::make_shared<data_buffer>();
    }
    MEDIA_PACKET(size_t len)
    {
        buffer_ptr_ = std::make_shared<data_buffer>(len);
    }
    ~MEDIA_PACKET()
    {
    }

    void copy_properties(const MEDIA_PACKET& pkt) {
        this->av_type_      = pkt.av_type_;
        this->codec_type_   = pkt.codec_type_;
        this->fmt_type_     = pkt.fmt_type_;
        this->dts_          = pkt.dts_;
        this->pts_          = pkt.pts_;
        this->is_key_frame_ = pkt.is_key_frame_;
        this->is_seq_hdr_   = pkt.is_seq_hdr_;

        this->key_        = pkt.key_;
        this->vhost_      = pkt.vhost_;
        this->app_        = pkt.app_;
        this->streamname_ = pkt.streamname_;
        this->streamid_   = pkt.streamid_;
        this->typeid_     = pkt.typeid_;
    }

    void copy_properties(const std::shared_ptr<MEDIA_PACKET> pkt_ptr) {
        this->av_type_      = pkt_ptr->av_type_;
        this->codec_type_   = pkt_ptr->codec_type_;
        this->fmt_type_     = pkt_ptr->fmt_type_;
        this->dts_          = pkt_ptr->dts_;
        this->pts_          = pkt_ptr->pts_;
        this->is_key_frame_ = pkt_ptr->is_key_frame_;
        this->is_seq_hdr_   = pkt_ptr->is_seq_hdr_;

        this->key_        = pkt_ptr->key_;
        this->vhost_      = pkt_ptr->vhost_;
        this->app_        = pkt_ptr->app_;
        this->streamname_ = pkt_ptr->streamname_;
        this->streamid_   = pkt_ptr->streamid_;
        this->typeid_     = pkt_ptr->typeid_;
    }

    std::string dump() {
        std::stringstream ss;
        
        ss << "av type:" << avtype_tostring(av_type_) << ", codec type:" << codectype_tostring(codec_type_)
           << ", format type:" << formattype_tostring(fmt_type_) << ", dts:" << dts_ << ", pts:" << pts_
           << ", is key frame:" << is_key_frame_ << ", is seq frame:" << is_seq_hdr_
           << ", data length:" << buffer_ptr_->data_len();
        return ss.str();
    }

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