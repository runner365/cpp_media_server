#ifndef AV_OUTPUT_HPP
#define AV_OUTPUT_HPP
#include "media_packet.hpp"
#include "av_format_interface.hpp"
#include "net/websocket/ws_server.hpp"
#include <uv.h>

class transcode;
class av_outputer : public av_format_callback
{
public:
    av_outputer();
    virtual ~av_outputer();

public:
    void release();

public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;

private:
    transcode* trans_ = nullptr;
};

class flv_websocket : public websocket_server_callbackI
{
public:
    flv_websocket(uv_loop_t* loop, uint16_t port);
    flv_websocket(uv_loop_t* loop, uint16_t port,
                const std::string& key_file,
                const std::string& cert_file);
    virtual ~flv_websocket();

public:
    virtual void on_accept(websocket_session* session) override;
    virtual void on_read(websocket_session* session, const char* data, size_t len) override;
    virtual void on_close(websocket_session* session) override;

private:
    websocket_server server_;
};

#endif