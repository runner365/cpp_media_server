#ifndef MEDIA_PACKET_HPP
#define MEDIA_PACKET_HPP
#include "data_buffer.hpp"
#include <stdint.h>
#include <string>
#include <memory>

enum H264AvcNaluType
{   
    // Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
    kAvcNaluTypeNonIDR = 1,
    // Coded slice data partition A slice_data_partition_a_layer_rbsp( )
    kAvcNaluTypeDataPartitionA = 2,
    // Coded slice data partition B slice_data_partition_b_layer_rbsp( )
    kAvcNaluTypeDataPartitionB = 3,
    // Coded slice data partition C slice_data_partition_c_layer_rbsp( )
    kAvcNaluTypeDataPartitionC = 4,
    // Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
    kAvcNaluTypeIDR = 5,
    // Supplemental enhancement information (SEI) sei_rbsp( )
    kAvcNaluTypeSEI = 6,
    // Sequence parameter set seq_parameter_set_rbsp( )
    kAvcNaluTypeSPS = 7,
    // Picture parameter set pic_parameter_set_rbsp( )
    kAvcNaluTypePPS = 8,
    // Access unit delimiter access_unit_delimiter_rbsp( )
    kAvcNaluTypeAccessUnitDelimiter = 9,
    // End of sequence end_of_seq_rbsp( )
    kAvcNaluTypeEOSequence = 10,
    // End of stream end_of_stream_rbsp( )
    kAvcNaluTypeEOStream = 11,
    // Filler data filler_data_rbsp( )
    kAvcNaluTypeFilterData = 12,
    // Sequence parameter set extension seq_parameter_set_extension_rbsp( )
    kAvcNaluTypeSPSExt = 13,
    // Prefix NAL unit prefix_nal_unit_rbsp( )
    kAvcNaluTypePrefixNALU = 14,
    // Subset sequence parameter set subset_seq_parameter_set_rbsp( )
    kAvcNaluTypeSubsetSPS = 15,
    // Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
    kAvcNaluTypeLayerWithoutPartition = 19,
    // Coded slice extension slice_layer_extension_rbsp( )
    kAvcNaluTypeCodedSliceExt = 20,
};

inline std::string avc_nalu2str(H264AvcNaluType nalu_type) {
    switch (nalu_type) {
        case kAvcNaluTypeNonIDR: return "NonIDR";
        case kAvcNaluTypeDataPartitionA: return "DataPartitionA";
        case kAvcNaluTypeDataPartitionB: return "DataPartitionB";
        case kAvcNaluTypeDataPartitionC: return "DataPartitionC";
        case kAvcNaluTypeIDR: return "IDR";
        case kAvcNaluTypeSEI: return "SEI";
        case kAvcNaluTypeSPS: return "SPS";
        case kAvcNaluTypePPS: return "PPS";
        case kAvcNaluTypeAccessUnitDelimiter: return "AccessUnitDelimiter";
        case kAvcNaluTypeEOSequence: return "EOSequence";
        case kAvcNaluTypeEOStream: return "EOStream";
        case kAvcNaluTypeFilterData: return "FilterData";
        case kAvcNaluTypeSPSExt: return "SPSExt";
        case kAvcNaluTypePrefixNALU: return "PrefixNALU";
        case kAvcNaluTypeSubsetSPS: return "SubsetSPS";
        case kAvcNaluTypeLayerWithoutPartition: return "LayerWithoutPartition";
        case kAvcNaluTypeCodedSliceExt: return "CodedSliceExt";
        default: return "";
    }
}

inline void get_video_extradata(unsigned char *pps, int pps_len, 
                                unsigned char *sps, int sps_len, 
                                unsigned char *extra_data, int& extra_len)
{
    unsigned char * body= nullptr;
    int index = 0;
    
    body = extra_data;

    body[index++] = 0x01;
    body[index++] = sps[1];
    body[index++] = sps[2];
    body[index++] = sps[3];
    body[index++] = 0xff;
    
    /*sps*/
    body[index++] = 0xe1;
    body[index++] = (sps_len >> 8) & 0xff;
    body[index++] = sps_len & 0xff;
    memcpy(&body[index],sps,sps_len);
    index +=  sps_len;
    
    /*pps*/
    body[index++]   = 0x01;
    body[index++] = (pps_len >> 8) & 0xff;
    body[index++] = (pps_len) & 0xff;
    memcpy(&body[index], pps, pps_len);
    index +=  pps_len;
    extra_len = index;

    return;
}

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