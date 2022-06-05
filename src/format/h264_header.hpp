#ifndef H264_HEADER_HPP
#define H264_HEADER_HPP
#include "byte_stream.hpp"
#include "data_buffer.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

enum NaluType : uint8_t
{
    kSlice         = 1,
    kIdr           = 5,
    kSei           = 6,
    kSps           = 7,
    kPps           = 8,
    kAud           = 9,
    kEndOfSequence = 10,
    kEndOfStream   = 11,
    kFiller        = 12,
    kStapA         = 24,
    kFuA           = 28
}; 

enum HEVC_NALU_TYPE
{
        NAL_UNIT_CODED_SLICE_TRAIL_N = 0,
        NAL_UNIT_CODED_SLICE_TRAIL_R, //1
        NAL_UNIT_CODED_SLICE_TSA_N,   //2
        NAL_UNIT_CODED_SLICE_TLA,     //3
        NAL_UNIT_CODED_SLICE_STSA_N,  //4
        NAL_UNIT_CODED_SLICE_STSA_R,  //5
        NAL_UNIT_CODED_SLICE_RADL_N,  //6
        NAL_UNIT_CODED_SLICE_DLP,     //7
        NAL_UNIT_CODED_SLICE_RASL_N,  //8
        NAL_UNIT_CODED_SLICE_TFD,     //9
        NAL_UNIT_RESERVED_10,
        NAL_UNIT_RESERVED_11,
        NAL_UNIT_RESERVED_12,
        NAL_UNIT_RESERVED_13,
        NAL_UNIT_RESERVED_14,
        NAL_UNIT_RESERVED_15,
        NAL_UNIT_CODED_SLICE_BLA,      //16
        NAL_UNIT_CODED_SLICE_BLANT,    //17
        NAL_UNIT_CODED_SLICE_BLA_N_LP, //18
        NAL_UNIT_CODED_SLICE_IDR,      //19
        NAL_UNIT_CODED_SLICE_IDR_N_LP, //20
        NAL_UNIT_CODED_SLICE_CRA,      //21
        NAL_UNIT_RESERVED_22,
        NAL_UNIT_RESERVED_23,
        NAL_UNIT_RESERVED_24,
        NAL_UNIT_RESERVED_25,
        NAL_UNIT_RESERVED_26,
        NAL_UNIT_RESERVED_27,
        NAL_UNIT_RESERVED_28,
        NAL_UNIT_RESERVED_29,
        NAL_UNIT_RESERVED_30,
        NAL_UNIT_RESERVED_31,
        NAL_UNIT_VPS,                   // 32
        NAL_UNIT_SPS,                   // 33
        NAL_UNIT_PPS,                   // 34
        NAL_UNIT_ACCESS_UNIT_DELIMITER, // 35
        NAL_UNIT_EOS,                   // 36
        NAL_UNIT_EOB,                   // 37
        NAL_UNIT_FILLER_DATA,           // 38
        NAL_UNIT_SEI ,                  // 39 Prefix SEI
        NAL_UNIT_SEI_SUFFIX,            // 40 Suffix SEI
        NAL_UNIT_RESERVED_41,
        NAL_UNIT_RESERVED_42,
        NAL_UNIT_RESERVED_43,
        NAL_UNIT_RESERVED_44,
        NAL_UNIT_RESERVED_45,
        NAL_UNIT_RESERVED_46,
        NAL_UNIT_RESERVED_47,
        NAL_UNIT_UNSPECIFIED_48,
        NAL_UNIT_UNSPECIFIED_49,
        NAL_UNIT_UNSPECIFIED_50,
        NAL_UNIT_UNSPECIFIED_51,
        NAL_UNIT_UNSPECIFIED_52,
        NAL_UNIT_UNSPECIFIED_53,
        NAL_UNIT_UNSPECIFIED_54,
        NAL_UNIT_UNSPECIFIED_55,
        NAL_UNIT_UNSPECIFIED_56,
        NAL_UNIT_UNSPECIFIED_57,
        NAL_UNIT_UNSPECIFIED_58,
        NAL_UNIT_UNSPECIFIED_59,
        NAL_UNIT_UNSPECIFIED_60,
        NAL_UNIT_UNSPECIFIED_61,
        NAL_UNIT_UNSPECIFIED_62,
        NAL_UNIT_UNSPECIFIED_63,
        NAL_UNIT_INVALID,
};

#define GET_HEVC_NALU_TYPE(code) (HEVC_NALU_TYPE)((code & 0x7E)>>1)

typedef struct HEVC_NALU_DATA_S {
    std::vector<uint8_t>  nalu_data;
} HEVC_NALU_DATA;

