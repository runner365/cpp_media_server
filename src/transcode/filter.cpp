#include "filter.hpp"
#include "utils/logger.hpp"

stream_filter::stream_filter():_video_init_flag(false)
    , _audio_init_flag(false)
{
    memset(&_video_filter_ctx, 0, sizeof(FilteringContext));
    memset(&_audio_filter_ctx, 0, sizeof(FilteringContext));
    _sample_fmt = AV_SAMPLE_FMT_S16;
    _sample_rate = -1;
    _channels = -1;
    _channel_layout = 0;

    _width = 0;
    _height = 0;
    memset(&_audio_codec_ctx, 0, sizeof(_audio_codec_ctx));
    memset(&_video_codec_ctx, 0, sizeof(_video_codec_ctx));

    memset(_audio_filter_spec_sz, 0, sizeof(_audio_filter_spec_sz));
    memset(_video_filter_spec_sz, 0, sizeof(_video_filter_spec_sz));
    log_infof("filter construct...");
}

stream_filter::~stream_filter() {
    release_audio();
    release_video();
    log_infof("filter destruct...");
}

void stream_filter::reinit(AVFrame* frame_p, int media_index) {
    if (frame_p == nullptr) {
        return;
    }

    if (media_index == AUDIO_STREAM_ID) {
        if (!_audio_init_flag) {
            return;
        }
        if ((_sample_rate != frame_p->sample_rate) ||
            (_channels != frame_p->channels) ||
            (_channel_layout != frame_p->channel_layout)) {
            log_infof("audio refilter");
            release_audio();
            init_audio_filter((enum AVSampleFormat)frame_p->format, frame_p->sample_rate, 
                frame_p->channels,frame_p->channel_layout, &_audio_codec_ctx, 
                _audio_filter_spec_sz);
        }
    } else if (media_index == VIDEO_STREAM_ID) {
        if (!_video_init_flag) {
            return;
        }
        if ((_width != frame_p->width) || (_height != frame_p->height)
            || (_pix_fmt != (enum AVPixelFormat)frame_p->format)) {
            log_infof("video refilter");
            release_video();
            init_video_filter((enum AVPixelFormat)frame_p->format, frame_p->width, frame_p->height, frame_p->sample_aspect_ratio, _filter_tb,
                &_video_codec_ctx, _video_filter_spec_sz);
        }
    } else {
        log_errorf("reinit avfilter media type is unkown:%d", media_index);
    }
    return;
}

