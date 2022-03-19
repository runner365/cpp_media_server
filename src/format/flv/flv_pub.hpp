#ifndef FLV_PUB_HPP
#define FLV_PUB_HPP

#define FLV_TAG_AUDIO 0x08
#define FLV_TAG_VIDEO 0x09

#define FLV_VIDEO_KEY_FLAG   0x10
#define FLV_VIDEO_INTER_FLAG 0x20

#define FLV_VIDEO_AVC_SEQHDR 0x00
#define FLV_VIDEO_AVC_NALU   0x01

#define FLV_VIDEO_H264_CODEC 0x07
#define FLV_VIDEO_H265_CODEC 0x0c
#define FLV_VIDEO_AV1_CODEC  0x0d
#define FLV_VIDEO_VP8_CODEC  0x0e
#define FLV_VIDEO_VP9_CODEC  0x0f

#define FLV_AUDIO_OPUS_CODEC  0x90
#define FLV_AUDIO_AAC_CODEC   0xa0

/* offsets for packed values */
#define FLV_AUDIO_SAMPLESSIZE_OFFSET 1
#define FLV_AUDIO_SAMPLERATE_OFFSET  2
#define FLV_AUDIO_CODECID_OFFSET     4

enum {
    FLV_MONO   = 0,
    FLV_STEREO = 1,
};

enum {
    FLV_SAMPLESSIZE_8BIT  = 0,
    FLV_SAMPLESSIZE_16BIT = 1 << FLV_AUDIO_SAMPLESSIZE_OFFSET,
};

enum {
    FLV_SAMPLERATE_SPECIAL = 0, /**< signifies 5512Hz and 8000Hz in the case of NELLYMOSER */
    FLV_SAMPLERATE_11025HZ = 1 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_22050HZ = 2 << FLV_AUDIO_SAMPLERATE_OFFSET,
    FLV_SAMPLERATE_44100HZ = 3 << FLV_AUDIO_SAMPLERATE_OFFSET,
};

/*
ASC flag：xxxx xyyy yzzz z000
x： aac type，类型2表示AAC-LC，5是SBR, 29是ps，5和29比较特殊ascflag的长度会变成4；
y:  sample rate, 采样率, 7表示22050采样率
z:  通道数，2是双通道
*/
inline void flv_audio_asc_decode(char* data, uint8_t& audio_type, uint8_t& sample_rate_index, uint8_t& channel) {
//2    b    9    2    0    8    0    0
//0010 1011 1001 0010 0000 1000 0000 0000
//aac type: 5
// samplerate index: 7
}
#endif