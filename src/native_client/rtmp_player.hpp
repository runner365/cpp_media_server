#ifndef RTMP_PLAYER_HPP
#define RTMP_PLAYER_HPP
#include "client.hpp"
#include "net/rtmp/rtmp_client_session.hpp"
#include "transcode/decode.hpp"
#include "sdl_player.hpp"
#include <uv.h>
#include <vector>

class stream_filter;

class rtmp_player : public native_clientI, public rtmp_client_callbackI, public decode_callback
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

public:
    virtual void on_avframe_callback(int stream_index, AVFrame* frame) override;

private:
    void on_video_packet_callback(MEDIA_PACKET_PTR pkt_ptr);
    void on_audio_packet_callback(MEDIA_PACKET_PTR pkt_ptr);

private:
    void handle_video_extra_data(uint8_t* extra_data, size_t extra_len);
    void updata_sps(uint8_t* sps, size_t sps_len);
    void updata_pps(uint8_t* pps, size_t pps_len);

private:
    void handle_video_avframe(AVFrame* frame);
    void handle_audio_avframe(AVFrame* frame);

private:
    uv_loop_t* loop_ = nullptr;
    std::string url_;
    rtmp_client_session* session_ = nullptr;
    decode_oper* dec_ = nullptr;

private:
    uint8_t aac_type_ = 0;
    int sample_rate_  = 0;
    uint8_t channel_  = 0;

private:
    std::vector<uint8_t> pps_;
    std::vector<uint8_t> sps_;

private:
    stream_filter* filter_ = nullptr;

private:
    sdl_player* player_ = nullptr;
};


#endif