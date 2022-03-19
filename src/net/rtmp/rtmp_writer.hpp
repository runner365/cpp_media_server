#ifndef RTMP_WRITER_HPP
#define RTMP_WRITER_HPP
#include "rtmp_pub.hpp"
#include "media_packet.hpp"
#include <memory>

class rtmp_server_session;
class rtmp_writer : public av_writer_base
{
public:
    rtmp_writer(rtmp_server_session* session);
    virtual ~rtmp_writer();

public:
    virtual int write_packet(MEDIA_PACKET_PTR) override;
    virtual std::string get_key() override;
    virtual std::string get_writerid() override;
    virtual void close_writer() override;
    virtual bool is_inited() override;
    virtual void set_init_flag(bool flag) override;

private:
    rtmp_server_session* session_;
    bool init_flag_ = false;
};

typedef std::shared_ptr<rtmp_writer> RTMP_WRITER_PTR;

#endif