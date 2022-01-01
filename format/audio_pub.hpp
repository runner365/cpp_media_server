#ifndef AUDIO_PUB_HPP
#define AUDIO_PUB_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>

static const int mpeg4audio_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000,
    24000, 22050, 16000, 12000, 11025, 8000, 7350
};

inline int get_samplerate_index(int samplerate) {
    int index = 4;//default 44100
    for (; index < sizeof(mpeg4audio_sample_rates); index++) {
        if (samplerate == mpeg4audio_sample_rates[index]) {
            break;
        }
    }
    return index;
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
    int i = 0;
    int sample_rate_index = get_samplerate_index(sample_rate);

    full_frame_size &= 0x1FFF;
    //AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP

    // - Sync 0xFFFx
    data[i++] = 0xff;
    data[i++] = 0xf1; //ID(1bit): 0, layer(2bits): 0, protection_absent(1bit): 1

    data[i] = (object_type << 6) & 0xc; //profile_objecttype
    data[i] |= (sample_rate_index << 2) & 0x3c; //sample_rate_index
    data[i] &= 0xFD; /* private_bit */

    //channel highest 1bit
    if (channel >= 4) {
        data[i] |= 0x01;
    } else {
        data[i] &= 0xFE;
    }
    i++;

    data[i] = (channel & 0x03) << 6;//channel low 2bits
    data[i] &= 0xc3;//clear 4bits: original_copy(1bit)
                    //             home(1bit)
                    //             adts_variable_header: copyright_identification_bit(1bit)
                    //             adts_variable_header: copyright_identification_start(1bit)
    data[i] |= (full_frame_size >> 11) & 0x03;//aac_frame_length highest 2bits
    i++;

    data[i] |= (full_frame_size >> 3) & 0xff;
    i++;

    data[i] |= (full_frame_size & 0x07) << 5;
    data[i] |= 0x1f;
    i++;

    data[i] = 0xFC;
    
    return i+1;
}

#endif //AUDIO_PUB_HPP