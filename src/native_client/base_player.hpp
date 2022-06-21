#ifndef BASE_PLAYER_HPP
#define BASE_PLAYER_HPP
#include "transcode/decode.hpp"
#include "transcode/filter.hpp"
#include "transcode/transcode_pub.hpp"
#include "sdl_player.hpp"
#include <vector>

class base_player : public decode_callback
{
public:
    base_player();
    virtual ~base_player();

public:
    virtual int open_url(const std::string& url) = 0;
    virtual void run() = 0;

public://decode_callback
    virtual void on_avframe_callback(int stream_index, AVFrame* frame) override;

protected:
    void on_video_packet_callback(MEDIA_PACKET_PTR pkt_ptr, bool flv_flag = false);
    void on_audio_packet_callback(MEDIA_PACKET_PTR pkt_ptr, bool flv_flag = false);

protected:
    void handle_video_extra_data(uint8_t* extra_data, size_t extra_len);
    void updata_sps(uint8_t* sps, size_t sps_len);
    void updata_pps(uint8_t* pps, size_t pps_len);

protected:
    void handle_video_avframe(AVFrame* frame);
    void handle_audio_avframe(AVFrame* frame);

protected:
    decode_oper* dec_      = nullptr;
    stream_filter* filter_ = nullptr;
    sdl_player* player_    = nullptr;

protected:
    uint8_t aac_type_ = 0;
    int sample_rate_  = 0;
    uint8_t channel_  = 0;

protected:
    std::vector<uint8_t> pps_;
    std::vector<uint8_t> sps_;
};

#endif