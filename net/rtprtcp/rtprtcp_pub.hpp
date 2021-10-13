#ifndef RTP_RTCP_PUB_HPP
#define RTP_RTCP_PUB_HPP
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()

#define RTP_PACKET_MAX_SIZE 1500

typedef enum
{
    RTCP_SR    = 200,
    RTCP_RR    = 201,
    RTCP_SDES  = 202,
    RTCP_BYE   = 203,
    RTCP_APP   = 204,
    RTCP_RTPFB = 205,
    RTCP_PSFB  = 206,
    RTCP_XR    = 207
} RTCP_TYPE;

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
    uint8_t csrc_count : 4;
    uint8_t extension : 1;
    uint8_t padding : 1;
    uint8_t version : 2;
    uint8_t payload_type : 7;
    uint8_t marker : 1;
    uint16_t sequence;
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

inline uint16_t get_rtcp_length(rtcp_common_header* header) {
    return ntohs(header->length) * 4;
}

inline std::string rtcp_header_dump(rtcp_common_header* header) {
    std::stringstream ss;

    ss << "rtcp common header, version:" << (int)header->version << ", padding:" << (int)header->padding << ", count:" << (int)header->count;
    ss << ", payloadtype:" << (int)header->packet_type << ", length:" << get_rtcp_length(header) << "\r\n";

    return ss.str();
}

#endif