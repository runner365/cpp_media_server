#ifndef FFTW_VAD_HPP
#define FFTW_VAD_HPP

#include "vad.hpp"
#include <vector>
#include <fftw3.h>

class FftwVad : public VadI
{
public:
    FftwVad();
    virtual ~FftwVad();

public:
    virtual int Init(int sample_rate, int frame_size, int threshold) override;
    virtual int Process(const int16_t* audio_frame, int num_samples) override;
    virtual void Release() override;

// private:
//     static bool s_init_;

private:
    bool init_ = false;
    fftw_complex *fft_out_ = nullptr;
    fftw_plan plan_;

private:
    int sample_rate_    = VAD_DEF_SAMPLE_RATE;
    int frame_size_     = VAD_DEF_FRAME_SIZE;
    std::vector<int16_t> pcm_buffer_;
};

#endif // FFTW_VAD_HPP