int stream_filter::init_video_filter(enum AVPixelFormat pix_fmt, int width, int height,
        AVRational sample_aspect_ratio, AVRational filter_tb,
        AVCodecContext* enc_codec_ctx_p, const char* filter_spec_sz)
{
    int ret = 0;
    char args[512];

    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    if(_video_init_flag) {
        return 0;
    }
    _pix_fmt = pix_fmt;
    _width = width;
    _height = height;
    _sample_aspect_ratio = sample_aspect_ratio;
    _filter_tb = filter_tb;
    memcpy(&_video_codec_ctx, enc_codec_ctx_p, sizeof(_video_codec_ctx));
    strcpy(_video_filter_spec_sz, filter_spec_sz);

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    buffersrc = avfilter_get_by_name("buffer");
    buffersink = avfilter_get_by_name("buffersink");
    if (!buffersrc || !buffersink) {
        av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            width,height, pix_fmt,
            filter_tb.num, filter_tb.den,
            sample_aspect_ratio.num,
            sample_aspect_ratio.den);

    log_infof("init_video_filter input args:%s", args);
    log_infof("init_video_filter output dscr:%s", filter_spec_sz);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create buffer source");
        goto end;
    }

    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create buffer sink");
        goto end;
    }

    ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
            (uint8_t*)&enc_codec_ctx_p->pix_fmt, sizeof(enc_codec_ctx_p->pix_fmt),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output pixel format");
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec_sz,
            &inputs, &outputs, NULL)) < 0) {
        log_errorf("avfilter_graph_parse_ptr");
        goto end;
    }
        

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
        log_errorf("avfilter_graph_config");
        goto end;
    }

    /* Fill FilteringContext */
    _video_filter_ctx.buffersrc_ctx = buffersrc_ctx;
    _video_filter_ctx.buffersink_ctx = buffersink_ctx;
    _video_filter_ctx.filter_graph = filter_graph;

   _video_init_flag = true;
   log_infof("init_video_filter ok output dscr:%s", filter_spec_sz);
   log_infof("init_video_filter ok sink_ctx timebase:%d/%d",
       buffersink_ctx->inputs[0]->time_base.num, 
       buffersink_ctx->inputs[0]->time_base.den);
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int stream_filter::init_audio_filter(AVCodecContext* dec_codec_ctx_p, const char* filter_spec_sz,
            enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int frame_size)
{
    if(_audio_init_flag) {
        return 0;
    }
    
    int ret = 0;
    char args[512];

    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    const char* audio_fmt_name = av_get_sample_fmt_name(dec_codec_ctx_p->sample_fmt);
    if (audio_fmt_name == nullptr) {
        audio_fmt_name = av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP);
    }
    
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    _sample_fmt = sample_fmt;
    _sample_rate = sample_rate;
    _channels = dec_codec_ctx_p->channels;
    _channel_layout = channel_layout;
    strcpy(_audio_filter_spec_sz, filter_spec_sz);

    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("abuffersink");
    if (!buffersrc || !buffersink) {
        log_errorf("filtering source or sink element not found");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    if (!dec_codec_ctx_p->channel_layout)
        dec_codec_ctx_p->channel_layout =
            av_get_default_channel_layout(dec_codec_ctx_p->channels);

    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=%lu",
            1, dec_codec_ctx_p->sample_rate, dec_codec_ctx_p->sample_rate,
            audio_fmt_name, channel_layout);
 
    log_infof("init_audio_filter input args:%s", args);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer source");
        goto end;
    }
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer sink");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
            (uint8_t*)&sample_fmt, sizeof(sample_fmt),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample format");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
            (uint8_t*)&channel_layout,
            sizeof(channel_layout), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output channel layout");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
            (uint8_t*)&sample_rate, sizeof(sample_rate),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample rate");
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec_sz,
                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

    log_infof("before set: input request_samples:%d", 
        buffersink_ctx->inputs[0]->request_samples);
    buffersink_ctx->inputs[0]->request_samples = frame_size;
    /* Fill FilteringContext */
    _audio_filter_ctx.buffersrc_ctx = buffersrc_ctx;
    _audio_filter_ctx.buffersink_ctx = buffersink_ctx;
    _audio_filter_ctx.filter_graph = filter_graph;

    _audio_init_flag = true;
    log_infof("init_audio_filter ok, output dscr:%s", filter_spec_sz);
    log_infof("init_audio_filter ok sink_ctx timebase:%d/%d, input request_samples:%d",
       buffersink_ctx->inputs[0]->time_base.num, buffersink_ctx->inputs[0]->time_base.den,
       buffersink_ctx->inputs[0]->request_samples);
    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int stream_filter::init_audio_filter(enum AVSampleFormat sample_fmt, int sample_rate, 
        int channels, uint64_t channel_layout,
        enum AVSampleFormat target_sample_fmt, int target_sample_rate,
        int target_channels, uint64_t target_channel_layout,
        int target_frame_size, const char* filter_spec_sz)
{
    if(_audio_init_flag) {
        return 0;
    }
    int ret = 0;
    char args[512];

    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    _sample_fmt = sample_fmt;
    _sample_rate = sample_rate;
    _channels = channels;
    _channel_layout = channel_layout;
    strcpy(_audio_filter_spec_sz, filter_spec_sz);

    const char* audio_fmt_name = av_get_sample_fmt_name(sample_fmt);
    if (audio_fmt_name == nullptr) {
        audio_fmt_name = av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP);
    }

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("abuffersink");
    if (!buffersrc || !buffersink) {
        log_errorf("filtering source or sink element not found");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    if (!channel_layout)
        channel_layout = av_get_default_channel_layout(channels);
    _channel_layout = channel_layout;
    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%lu",
            1, sample_rate, sample_rate,
            audio_fmt_name, channel_layout);
    
    log_infof("init_audio_filter input args:%s", args);
    log_infof("init audio filter output dscr:%s", _audio_filter_spec_sz);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer source");
        goto end;
    }
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer sink");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
            (uint8_t*)&target_sample_fmt, sizeof(target_sample_fmt),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample format");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
            (uint8_t*)&target_channel_layout,
            sizeof(target_channel_layout), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output channel layout");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
            (uint8_t*)&target_sample_rate, sizeof(target_sample_rate),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample rate");
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        log_errorf("audio avfilter out of memory error:%d", ret);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec_sz,
                    &inputs, &outputs, NULL)) < 0){
        log_errorf("audio avfilter_graph_parse_ptr error:%d", ret);
        goto end;
    }

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
        log_errorf("audio avfilter_graph_config error:%d", ret);
        goto end;
    }

    log_infof("before set: input request_samples:%d", 
        buffersink_ctx->inputs[0]->request_samples);
    buffersink_ctx->inputs[0]->request_samples = target_frame_size;
    /* Fill FilteringContext */
    _audio_filter_ctx.buffersrc_ctx = buffersrc_ctx;
    _audio_filter_ctx.buffersink_ctx = buffersink_ctx;
    _audio_filter_ctx.filter_graph = filter_graph;

    _audio_init_flag = true;
    log_infof("init_audio_filter ok, output dscr:%s", filter_spec_sz);
    log_infof("init_audio_filter ok sink_ctx timebase:%d/%d, input request_samples:%d",
       buffersink_ctx->inputs[0]->time_base.num, buffersink_ctx->inputs[0]->time_base.den,
       buffersink_ctx->inputs[0]->request_samples);
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int stream_filter::init_audio_filter(enum AVSampleFormat sample_fmt, int sample_rate, 
        int channels, uint64_t channel_layout,
        AVCodecContext* enc_codec_ctx_p, const char* filter_spec_sz)
{
    if(_audio_init_flag) {
        return 0;
    }
    int ret = 0;
    char args[512];

    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;

    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();

    _sample_fmt = sample_fmt;
    _sample_rate = sample_rate;
    _channels = channels;
    _channel_layout = channel_layout;
    memcpy(&_audio_codec_ctx, enc_codec_ctx_p, sizeof(_audio_codec_ctx));
    strcpy(_audio_filter_spec_sz, filter_spec_sz);

    const char* audio_fmt_name = av_get_sample_fmt_name(sample_fmt);
    if (audio_fmt_name == nullptr) {
        audio_fmt_name = av_get_sample_fmt_name(AV_SAMPLE_FMT_FLTP);
    }

    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    buffersrc = avfilter_get_by_name("abuffer");
    buffersink = avfilter_get_by_name("abuffersink");
    if (!buffersrc || !buffersink) {
        log_errorf("filtering source or sink element not found");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    if (!channel_layout)
        channel_layout = av_get_default_channel_layout(channels);
    _channel_layout = channel_layout;
    snprintf(args, sizeof(args),
            "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%lu",
            1, sample_rate, sample_rate,
            audio_fmt_name, channel_layout);
    
    log_infof("init_audio_filter input args:%s", args);
    log_infof("init audio filter output dscr:%s", _audio_filter_spec_sz);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer source");
        goto end;
    }
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, filter_graph);
    if (ret < 0) {
        log_errorf("Cannot create audio buffer sink");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
            (uint8_t*)&enc_codec_ctx_p->sample_fmt, sizeof(enc_codec_ctx_p->sample_fmt),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample format");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
            (uint8_t*)&enc_codec_ctx_p->channel_layout,
            sizeof(enc_codec_ctx_p->channel_layout), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output channel layout");
        goto end;
    }
    ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
            (uint8_t*)&enc_codec_ctx_p->sample_rate, sizeof(enc_codec_ctx_p->sample_rate),
            AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        log_errorf("Cannot set output sample rate");
        goto end;
    }

    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        log_errorf("audio avfilter out of memory error:%d", ret);
        goto end;
    }

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec_sz,
                    &inputs, &outputs, NULL)) < 0){
        log_errorf("audio avfilter_graph_parse_ptr error:%d", ret);
        goto end;
    }

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0) {
        log_errorf("audio avfilter_graph_config error:%d", ret);
        goto end;
    }

    log_infof("before set: input request_samples:%d", 
        buffersink_ctx->inputs[0]->request_samples);
    buffersink_ctx->inputs[0]->request_samples = enc_codec_ctx_p->frame_size;
    /* Fill FilteringContext */
    _audio_filter_ctx.buffersrc_ctx = buffersrc_ctx;
    _audio_filter_ctx.buffersink_ctx = buffersink_ctx;
    _audio_filter_ctx.filter_graph = filter_graph;

    _audio_init_flag = true;
    log_infof("init_audio_filter ok, output dscr:%s", filter_spec_sz);
    log_infof("init_audio_filter ok sink_ctx timebase:%d/%d, input request_samples:%d",
       buffersink_ctx->inputs[0]->time_base.num, buffersink_ctx->inputs[0]->time_base.den,
       buffersink_ctx->inputs[0]->request_samples);
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

