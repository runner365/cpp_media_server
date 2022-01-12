#include "flv_mux.hpp"
#include "flv_pub.hpp"
#include "logger.hpp"

flv_muxer::flv_muxer(bool has_video, bool has_audio, av_format_callback* cb):has_video_(has_video)
    , has_audio_(has_audio)
    , cb_(cb)
{
}

flv_muxer::~flv_muxer()
{
}

int flv_muxer::input_packet(MEDIA_PACKET_PTR pkt_ptr) {
    MEDIA_PACKET_PTR output_pkt_ptr = std::make_shared<MEDIA_PACKET>();

    if (!header_ready_) {
        header_ready_ = true;
        mux_flv_header(output_pkt_ptr);
    }

    size_t data_size = pkt_ptr->buffer_ptr_->data_len();
    size_t media_size = 0;
    uint8_t header_data[16];

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        //11 bytes header | 0x17 00 | 00 00 00 | data[...] | pre tag size
        media_size = 2 + 3 + data_size;
        header_data[0] = FLV_TAG_VIDEO;

    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        //11 bytes header | 0xaf 00 | data[...] | pre tag size
        media_size = 2 + data_size;

        header_data[0] = FLV_TAG_AUDIO;
    } else {
        log_warnf("flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        return 0;
    }

    //Set DataSize(24)
    header_data[1] = (media_size >> 16) & 0xff;
    header_data[2] = (media_size >> 8) & 0xff;
    header_data[3] = media_size & 0xff;

    //Set Timestamp(24)|TimestampExtended(8)
    uint32_t timestamp_base = (uint32_t)(pkt_ptr->dts_ & 0xffffff);
    uint8_t timestamp_ext   = ((uint32_t)(pkt_ptr->dts_ & 0xffffff) >> 24) & 0xff;

    header_data[4] = (timestamp_base >> 16) & 0xff;
    header_data[5] = (timestamp_base >> 8) & 0xff;
    header_data[6] = timestamp_base & 0xff;
    header_data[7] = timestamp_ext & 0xff;

    //Set StreamID(24) as 1
    header_data[8]  = 0;
    header_data[9]  = 0;
    header_data[10] = 1;

    size_t header_size = 0;
    size_t pre_size = 0;

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        //set media header
        header_size = 16;
        pre_size = 11 + 5 + pkt_ptr->buffer_ptr_->data_len();
        if (pkt_ptr->is_seq_hdr_) {
            header_data[11] = FLV_VIDEO_KEY_FLAG;
            header_data[12] = FLV_VIDEO_AVC_SEQHDR;
        } else if (pkt_ptr->is_key_frame_) {
            header_data[11] = FLV_VIDEO_KEY_FLAG;
            header_data[12] = FLV_VIDEO_AVC_NALU;
        } else {
            header_data[11] = FLV_VIDEO_INTER_FLAG;
            header_data[12] = FLV_VIDEO_AVC_NALU;
        }
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            header_data[11] |= FLV_VIDEO_H264_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
            header_data[11] |= FLV_VIDEO_H265_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP8) {
            header_data[11] |= FLV_VIDEO_VP8_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP9) {
            header_data[11] |= FLV_VIDEO_VP9_CODEC;
        } else {
            log_errorf("flv mux unsuport video codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }
        uint32_t ts_delta = (pkt_ptr->pts_ > pkt_ptr->dts_) ? (pkt_ptr->pts_ - pkt_ptr->dts_) : 0;
        header_data[13] = (ts_delta >> 16) & 0xff;
        header_data[14] = (ts_delta >> 8) & 0xff;
        header_data[15] = ts_delta & 0xff;
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        header_size = 13;
        pre_size = 11 + 2 + pkt_ptr->buffer_ptr_->data_len();

        header_data[11] = 0x0f;
        if (pkt_ptr->is_seq_hdr_) {
            header_data[12] = 0x00;
        } else {
            header_data[12] = 0x01;
        }
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            header_data[11] |= FLV_AUDIO_AAC_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            header_data[11] |= FLV_AUDIO_OPUS_CODEC;
        } else {
            log_errorf("flv mux unsuport audio codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }
    } else {
        log_warnf("flv mux does not suport media type:%d", (int)pkt_ptr->av_type_);
        return 0;
    }

    output_pkt_ptr->buffer_ptr_->append_data((char*)header_data, header_size);
    output_pkt_ptr->buffer_ptr_->append_data(pkt_ptr->buffer_ptr_->data(), pkt_ptr->buffer_ptr_->data_len());

    uint8_t pre_tag_size_p[4];
    pre_tag_size_p[0] = (pre_size >> 24) & 0xff;
    pre_tag_size_p[1] = (pre_size >> 16) & 0xff;
    pre_tag_size_p[2] = (pre_size >> 8) & 0xff;
    pre_tag_size_p[3] = pre_size & 0xff;
    output_pkt_ptr->buffer_ptr_->append_data((char*)pre_tag_size_p, sizeof(pre_tag_size_p));
    output_pkt_ptr->av_type_    = pkt_ptr->av_type_;
    output_pkt_ptr->codec_type_ = pkt_ptr->codec_type_;
    output_pkt_ptr->fmt_type_   = MEDIA_FORMAT_FLV;
    output_pkt_ptr->dts_  = pkt_ptr->dts_;
    output_pkt_ptr->pts_  = pkt_ptr->pts_;

    cb_->output_packet(output_pkt_ptr);
    return 0;
}

int flv_muxer::mux_flv_header(MEDIA_PACKET_PTR pkt_ptr) {
    uint8_t flag = 0;
    /*|'F'(8)|'L'(8)|'V'(8)|version(8)|TypeFlagsReserved(5)|TypeFlagsAudio(1)|TypeFlagsReserved(1)|TypeFlagsVideo(1)|DataOffset(32)|PreviousTagSize(32)|*/

    if (has_video_) {
        flag |= 0x01;
    }
    if (has_audio_) {
        flag |= 0x04;
    }
    uint8_t header_data[13] = {0x46, 0x4c, 0x56, 0x01, flag, 0x00, 0x00, 0x00, 0x09, 0, 0, 0, 0};

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    pkt_ptr->buffer_ptr_->append_data((char*)header_data, sizeof(header_data));

    return 0;
}

int flv_muxer::add_flv_media_header(MEDIA_PACKET_PTR pkt_ptr) {
    uint8_t* p;

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        p = (uint8_t*)pkt_ptr->buffer_ptr_->consume_data(-2);

        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            p[0] = FLV_AUDIO_AAC_CODEC | 0x0f;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            p[0] = FLV_AUDIO_OPUS_CODEC | 0x0f;
        } else {
            log_errorf("unsuport audio codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }

        if (pkt_ptr->is_seq_hdr_) {
            p[1] = 0x00;
        } else {
            p[1] = 0x01;
        }
    } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        p = (uint8_t*)pkt_ptr->buffer_ptr_->consume_data(-5);

        p[0] = 0;
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            p[0] |= FLV_VIDEO_H264_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_H265) {
            p[0] |= FLV_VIDEO_H265_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP8) {
            p[0] |= FLV_VIDEO_VP8_CODEC;
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_VP9) {
            p[0] |= FLV_VIDEO_VP9_CODEC;
        }  else {
            log_errorf("unsuport audio codec type:%d", pkt_ptr->codec_type_);
            return -1;
        }

        if (pkt_ptr->is_key_frame_ || pkt_ptr->is_seq_hdr_) {
            p[0] |= FLV_VIDEO_KEY_FLAG;
            if (pkt_ptr->is_seq_hdr_) {
                p[1] = 0x00;
            } else {
                p[1] = 0x01;
            }
        } else {
            p[0] |= FLV_VIDEO_INTER_FLAG;
            p[1] = 0x01;
        }
        uint32_t ts_delta = (pkt_ptr->pts_ > pkt_ptr->dts_) ? (pkt_ptr->pts_ - pkt_ptr->dts_) : 0;
        p[2] = (ts_delta >> 16) & 0xff;
        p[3] = (ts_delta >> 8) & 0xff;
        p[4] = ts_delta & 0xff;
    }

    return 0;
}