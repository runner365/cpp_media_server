#ifndef H264_HEADER_HPP
#define H264_HEADER_HPP
#include "byte_stream.hpp"
#include "data_buffer.hpp"
#include <stdint.h>
#include <string>
#include <vector>

static const uint8_t kIdr = 5;
static const uint8_t kSps = 7;
static const uint8_t kPps = 8;
static const uint8_t kAud = 9;

static const uint8_t H264_START_CODE[4] = {0x00, 0x00, 0x00, 0x01};

inline bool H264_IS_KEYFRAME(uint8_t nalu_type) {
    return ((nalu_type & kIdr) == kIdr);
}

inline bool H264_IS_AUD(uint8_t nalu_type) {
    return ((nalu_type & kAud) == kAud);
}

inline bool H264_IS_SEQ(uint8_t nalu_type) {
    return ((nalu_type & kSps) == kSps) || ((nalu_type & kPps) == kPps);
}

inline bool H264_IS_SPS(uint8_t nalu_type) {
    return ((nalu_type & kSps) == kSps);
}

inline bool H264_IS_PPS(uint8_t nalu_type) {
    return ((nalu_type & kPps) == kPps);
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

inline bool annexb_to_nalus(uint8_t* data, size_t len, std::vector<data_buffer>& nalus) {
    if (len < 4) {
        return false;
    }

    int64_t data_len = (int64_t)len;
    uint8_t* p = data;

    while(data_len > 0) {
        uint32_t nalu_len = read_4bytes(p);
        p += 4;
        data_len -= 4;

        data_buffer nalu;
        uint8_t nalu_data[4];
        nalu_data[0] = 0x00;
        nalu_data[1] = 0x00;
        nalu_data[2] = 0x00;
        nalu_data[3] = 0x01;

        nalu.append_data((char*)nalu_data, sizeof(nalu_data));
        nalu.append_data((char*)p, nalu_len);

        nalus.push_back(nalu);
        p += nalu_len;
        data_len -= nalu_len;
    }
    return true;
}

inline void get_sps_pps_from_extradata(unsigned char *pps, int& pps_len, 
                                unsigned char *sps, int& sps_len, 
                                const unsigned char *extra_data, int extra_len)
{
    if (extra_len == 0) {
        return;
    }
    const unsigned char * body= nullptr;
    int iIndex = 0;
    
    body = extra_data;

    if(extra_len >4){
        if(body[3] != 0xff || body[4] != 0xe1) {
            return;
        }
    }
    sps[0] = 1;//?
    sps[1] = body[iIndex++];
    sps[2] = body[iIndex++];
    sps[3] = body[iIndex++];
    iIndex++;//0xff
    
    /*sps*/
    iIndex++;//0xe1
    sps_len = body[iIndex++] << 8;
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

    return;
}


#endif
