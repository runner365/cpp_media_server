#ifndef TRANS_PUB_HPP
#define TRANS_PUB_HPP

#ifdef __cplusplus  
extern "C"  
{  
#endif
#define __STDC_CONSTANT_MACROS

#include <libavutil/log.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include <libavutil/avstring.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavfilter/avfilter.h>
#include <libavutil/parseutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/dict.h>

#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

#ifdef __cplusplus  
}
#endif
#include "utils/av/av.hpp"

#define VIDEO_STREAM_ID 0
#define AUDIO_STREAM_ID 1

#define ffmpeg_log_err(err)                               \
    do {                                                  \
        char errbuf[128];                                 \
        const char *errbuf_ptr = errbuf;                  \
        if (av_strerror(err, errbuf, sizeof(errbuf)) < 0) \
            errbuf_ptr = strerror(AVUNERROR(err));        \
        log_errorf("ffmpeg error: %s\n", errbuf_ptr);    \
    }while(0)

inline enum AVCodecID get_codecid(MEDIA_CODEC_TYPE codec_type) {
    enum AVCodecID ret_id = AV_CODEC_ID_NONE;

    switch (codec_type)
    {
        case MEDIA_CODEC_H264:
            ret_id = AV_CODEC_ID_H264;
            break;
        case MEDIA_CODEC_H265:
            ret_id = AV_CODEC_ID_H265;
            break;
        case MEDIA_CODEC_VP8:
            ret_id = AV_CODEC_ID_VP8;
            break;
        case MEDIA_CODEC_VP9:
            ret_id = AV_CODEC_ID_VP9;
            break;
        case MEDIA_CODEC_AAC:
            ret_id = AV_CODEC_ID_AAC;
            break;
        case MEDIA_CODEC_OPUS:
            ret_id = AV_CODEC_ID_OPUS;
            break;
        case MEDIA_CODEC_MP3:
            ret_id = AV_CODEC_ID_MP3;
            break;
        default:
            ret_id = AV_CODEC_ID_NONE;
    }

    return ret_id;
}

class decode_callback
{
public:
    virtual void on_avframe_callback(int stream_index, AVFrame* frame) = 0;
};

class encode_callback
{
public:
    virtual void on_avpacket_callback(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type,
                                      uint8_t* extra_data, size_t extra_data_size) = 0;
};
#endif