typedef struct HEVC_NALUnit_S {
    uint8_t  array_completeness;
    uint8_t  nal_unit_type;
    uint16_t num_nalus;
    std::vector<HEVC_NALU_DATA> nal_data_vec;
} HEVC_NALUnit;

typedef struct HEVC_DEC_CONF_RECORD_S {
    uint8_t  configuration_version;
    uint8_t  general_profile_space;
    uint8_t  general_tier_flag;
    uint8_t  general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    uint8_t  parallelism_type;
    uint8_t  chroma_format;
    uint8_t  bitdepth_lumaminus8;
    uint8_t  bitdepth_chromaminus8;
    uint16_t avg_framerate;
    uint8_t  constant_frameRate;
    uint8_t  num_temporallayers;
    uint8_t  temporalid_nested;
    uint8_t  lengthsize_minusone;
    std::vector<HEVC_NALUnit> nalu_vec;
} HEVC_DEC_CONF_RECORD;

static const uint8_t H264_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

inline bool H264_IS_KEYFRAME(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kIdr);
}

inline bool H264_IS_AUD(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kAud);
}

inline bool H264_IS_SEQ(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kSps) || ((nalu_type & 0x1f) == kPps);
}

inline bool H264_IS_SPS(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kSps);
}

inline bool H264_IS_PPS(uint8_t nalu_type) {
    return ((nalu_type & 0x1f) == kPps);
}

inline bool Is_Nalu_Header(uint8_t* data, size_t len) {
    if (len < 4) {
        return false;
    }
    if (data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x00 && data[3] == 0x01) {
        return true;
    }
    if (data[0] == 0x00 && data[1] == 0x00 &&
        data[2] == 0x01) {
        return true;
    }
    return false;
}

inline bool annexb_to_nalus(uint8_t* data, size_t len, std::vector<std::shared_ptr<data_buffer>>& nalus) {
    const uint32_t kMaxLen = 10*1000*1000;

    if (len < 4) {
        return false;
    }

    int64_t data_len = (int64_t)len;
    uint8_t* p = data;

    while(data_len > 0) {
        uint32_t nalu_len = read_4bytes(p);
        if (nalu_len > kMaxLen) {
            log_errorf("nalu len is too large, %u", nalu_len);
            return false;
        }
        p += 4;
        data_len -= 4;

        std::shared_ptr<data_buffer> nalu_ptr = std::make_shared<data_buffer>(4 + nalu_len + 1024);
        uint8_t nalu_data[4];
        nalu_data[0] = 0x00;
        nalu_data[1] = 0x00;
        nalu_data[2] = 0x00;
        nalu_data[3] = 0x01;

        nalu_ptr->append_data((char*)nalu_data, sizeof(nalu_data));
        nalu_ptr->append_data((char*)p, nalu_len);

        nalus.push_back(nalu_ptr);
        p += nalu_len;
        data_len -= nalu_len;
    }
    return true;
}

inline int get_sps_pps_from_extradata(uint8_t *pps, size_t& pps_len, 
                                uint8_t *sps, size_t& sps_len, 
                                const uint8_t *extra_data, size_t extra_len)
{
    if (extra_len == 0) {
        return -1;
    }
    const unsigned char * body= nullptr;
    int iIndex = 0;
    
    body = extra_data;

    if(extra_len >4){
        if(body[4] != 0xff || body[5] != 0xe1) {
            return -1;
        }
    }

    iIndex += 4;//0xff
    
    /*sps*/
    iIndex += 1;//0xe1
    iIndex += 1;//sps len start
    sps_len = (size_t)body[iIndex++] << 8;
    sps_len += body[iIndex++];
    memcpy(sps, &body[iIndex], sps_len);
    iIndex +=  sps_len;

    /*pps*/
    iIndex++;//0x01
    pps_len = body[iIndex++] << 8;
    pps_len += body[iIndex++];
    memcpy(pps, &body[iIndex], pps_len);
    iIndex +=  pps_len;
    extra_len = iIndex;

    return 0;
}

inline int get_vps_sps_pps_from_hevc_dec_info(HEVC_DEC_CONF_RECORD* hevc_dec_info,
                                uint8_t* vps, size_t& vps_len,
                                uint8_t* sps, size_t& sps_len,
                                uint8_t* pps, size_t& pps_len)
{
    vps_len = 0;
    sps_len = 0;
    pps_len = 0;

    for(size_t i = 0; i < hevc_dec_info->nalu_vec.size(); i++) {
        HEVC_NALUnit nalu_unit = hevc_dec_info->nalu_vec[i];
        if (nalu_unit.nal_unit_type == NAL_UNIT_VPS) {
            vps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(vps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), vps_len);
        }
        if (nalu_unit.nal_unit_type == NAL_UNIT_SPS) {
            sps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(sps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), sps_len);
        }
        if (nalu_unit.nal_unit_type == NAL_UNIT_PPS) {
            pps_len = nalu_unit.nal_data_vec[0].nalu_data.size();
            memcpy(pps, (uint8_t*)(&nalu_unit.nal_data_vec[0].nalu_data[0]), pps_len);
        }
    }

    if ((vps_len == 0) || (sps_len == 0) || (pps_len == 0)) {
        log_errorf("fail to get vps/sps/pps from extra data");
        return -1;
    }
    return 0;
}

