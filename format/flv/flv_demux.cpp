#include "flv_demux.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "flv_pub.hpp"

flv_demuxer::flv_demuxer(av_format_callback* cb):callback_(cb)
{
}

flv_demuxer::~flv_demuxer()
{
}

int flv_demuxer::handle_packet() {
    uint8_t* p;
    uint32_t ts_delta = 0;

    if (!flv_header_ready_) {
        if (!buffer_.require(FLV_HEADER_LEN + FLV_TAG_PRE_SIZE)) {
            return FLV_RET_NEED_MORE;
        }

        p = (uint8_t*)buffer_.data();

        if ((p[0] != 'F') || (p[1] != 'L') || (p[2] != 'V')) {
            return -1;
        }
        if ((p[4] & 0x01) == 0x01) {
            has_video_ = true;
        }
        if ((p[4] & 0x04) == 0x04) {
            has_audio_ = true;
        }

        if ((p[5] != 0) || (p[6] != 0) || (p[7] != 0) || (p[8] != 9)) {
            return -1;
        }
        buffer_.consume_data(FLV_HEADER_LEN + FLV_TAG_PRE_SIZE);
        flv_header_ready_ = true;
    }

    if (!tag_header_ready_) {
        if (!buffer_.require(FLV_TAG_HEADER_LEN)) {
            return FLV_RET_NEED_MORE;
        }
        p = (uint8_t*)buffer_.data();
        tag_type_ = p[0];
        p++;
        tag_data_size_ = read_3bytes(p);
        p += 3;
        tag_timestamp_ = read_3bytes(p);
        p += 3;
        tag_timestamp_ |= ((uint32_t)p[0]) << 24;

        tag_header_ready_ = true;
        buffer_.consume_data(FLV_TAG_HEADER_LEN);
    }

    if (!buffer_.require(tag_data_size_ + FLV_TAG_PRE_SIZE)) {
        return FLV_RET_NEED_MORE;
    }
    p = (uint8_t*)buffer_.data();

    MEDIA_PACKET_PTR output_pkt_ptr = std::make_shared<MEDIA_PACKET>();
    bool is_ready = true;
    int header_len = 0;

    output_pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
    output_pkt_ptr->key_ = key_;
    if (tag_type_ == FLV_TAG_AUDIO) {
        header_len = 2;
        output_pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;

        if ((p[0] & 0xf0) == FLV_AUDIO_AAC_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_AAC;
        } else if ((p[0] & 0xf0) == FLV_AUDIO_OPUS_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
        } else {
            is_ready = false;
            log_errorf("does not suport audio codec type:0x%02x", p[0]);
            return -1;
        }
        if (p[1] == 0x00) {
            output_pkt_ptr->is_seq_hdr_ = true;
        } else {
            output_pkt_ptr->is_key_frame_ = false;
            output_pkt_ptr->is_seq_hdr_   = false;
        }
    } else if (tag_type_ == FLV_TAG_VIDEO) {
        header_len = 2 + 3;
        output_pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
        if ((p[0]&0x0f) == FLV_VIDEO_H264_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        } else if ((p[0]&0x0f) == FLV_VIDEO_H265_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_H265;
        } else if ((p[0]&0x0f) == FLV_VIDEO_VP8_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_VP8;
        } else if ((p[0]&0x0f) == FLV_VIDEO_VP9_CODEC) {
            output_pkt_ptr->codec_type_ = MEDIA_CODEC_VP9;
        } else {
            is_ready = false;
            log_errorf("does not support codec type:0x%02x.", p[0]);
            return -1;
        }

        if ((p[0] & 0xf0) == FLV_VIDEO_KEY_FLAG) {
            if (p[1] == 0x00) {
                output_pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                output_pkt_ptr->is_seq_hdr_   = false;
                output_pkt_ptr->is_key_frame_ = true;
            } else {
                is_ready = false;
                log_infof("unkown key byte:0x%02x.", p[1]);
                return -1;
            }
        }
        ts_delta = read_3bytes(p + 2);
    } else {
        is_ready = false;
        buffer_.consume_data(tag_data_size_ + FLV_TAG_PRE_SIZE);
        tag_header_ready_ = false;
        log_errorf("does not suport tag type:0x%02x", tag_type_);
        return 0;
    }

    int ret = 0;
    if (is_ready && ((int)tag_data_size_ > header_len)) {
        output_pkt_ptr->dts_ = tag_timestamp_;
        output_pkt_ptr->pts_ = tag_timestamp_ + ts_delta;
        output_pkt_ptr->buffer_ptr_->append_data((char*)p + header_len, tag_data_size_ - header_len);
        ret = callback_->output_packet(output_pkt_ptr);
    }

    buffer_.consume_data(tag_data_size_ + FLV_TAG_PRE_SIZE);
    tag_header_ready_ = false;
    return ret;
}

int flv_demuxer::input_packet(MEDIA_PACKET_PTR pkt_ptr) {
    buffer_.append_data(pkt_ptr->buffer_ptr_->data(), pkt_ptr->buffer_ptr_->data_len());
    if (key_.empty() && !pkt_ptr->key_.empty()) {
        key_ = pkt_ptr->key_;
    }
    int ret = 0;
    do {
        ret = handle_packet();
    } while (ret == 0);
    
    return ret;
}
