#include "sdl_player.hpp"
#include "utils/logger.hpp"
#include "utils/timeex.hpp"
#include <thread>
#include <chrono>

#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define FFMAX(a,b) ((a) > (b) ? (a) : (b))

/* Minimum SDL audio buffer size, in samples. */
#define SDL_AUDIO_MIN_BUFFER_SIZE 512
/* Calculate actual buffer size keeping in mind not cause too frequent audio callbacks */
#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

static const struct TextureFormatEntry {
    enum AVPixelFormat format;
    int texture_fmt;
} sdl_texture_format_map[] = {
    { AV_PIX_FMT_RGB8,           SDL_PIXELFORMAT_RGB332 },
    { AV_PIX_FMT_RGB444,         SDL_PIXELFORMAT_RGB444 },
    { AV_PIX_FMT_RGB555,         SDL_PIXELFORMAT_RGB555 },
    { AV_PIX_FMT_BGR555,         SDL_PIXELFORMAT_BGR555 },
    { AV_PIX_FMT_RGB565,         SDL_PIXELFORMAT_RGB565 },
    { AV_PIX_FMT_BGR565,         SDL_PIXELFORMAT_BGR565 },
    { AV_PIX_FMT_RGB24,          SDL_PIXELFORMAT_RGB24 },
    { AV_PIX_FMT_BGR24,          SDL_PIXELFORMAT_BGR24 },
    { AV_PIX_FMT_0RGB32,         SDL_PIXELFORMAT_RGB888 },
    { AV_PIX_FMT_0BGR32,         SDL_PIXELFORMAT_BGR888 },
    { AV_PIX_FMT_NE(RGB0, 0BGR), SDL_PIXELFORMAT_RGBX8888 },
    { AV_PIX_FMT_NE(BGR0, 0RGB), SDL_PIXELFORMAT_BGRX8888 },
    { AV_PIX_FMT_RGB32,          SDL_PIXELFORMAT_ARGB8888 },
    { AV_PIX_FMT_RGB32_1,        SDL_PIXELFORMAT_RGBA8888 },
    { AV_PIX_FMT_BGR32,          SDL_PIXELFORMAT_ABGR8888 },
    { AV_PIX_FMT_BGR32_1,        SDL_PIXELFORMAT_BGRA8888 },
    { AV_PIX_FMT_YUV420P,        SDL_PIXELFORMAT_IYUV },
    { AV_PIX_FMT_YUYV422,        SDL_PIXELFORMAT_YUY2 },
    { AV_PIX_FMT_UYVY422,        SDL_PIXELFORMAT_UYVY },
    { AV_PIX_FMT_NONE,           SDL_PIXELFORMAT_UNKNOWN },
};

static void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode)
{
    int i;
    *sdl_blendmode = SDL_BLENDMODE_NONE;
    *sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;
    if (format == AV_PIX_FMT_RGB32   ||
        format == AV_PIX_FMT_RGB32_1 ||
        format == AV_PIX_FMT_BGR32   ||
        format == AV_PIX_FMT_BGR32_1)
        *sdl_blendmode = SDL_BLENDMODE_BLEND;
    for (i = 0; i < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; i++) {
        if (format == sdl_texture_format_map[i].format) {
            *sdl_pix_fmt = sdl_texture_format_map[i].texture_fmt;
            return;
        }
    }
}

void sdl_audio_callback(void *opaque, uint8_t *stream, int len) {
    sdl_player* this_p = (sdl_player*)opaque;
    if (this_p) {
        this_p->on_callback(stream, len);
    }
    return;
}

sdl_player::sdl_player(const std::string& title):title_(title)
                                        , audio_buffer_(100*1024)
{
}

sdl_player::~sdl_player()
{
}

int sdl_player::insertVideoFrame(AVFrame* frame) {
    std::unique_lock<std::mutex> lock(videoFrameQueueMutex_);
    if(videoFrameQueue_.size() > 500) {
        log_errorf("video frame queue is overflow, pop all...\r\n");
        do {
            AVFrame* freeFrame = videoFrameQueue_.front();
            videoFrameQueue_.pop();
            av_frame_free(&freeFrame);
        } while (!videoFrameQueue_.empty());
    }
    AVFrame* new_frame = av_frame_clone(frame);
    videoFrameQueue_.push(new_frame);

    return videoFrameQueue_.size();
}

AVFrame* sdl_player::getVideoFrame() {
    std::unique_lock<std::mutex> lock(videoFrameQueueMutex_);
    AVFrame* frame = nullptr;

    if (videoFrameQueue_.empty()) {
        return frame;
    }

    frame = videoFrameQueue_.front();
    videoFrameQueue_.pop();

    return frame;
}

int64_t sdl_player::get_videoframe_pts() {
    std::unique_lock<std::mutex> lock(videoFrameQueueMutex_);
    AVFrame* frame = nullptr;

    if (videoFrameQueue_.empty()) {
        return -1;
    }

    frame = videoFrameQueue_.front();

    return frame->pts;
}

