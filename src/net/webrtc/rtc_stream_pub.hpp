#ifndef RTC_STREAM_PUB_HPP
#define RTC_STREAM_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <cstring>

#define RTT_DEFAULT 30 //ms
#define RETRANSMIT_MAX_COUNT 20

class rtc_stream_callback
{
public:
    virtual void stream_send_rtp(uint8_t* data, size_t len) = 0;
    virtual void stream_send_rtcp(uint8_t* data, size_t len) = 0;
};


#endif