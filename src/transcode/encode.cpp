#include "encode.hpp"
#include "utils/logger.hpp"

encode_base::encode_base(encode_callback* cb):cb_(cb)
{

}

encode_base::~encode_base() {
    cb_ = nullptr;
}

audio_encode::audio_encode(encode_callback* cb):encode_base(cb)
{
    log_infof("audio_encode construct ...");
}

audio_encode::~audio_encode()
{
    release_audio();
    log_infof("audio_encode destruct ...");
}

int audio_encode::add_samples_to_fifo(uint8_t **data, const int frame_size) {
    int error = av_audio_fifo_realloc(_fifo_p, av_audio_fifo_size(_fifo_p) + frame_size);
    if(error < 0 ) {
        return error;
    }

    if(av_audio_fifo_write(_fifo_p, (void **)data, frame_size) < frame_size) {
        log_errorf("Could not write data to fifo");
        return AVERROR_EXIT;
    }

    return 0;
}

void audio_encode::release_audio() {
    int ret = 0;

    if(codec_ctx_) {
        //clean the encodec
        while (ret >= 0) {
            AVPacket* audio_pkt_p = av_packet_alloc();

            memset(audio_pkt_p, 0, sizeof(AVPacket));
            av_init_packet(audio_pkt_p);

            ret = avcodec_receive_packet(codec_ctx_, audio_pkt_p);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&audio_pkt_p);
                break;
            } else if (ret < 0) {
                log_errorf("audio encode unkown error, return %d", ret);
                av_packet_free(&audio_pkt_p);
                break;
            }
            av_packet_free(&audio_pkt_p);
            log_infof("clear the audio encode queue....");
        }

        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = NULL;
        log_infof("encode release_audio");
    }
    return;
}

int audio_encode::init_audio(const std::string& fmt, int samplerate, int channel, int bitrate) {
    int ret = 0;
    AVDictionary *pAudioOpt = NULL;

    _audio_fmt = fmt;

    if (codec_ctx_ != NULL) {
        log_errorf("init_audio error, already init audio");
        return -1;
    }
    _samplerate = samplerate;
    _channel = channel;
    _audio_bitrate = bitrate;

    codec_ = avcodec_find_encoder_by_name(fmt.c_str());
    if (codec_ == NULL) {
        log_errorf("init_audio avcodec_find_encoder_by_name error, fmt=%s", fmt.c_str());
        return -1;
    }
    codec_ctx_ = avcodec_alloc_context3(codec_);

    codec_ctx_->codec_type = AVMEDIA_TYPE_AUDIO;

    codec_ctx_->sample_rate = _samplerate;
    codec_ctx_->bit_rate = _audio_bitrate * 1000;
    codec_ctx_->bit_rate_tolerance = _audio_bitrate * 1000 * 3 / 2;
    codec_ctx_->channels = channel;
    codec_ctx_->channel_layout = AV_CH_LAYOUT_STEREO;

    if (codec_ctx_->channels == 2) {
        codec_ctx_->channel_layout = AV_CH_LAYOUT_STEREO;
    }
    if (codec_ctx_->channels == 6) {
        codec_ctx_->channel_layout = AV_CH_LAYOUT_5POINT1_BACK;
    }
    if (codec_ctx_->channels == 8) {
        codec_ctx_->channel_layout = AV_CH_LAYOUT_7POINT1;
    }
    codec_ctx_->time_base.num = 1;
    codec_ctx_->time_base.den = samplerate;
    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (fmt.compare("libfdk_aac") == 0) {
        codec_ctx_->sample_fmt = AV_SAMPLE_FMT_S16;
        audio_codec_type_ = MEDIA_CODEC_AAC;
        av_dict_set(&pAudioOpt, "profile", "aac_low", 0);
        av_dict_set(&pAudioOpt, "threads", "1"/*"auto"*/, 0);
    } else if (fmt.compare("libopus") == 0) {
        codec_ctx_->sample_fmt = AV_SAMPLE_FMT_S16;
        audio_codec_type_ = MEDIA_CODEC_OPUS;
    } else {
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = NULL;
        codec_ = NULL;
        return -1;
    }

    log_infof("init_audio fmt=%s, codec_id=0x%08x, samplerate=%d, channel=%d, channel_layout=%lu, bitrate=%ld, time_base=%d/%d", 
        fmt.c_str(), codec_->id, codec_ctx_->sample_rate, codec_ctx_->channels, codec_ctx_->channel_layout, 
        codec_ctx_->bit_rate, codec_ctx_->time_base.num, codec_ctx_->time_base.den);
    log_infof("get audio sampte format=%s", 
        av_get_sample_fmt_name(codec_ctx_->sample_fmt));

    if ((ret = avcodec_open2(codec_ctx_, codec_, &pAudioOpt)) < 0) {
        log_errorf("init_audio open audio encoder failed. ret=%d", ret);
        return ret;
    }

    log_infof("init_audio open audio encoder ok, time_base=%d/%d, frame_size:%d", 
        codec_ctx_->time_base.num, codec_ctx_->time_base.den, codec_ctx_->frame_size);

    if (codec_ctx_->extradata && codec_ctx_->extradata_size > 0) {
        extra_data_ = new uint8_t[codec_ctx_->extradata_size];
        extra_data_size_ =codec_ctx_->extradata_size;
        memcpy(extra_data_, codec_ctx_->extradata, extra_data_size_);
        log_info_data(extra_data_, extra_data_size_, "audio extra data:");
    }
    release_fifo();
    init_fifo();

    return 0;
}

