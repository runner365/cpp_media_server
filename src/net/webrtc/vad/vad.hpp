#ifndef VAD_HPP
#define VAD_HPP 
#include <stdint.h>
#include <stddef.h>
#include <memory>

#define VAD_DEF_SAMPLE_RATE 48000      // 采样率提高到 48000 Hz
#define VAD_DEF_FRAME_SIZE 1024        // 增加帧大小以提高频率分辨率

class VadI
{
public:
    virtual int Init(int sample_rate, int frame_size, int threshold) = 0;
    virtual int Process(const int16_t* audio_frame, int num_samples) = 0;
    virtual void Release() = 0;
};

typedef enum VadType
{
    VAD_NONE = 0,
    VAD_FFTW = 1
} VadType;

std::shared_ptr<VadI> CreateVadFactory(VadType type);

#endif // VAD_HPP