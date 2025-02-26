#include "fftw_vad.hpp"
#include "utils/logger.hpp"
#include <math.h>
#include <cstring>

#define FFTW_DEF_THRESHOLD 30000.0

// bool FftwVad::s_init_ = false;

// 计算能量
double calculate_energy(int16_t *frame, int frame_size) {
    double energy = 0.0;
    for (int i = 0; i < frame_size; i++) {
        double sample = (double)frame[i] / 32768.0; // 归一化到 [-1.0, 1.0]
        energy += sample * sample;
    }
    return energy;
}

// 进行FFT变换
void perform_fft(int16_t *frame, fftw_complex *fft_out, fftw_plan plan, int frame_size) {
    // 将 int16_t 数据归一化并转换为 double 类型
    double *normalized_frame = (double *)malloc(sizeof(double) * frame_size);
    for (int i = 0; i < frame_size; i++) {
        normalized_frame[i] = (double)frame[i] / 32768.0; // 归一化
    }

    // 将实数帧数据复制到复数数组中，虚部设为0
    for (int i = 0; i < frame_size; i++) {
        fft_out[i][0] = normalized_frame[i];
        fft_out[i][1] = 0.0;
    }

    // 执行FFT计划
    fftw_execute(plan);

    free(normalized_frame);
}

// 判断是否为语音帧
int is_voice_frame(int16_t *frame, fftw_complex *fft_out, int frame_size, int threshold = FFTW_DEF_THRESHOLD) {
    double energy = calculate_energy(frame, frame_size);

    // 计算频谱幅度
    double magnitude = 0.0;
    for (int i = 0; i < frame_size / 2 + 1; i++) {
        magnitude += sqrt(fft_out[i][0] * fft_out[i][0] + fft_out[i][1] * fft_out[i][1]);
    }
    magnitude /= (frame_size / 2 + 1);

    // 根据能量和频谱幅度判断是否为语音帧
    if (energy > threshold && magnitude > threshold) {
        return 1;
    } else {
        return 0;
    }
}

FftwVad::FftwVad() {
    log_infof("FftwVad construct...");
}

FftwVad::~FftwVad() {
    log_infof("FftwVad destruct...");
}

int FftwVad::Init(int sample_rate, int frame_size, int threshold) {
    if (init_) {
        return 0;
    }
    /*
        if (!s_init_) {
        log_infof("init fftw...");
        fftw_init_threads();
        fftw_plan_with_nthreads(4);
        s_init_ = true;
    }
    */

    init_ = true;

    sample_rate_    = sample_rate;
    frame_size_     = frame_size;

    fft_out_ = (fftw_complex *) fftw_malloc(sizeof(fftw_complex) * frame_size);
    plan_ = fftw_plan_dft_r2c_1d(frame_size, NULL, fft_out_, FFTW_ESTIMATE);

    log_infof("FftwVad init ok, sample rate:%d, frame size:%d, threshold:%d", sample_rate_, frame_size_, threshold);
    pcm_buffer_.resize(frame_size + 8*1024);
    return 0;
}

int FftwVad::Process(const int16_t* audio_frame, int num_samples) {
    int ret = 0;

    if (!init_) {
        log_errorf("FftwVad not init...");
        return -1;
    }
    if ((int)pcm_buffer_.size() < num_samples) {
        pcm_buffer_.resize(num_samples + 8 * 1024);
    }
    int16_t *pcm_data = &pcm_buffer_[0];
    memcpy(pcm_data, audio_frame, num_samples * sizeof(int16_t));

    ret = is_voice_frame(pcm_data, fft_out_, num_samples);
    log_infof("is_voice_frame:%d, input sample count:%d", ret, num_samples);
    return ret;
}

void FftwVad::Release() {
    if (!init_) {
        return;
    }
    init_ = false;
    fftw_destroy_plan(plan_);
    fftw_free(fft_out_);
    
    return;
}
