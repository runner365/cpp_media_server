#ifndef FILTER_HPP
#define FILTER_HPP
#include "transcode_pub.hpp"

typedef struct FilteringContext {
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;
} FilteringContext;

class stream_filter {
public:
    stream_filter();
    ~stream_filter();

public: //init video filter
    int init_video_filter(enum AVPixelFormat pix_fmt, int width, int height,
        AVRational sample_aspect_ratio, AVRational _filter_tb,
        AVCodecContext* enc_codec_ctx_p, const char* filter_spec_sz);

public: //init audio filter
    int init_audio_filter(enum AVSampleFormat sample_fmt, int sample_rate, 
        int channels, uint64_t channel_layout,
        AVCodecContext* enc_codec_ctx_p, const char* filter_spec_sz);
    int init_audio_filter(enum AVSampleFormat sample_fmt, int sample_rate, 
                        int channels, uint64_t channel_layout,
                        enum AVSampleFormat target_sample_fmt, int target_sample_rate,
                        int target_channels, uint64_t target_channel_layout,
                        int target_frame_size, const char* filter_spec_sz);
    int init_audio_filter(AVCodecContext* dec_codec_ctx_p, const char* filter_spec_sz, enum AVSampleFormat fmt,
            uint64_t channel_layout, int sample_rate, int frame_size);

public: //release
    void release_video();
    void release_audio();

public:
    void reinit(AVFrame* frame_p, int media_index);
    int filter_write_frame(AVFrame* frame, int stream_index, AVFrame** ret_frame_pp);
    AVRational get_outputlink_timebase(int stream_index);
    void set_outputlink_request_samples(int stream_index, int samples);
    void set_inputlink_request_samples(int stream_index, int samples);

public:
    bool get_init_flag();
    bool is_video_init();
    bool is_audio_init();

public://audio parameter
    enum AVSampleFormat _sample_fmt;
    int _sample_rate;
    int _channels;
    uint64_t _channel_layout;
    AVCodecContext _audio_codec_ctx;//video encodec ctx, output
    char _audio_filter_spec_sz[512];

public://video parameter
    enum AVPixelFormat _pix_fmt;
    int _width;
    int _height;
    AVRational _sample_aspect_ratio;
    AVRational _framerate;
    AVRational _filter_tb;
    AVCodecContext _video_codec_ctx;//video encodec ctx, output
    char _video_filter_spec_sz[512];

private:
    FilteringContext _video_filter_ctx;
    FilteringContext _audio_filter_ctx;

    bool _video_init_flag = false;
    bool _audio_init_flag = false;
};



#endif
