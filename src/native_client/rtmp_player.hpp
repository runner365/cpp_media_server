#ifndef RTMP_PLAYER_HPP
#define RTMP_PLAYER_HPP
#include "net/rtmp/rtmp_client_session.hpp"
#include "transcode/decode.hpp"
#include "sdl_player.hpp"
#include "base_player.hpp"
#include <uv.h>
#include <vector>

class stream_filter;

class rtmp_player : public base_player, public rtmp_client_callbackI
{
public:
    rtmp_player(uv_loop_t* loop);
    virtual ~rtmp_player();

public:
    virtual int open_url(const std::string& url) override;
    virtual void run() override;

public:
    virtual void on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) override;
    virtual void on_close(int ret_code) override;

private:
    uv_loop_t* loop_ = nullptr;
    std::string url_;
    rtmp_client_session* session_ = nullptr;
};


#endif