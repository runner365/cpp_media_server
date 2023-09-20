#ifndef RTCP_FB_PUB_HPP
#define RTCP_FB_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>

#include <stdio.h>
#ifndef _WIN32
#include <arpa/inet.h>  // htonl(), htons(), ntohl(), ntohs()
#else
#include <WinSock2.h>
#endif

/*
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|   FMT   |       PT      |          length               |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of packet sender                        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                  SSRC of media source                         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   :            Feedback Control Information (FCI)                 :
   :                                                               :
*/

typedef struct rtcp_fb_header_s
{
    uint32_t sender_ssrc;
    uint32_t media_ssrc;
} rtcp_fb_header;

#endif