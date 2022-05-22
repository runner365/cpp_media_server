#ifndef ENCODEC_HPP
#define ENCODEC_HPP
#include "transcode_pub.hpp"
#include "filter.hpp"
#include "utils/av/av.hpp"

class encode_base
{
public:
    encode_base(encode_callback* cb);
    virtual ~encode_base();

public:
    virtual int send_frame(AVFrame* frame) = 0;

protected:
    stream_filter filter_;
    encode_callback* cb_ = nullptr;

protected:
    bool init_  = false;
    std::string codec_desc_;
    AVCodec* codec_ = nullptr;
    AVCodecContext* codec_ctx_ = nullptr;
    
protected:
    MEDIA_CODEC_TYPE audio_codec_type_ = MEDIA_CODEC_UNKOWN;
    MEDIA_CODEC_TYPE video_codec_type_ = MEDIA_CODEC_UNKOWN;
    uint8_t* extra_data_ = nullptr;
    size_t extra_data_size_ = 0;
};

class audio_encode : public encode_base
{
public:
    audio_encode(encode_callback* cb);
    virtual ~audio_encode();

public:
    int init_audio(const std::string& fmt, int samplerate_i, int channel, int bitrate);
    void release_audio();
    AVFrame* get_new_audio_frame(uint8_t*& frame_buf);
    int add_samples_to_fifo(uint8_t **data, const int frame_size);

public:
    virtual int send_frame(AVFrame* frame) override;

private:
    int init_fifo();
    void release_fifo();
    void reset_opus_timestamp(AVPacket* pkt);

private:
    std::string _audio_fmt;
    int _samplerate;
    int _channel;
    int _audio_bitrate;
    int64_t last_audio_pts_ = -1;
    int64_t last_pkt_dts_   = -1;
    AVAudioFifo* _fifo_p = nullptr;
};

class video_encode : public encode_base
{
public:
    video_encode(encode_callback* cb);
    virtual ~video_encode();

public:
    int init_video(const std::string& vcodec, int width, int height, const std::string& profile, const std::string& preset, int bitrate, int framerate);
    void release_video();

public:
    virtual int send_frame(AVFrame* frame) override;

private:
    int width_  = 0;
    int height_ = 0;
    std::string profile_;
    std::string preset_;
    int bitrate_   = 0;
    int framerate_ = 0;

private:
    stream_filter filter_;
};

#endif
