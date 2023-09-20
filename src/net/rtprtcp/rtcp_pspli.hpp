#ifndef RTCP_PS_PLI_HPP
#define RTCP_PS_PLI_HPP
#include "rtprtcp_pub.hpp"
#include "rtcp_fb_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "stringex.hpp"
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

class rtcp_pspli
{
public:
    rtcp_pspli() {
        memset(this->data, 0, sizeof(this->data));
        header_ = (rtcp_fb_common_header*)(this->data);
        fb_header_ = (rtcp_fb_header*)(header_ + 1);

        //init field...
        header_->version     = 2;
        header_->padding     = 0;
        header_->fmt         = (uint8_t)FB_PS_PLI;
        header_->packet_type = (uint8_t)RTCP_PSFB;
        header_->length      = htons(2);
    }

    ~rtcp_pspli() {
    }

    static rtcp_pspli* parse(uint8_t* data, size_t len) {
        if (len != (sizeof(rtcp_fb_common_header) + sizeof(rtcp_fb_header))) {
            return nullptr;
        }
        rtcp_pspli* pkt = new rtcp_pspli();
        memcpy(pkt->data, data, len);
        pkt->header_ = (rtcp_fb_common_header*)(pkt->data);
        pkt->fb_header_ = (rtcp_fb_header*)(pkt->header_ + 1);

        return pkt;
    }

public:
    void set_sender_ssrc(uint32_t sender_ssrc) { fb_header_->sender_ssrc = (uint32_t)htonl(sender_ssrc);}
    void set_media_ssrc(uint32_t media_ssrc) { fb_header_->media_ssrc = (uint32_t)htonl(media_ssrc);}
    uint32_t get_sender_ssrc() { return (uint32_t)ntohl(fb_header_->sender_ssrc); }
    uint32_t get_media_ssrc() { return (uint32_t)ntohl(fb_header_->media_ssrc); }

    uint8_t* get_data() { return this->data; }
    size_t get_data_len() { return sizeof(rtcp_fb_common_header) + sizeof(rtcp_fb_header); }

    std::string dump() {
        std::stringstream ss;
        
        ss << "rtcp ps feedback length:" << this->get_data_len();
        ss << ", sender ssrc:" << this->get_sender_ssrc();
        ss << ", media ssrc:" << this->get_media_ssrc() << "\r\n";
        return ss.str();
    }

private:
    uint8_t data[RTP_PACKET_MAX_SIZE];
    rtcp_fb_common_header* header_;
    rtcp_fb_header* fb_header_;
};

#endif