int audio_encode::init_fifo() {
    int NB_SAMPLES = codec_ctx_->frame_size;

    _fifo_p = av_audio_fifo_alloc(AV_SAMPLE_FMT_S16, _channel, NB_SAMPLES);
    if(!_fifo_p) {
        log_errorf("Could not aoolcate fifo");
        return AVERROR(ENOMEM);
    }
    log_infof("init_fifo ok, fmt:%d, channel:%d, nb_samples:%d",
            AV_SAMPLE_FMT_S16, codec_ctx_->channels, codec_ctx_->frame_size);
    return 0;
}

void audio_encode::release_fifo() {
    if (_fifo_p) {
        log_infof("init_fifo release");
        av_audio_fifo_free(_fifo_p);
        _fifo_p = nullptr;
    }
}

AVFrame* audio_encode::get_new_audio_frame(uint8_t*& frame_buf) {
    AVFrame* ret_frame_p = av_frame_alloc();

    ret_frame_p->nb_samples= codec_ctx_->frame_size;
    ret_frame_p->format= codec_ctx_->sample_fmt;
    ret_frame_p->channels = codec_ctx_->channels;
    ret_frame_p->sample_rate = codec_ctx_->sample_rate;
    int size = av_samples_get_buffer_size(NULL, codec_ctx_->channels, codec_ctx_->frame_size, codec_ctx_->sample_fmt, 0);

    frame_buf = (uint8_t *)av_malloc(size);

    avcodec_fill_audio_frame(ret_frame_p, codec_ctx_->channels,
        codec_ctx_->sample_fmt, (const uint8_t*)frame_buf, size, 1);

    return ret_frame_p;
}