void stream_filter::release_video() {
    int ret = 0;
    if (!_video_init_flag) {
        return;
    }
    _video_init_flag = false;
    log_infof("avfilter release video");
    if (_video_filter_ctx.filter_graph == nullptr) {
        return;
    }
    ret = av_buffersrc_add_frame_flags(_video_filter_ctx.buffersrc_ctx, nullptr, 0);
    avfilter_graph_free(&_video_filter_ctx.filter_graph);
    memset(&_video_filter_ctx, 0, sizeof(FilteringContext));
    log_infof("release video filter return:%d", ret);
}

void stream_filter::release_audio() {
    int ret = 0;
    if (!_audio_init_flag) {
        return;
    }
    _audio_init_flag = false;
    log_infof("avfilter release audio");
    if (_audio_filter_ctx.filter_graph == nullptr) {
        return;
    }
    ret = av_buffersrc_add_frame_flags(_audio_filter_ctx.buffersrc_ctx, nullptr, 0);
    avfilter_graph_free(&_audio_filter_ctx.filter_graph);
    memset(&_audio_filter_ctx, 0, sizeof(FilteringContext));
    log_infof("release audio filter return:%d", ret);
}

//write frame in filter;
//read new frame out of filter: video for scale, video for resample;
//if input frame is nullptr, it means we want to get new frame from filter buffer;
int stream_filter::filter_write_frame(AVFrame* frame, int stream_index, AVFrame** ret_frame_pp) {
	int ret = 0;
    AVFilterContext *buffersink_ctx = nullptr;
    AVFilterContext *buffersrc_ctx = nullptr;
    
    if (stream_index == VIDEO_STREAM_ID) {
        if (!_video_init_flag) {
            log_errorf("video filter is not inited");
            return -1;
        }
        buffersrc_ctx = _video_filter_ctx.buffersrc_ctx;
        buffersink_ctx = _video_filter_ctx.buffersink_ctx;
    } else if (stream_index == AUDIO_STREAM_ID) {
        if (!_audio_init_flag) {
            log_errorf("audio filter is not inited");
            return -1;
        }
        buffersrc_ctx = _audio_filter_ctx.buffersrc_ctx;
        buffersink_ctx = _audio_filter_ctx.buffersink_ctx;
    } else {
        log_errorf("stream index:%d error", stream_index);
        return -1;
    }

    if ((buffersrc_ctx == nullptr) || (buffersink_ctx == nullptr)) {
        log_errorf("filter not ready, media type:%s", 
            (stream_index == AUDIO_STREAM_ID) ? "AUDIO" : "VIDEO");
        return -1;
    }

    if (frame) {
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_PUSH|AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            log_errorf("Error while feeding the %s filtergraph. ret=%d, nb_samples:%d",
                (stream_index == VIDEO_STREAM_ID) ? "video" : "audio", 
                ret, frame->nb_samples);
            return ret;
        }
    }

    AVFrame *filt_frame;
    /* pull filtered frames from the filtergraph */
    while (true) {
        filt_frame = av_frame_alloc();
        if (!filt_frame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        filt_frame->nb_samples = buffersink_ctx->inputs[0]->request_samples;
        ret = av_buffersink_get_frame_flags(buffersink_ctx, filt_frame, AV_BUFFERSINK_FLAG_NO_REQUEST);
        if (ret < 0) {
            /* if no more frames for output - returns AVERROR(EAGAIN)
             * if flushed and no more frames for output - returns AVERROR_EOF
             * rewrite retcode to 0 to show it as normal procedure completion
             */
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                char szDbg[256];
                av_strerror(ret, szDbg, sizeof(szDbg));
                log_warnf("av_buffersink_get_frame_flags error:%s", szDbg);
            } else if (ret == AVERROR_EOF) {
                log_errorf("av_buffersink_get_frame_flags AVERROR_EOF");
            }
            av_frame_free(&filt_frame);
            break;
        }

        *ret_frame_pp = filt_frame;
        break;
    }

	return ret;
}

