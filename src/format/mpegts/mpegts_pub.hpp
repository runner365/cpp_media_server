#ifndef MPEGTS_PUB_HPP
#define MPEGTS_PUB_HPP

#include "av_format_interface.hpp"
#include "data_buffer.hpp"
#include "media_packet.hpp"
#include <string>
#include <memory>
#include <vector>
#include <stdint.h>
#include <unordered_map>

#define TS_PACKET_SIZE 188

/* mpegts stream type in ts pmt
Value    Description
0x00     ITU-T | ISO/IEC Reserved
0x01     ISO/IEC 11172-2 Video (mpeg video v1)
0x02     ITU-T Rec. H.262 | ISO/IEC 13818-2 Video(mpeg video v2)or ISO/IEC 11172-2 constrained parameter video stream
0x03     ISO/IEC 11172-3 Audio (MPEG 1 Audio codec Layer I, Layer II and Layer III audio specifications) 
0x04     ISO/IEC 13818-3 Audio (BC Audio Codec) 
0x05     ITU-T Rec. H.222.0 | ISO/IEC 13818-1 private_sections 
0x06     ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data 
0x07     ISO/IEC 13522 MHEG 
0x08     ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Annex A DSM-CC 
0x09     ITU-T Rec. H.222.1 
0x0A     ISO/IEC 13818-6 type A 
0x0B     ISO/IEC 13818-6 type B 
0x0C     ISO/IEC 13818-6 type C 
0x0D     ISO/IEC 13818-6 type D 
0x0E     ITU-T Rec. H.222.0 | ISO/IEC 13818-1 auxiliary 
0x0F     ISO/IEC 13818-7 Audio with ADTS transport syntax 
0x10     ISO/IEC 14496-2 Visual 
0x11     ISO/IEC 14496-3 Audio with the LATM transport syntax as defined in ISO/IEC 14496-3/Amd.1 
0x12     ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in PES packets 
0x13     ISO/IEC 14496-1 SL-packetized stream or FlexMux stream carried in ISO/IEC 14496_sections 
0x14     ISO/IEC 13818-6 Synchronized Download Protocol 
0x15     Metadata carried in PES packets 
0x16     Metadata carried in metadata_sections 
0x17     Metadata carried in ISO/IEC 13818-6 Data Carousel 
0x18     Metadata carried in ISO/IEC 13818-6 Object Carousel 
0x19     Metadata carried in ISO/IEC 13818-6 Synchronized Download Protocol 
0x1A     IPMP stream (defined in ISO/IEC 13818-11, MPEG-2 IPMP) 
0x1B     AVC video stream as defined in ITU-T Rec. H.264 | ISO/IEC 14496-10 Video (h.264) 
0x1C     ISO/IEC 14496-3 Audio, without using any additional transport syntax, such as DST, ALS and SLS 
0x1D     ISO/IEC 14496-17 Text 
0x1E     Auxiliary video stream as defined in ISO/IEC 23002-3 (AVS) 
0x1F-0x7E ITU-T Rec. H.222.0 | ISO/IEC 13818-1 Reserved 
0x7F     IPMP stream 0x80-0xFF User Private
*/
#define STREAM_TYPE_VIDEO_MPEG1     0x01
#define STREAM_TYPE_VIDEO_MPEG2     0x02
#define STREAM_TYPE_AUDIO_MPEG1     0x03
#define STREAM_TYPE_AUDIO_MPEG2     0x04
#define STREAM_TYPE_PRIVATE_SECTION 0x05
#define STREAM_TYPE_PRIVATE_DATA    0x06
#define STREAM_TYPE_AUDIO_AAC       0x0f
#define STREAM_TYPE_AUDIO_AAC_LATM  0x11
#define STREAM_TYPE_VIDEO_MPEG4     0x10
#define STREAM_TYPE_METADATA        0x15
#define STREAM_TYPE_VIDEO_H264      0x1b
#define STREAM_TYPE_VIDEO_HEVC      0x24
#define STREAM_TYPE_VIDEO_CAVS      0x42
#define STREAM_TYPE_VIDEO_VC1       0xea
#define STREAM_TYPE_VIDEO_DIRAC     0xd1

#define STREAM_TYPE_AUDIO_AC3       0x81
#define STREAM_TYPE_AUDIO_DTS       0x82
#define STREAM_TYPE_AUDIO_TRUEHD    0x83
#define STREAM_TYPE_AUDIO_EAC3      0x87

