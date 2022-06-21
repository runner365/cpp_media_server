#ifndef HTTPFLV_PLAYER_HPP
#define HTTPFLV_PLAYER_HPP
#include "net/http/http_client.hpp"
#include "format/flv/flv_demux.hpp"
#include "base_player.hpp"
#include <uv.h>
#include <vector>
#include <memory>

class httpflv_player : public base_player, public http_client_callbackI, public av_format_callback
{
public:
    httpflv_player(uv_loop_t* loop);
    virtual ~httpflv_player();

private:
    virtual int open_url(const std::string& url) override;
    virtual void run() override;

private:
    virtual void on_http_read(int ret, std::shared_ptr<http_client_response> resp_ptr) override;

private:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;

private:
    uv_loop_t* loop_ = nullptr;
    std::unique_ptr<http_client> hc_ptr_;

private:
    flv_demuxer flvdemux_;

private:
    std::string key_;
};

#endif