int audio_encode::send_frame(AVFrame* frame) {
    int ret;
    AVFrame* filtered_frame = nullptr;
    char dscr[512] = {0};
    char layout_name[256];

    av_get_channel_layout_string(layout_name, sizeof(layout_name),
                                 codec_ctx_->channels, codec_ctx_->channel_layout);
    snprintf(dscr, sizeof(dscr),
        "aformat=sample_fmts=%s:channel_layouts=%s:sample_rates=%d",
        av_get_sample_fmt_name(codec_ctx_->sample_fmt), layout_name, codec_ctx_->sample_rate);
    filter_.set_outputlink_request_samples(AUDIO_STREAM_ID, codec_ctx_->frame_size);
    filter_.set_inputlink_request_samples(AUDIO_STREAM_ID, codec_ctx_->frame_size);
    filter_.init_audio_filter((enum AVSampleFormat)frame->format, frame->sample_rate,
                             frame->channels,frame->channel_layout, codec_ctx_, dscr);

    AVFrame* input_frame_p = frame;
    while (true) {
        ret = filter_.filter_write_frame(input_frame_p, AUDIO_STREAM_ID, &filtered_frame);
        if ((ret  < 0) || (filtered_frame == nullptr)) {
            if (ret == AVERROR_EOF) {
                filter_.release_audio();
            }
            return ret;
        }
        AVRational filter_tb = filter_.get_outputlink_timebase(AUDIO_STREAM_ID);

        input_frame_p = nullptr;//set nullptr for only reading in next time
        filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
        add_samples_to_fifo(filtered_frame->data, filtered_frame->nb_samples);
        while(true) {
            int filo_size = av_audio_fifo_size(_fifo_p);
            if (filo_size < codec_ctx_->frame_size) {
                break;
            }
            uint8_t* frame_buf = nullptr;
            AVFrame* dst_frame_p = get_new_audio_frame(frame_buf);
            av_audio_fifo_read(_fifo_p, (void **)dst_frame_p->data, codec_ctx_->frame_size);
            av_frame_copy_props(dst_frame_p, filtered_frame);
            dst_frame_p->nb_samples     = codec_ctx_->frame_size;
            dst_frame_p->channels       = codec_ctx_->channels;
            dst_frame_p->channel_layout = codec_ctx_->channel_layout;
            dst_frame_p->format         = codec_ctx_->sample_fmt;
            dst_frame_p->pkt_dts        = filtered_frame->pkt_dts;
            dst_frame_p->pts            = filtered_frame->pts;
            if (last_audio_pts_ >= filtered_frame->pts) {
                dst_frame_p->pts = last_audio_pts_ + codec_ctx_->frame_size;
            }
            last_audio_pts_ = dst_frame_p->pts;
            dst_frame_p->pts = av_rescale_q(dst_frame_p->pts, filter_tb, codec_ctx_->time_base);
            dst_frame_p->pict_type = AV_PICTURE_TYPE_NONE;

            ret = avcodec_send_frame(codec_ctx_, dst_frame_p);
            av_frame_free(&dst_frame_p);
            av_freep(&frame_buf);
            if (ret < 0) {
                log_errorf("audio encode error, return %d, sample fmt:%s",
                           ret, av_get_sample_fmt_name(codec_ctx_->sample_fmt));
                break;
            }

            while (ret >= 0) {
                AVPacket* audio_pkt_p = av_packet_alloc();

                memset(audio_pkt_p, 0, sizeof(AVPacket));
                av_init_packet(audio_pkt_p);

                ret = avcodec_receive_packet(codec_ctx_, audio_pkt_p);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_packet_free(&audio_pkt_p);
                    break;
                } else if (ret < 0) {
                    log_errorf("audio encode unkown error, return %d", ret);
                    av_packet_free(&audio_pkt_p);
                    break;
                }
                audio_pkt_p->stream_index = AUDIO_STREAM_ID;

                audio_pkt_p->pts = (audio_pkt_p->pts < 0) ? 0 :audio_pkt_p->pts;
                audio_pkt_p->dts = (audio_pkt_p->dts < 0) ? 0 :audio_pkt_p->dts;
                av_packet_rescale_ts(audio_pkt_p, codec_ctx_->time_base, AV_TIME_BASE_Q);
                audio_pkt_p->dts = audio_pkt_p->dts / 1000;
                audio_pkt_p->pts = audio_pkt_p->pts / 1000;
                if (audio_codec_type_ == MEDIA_CODEC_OPUS) {
                    reset_opus_timestamp(audio_pkt_p);
                }
                if (cb_) {
                    cb_->on_avpacket_callback(audio_pkt_p, audio_codec_type_, extra_data_, extra_data_size_);
                }
                av_packet_free(&audio_pkt_p);
            }
        }
        av_frame_free(&filtered_frame);
    }

    return 0;
}

