#ifndef DECODE_HPP
#define DECODE_HPP
#include "transcode_pub.hpp"
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

class decode_oper
{
public:
    decode_oper(decode_callback* cb);
    ~decode_oper();

public:
    int input_avpacket(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type, bool change_pts = true);

private:
    int open_decode(AVCodecContext*& ctx, enum AVCodecID codec_id);

private:
    AVCodecContext* adec_ctx_ = nullptr;
    AVCodecContext* vdec_ctx_ = nullptr;

private:
    decode_callback* cb_ = nullptr;
};

#endif
