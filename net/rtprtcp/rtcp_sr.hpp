#ifndef RTCP_SR_HPP
#define RTCP_SR_HPP
#include "logger.hpp"
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>

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

    ~rtcp_sr_packet() {

    }

    uint32_t get_ssrc() {
        return ntohl(header_->ssrc);
    }

    uint32_t get_ntp_sec() {
        return ntohl(header_->ntp_sec);
    }

    uint32_t get_ntp_frac() {
        return ntohl(header_->ntp_frac);
    }

    uint32_t get_rtp_timestamp() {
        return ntohl(header_->rtp_timestamp);
    }

    uint32_t get_pkt_count() {
        return ntohl(header_->pkt_count);
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

private:
    rtcp_common_header* rtcp_header_ = nullptr;
    rtcp_sr_header* header_          = nullptr;
    uint8_t data[1500];
};

#endif