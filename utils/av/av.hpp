#ifndef AV_DEF_HPP
#define AV_DEF_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>

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
    MEDIA_FORMAT_FLV,
    MEDIA_FORMAT_MPEGTS,
} MEDIA_FORMAT_TYPE;


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

inline std::string avtype_tostring(MEDIA_PKT_TYPE type) {
    switch(type)
    {
        case MEDIA_VIDEO_TYPE:
            return "video";
        case MEDIA_AUDIO_TYPE:
            return "audio";
        case MEDIA_METADATA_TYPE:
            return "metadata";
        default:
            return "unkown";
    }
}

inline std::string codectype_tostring(MEDIA_CODEC_TYPE type) {
    switch(type)
    {
        case MEDIA_CODEC_H264:
            return "h264";
        case MEDIA_CODEC_H265:
            return "h265";
        case MEDIA_CODEC_VP8:
            return "vp8";
        case MEDIA_CODEC_VP9:
            return "vp9";
        case MEDIA_CODEC_AAC:
            return "aac";
        case MEDIA_CODEC_OPUS:
            return "opus";
        case MEDIA_CODEC_MP3:
            return "mp3";
        default:
            return "unkown";
    }
}

inline std::string formattype_tostring(MEDIA_FORMAT_TYPE type) {
    switch (type)
    {
        case MEDIA_FORMAT_RAW:
            return "raw";
        case MEDIA_FORMAT_FLV:
            return "flv";
        case MEDIA_FORMAT_MPEGTS:
            return "mpegts";
        default:
            return "unkown";
    }
}

#endif