// 2.4.4.4 Table_id assignments
// Table 2-31 - table_id assignment values(p66/p39)
enum EPAT_TID
{
	PAT_TID_PAS				= 0x00, // program_association_section
	PAT_TID_CAS				= 0x01, // conditional_access_section(CA_section)
	PAT_TID_PMS				= 0x02, // TS_program_map_section
	PAT_TID_SDS				= 0x03, // TS_description_section
	PAT_TID_MPEG4_scene		= 0x04, // ISO_IEC_14496_scene_description_section
	PAT_TID_MPEG4_object	= 0x05, // ISO_IEC_14496_object_descriptor_section
	PAT_TID_META			= 0x06, // Metadata_section
	PAT_TID_IPMP			= 0x07, // IPMP_Control_Information_section(defined in ISO/IEC 13818-11)
	PAT_TID_H222			= 0x08, // Rec. ITU-T H.222.0 | ISO/IEC 13818-1 reserved
	PAT_TID_USER			= 0x40,	// User private
    PAT_TID_SDT             = 0x42, // service_description_section 
	PAT_TID_Forbidden		= 0xFF,
};

enum EPES_STREAM_ID
{
	PES_SID_SUB			= 0x20, // ffmpeg/libavformat/mpeg.h
	PES_SID_AC3			= 0x80, // ffmpeg/libavformat/mpeg.h
	PES_SID_DTS			= 0x88, // ffmpeg/libavformat/mpeg.h
	PES_SID_LPCM		= 0xA0, // ffmpeg/libavformat/mpeg.h

	PES_SID_EXTENSION	= 0xB7, // PS system_header extension(p81)
	PES_SID_END			= 0xB9, // MPEG_program_end_code
	PES_SID_START		= 0xBA, // Pack start code
	PES_SID_SYS			= 0xBB, // System header start code

	PES_SID_PSM			= 0xBC, // program_stream_map
	PES_SID_PRIVATE_1	= 0xBD, // private_stream_1
	PES_SID_PADDING		= 0xBE, // padding_stream
	PES_SID_PRIVATE_2	= 0xBF, // private_stream_2
	PES_SID_AUDIO		= 0xC0, // ISO/IEC 13818-3/11172-3/13818-7/14496-3 audio stream '110x xxxx'
	PES_SID_VIDEO		= 0xE0, // H.262 | H.264 | H.265 | ISO/IEC 13818-2/11172-2/14496-2/14496-10 video stream '1110 xxxx'
	PES_SID_ECM			= 0xF0, // ECM_stream
	PES_SID_EMM			= 0xF1, // EMM_stream
	PES_SID_DSMCC		= 0xF2, // H.222.0 | ISO/IEC 13818-1/13818-6_DSMCC_stream
	PES_SID_13522		= 0xF3, // ISO/IEC_13522_stream
	PES_SID_H222_A		= 0xF4, // Rec. ITU-T H.222.1 type A
	PES_SID_H222_B		= 0xF5, // Rec. ITU-T H.222.1 type B
	PES_SID_H222_C		= 0xF6, // Rec. ITU-T H.222.1 type C
	PES_SID_H222_D		= 0xF7, // Rec. ITU-T H.222.1 type D
	PES_SID_H222_E		= 0xF8, // Rec. ITU-T H.222.1 type E
	PES_SID_ANCILLARY	= 0xF9, // ancillary_stream
	PES_SID_MPEG4_SL	= 0xFA, // ISO/IEC 14496-1_SL_packetized_stream
	PES_SID_MPEG4_Flex	= 0xFB, // ISO/IEC 14496-1_FlexMux_stream
	PES_SID_META		= 0xFC, // metadata stream
	PES_SID_EXTEND		= 0xFD,	// extended_stream_id
	PES_SID_RESERVED	= 0xFE,	// reserved data stream
	PES_SID_PSD			= 0xFF, // program_stream_directory
};

class adaptation_field {
public:
    adaptation_field(){};
    ~adaptation_field(){};

public:
    unsigned char _adaptation_field_length;

    unsigned char _discontinuity_indicator:1;
    unsigned char _random_access_indicator:1;
    unsigned char _elementary_stream_priority_indicator:1;
    unsigned char _PCR_flag:1;
    unsigned char _OPCR_flag:1;
    unsigned char _splicing_point_flag:1;
    unsigned char _transport_private_data_flag:1;
    unsigned char _adaptation_field_extension_flag:1;
    
    //if(PCR_flag == '1')
    unsigned long _program_clock_reference_base;//33 bits
    unsigned short _program_clock_reference_extension;//9bits
    //if (OPCR_flag == '1')
    unsigned long _original_program_clock_reference_base;//33 bits
    unsigned short _original_program_clock_reference_extension;//9bits
    //if (splicing_point_flag == '1')
    unsigned char _splice_countdown;
    //if (transport_private_data_flag == '1') 
    unsigned char _transport_private_data_length;
    unsigned char _private_data_byte[256];
    //if (adaptation_field_extension_flag == '1')
    unsigned char _adaptation_field_extension_length;
    unsigned char _ltw_flag;
    unsigned char _piecewise_rate_flag;
    unsigned char _seamless_splice_flag;
    unsigned char _reserved0;
    //if (ltw_flag == '1')
    unsigned short _ltw_valid_flag:1;
    unsigned short _ltw_offset:15;
    //if (piecewise_rate_flag == '1')
    unsigned int _piecewise_rate;//22bits
    //if (seamless_splice_flag == '1')
    unsigned char _splice_type;//4bits
    unsigned char _DTS_next_AU1;//3bits
    unsigned char _marker_bit1;//1bit
    unsigned short _DTS_next_AU2;//15bit
    unsigned char _marker_bit2;//1bit
    unsigned short _DTS_next_AU3;//15bit
};

