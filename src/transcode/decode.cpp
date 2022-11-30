#include "utils/logger.hpp"
#include "decode.hpp"

decode_oper::decode_oper(decode_callback* cb):cb_(cb)
{
    log_infof("decode_oper construct...");
}

decode_oper::~decode_oper()
{
    cb_ = nullptr;

    if (adec_ctx_ != nullptr) {
        avcodec_free_context(&adec_ctx_);
        adec_ctx_ = nullptr;
    }
    if (vdec_ctx_ != nullptr) {
        avcodec_free_context(&vdec_ctx_);
        vdec_ctx_ = nullptr;
    }
    log_infof("decode_oper destruct...");
}

int decode_oper::open_decode(AVCodecContext*& ctx, enum AVCodecID codec_id) {
    int ret = 0;
    const AVCodec* dec = avcodec_find_decoder(codec_id);
    if (dec == NULL) {
        log_errorf("find decoder from id: %d failed\n", codec_id);
        return -1;
    }
    ctx = avcodec_alloc_context3(dec);
    if (ctx == NULL) {
        log_errorf("alloc dec context for id: %d\n", codec_id);
        return -1;
    }

    AVDictionary* opts = NULL;
    av_dict_set(&opts, "threads", "1", 0);
    if ((ret = avcodec_open2(ctx, dec, &opts)) < 0) {
        log_errorf("failed to open decoder for id: %d, ret=%d\n", codec_id, ret);
        return ret;
    }

    log_infof("avcodec open codec id:%d, codec desc:%s\r\n",
            codec_id, avcodec_get_name(codec_id));

    return ret;
}

int decode_oper::input_avpacket(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type, bool change_pts) {
    int ret = 0;
    AVCodecContext* dec_ctx = NULL;

    if (cb_ == nullptr) {
        return -1;
    }

    if (pkt->stream_index == VIDEO_STREAM_ID){
        if (vdec_ctx_ == NULL) {
            open_decode(vdec_ctx_, get_codecid(codec_type));
        }
        dec_ctx = vdec_ctx_;
    }
    else if (pkt->stream_index == AUDIO_STREAM_ID) {
        if (adec_ctx_ == NULL) {
            open_decode(adec_ctx_, get_codecid(codec_type));
        }
        dec_ctx = adec_ctx_;
    }
    else {
        log_errorf("unkown packet stream index:%d.\r\n", pkt->stream_index);
        return -1;
    }

    if ((ret = avcodec_send_packet(dec_ctx, pkt)) < 0) {
        log_errorf("avcodec_send_packet type %d failed, ret=%d\n",
                pkt->stream_index, ret);
        ffmpeg_log_err(ret);
        log_info_data(pkt->data, pkt->size, "error data");
        return ret;
    }

    while (true) {
        AVFrame* dec_frame = av_frame_alloc();
        if ((ret = avcodec_receive_frame(dec_ctx, dec_frame)) < 0) {
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                log_errorf("avcodec_receive_frame type %d failed, ret=%d\n", pkt->stream_index, ret);
            }
            av_frame_free(&dec_frame);
            break;
        }

        AVRational standard_ration = av_make_q(1, 1000);
        if (change_pts) {
            int64_t pre_dts = dec_frame->pts;
            if ((dec_ctx->time_base.den != 0) && (dec_ctx->time_base.num != 0)) {
                dec_frame->pts = av_rescale_q(dec_frame->pts, standard_ration, dec_ctx->time_base);
            } else {
                AVRational dec_tb;
                dec_tb.den = dec_frame->sample_rate;
                dec_tb.num = 1;
                dec_frame->pts = av_rescale_q(dec_frame->pts, standard_ration, dec_tb);
            }
            log_debugf("opus decode pre_dts:%ld, decode frame pts:%ld", pre_dts, dec_frame->pts);
        }
        
        if (cb_) {
            log_debugf("decode pts:%ld, dec tb:%d/%d, stream index:%d",
                    dec_frame->pts, dec_ctx->time_base.den, dec_ctx->time_base.num, pkt->stream_index);
            cb_->on_avframe_callback(pkt->stream_index, dec_frame);
        }
        av_frame_free(&dec_frame);
    }
    return 0;
}


