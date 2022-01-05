#ifndef HLS_WRITER_HPP
#define HLS_WRITER_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>

#include "media_packet.hpp"


class hls_writer : public av_writer_base
{
public:
    hls_writer();
    virtual ~hls_writer();

public:
    virtual int write_packet(MEDIA_PACKET_PTR) override;
    virtual std::string get_key() override;
    virtual std::string get_writerid() override;
    virtual void close_writer() override;
    virtual bool is_inited() override;
    virtual void set_init_flag(bool flag) override;

};

#endif