class ts_header {
public:
    ts_header(){}
    ~ts_header(){}

public:
    unsigned char _sync_byte;

    unsigned short _transport_error_indicator:1;
    unsigned short _payload_unit_start_indicator:1;
    unsigned short _transport_priority:1;
    unsigned short _PID:13;

    unsigned char _transport_scrambling_control:2;
    unsigned char _adaptation_field_control:2;
    unsigned char _continuity_counter:4;

    adaptation_field _adaptation_field_info;
};

typedef struct {
    unsigned short _program_number;
    unsigned short _pid;
    unsigned short _network_id;
} PID_INFO;

class pat_info {
public:
    pat_info(){};
    ~pat_info(){};

public:
    unsigned char _table_id;

    unsigned short _section_syntax_indicator:1;
    unsigned short _reserved0:1;
    unsigned short _reserved1:2;
    unsigned short _section_length:12;

    unsigned short _transport_stream_id;

    unsigned char _reserved3:2;
    unsigned char _version_number:5;
    unsigned char _current_next_indicator:1;

    unsigned char _section_number;
    unsigned char _last_section_number;
    std::vector<PID_INFO> _pid_vec;
};

typedef struct {
    unsigned char  _stream_type;
    unsigned short _reserved1:3;
    unsigned short _elementary_PID:13;
    unsigned short _reserved:4;
    unsigned short _ES_info_length;
    unsigned char  _dscr[4096];
    unsigned int   _crc_32;
} STREAM_PID_INFO;

class pmt_info {
public:
    pmt_info(){};
    ~pmt_info(){};
public:
    unsigned char _table_id;
    unsigned short _section_syntax_indicator:1;
    unsigned short _reserved1:1;
    unsigned short _reserved2:2;
    unsigned short _section_length:12;
    unsigned short _program_number:16;
    unsigned char  _reserved:2;
    unsigned char  _version_number:5;
    unsigned char  _current_next_indicator:5;
    unsigned char  _section_number;
    unsigned char  _last_section_number;
    unsigned short _reserved3:3;
    unsigned short _PCR_PID:13;
    unsigned short _reserved4:4;
    unsigned short _program_info_length:12;
    unsigned char  _dscr[4096];

    std::unordered_map<unsigned short, unsigned char> _pid2steamtype;
    std::vector<STREAM_PID_INFO> _stream_pid_vec;
};

inline int get_media_info_by_streamtype(uint8_t streamtype, MEDIA_PKT_TYPE& media_type, MEDIA_CODEC_TYPE& codec_type) {
    int ret = 0;

    switch (streamtype) {
        case STREAM_TYPE_AUDIO_AAC:
        {
            media_type = MEDIA_AUDIO_TYPE;
            codec_type = MEDIA_CODEC_AAC;
            ret = 0;
            break;
        }
        case STREAM_TYPE_VIDEO_H264:
        {
            media_type = MEDIA_VIDEO_TYPE;
            codec_type = MEDIA_CODEC_H264;
            ret = 0;
            break;
        }
        case STREAM_TYPE_VIDEO_HEVC:
        {
            media_type = MEDIA_VIDEO_TYPE;
            codec_type = MEDIA_CODEC_H265;
            ret = 0;
            break;
        }
        case STREAM_TYPE_VIDEO_MPEG1:
        case STREAM_TYPE_VIDEO_MPEG2:
        case STREAM_TYPE_AUDIO_MPEG1:
        case STREAM_TYPE_AUDIO_MPEG2:
        case STREAM_TYPE_PRIVATE_SECTION:
        case STREAM_TYPE_PRIVATE_DATA:
        case STREAM_TYPE_AUDIO_AAC_LATM:
        case STREAM_TYPE_VIDEO_MPEG4:
        case STREAM_TYPE_METADATA:
        case STREAM_TYPE_VIDEO_CAVS:
        case STREAM_TYPE_VIDEO_VC1:
        case STREAM_TYPE_VIDEO_DIRAC:
        case STREAM_TYPE_AUDIO_AC3:
        case STREAM_TYPE_AUDIO_DTS:
        case STREAM_TYPE_AUDIO_TRUEHD:
        case STREAM_TYPE_AUDIO_EAC3:
        {
            ret = -1;
            break;
        }
        default:
            ret = -1;
    }
    return ret;
}

#endif