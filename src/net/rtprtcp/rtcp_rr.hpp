#ifndef RTCP_RR_HPP
#define RTCP_RR_HPP
#include "rtprtcp_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "stringex.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <sstream>

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=RR=201   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                 SSRC_1 (reporter ssrc)                        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
report |                 SSRC_2 (reportee ssrc)                        |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  1    | fraction lost |       cumulative number of packets lost       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           extended highest sequence number received           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      interarrival jitter                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         last SR (LSR)                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   delay since last SR (DLSR)                  |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

typedef struct rtcp_rr_header_s {
    uint32_t reporter_ssrc;
    uint32_t reportee_ssrc;
    uint32_t loss_fraction : 8;
    uint32_t cumulative_lost : 24;
    uint32_t highest_seq;
    uint32_t jitter;
    uint32_t lsr;//Last sender report timestamp
    uint32_t dlsr;//delay since last sender report
} rtcp_rr_header;

class rtcp_rr_packet
{
public:
    rtcp_rr_packet() {
        memset(this->data, 0, sizeof(this->data));
        this->rtcp_header_ = (rtcp_common_header*)(this->data);
        this->header_      = (rtcp_rr_header*)(this->rtcp_header_ + 1);
    }

    rtcp_rr_packet(rtcp_common_header* rtcp_header) {
        memcpy(this->data, (uint8_t*)rtcp_header, sizeof(rtcp_common_header) + sizeof(rtcp_rr_header));
        this->rtcp_header_ = (rtcp_common_header*)this->data;
        this->header_      = (rtcp_rr_header*)(this->rtcp_header_ + 1);
    }

    ~rtcp_rr_packet() {

    }

public:
    static rtcp_rr_packet* parse(uint8_t* data, size_t len) {
        if (len != (sizeof(rtcp_common_header) + sizeof(rtcp_rr_header))) {
            MS_THROW_ERROR("rtcp sr len(%lu) error", len);
        }
        rtcp_common_header* rtcp_header = (rtcp_common_header*)data;

        return new rtcp_rr_packet(rtcp_header);
    }

public:
    uint32_t get_reporter_ssrc() {
        return ntohl(header_->reporter_ssrc);
    }

    void set_reporter_ssrc(uint32_t ssrc) {
        header_->reporter_ssrc = (uint32_t)htonl(ssrc);
    }

    uint32_t get_reportee_ssrc() {
        return ntohl(header_->reportee_ssrc);
    }

    void set_reportee_ssrc(uint32_t ssrc) {
        header_->reportee_ssrc = (uint32_t)htonl(ssrc);
    }

    uint32_t get_highest_seq() {
        return ntohl(header_->highest_seq);
    }

    void set_highest_seq(uint32_t highest_seq) {
        header_->highest_seq = (uint32_t)htonl(highest_seq);
    }

    uint8_t get_fraclost() {
        uint8_t* p = (uint8_t*)header_;
        p += 4 + 4;
        return *p;
    }

    void set_fraclost(uint8_t fraclost) {
        uint8_t* p = (uint8_t*)header_;
        p += 4 + 4;
        *p = fraclost;
    }

    uint32_t get_cumulative_lost() {
        uint8_t* p = (uint8_t*)header_;
        p += 4 + 4 + 1;
        return read_3bytes(p);
    }

    void set_cumulative_lost(uint32_t cumulative_lost) {
        uint8_t* p = (uint8_t*)header_;
        p += 4 + 4 + 1;

        write_3bytes(p, cumulative_lost);
    }

    uint32_t get_jitter() {
        return (uint32_t)ntohl(header_->jitter);
    }

    void set_jitter(uint32_t jitter) {
        header_->jitter = (uint32_t)htonl(jitter);
    }

    uint32_t get_lsr() {
        return (uint32_t)ntohl(header_->lsr);
    }

    void set_lsr(uint32_t lsr) {
        header_->lsr = (uint32_t)htonl(lsr);
    }

    uint32_t get_dlsr() {
        return (uint32_t)ntohl(header_->dlsr);
    }

    void set_dlsr(uint32_t dlsr) {
        header_->dlsr = (uint32_t)htonl(dlsr);
    }

    uint8_t* get_data(size_t& data_len) {
        data_len = sizeof(rtcp_common_header) + sizeof(rtcp_rr_header);
        rtcp_common_header* rtcp_header = (rtcp_common_header*)this->data;
        rtcp_header->version     = 2;
        rtcp_header->padding     = 0;
        rtcp_header->count       = 0;
        rtcp_header->packet_type = RTCP_RR;
        rtcp_header->length      = (uint32_t)htons(data_len/4) - 1;
        
        return (uint8_t*)this->data;
    }

    std::string dump() {
        std::stringstream ss;

        ss << "rtcp receive reporter ssrc:" << this->get_reporter_ssrc()
           << ", reportee ssrc:" << this->get_reportee_ssrc()
           << ", frac lost:" << (int)this->get_fraclost()
           << "(" << (float)this->get_fraclost()/256.0 << ")"
           << ", cumulative lost:" << (int)this->get_cumulative_lost() << ", highest sequence:" << this->get_highest_seq()
           << ", jitter:" << this->get_jitter() << ", lsr:" << this->get_lsr() << ", dlsr:" << this->get_dlsr() << "\r\n";

        std::string data_str = data_to_string(this->data, sizeof(rtcp_common_header) + sizeof(rtcp_rr_header));
        ss << "data bytes:" << "\r\n";
        ss << data_str << "\r\n";

        return ss.str();
    }

private:
    rtcp_common_header* rtcp_header_ = nullptr;
    rtcp_rr_header* header_          = nullptr;
    uint8_t data[1500];
};
#endif