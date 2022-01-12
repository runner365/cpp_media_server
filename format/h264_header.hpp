#ifndef H264_HEADER_HPP
#define H264_HEADER_HPP
#include "byte_stream.hpp"
#include "data_buffer.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <string>
#include <vector>
#include <memory>

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

        std::shared_ptr<data_buffer> nalu_ptr = std::make_shared<data_buffer>();
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


#endif