void audio_encode::reset_opus_timestamp(AVPacket* pkt) {
    if (audio_codec_type_ != MEDIA_CODEC_OPUS) {
        return;
    }
    if (last_pkt_dts_ < 0) {
        last_pkt_dts_ = pkt->dts;
        log_infof("reset dts:%ld for opus", last_pkt_dts_);
    } else if (last_pkt_dts_ > pkt->dts) {
        log_infof("reverse old dts:%ld to new dts:%ld for opus", last_pkt_dts_, pkt->dts);
        last_pkt_dts_ = pkt->dts;
    } else if ((pkt->dts - last_pkt_dts_) > (3000*1000)) {
        log_infof("audio dts jump a lot, old dts:%ld to new dts:%ld for opus", last_pkt_dts_, pkt->dts);
        last_pkt_dts_ = pkt->dts;
    } else {
        last_pkt_dts_ += 20;
    }
    
    pkt->dts = pkt->pts = last_pkt_dts_;
}

video_encode::video_encode(encode_callback* cb):encode_base(cb)
{
    log_infof("video_encode construct...");
}

video_encode::~video_encode() {
    release_video();
    log_infof("video_encode destruct...");
}

int video_encode::init_video(const std::string& vcodec, int width, int height, const std::string& profile, const std::string& preset, int bitrate, int framerate) {
	AVDictionary *videoopt_p = nullptr;
    if (init_) {
        log_errorf("init video error, already init video");
        return -1;
    }

    codec_desc_ = vcodec;
    width_  = width;
    height_ = height;

    profile_ = profile;
    preset_  = preset;

    bitrate_   = bitrate;
    framerate_ = framerate;

    if (vcodec == "libx264") {
        video_codec_type_ = MEDIA_CODEC_H264;
    } else if (vcodec == "libvpx") {
        video_codec_type_ = MEDIA_CODEC_VP8;
    } else if (vcodec == "libx265") {
        video_codec_type_ = MEDIA_CODEC_H265;
    } else {
        log_infof("unkown video codec:%s", vcodec.c_str());
        return -1;
    }
    
    codec_ = avcodec_find_encoder_by_name(vcodec.c_str());
    if (codec_ == nullptr) {
    	log_errorf("init video avcodec_find_encoder_by_name %s error", vcodec.c_str());
    	return -1;
    }
    codec_ctx_ = avcodec_alloc_context3(codec_);
    codec_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
    log_infof("init video codec_id=%d, w=%d, h=%d, profile=%s, preset=%s",
        codec_ctx_->codec_id, width, height, profile.c_str(), preset.c_str());

    codec_ctx_->width = (width%2==0) ? width : width + 1;
    codec_ctx_->height = (height%2==0) ? height : height + 1;

    AVRational fps;

    fps.num = 1;
    fps.den = framerate;

    codec_ctx_->time_base = fps;
    codec_ctx_->bit_rate = bitrate_*1000;
    codec_ctx_->framerate = av_inv_q(fps);
    codec_ctx_->rc_max_rate = codec_ctx_->bit_rate * 1.5;
    codec_ctx_->has_b_frames = 0;
    
    codec_ctx_->rc_buffer_size = codec_ctx_->rc_max_rate;
    codec_ctx_->rc_initial_buffer_occupancy = codec_ctx_->rc_buffer_size * 0.9;

    codec_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->gop_size = framerate*2;

    char szIFrameInterval[80];
    char szIFrameMax[80];

    sprintf(szIFrameInterval, "%d", framerate*2);
    sprintf(szIFrameMax, "%d", framerate*2);

    av_dict_set(&videoopt_p, "preset", preset.c_str(), 0);
    av_dict_set(&videoopt_p, "profile", profile.c_str(), 0);
    av_dict_set(&videoopt_p, "tune", "zerolatency", 0);
    av_dict_set(&videoopt_p, "keyint_min", szIFrameInterval, 0);
    av_dict_set(&videoopt_p, "g", szIFrameMax, 0);
    av_dict_set(&videoopt_p, "threads", "auto", 0);
    
    codec_ctx_->flags |= AVFMT_GLOBALHEADER;

    int ret = avcodec_open2(codec_ctx_, codec_, &videoopt_p);
    if (ret != 0) {
    	log_errorf("avcodec_open2 error");
    	return -1;
    }
    av_dict_free(&videoopt_p);

    log_infof("init video open video encoder ok, time_base=%d/%d, frame_size:%d", 
        codec_ctx_->time_base.num, codec_ctx_->time_base.den,
        codec_ctx_->frame_size);
    init_ = true;
    return 0;
}

