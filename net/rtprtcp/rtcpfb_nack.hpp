#ifndef RTCP_FEEDBACK_NACK_HPP
#define RTCP_FEEDBACK_NACK_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <sstream>
#include <stdio.h>
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()
#include <vector>

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
} rtcp_nack_header;

typedef struct rtcp_nack_block_s
{
    uint16_t packet_id;//base seq
    uint16_t lost_bitmap;
} rtcp_nack_block;

class rtcp_fb_nack
{
public:
    rtcp_fb_nack(uint32_t sender_ssrc, uint32_t media_ssrc)
    {
        fb_common_header_ = (rtcp_fb_rtp_header*)(this->data);
        nack_header_ = (rtcp_nack_header*)(fb_common_header_ + 1);
        this->data_len = sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header);

        fb_common_header_->version     = 2;
        fb_common_header_->padding     = 0;
        fb_common_header_->fmt         = (int)FB_RTP_NACK;
        fb_common_header_->packet_type = RTCP_RTPFB;
        fb_common_header_->length      = (uint16_t)htons((uint16_t)this->data_len/4) - 1;

        nack_header_->sender_ssrc = (uint32_t)htonl(sender_ssrc);
        nack_header_->media_ssrc  = (uint32_t)htonl(media_ssrc);
    }

    ~rtcp_fb_nack()
    {

    }

public:
    static rtcp_fb_nack* parse(uint8_t* data, size_t len) {
        if (len <= sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header)) {
            return nullptr;
        }
        
        rtcp_fb_nack* pkt = new rtcp_fb_nack(0, 0);
        (void)pkt->update_data(data, len);
        return pkt;
    }


    void insert_seq_list(const std::vector<uint16_t>& seq_vec) {
        std::vector<uint16_t> report_seqs;

        for (size_t index = 0; index < seq_vec.size(); index++) {
            uint16_t lost_seq = seq_vec[index];
            report_seqs.push_back(lost_seq);

            if ((report_seqs[report_seqs.size() - 1] - report_seqs[0]) > 16) {
                insert_block(report_seqs);
                report_seqs.clear();
            }
        }
        if (!report_seqs.empty()) {
            insert_block(report_seqs);
            report_seqs.clear();
        }
    }

    void insert_block(const std::vector<uint16_t>& report_seqs) {
        uint16_t packet_id = report_seqs[0];
        uint16_t bitmap    = 0;

        for (size_t r = 1; r < report_seqs.size(); r++) {
            uint16_t temp_seq = report_seqs[r];
            bitmap |= 1 << (16 - (temp_seq - packet_id));
        }
        rtcp_nack_block* block = (rtcp_nack_block*)(this->data + this->data_len);
        block->packet_id   = htons(packet_id);
        block->lost_bitmap = htons(bitmap);

        nack_blocks_.push_back(block);

        this->data_len += sizeof(rtcp_nack_block);
        fb_common_header_->length = htons((uint16_t)(this->data_len/4 - 1));
    }

    std::vector<uint16_t> get_lost_seqs() {
        std::vector<uint16_t> seqs;

        for (auto block : nack_blocks_) {
            uint16_t seq_base = ntohs(block->packet_id);
            uint16_t bit_mask = ntohs(block->lost_bitmap);

            seqs.push_back(seq_base);
            for (int i = 0; i < 16; i++) {
                uint8_t enable = (bit_mask >> (15 - i)) & 0x01;
                if (enable) {
                    seqs.push_back(seq_base + i + 1);
                }
            }
        }

        return seqs;
    }

public:
    uint32_t get_sender_ssrc() {return (uint32_t)ntohl(nack_header_->sender_ssrc);}
    uint32_t get_media_ssrc() {return (uint32_t)ntohl(nack_header_->media_ssrc);}
    uint8_t* get_data() {return this->data;}
    size_t get_len() {return this->data_len;}
    size_t get_payload_len() {
        rtcp_fb_rtp_header* header = (rtcp_fb_rtp_header*)(this->data);
        return (size_t)ntohs(header->length);
    }
    uint16_t get_base_seq(rtcp_nack_block* block) {
        return ntohs(block->packet_id);
    }
    uint16_t get_bit_mask(rtcp_nack_block* block) {
        return ntohs(block->lost_bitmap);
    }

    std::string dump() {
        std::stringstream ss;

        ss << "rtcp fb nack: sender ssrc=" << get_sender_ssrc() << ", media ssrc=" << get_media_ssrc() << "\r\n";
        
        for (auto block : nack_blocks_) {
            char bitmap_sz[80];
            snprintf(bitmap_sz, sizeof(bitmap_sz), "0x%04x", ntohs(block->lost_bitmap));
            ss << " base seq:" << ntohs(block->packet_id) << ", " << "bitmap:" << std::string(bitmap_sz) << "\r\n";
        }
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
    bool update_data(uint8_t* data, size_t len) {
        if (len <= sizeof(rtcp_fb_rtp_header) + sizeof(rtcp_nack_header)) {
            return false;
        }
        memcpy(this->data, data, len);
        this->data_len = len;
        fb_common_header_ = (rtcp_fb_rtp_header*)(this->data);
        rtcp_nack_header* nack_header = (rtcp_nack_header*)(fb_common_header_ + 1);
        rtcp_nack_block* block = (rtcp_nack_block*)(nack_header + 1);
        uint8_t* p = (uint8_t*)block;

        while ((size_t)(p - this->data) < this->data_len) {
            nack_blocks_.push_back(block);
            block++;
            p = (uint8_t*)block;
        }
        return true;
    }

private:
    uint8_t data[1500];
    size_t  data_len = 0;
    rtcp_fb_rtp_header* fb_common_header_ = nullptr;
    rtcp_nack_header* nack_header_        = nullptr;
    std::vector<rtcp_nack_block*>  nack_blocks_;
    

};

#endif