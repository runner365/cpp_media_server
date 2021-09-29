#ifndef RTP_RTCP_PUB_HPP
#define RTP_RTCP_PUB_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>

typedef struct rtcp_common_header_s
{
    uint8_t count : 5;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t packet_type : 8;
    uint16_t length : 16;
} rtcp_common_header;

typedef struct rtp_common_header_s
{
    uint8_t csrcCount : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payloadType : 7;
    uint8_t marker : 1;
    uint16_t sequenceNumber;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_common_header;

inline bool is_rtcp(const uint8_t* data, size_t len) {
    rtcp_common_header* header = (rtcp_common_header*)data;

    return ((len >= sizeof(rtcp_common_header)) &&
        // DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
        (data[0] > 127 && data[0] < 192) &&
        (header->version == 2) &&
        // http://www.iana.org/assignments/rtp-parameters/rtp-parameters.xhtml#rtp-parameters-4
        (header->packet_type >= 192 && header->packet_type <= 223));
}

inline bool is_rtp(const uint8_t* data, size_t len) {
    // NOTE: is_rtcp must be called before this method.
    rtp_common_header* header = (rtp_common_header*)data;

    return ((len >= sizeof(rtp_common_header)) &&
        // DOC: https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
        (data[0] > 127 && data[0] < 192) && (header->version == 2));
}
#endif