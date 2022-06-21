#ifndef SDL_PLAY_HPP
#define SDL_PLAY_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
};
#endif
#include <queue>
#include <mutex>
#include "utils/data_buffer.hpp"

#define SDL_SAMPLE_MAX (960*4)

void sdl_audio_callback(void *opaque, uint8_t *stream, int len);

class sdl_player
{
friend void sdl_audio_callback(void *opaque, uint8_t *stream, int len);

public:
    sdl_player(const std::string& title);
    ~sdl_player();

public:
    void run();
    void on_callback(uint8_t* stream, int len);

public:
    int insertVideoFrame(AVFrame* frame);
    int insertAudioFrame(AVFrame* frame);

private:
    int initVideo(int width, int height);
    int initAudio(int channels, int sampleRate, int format, int samples);
    void playVideo(AVFrame* frame);
    void playAudio(AVFrame* frame);
    void on_work();

    AVFrame* getVideoFrame();
    AVFrame* getAudioFrame();
    int64_t get_videoframe_pts();
    int64_t get_audioframe_pts();
    size_t get_videoframe_queue_len();
    size_t get_audioframe_queue_len();
    int64_t get_videoframe_queue_duration();
    int64_t get_audioframe_queue_duration();

private:
    std::string title_;

private:
    std::queue<AVFrame*> videoFrameQueue_;
    std::queue<AVFrame*> audioFrameQueue_;
    std::mutex videoFrameQueueMutex_;
    std::mutex audioFrameQueueMutex_;
    data_buffer audio_buffer_;
    int64_t start_pts_ = -1;
    int64_t video_pts_ = -1;
    int64_t audio_pts_ = -1;
    int64_t buffer_duration_ = 500;//ms

private:
    bool videoInit_             = false;
    bool audioInit_             = false;
    int width_                  = 0;
    int height_                 = 0;
    SDL_Window *screen_         = nullptr;
    SDL_Renderer* sdl_renderer_ = nullptr;
    SDL_Texture* sdl_texture_   = nullptr;
    SDL_Rect sdl_rect_;

    SDL_AudioSpec wantedSpec_;
    SDL_AudioSpec obtainedSpec_;
    SDL_AudioDeviceID audioDev_;
    uint8_t sampleBuffer_[SDL_SAMPLE_MAX];
    int samplePos_ = 0;
};


#endif