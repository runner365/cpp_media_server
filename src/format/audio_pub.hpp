#ifndef AUDIO_PUB_HPP
#define AUDIO_PUB_HPP
#include "utils/byte_stream.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <iostream>

static const int mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

inline int get_samplerate_index(int samplerate) {
    int index = 0;//default 44100
    bool found = false;
    for (; index < (int)(sizeof(mpeg4audio_sample_rates)/sizeof(int)); index++) {
        if (samplerate == mpeg4audio_sample_rates[index]) {
            found = true;
            break;
        }
    }
    if (!found) {
        index = 4;
    }
    return index;
}

//xxxx xyyy yzzz z000
//aac type==5(or 29): xxx xyyy yzzz zyyy yzzz z000
inline bool get_audioinfo_by_asc(uint8_t* data, size_t data_size,
                        uint8_t& audio_type, int& sample_rate, uint8_t& channel) {
    uint8_t sample_rate_index = 0;
    uint8_t* p = data;

    audio_type = (*p >> 3) & 0x1F;
    if (audio_type == 31) {
        return false;//not supported in the version
    }

    sample_rate_index = (*p & 0x07) << 1;
    p++;
    sample_rate_index |= (*p & 0x80) >> 7;
    if (sample_rate_index > sizeof(mpeg4audio_sample_rates)) {
        return false;//not supported in the version
    }

    sample_rate = mpeg4audio_sample_rates[sample_rate_index];
    channel = (*p >> 3) & 0x0F;
    
    return true;
}

inline bool get_audioinfo2_by_asc(uint8_t* data, size_t data_size,
                        uint8_t& audio_type, int& sample_rate, uint8_t& channel) {
    uint8_t sample_rate_index = 0;
    uint8_t* p = data;

    audio_type = (*p >> 3) & 0x1F;
    if (audio_type == 31) {
        return false;//not supported in the version
    }

    sample_rate_index = (*p & 0x07) << 1;
    p++;
    sample_rate_index |= (*p & 0x80) >> 7;
    if (sample_rate_index > sizeof(mpeg4audio_sample_rates)) {
        return false;//not supported in the version
    }

    sample_rate = mpeg4audio_sample_rates[sample_rate_index];
    channel = (*p >> 3) & 0x0F;
    
    if ((audio_type == 5) || (audio_type == 29)) {
        sample_rate_index = ((p[0] << 1) & 0x0E) | (p[1] >> 7);
        sample_rate = mpeg4audio_sample_rates[sample_rate_index];
    }
    return true;
}

/*
    ADTS HEADER: 7 Bytes. See ISO 13818-7 (2004)
    AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP
    A - Sync 0xFFFx
    B   1   MPEG Version: 0 for MPEG-4, 1 for MPEG-2
    C   2   Layer: always 0
    D   1   protection absent, Warning, set to 1 if there is no CRC and 0 if there is CRC
    E   2   profile, the MPEG-4 Audio Object Type minus 1
    F   4   MPEG-4 Sampling Frequency Index (15 is forbidden)
    G   1   private bit, guaranteed never to be used by MPEG, set to 0 when encoding, ignore when decoding
    H   3   MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
    I   1   originality, set to 0 when encoding, ignore when decoding
    J   1   home, set to 0 when encoding, ignore when decoding
    K   1   copyrighted id bit, the next bit of a centrally registered copyright identifier, set to 0 when encoding, ignore when decoding
    L   1   copyright id start, signals that this frame's copyright id bit is the first bit of the copyright id, set to 0 when encoding, ignore when decoding
    M   13  frame length, this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
    O   11  Buffer fullness
    P   2   Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
    Q   16  CRC if protection absent is 0
*/
inline int make_adts(uint8_t* data, uint8_t object_type,
                int sample_rate, int channel, int full_frame_size) {
    const int ADTS_LEN = 7;
    int sample_rate_index = get_samplerate_index(sample_rate);
    full_frame_size += ADTS_LEN;
    full_frame_size &= 0x1FFF;
    
    //AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP
    // - Sync 0xFFFx

	//first write adts header
	data[0] = 0xff;
	data[1] = 0xf1;

	data[2] = 0x00;
	data[2] = data[2] | ((object_type-1)<<6);
	data[2] = data[2] | ((sample_rate_index)<<2);
	data[2] = data[2] | ((channel >> 2));

    //0x80
	data[3] &= 0x00;
	data[3] = data[3] | (channel << 6);
	data[3] = data[3] | ((full_frame_size<<3)>>14);

	data[4] &= 0x00;
	data[4] = data[4] | ((full_frame_size<<5)>>8);

	data[5] &= 0x00;
	data[5] = data[5] | (((full_frame_size<<13)>>13)<<5);
	data[5] = data[5] | ((0x7C<<1)>>3);
	data[6] = 0xfc;
    
    return ADTS_LEN;
}

inline size_t make_opus_header(uint8_t* data, int sample_rate, int channel) {
    uint8_t* p = data;
    const char opus_str[] = "OpusHead";
    const size_t opus_str_len = 8;
    size_t header_len = 0;

    memcpy(p, (uint8_t*)opus_str, opus_str_len);
    p += opus_str_len;

    *p = 1;
    p++;
    *p = (uint8_t)channel;
    p++;
    write_2bytes_be(p, 0);//initial_padding
    p += 2;
    write_4bytes_be(p, sample_rate);
    p += 4;
    write_2bytes_be(p, 0);
    p += 2;
    *p = 0;//mapping_family
    p++;
/*
{ 0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64,
  0x01, 0x02, 0x38, 0x01, 0x80, 0xbb, 0x00, 0x00,
  0x00, 0x00, 0x00}
*/
    header_len = (size_t)(p - data);
    return header_len;
}

#endif //AUDIO_PUB_HPP
