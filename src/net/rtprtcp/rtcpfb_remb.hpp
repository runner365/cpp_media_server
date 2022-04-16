#ifndef RTCP_FB_REMB_HPP
#define RTCP_FB_REMB_HPP
#include "rtprtcp_pub.hpp"
#include "rtcp_fb_pub.hpp"
#include "utils/byte_stream.hpp"
#include "utils/logger.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>

#define REMB_ASCII 0x52454D42 //REMB

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P| FMT=15  |   PT=206      |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of packet sender                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of media source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Unique identifier 'R' 'E' 'M' 'B'                            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |   SSRC feedback                                               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |  ...                                                          |
*/

typedef struct rtcpfb_remb_block_s
{
    uint8_t unique_identifier[4];
    uint8_t ssrc_num;
    uint8_t br_exp;
    uint16_t br_mantissa;
} rtcpfb_remb_block;

class rtcpfb_remb
{
public:
    rtcpfb_remb(uint32_t sender_ssrc, uint32_t media_ssrc): sender_ssrc_(sender_ssrc)
                                                        , media_ssrc_(media_ssrc)
    {
        memset(data_, 0, sizeof(data_));

        header_ = (rtcp_fb_common_header*)data_;
        header_->version     = 2;
        header_->padding     = 0;
        header_->fmt         = FB_PS_AFB;
        header_->packet_type = RTCP_PSFB;
        header_->length      = 0;//update in serial.

        fb_header_ = (rtcp_fb_header*)(header_ + 1);
        fb_header_->sender_ssrc = htonl(sender_ssrc_);
        fb_header_->media_ssrc  = htonl(media_ssrc_);

        remb_block_ = (rtcpfb_remb_block*)(fb_header_ + 1);
        remb_block_->unique_identifier[0] = 'R';
        remb_block_->unique_identifier[1] = 'E';
        remb_block_->unique_identifier[2] = 'M';
        remb_block_->unique_identifier[3] = 'B';
    }

    ~rtcpfb_remb() {

    }

    static rtcpfb_remb* parse(uint8_t* data, size_t len) {
        rtcpfb_remb* remb = new rtcpfb_remb(0, 0);

        memcpy(remb->get_data(), data, len);

        remb->sender_ssrc_ = ntohl(remb->fb_header_->sender_ssrc);
        remb->media_ssrc_  = ntohl(remb->fb_header_->media_ssrc);

        uint8_t exponenta = remb->remb_block_->br_exp >> 2;
        uint64_t mantissa = (static_cast<uint32_t>(remb->remb_block_->br_exp & 0x03) << 16) | ntohs(remb->remb_block_->br_mantissa);
        remb->bitrate_ = (mantissa << exponenta);
        bool shift_overflow = (static_cast<uint64_t>(remb->bitrate_) >> exponenta) != mantissa;
        if (shift_overflow) {
          return nullptr;
        }

        uint8_t ssrc_num = remb->remb_block_->ssrc_num;
        uint8_t* p = (uint8_t*)(&(remb->remb_block_->ssrc_num));

        //log_infof("ssrc num:%d, br_exp:%d, br_mantissa:%d",
        //    ssrc_num, remb->remb_block_->br_exp, ntohs(remb->remb_block_->br_mantissa));
        p += 4;
        uint32_t* ssrc_p = (uint32_t*)p;
        for (uint8_t index = 0; index < ssrc_num; index++) {
            //log_infof("ssrc:%u", ntohl(*ssrc_p));
            remb->ssrcs_.push_back(ntohl(*ssrc_p));
            ssrc_p++;
        }
        return remb;
    }

    uint8_t* serial(size_t& data_len) {
        data_len = get_data_len();
        header_->length = htons((uint16_t)(data_len/4 - 1));

        remb_block_->ssrc_num = (uint8_t)ssrcs_.size();
        
        int64_t mantissa = bitrate_;
        uint8_t exponent = 0;

        while (mantissa > 0x3FFFF /* max mantissa (18 bits) */) {
            mantissa >>= 1;
            ++exponent;
        }

        remb_block_->br_exp = (exponent << 2) | (mantissa >> 16);
        remb_block_->br_mantissa = htons((uint16_t)(mantissa & 0xffff));

        uint8_t* p = (uint8_t*)(remb_block_ + 1);
        for (auto ssrc : ssrcs_) {
            write_4bytes(p, ssrc);
            p += sizeof(uint32_t);
        }
        return data_;
    }

    void set_ssrcs(const std::vector<uint32_t>& ssrcs) {
        ssrcs_ = ssrcs;
    }

    std::vector<uint32_t> get_ssrcs() {
        return ssrcs_;
    }

    int64_t get_bitrate() {
        return bitrate_;
    }

    void set_bitrate(int64_t bitrate) {
        bitrate_ = bitrate;
    }

    uint8_t* get_data() {
        return data_;
    }

    size_t get_data_len() {
        size_t data_len = sizeof(rtcp_fb_common_header) + sizeof(rtcp_fb_header) + sizeof(rtcpfb_remb_block);

        data_len += sizeof(uint32_t) * ssrcs_.size();

        return data_len;
    }

private:
    uint8_t data_[RTP_PACKET_MAX_SIZE];

private:
    uint32_t sender_ssrc_ = 0;
    uint32_t media_ssrc_  = 0;
    std::vector<uint32_t> ssrcs_;
    int64_t bitrate_ = 0;

private:
    rtcp_fb_common_header* header_  = nullptr;
    rtcp_fb_header* fb_header_      = nullptr;
    rtcpfb_remb_block* remb_block_  = nullptr;
};

#endif