inline int get_hevc_dec_info_from_extradata(HEVC_DEC_CONF_RECORD* hevc_dec_info, 
                                const uint8_t *extra_data, size_t extra_len)
{
    const uint8_t* p = extra_data;
    const uint8_t* end = extra_data + extra_len;

    hevc_dec_info->configuration_version = *p;
    if (hevc_dec_info->configuration_version != 1) {
        log_errorf("configuration_version(%d) is not 1",
                hevc_dec_info->configuration_version);
        return -1;
    }
    p++;

    //general_profile_space(2bits), general_tier_flag(1bit), general_profile_idc(5bits)
    hevc_dec_info->general_profile_space = (*p >> 6) & 0x03;
    hevc_dec_info->general_tier_flag = (*p >> 5) & 0x01;
    hevc_dec_info->general_profile_idc = *p & 0x1F;
    p++;

    //general_profile_compatibility_flags: 32bits
    hevc_dec_info->general_profile_compatibility_flags = read_4bytes(p);
    p += 4;

    //general_constraint_indicator_flags: 48bits
    int64_t general_constraint_indicator_flags = read_4bytes(p);
    p += 4;
    general_constraint_indicator_flags = (general_constraint_indicator_flags << 16) | (read_2bytes(p));
    p += 2;
    hevc_dec_info->general_constraint_indicator_flags = general_constraint_indicator_flags;

    //general_level_idc: 8bits
    hevc_dec_info->general_level_idc = *p;
    p++;

    //min_spatial_segmentation_idc: xxxx 14bits
    hevc_dec_info->min_spatial_segmentation_idc = read_2bytes(p) & 0x0fff;
    p += 2;

    //parallelismType: xxxx xx 2bits
    hevc_dec_info->parallelism_type = *p & 0x03;
    p++;

    //chromaFormat: xxxx xx 2bits
    hevc_dec_info->chroma_format = *p & 0x03;
    p++;

    //bitDepthLumaMinus8: xxxx x 3bits
    hevc_dec_info->bitdepth_lumaminus8 = *p & 0x07;
    p++;

    //bitDepthChromaMinus8: xxxx x 3bits
    hevc_dec_info->bitdepth_chromaminus8 = *p & 0x07;
    p++;

    //avgFrameRate: 16bits
    hevc_dec_info->avg_framerate = read_2bytes(p);
    p += 2;

    //8bits: constantFrameRate(2bits), numTemporalLayers(3bits), 
    //       temporalIdNested(1bit), lengthSizeMinusOne(2bits)
    hevc_dec_info->constant_frameRate  = (*p >> 6) & 0x03;
    hevc_dec_info->num_temporallayers  = (*p >> 3) & 0x07;
    hevc_dec_info->temporalid_nested   = (*p >> 2) & 0x01;
    hevc_dec_info->lengthsize_minusone = *p & 0x03;
    p++;

    uint8_t arrays_num = *p;
    p++;

    //parse vps/pps/sps
    for (int index = 0; index < arrays_num; index++) {
        HEVC_NALUnit hevc_unit;

        if ((p + 5) > end) {
            log_infof("hevc p+5=%p, end=%p", p + 5, end);
            return -1;
        }
        hevc_unit.array_completeness = (*p >> 7) & 0x01;
        hevc_unit.nal_unit_type = *p & 0x3f;
        p++;
        hevc_unit.num_nalus = read_2bytes(p);
        p += 2;

        log_infof("nalu type:0x%02x", hevc_unit.nal_unit_type);
        for (int i = 0; i < hevc_unit.num_nalus; i++) {
            HEVC_NALU_DATA data_item;
            uint16_t nalUnitLength = read_2bytes(p);
            p += 2;

            log_infof("p:%p, nalUnitLength:%d", p, nalUnitLength);

            if ((p + nalUnitLength) > end) {
                log_infof("hevc p+nalUnitLength=%p, end=%p", p + nalUnitLength, end);
                return -1;
            }
            //copy vps/pps/sps data
            data_item.nalu_data.resize(nalUnitLength);
            memcpy((uint8_t*)(&data_item.nalu_data[0]), p, nalUnitLength);
            p += nalUnitLength;

            hevc_unit.nal_data_vec.push_back(data_item);
        }
        hevc_dec_info->nalu_vec.push_back(hevc_unit);
    }
    return 0;
}

#endif