bool stream_filter::get_init_flag() {
    return _video_init_flag && _audio_init_flag;
}

AVRational stream_filter::get_outputlink_timebase(int stream_index) {
    AVRational filter_tb = {1, 1};
    
    if (stream_index == VIDEO_STREAM_ID) {
        if (_video_filter_ctx.buffersink_ctx && _video_filter_ctx.buffersink_ctx->inputs) {
            return _video_filter_ctx.buffersink_ctx->inputs[0]->time_base;
        }
    }
    if (stream_index == AUDIO_STREAM_ID) {
        if (_audio_filter_ctx.buffersink_ctx && _audio_filter_ctx.buffersink_ctx->inputs) {
            return _audio_filter_ctx.buffersink_ctx->inputs[0]->time_base;
        }
    }
    return filter_tb;
}

void stream_filter::set_outputlink_request_samples(int stream_index, int samples) {
    if (stream_index == VIDEO_STREAM_ID) {
        if (_video_filter_ctx.buffersink_ctx && _video_filter_ctx.buffersink_ctx->inputs) {
            _video_filter_ctx.buffersink_ctx->inputs[0]->request_samples = samples;
        }
    }
    if (stream_index == AUDIO_STREAM_ID) {
        if (_audio_filter_ctx.buffersink_ctx && _audio_filter_ctx.buffersink_ctx->inputs) {
            _audio_filter_ctx.buffersink_ctx->inputs[0]->request_samples = samples;
        }
    }
    return;
}

void stream_filter::set_inputlink_request_samples(int stream_index, int samples) {
    if (stream_index == VIDEO_STREAM_ID) {
        if (_video_filter_ctx.buffersrc_ctx && _video_filter_ctx.buffersrc_ctx->outputs) {
            _video_filter_ctx.buffersrc_ctx->outputs[0]->request_samples = samples;
        }
    }
    if (stream_index == AUDIO_STREAM_ID) {
        if (_audio_filter_ctx.buffersrc_ctx && _audio_filter_ctx.buffersrc_ctx->outputs) {
            _audio_filter_ctx.buffersrc_ctx->outputs[0]->request_samples = samples;
        }
    }
    return;
}

bool stream_filter::is_video_init() {
    return _video_init_flag;
}

bool stream_filter::is_audio_init() {
    return _audio_init_flag;
}

