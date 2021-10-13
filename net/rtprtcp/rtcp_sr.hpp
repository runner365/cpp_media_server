#ifndef RTCP_SR_HPP
#define RTCP_SR_HPP
#include "logger.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <arpa/inet.h>

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
    rtcp_sr_packet(rtcp_sr_header* header):header_(header) {

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
        if (len != sizeof(rtcp_sr_header)) {
            MS_THROW_ERROR("rtcp sr len(%lu) error", len);
        }
        rtcp_sr_header* header = (rtcp_sr_header*)data;

        return new rtcp_sr_packet(header);
    }

private:
    rtcp_sr_header* header_ = nullptr;
};

#endif