#ifndef RTMP2RTC_HPP
#define RTMP2RTC_HPP
#include "utils/av/media_packet.hpp"
#include <map>
#include <memory>

class room_service;

class rtmp2rtc_writer : public av_writer_base
{
public:
    rtmp2rtc_writer();
    virtual ~rtmp2rtc_writer();

public:
    virtual int write_packet(MEDIA_PACKET_PTR) override;
    virtual std::string get_key() override;
    virtual std::string get_writerid() override;
    virtual void close_writer() override;
    virtual bool is_inited() override;
    virtual void set_init_flag(bool flag) override;
};

#endif
