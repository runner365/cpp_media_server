#ifndef RTCP_XR_RRT_HPP
#define RTCP_XR_RRT_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <string.h>
#include <assert.h>
#ifndef _WIN32
#include <arpa/inet.h>
#else
#include <WinSock2.h>
#endif
#include "rtprtcp_pub.hpp"

/*
  https://datatracker.ietf.org/doc/html/rfc3611#page-21

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|reserved |   PT=XR=207   |             length            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                              SSRC                             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :                         report blocks                         :
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

   rrt report block
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=4      |   reserved    |       block length = 2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              NTP timestamp, most significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             NTP timestamp, least significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

typedef struct xr_rrt_data_s {
    uint32_t ntp_sec;
    uint32_t ntp_frac;
} xr_rrt_data;

inline void init_rrt_header(rtcp_xr_header* rrt_header) {
    rrt_header->bt = XR_RRT;
    rrt_header->reserver = 0;
    rrt_header->block_length = htons(2);
}

class xr_rrt
{
public:
    xr_rrt()
    {
        memset(data, 0, sizeof(data));
        data_len = sizeof(rtcp_common_header) + 4 + sizeof(rtcp_xr_header) + sizeof(xr_rrt_data);

        header = (rtcp_common_header*)data;
        header->version = 2;
        header->padding = 0;
        header->count   = 0;
        header->packet_type = RTCP_XR;
        header->length      = htons(4 + sizeof(rtcp_xr_header) + sizeof(xr_rrt_data)) / 4;

        ssrc_p = (uint32_t*)(header + 1);
        rrt_header = (rtcp_xr_header*)(ssrc_p + 1);
        rrt_block = (xr_rrt_data*)(rrt_header + 1);
        init_rrt_header(rrt_header);
    }
    ~xr_rrt()
    {
    }

public:
    void set_ssrc(uint32_t ssrc) {
        *ssrc_p = htonl(ssrc);
    }

    uint32_t get_ssrc() {
        return ntohl(*ssrc_p);
    }

    void set_ntp(uint32_t ntp_sec, uint32_t ntp_frac) {
        rrt_block->ntp_sec  = htonl(ntp_sec);
        rrt_block->ntp_frac = htonl(ntp_frac);
    }

    void get_ntp(uint32_t& ntp_sec, uint32_t& ntp_frac) {
        ntp_sec  = ntohl(rrt_block->ntp_sec);
        ntp_frac = ntohl(rrt_block->ntp_frac);
    }

    uint8_t* get_data() {
        return data;
    }

    size_t get_data_len() {
        return data_len;
    }

    void parse(uint8_t* rtcp_data, size_t len) {
        assert(len == data_len);
        memcpy(data, rtcp_data, len);
    }

private:
    uint8_t data[RTP_PACKET_MAX_SIZE];
    size_t data_len = 0;
    rtcp_common_header* header = nullptr;
    uint32_t* ssrc_p           = nullptr;
    rtcp_xr_header*   rrt_header    = nullptr;
    xr_rrt_data* rrt_block     = nullptr;
};

#endif
