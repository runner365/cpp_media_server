#ifndef RTCP_SR_HPP
#define RTCP_SR_HPP
#include "logger.hpp"
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <WinSock2.h>
#endif
#include <sstream>

/*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=SR=200   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         SSRC of sender                        |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
sender |              NTP timestamp, most significant word             |
info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             NTP timestamp, least significant word             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         RTP timestamp                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     sender's packet count                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      sender's octet count                     |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/

typedef struct rtcp_sr_header_s {
    uint32_t ssrc;
    uint32_t ntp_sec;
    uint32_t ntp_frac;
    uint32_t rtp_timestamp;
    uint32_t pkt_count;
    uint32_t bytes_count;
} rtcp_sr_header;

class rtcp_sr_packet
{
public:
    rtcp_sr_packet(rtcp_common_header* rtcp_header) {
        memcpy(this->data, (uint8_t*)rtcp_header, sizeof(rtcp_common_header) + sizeof(rtcp_sr_header));
        this->rtcp_header_ = (rtcp_common_header*)(this->data);
        this->header_      = (rtcp_sr_header*)(this->data + sizeof(rtcp_common_header));
    }

    rtcp_sr_packet() {
        memset(this->data, 0, sizeof(this->data));
        rtcp_header_ = (rtcp_common_header*)(this->data);
        rtcp_header_->version     = 2;
        rtcp_header_->padding     = 0;
        rtcp_header_->count       = 0;
        rtcp_header_->packet_type = (uint8_t)RTCP_SR;
        rtcp_header_->length      = (uint32_t)htons(sizeof(rtcp_sr_header)/sizeof(uint32_t));
        this->header_             = (rtcp_sr_header*)(this->data + sizeof(rtcp_common_header));
    }

    ~rtcp_sr_packet() {

    }

    void set_ssrc(uint32_t ssrc) {
        header_->ssrc = (uint32_t)htonl(ssrc);
    }

    uint32_t get_ssrc() {
        return ntohl(header_->ssrc);
    }

    void set_ntp(uint32_t ntp_sec, uint32_t ntp_frac) {
        header_->ntp_sec  = (uint32_t)htonl(ntp_sec);
        header_->ntp_frac = (uint32_t)htonl(ntp_frac);
    }

    uint32_t get_ntp_sec() {
        return ntohl(header_->ntp_sec);
    }

    uint32_t get_ntp_frac() {
        return ntohl(header_->ntp_frac);
    }

    void set_rtp_timestamp(uint32_t ts) {
        header_->rtp_timestamp = (uint32_t)htonl(ts);
    }

    uint32_t get_rtp_timestamp() {
        return ntohl(header_->rtp_timestamp);
    }

    void set_pkt_count(uint32_t pkt_count) {
        header_->pkt_count = (uint32_t)htonl(pkt_count);
    }

    uint32_t get_pkt_count() {
        return ntohl(header_->pkt_count);
    }

    void set_bytes_count(uint32_t bytes_count) {
        header_->bytes_count = (uint32_t)htonl(bytes_count);
    }

    uint32_t get_bytes_count() {
        return ntohl(header_->bytes_count);
    }

public:
    static rtcp_sr_packet* parse(uint8_t* data, size_t len) {
        if (len != (sizeof(rtcp_common_header) + sizeof(rtcp_sr_header))) {
            MS_THROW_ERROR("rtcp sr len(%lu) error", len);
        }
        rtcp_common_header* rtcp_header = (rtcp_common_header*)data;

        return new rtcp_sr_packet(rtcp_header);
    }

    uint8_t* serial(size_t& ret_len) {
        uint8_t* ret_data = this->data;
        ret_len = sizeof(rtcp_common_header) + sizeof(rtcp_sr_header);

        return ret_data;
    }

    std::string dump() {
        std::stringstream ss;

        ss << "rtcp version:" << (int)rtcp_header_->version << ", pad:" << (int)rtcp_header_->padding
           << ", rc:" << (int)rtcp_header_->count << ", rtcp type:" << (int)rtcp_header_->packet_type
           << ", length:" << (int)rtcp_header_->length << "\r\n";
        ss << "  ssrc:" << header_->ssrc << ", ntp sec:" << header_->ntp_sec << ", ntp frac:"
           << header_->ntp_frac << ", rtp timestamp:" << header_->rtp_timestamp
           << ", packet count:" << header_->pkt_count << ", bytes:"
           << header_->bytes_count << "\r\n";

        return ss.str();
    }

    uint8_t* get_data() { return this->data; }
    size_t get_data_len() { return sizeof(rtcp_common_header) + sizeof(rtcp_sr_header); }

private:
    rtcp_common_header* rtcp_header_ = nullptr;
    rtcp_sr_header* header_          = nullptr;
    uint8_t data[1500];
};

#endif