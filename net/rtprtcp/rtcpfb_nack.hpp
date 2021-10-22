#ifndef RTCP_FEEDBACK_NACK_HPP
#define RTCP_FEEDBACK_NACK_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|  FMT=1  |   PT=205      |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                          sender ssrc                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                           media ssrc                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |     packet identifier         |     bitmap of loss packets    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

typedef struct rtcp_nack_header_s
{
    uint32_t sender_ssrc;
    uint32_t media_ssrc;
    uint16_t packet_id;//base seq
    uint16_t lost_bitmap;
} rtcp_nack_header;

class rtcp_fb_nack
{
public:
    rtcp_fb_nack(uint32_t sender_ssrc, uint32_t media_ssrc)
    {
        fb_common_header_ = (rtcp_fb_rtp_header*)(this->data);
        header_ = (rtcp_nack_header*)(fb_common_header_ + 1);
        this->data_len = sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header);

        fb_common_header_->version     = 2;
        fb_common_header_->padding     = 0;
        fb_common_header_->fmt         = (int)FB_RTP_NACK;
        fb_common_header_->packet_type = RTCP_RTPFB;
        fb_common_header_->length      = (uint32_t)htonl(this->data_len/4) - 1;

        header_->sender_ssrc = sender_ssrc;
        header_->media_ssrc  = media_ssrc;
    }

    ~rtcp_fb_nack()
    {

    }

public:
    static rtcp_fb_nack* parse(uint8_t* data, size_t len) {
        if (len != sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header)) {
            return nullptr;
        }
        
        rtcp_fb_nack* pkt = new rtcp_fb_nack(0, 0);
        (void)pkt->update_data(data, len);
        return pkt;
    }

public:
    uint32_t get_sender_ssrc() {return (uint32_t)ntohl(header_->sender_ssrc);}
    uint32_t get_media_ssrc() {return (uint32_t)ntohl(header_->media_ssrc);}
    uint16_t get_packet_id() {return (uint16_t)ntohs(header_->packet_id);}
    uint16_t get_bitmap() {return (uint16_t)ntohs(header_->lost_bitmap);}

    bool update_data(uint8_t* data, size_t len) {
        if (len != sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header)) {
            return false;
        }
        memcpy(this->data, data, len);
        this->data_len = len;
        fb_common_header_ = (rtcp_fb_rtp_header*)(this->data);
        header_ = (rtcp_nack_header*)(fb_common_header_ + 1);
        return true;
    }

    uint8_t* get_data(uint16_t seq, uint16_t bitmap, size_t& len) {
        header_->packet_id   = (uint16_t)htons(seq);
        header_->lost_bitmap = (uint16_t)htons(bitmap);

        len = this->data_len;
        return this->data;
    }

    std::string dump() {
        std::stringstream ss;
        char bitmap_sz[80];

        snprintf(bitmap_sz, sizeof(bitmap_sz), "0x%04x", get_bitmap());

        ss << "rtcp fb nack: sender ssrc=" << get_sender_ssrc() << ", media ssrc=" << get_media_ssrc()
           << ", base seq:" << get_packet_id() << ", bitmap:" << std::string(bitmap_sz);
        
        char print_data[4*1024];
        size_t print_len = 0;
        const size_t max_print = 256;
        size_t len = this->data_len;
    
        for (size_t index = 0; index < (len > max_print ? max_print : len); index++) {
            if ((index%16) == 0) {
                print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len, "\r\n");
            }
            
            print_len += snprintf(print_data + print_len, sizeof(print_data) - print_len,
                " %02x", this->data[index]);
        }
        ss << std::string(print_data) << "\r\n";

        return ss.str();
    }

private:
    uint8_t data[1500];
    size_t  data_len = 0;
    rtcp_fb_rtp_header* fb_common_header_;
    rtcp_nack_header*   header_;
    

};

#endif