void video_encode::release_video() {
    if (!init_) {
        return;
    }
    init_ = true;
    int ret = 0;

    if(codec_ctx_) {
        while (ret >= 0) {
            AVPacket* video_pkt_p = av_packet_alloc();

            av_init_packet(video_pkt_p);

            ret = avcodec_receive_packet(codec_ctx_, video_pkt_p);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&video_pkt_p);
                break;
            } else if (ret < 0) {
                log_errorf("video encode unkown error, return %d", ret);
                av_packet_free(&video_pkt_p);
                break;
            }
            av_packet_free(&video_pkt_p);
        }
        avcodec_free_context(&codec_ctx_);
        codec_ctx_ = nullptr;
        codec_ = nullptr;
        log_infof("encode release_video");
    }
    return;
}

int video_encode::send_frame(AVFrame* frame) {
    if (!init_) {
        return -1;
    }
    int ret;
    AVRational standard_tb = {1, 1000};
    char desc[256];
    
    snprintf(desc, sizeof(desc), "scale=%dx%d:flags=bicubic,format=pix_fmts=%s", 
                codec_ctx_->width, codec_ctx_->height, av_get_pix_fmt_name(codec_ctx_->pix_fmt));
    filter_.init_video_filter((enum AVPixelFormat)frame->format, frame->width, frame->height, frame->sample_aspect_ratio, standard_tb, codec_ctx_, desc);

    AVFrame* filtered_frame = nullptr;
    AVFrame* input_frame_p = frame;
    while (true) {
        ret = filter_.filter_write_frame(input_frame_p, VIDEO_STREAM_ID, &filtered_frame);
        if ((ret  < 0) || (filtered_frame == nullptr)) {
            if (ret == AVERROR_EOF) {
                filter_.release_video();
            }
            return ret;
        }
        input_frame_p = nullptr;//set nullptr for only reading in next time
        filtered_frame->pict_type = AV_PICTURE_TYPE_NONE;
    
        AVRational filter_tb = filter_.get_outputlink_timebase(VIDEO_STREAM_ID);
        filtered_frame->pts = av_rescale_q(filtered_frame->pts, filter_tb, codec_ctx_->time_base);

        ret = avcodec_send_frame(codec_ctx_, filtered_frame);
        av_frame_free(&filtered_frame);
        if (ret < 0) {
            log_errorf("video encode error, return %d", ret);
            break;
        }

        while (ret >= 0) {
            AVPacket* video_pkt_p = av_packet_alloc();

            av_init_packet(video_pkt_p);

            ret = avcodec_receive_packet(codec_ctx_, video_pkt_p);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_free(&video_pkt_p);
                break;
            } else if (ret < 0) {
                log_errorf("video encode unkown error, return %d", ret);
                av_packet_free(&video_pkt_p);
                break;
            }

            video_pkt_p->pts = (video_pkt_p->pts < 0) ? 0 :video_pkt_p->pts;
            video_pkt_p->dts = (video_pkt_p->dts < 0) ? 0 :video_pkt_p->dts;
            video_pkt_p->stream_index = VIDEO_STREAM_ID;

            if (cb_) {
                cb_->on_avpacket_callback(video_pkt_p, video_codec_type_, extra_data_, extra_data_size_);
            }
            av_packet_free(&video_pkt_p);
       }
    }

    return 0;
}