int64_t sdl_player::get_audioframe_pts() {
    std::unique_lock<std::mutex> lock(audioFrameQueueMutex_);
    AVFrame* frame = nullptr;

    if (audioFrameQueue_.empty()) {
        return -1;
    }

    frame = audioFrameQueue_.front();

    return frame->pts;
}

size_t sdl_player::get_videoframe_queue_len() {
    std::unique_lock<std::mutex> lock(videoFrameQueueMutex_);

    return videoFrameQueue_.size();
}

size_t sdl_player::get_audioframe_queue_len() {
    std::unique_lock<std::mutex> lock(audioFrameQueueMutex_);

    return audioFrameQueue_.size();
}

int sdl_player::insertAudioFrame(AVFrame* frame) {
    std::unique_lock<std::mutex> lock(audioFrameQueueMutex_);

    if(audioFrameQueue_.size() > 1000) {
        log_errorf("audio frame queue is overflow, pop all...\r\n");
        do {
            AVFrame* freeFrame = audioFrameQueue_.front();
            audioFrameQueue_.pop();
            av_frame_free(&freeFrame);
        } while (!audioFrameQueue_.empty());
    }
    AVFrame* new_frame = av_frame_clone(frame);
    audioFrameQueue_.push(new_frame);

    return audioFrameQueue_.size();
}

AVFrame* sdl_player::getAudioFrame() {
    std::unique_lock<std::mutex> lock(audioFrameQueueMutex_);
    AVFrame* frame = nullptr;

    if (audioFrameQueue_.empty()) {
        return frame;
    }

    frame = audioFrameQueue_.front();
    audioFrameQueue_.pop();

    return frame;
}

int64_t sdl_player::get_videoframe_queue_duration() {
    std::unique_lock<std::mutex> lock(videoFrameQueueMutex_);
    if (videoFrameQueue_.size() < 2) {
        return -1;
    }
    AVFrame* front_frame = videoFrameQueue_.front();
    AVFrame* back_frame  = videoFrameQueue_.back();

    return back_frame->pts - front_frame->pts;
}

int64_t sdl_player::get_audioframe_queue_duration() {
    std::unique_lock<std::mutex> lock(audioFrameQueueMutex_);
    if (audioFrameQueue_.size() < 2) {
        return -1;
    }
    AVFrame* front_frame = audioFrameQueue_.front();
    AVFrame* back_frame  = audioFrameQueue_.back();

    return back_frame->pts - front_frame->pts;
}

