#ifndef AV_OUTPUT_HPP
#define AV_OUTPUT_HPP
#include "media_packet.hpp"
#include "av_format_interface.hpp"
#include "net/websocket/ws_session_pub.hpp"
#include "flv_demux.hpp"

class av_outputer : public av_format_callback
{
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;
};

class flv_websocket : public ws_session_callback
{
public:
    flv_websocket();
    virtual ~flv_websocket();

public:
    virtual void on_accept();
    virtual void on_read(const char* data, size_t len);
    virtual void on_writen(int len);
    virtual void on_close(int err_code);

private:
    std::string uri_;
    flv_demuxer demuxer_;
    av_outputer outputer_;
};

#endif