void sdl_player::on_callback(uint8_t* stream, int len) {
    int pos = 0;

    while (len > 0) {
        while ((get_audioframe_queue_len() > 2) && (get_audioframe_queue_duration() > buffer_duration_)) {
            AVFrame* discard_frame = getAudioFrame();
            av_frame_free(&discard_frame);
        }
        
        AVFrame* frame = getAudioFrame();
        if (!frame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        log_infof("audio frame line:%d %d %d, nb samples:%d, pts:%ld, audio queue len:%lu(%ld)",
                frame->linesize[0], frame->linesize[1], frame->linesize[2],
                frame->nb_samples, frame->pts, get_audioframe_queue_len(),
                get_audioframe_queue_duration());
        audio_pts_ = frame->pts;
        audio_buffer_.append_data((char*)frame->data[0], frame->nb_samples * frame->channels * sizeof(uint16_t));

        av_frame_free(&frame);

        if (len > audio_buffer_.data_len()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        memcpy(stream, audio_buffer_.data(), len);
        audio_buffer_.consume_data(len);

        len  = 0;
    }

    return;
}

int sdl_player::initVideo(int width, int height) {
    if (videoInit_) {
        return 0;
    }
    width_  = width;
    height_ = height;

    log_infof("sdl init video w:%d, h:%d", width, height);

    int flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
    screen_ = SDL_CreateWindow(title_.c_str(),
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width_, height_, flags);
    if (screen_ == nullptr) {
        log_errorf("create sdl window error:%s\r\n", SDL_GetError());
        return -1;
    }
    log_infof("create sdl window ok....\r\n");
    
    sdl_renderer_ = SDL_CreateRenderer(screen_, -1, 0);
    if (sdl_renderer_ == nullptr)
    {
        log_errorf("create render error....\r\n");
        return -1;
    }
    log_infof("create render ok....\r\n");

    sdl_texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_IYUV,
            SDL_RENDERER_TARGETTEXTURE, width, height);  
    if (sdl_texture_ == nullptr) {
        log_errorf("create texture error....\r\n");
        return -1;
    }
    log_infof("create texture ok....\r\n");
    sdl_rect_.x = 0;
    sdl_rect_.y = 0;
    sdl_rect_.w = width;
    sdl_rect_.h = height;

    log_infof("sdl play init ok, w:%d, h:%d.\r\n", width, height);
    videoInit_ = true;
    return 0;
}

int sdl_player::initAudio(int channels, int sampleRate, int format, int samples) {
    if (audioInit_) {
        return 0;
    }
    char* env = SDL_getenv("SDL_AUDIO_CHANNELS");
    if (env) {
        int wanted_nb_channels = atoi(env);
        int64_t wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
        log_infof("audio init wanted channels:%d, channel layout:%ld", wanted_nb_channels, wanted_channel_layout);
    }
    SDL_memset(&wantedSpec_, 0, sizeof(wantedSpec_));
    wantedSpec_.channels = channels;
    wantedSpec_.freq     = sampleRate;
    wantedSpec_.format   = format;
    wantedSpec_.silence  = 0;
    wantedSpec_.samples  = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(sampleRate / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));;
    wantedSpec_.callback = sdl_audio_callback;
    wantedSpec_.userdata = this;

    memset(sampleBuffer_, 0, sizeof(sampleBuffer_));
    samplePos_ = 0;
    audioDev_ = SDL_OpenAudioDevice(NULL, 0, &wantedSpec_, &obtainedSpec_,
                            SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (audioDev_ > 0) {
        audioInit_ = true;
        log_infof("sdl play init audioDev_:%d, channels:%d, rate:%d, format:%d, samples:%d\r\n",
            audioDev_, channels, sampleRate, format, samples);
        log_infof("sdl play get spec channels:%d, format:%d, freq:%d, samples:%d\r\n",
            obtainedSpec_.channels, obtainedSpec_.format,
            obtainedSpec_.freq, obtainedSpec_.samples);
        SDL_PauseAudioDevice(audioDev_, 0);
    }
    else {
        log_errorf("sdl play init error:%d, channels:%d, sample rate:%d, foramt:%d, samples:%d, error:%s\r\n",
                audioDev_, channels, sampleRate, format, samples, SDL_GetError());
        return -1;
    }
    return 0;
}

void sdl_player::playVideo(AVFrame* frame) {
    if (initVideo(frame->width, frame->height) != 0) {
        return;
    }

    int ret = 0;

    uint32_t sdl_pix_fmt = 0;
    SDL_BlendMode mode   = SDL_BLENDMODE_NONE;
    get_sdl_pix_fmt_and_blendmode(frame->format, &sdl_pix_fmt, &mode);

    log_debugf("frame->format:%d, sdl_pix_fmt:%d, mode:%d.\r\n", frame->format, sdl_pix_fmt, mode);

    sdl_rect_.x = 0;
    sdl_rect_.y = 0;
    sdl_rect_.w = frame->width;
    sdl_rect_.h = frame->height;

    log_infof("frame data line:%d %d %d, pts:%ld, video queue len:%lu(%ld)",
        frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->pts,
        get_videoframe_queue_len(), get_videoframe_queue_duration());

    video_pts_ = frame->pts;

    ret = SDL_UpdateYUVTexture(sdl_texture_, &sdl_rect_, frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]);
    if (ret != 0) {
        log_errorf("SDL_UpdateYUVTexture error:%d.\r\n", ret);
        return;
    }
    //SDL_UpdateTexture(sdl_texture_, NULL, frame->data[0], frame->linesize[0]);

    ret = SDL_RenderClear(sdl_renderer_);
    if (ret != 0) {
        log_errorf("SDL_RenderClear error:%d.\r\n", ret);
        return;
    }

    ret = SDL_RenderCopy(sdl_renderer_, sdl_texture_, nullptr, nullptr);
    if (ret != 0) {
        log_errorf("SDL_RenderCopy error:%d.\r\n", ret);
        return;
    }
    //SDL_RenderCopy(sdl_renderer_, sdl_texture_, NULL, NULL);  
    SDL_RenderPresent(sdl_renderer_);

    //av_frame_free(&frame);
    return;
}

void sdl_player::playAudio(AVFrame* frame) {

    return;
}

void sdl_player::run() {
    SDL_Event event;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
        log_errorf( "Could not initialize SDL - %s\n", SDL_GetError());
        throw "sdl init error";
    }
    if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE")) {
        SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE","1", 1);
    }
    log_infof("SDL_Init ok...\r\n");

    start_pts_ = now_millisec();
    while (true) {
        SDL_PumpEvents();
        while (!SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT)) {
            on_work();
            SDL_PumpEvents();
        }
        if (event.type == SDL_QUIT) {
            break;
        }
    }
    return;
}

void sdl_player::on_work() {
    AVFrame* video_frame = nullptr;
    if (!audioInit_) {
        AVFrame* audio_frame = getAudioFrame();
        if (audio_frame) {
            initAudio(audio_frame->channels, audio_frame->sample_rate, AUDIO_S16SYS, audio_frame->nb_samples);
            av_frame_free(&audio_frame);
        }
    }

    while ((get_videoframe_queue_len() > 2) && (get_videoframe_queue_duration() > buffer_duration_)) {
        AVFrame* discard_frame = getVideoFrame();
        av_frame_free(&discard_frame);
    }

    if (get_videoframe_queue_len() > 0) {
        int64_t v_pts = get_videoframe_pts();
        if (audio_pts_ > 0) {
            if (v_pts < audio_pts_) {
                video_frame = getVideoFrame();
                if (video_frame) {
                    playVideo(video_frame);
                    av_frame_free(&video_frame);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        } else {
            int64_t diff_t = now_millisec() - start_pts_;
            if (diff_t > 6000) {
                video_frame = getVideoFrame();
                if (video_frame) {
                    playVideo(video_frame);
                    av_frame_free(&video_frame);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }
    }
}