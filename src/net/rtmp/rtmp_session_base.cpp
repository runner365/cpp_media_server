#include "rtmp_session_base.hpp"
#include "rtmp_pub.hpp"
#include "flv_pub.hpp"

const char* server_phase_desc_list[] = {"handshake c2 phase",
                                        "connect phase",
                                        "create stream phase",
                                        "create publish/play phase",
                                        "media handle phase"};

const char* client_phase_desc_list[] = {"c0c1 phase",
                                        "connect phase",
                                        "connect response phase",
                                        "create stream phase",
                                        "create stream response phase",
                                        "create play phase",
                                        "create publish phase",
                                        "media handle phase"};

const char* get_server_phase_desc(RTMP_SERVER_SESSION_PHASE phase) {
    return server_phase_desc_list[phase];
}

const char* get_client_phase_desc(RTMP_CLIENT_SESSION_PHASE phase) {
    return client_phase_desc_list[phase];
}

rtmp_session_base::rtmp_session_base():recv_buffer_(50*1024)
{
}

rtmp_session_base::~rtmp_session_base()
{
}

int rtmp_session_base::read_fmt_csid() {
    uint8_t* p = nullptr;

    if (recv_buffer_.require(1)) {
        p = (uint8_t*)recv_buffer_.data();
        fmt_  = ((*p) >> 6) & 0x3;
        csid_ = (*p) & 0x3f;
        recv_buffer_.consume_data(1);
    } else {
        return RTMP_NEED_READ_MORE;
    }

    if (csid_ == 0) {
        if (recv_buffer_.require(1)) {//need 1 byte
            p = (uint8_t*)recv_buffer_.data();
            recv_buffer_.consume_data(1);
            csid_ = 64 + *p;
        } else {
            return RTMP_NEED_READ_MORE;
        }
    } else if (csid_ == 1) {
        if (recv_buffer_.require(2)) {//need 2 bytes
            p = (uint8_t*)recv_buffer_.data();
            recv_buffer_.consume_data(2);
            csid_ = 64;
            csid_ += *p++;
            csid_ += *p;
        } else {
            return RTMP_NEED_READ_MORE;
        }
    } else {
        //normal csid: 2~64
    }

    return RTMP_OK;
}

void rtmp_session_base::set_chunk_size(uint32_t chunk_size) {
    chunk_size_ = chunk_size;
}

uint32_t rtmp_session_base::get_chunk_size() {
    return chunk_size_;
}

bool rtmp_session_base::is_publish() {
    return req_.publish_flag_;
}

const char* rtmp_session_base::is_publish_desc() {
    return req_.publish_flag_ ? "publish" : "play";
}


MEDIA_PACKET_PTR rtmp_session_base::get_media_packet(CHUNK_STREAM_PTR cs_ptr) {
    MEDIA_PACKET_PTR pkt_ptr;
    uint32_t ts_delta = 0;

    if (cs_ptr->chunk_data_ptr_->data_len() < 2) {
        log_errorf("rtmp chunk media size:%lu is too small", cs_ptr->chunk_data_ptr_->data_len());
        return pkt_ptr;
    }
    uint8_t* p = (uint8_t*)cs_ptr->chunk_data_ptr_->data();

    pkt_ptr = std::make_shared<MEDIA_PACKET>();

    pkt_ptr->typeid_   = cs_ptr->type_id_;
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;

    if (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_VIDEO) {
        uint8_t codec = p[0] & 0x0f;

        pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
        if (codec == FLV_VIDEO_H264_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        } else if (codec == FLV_VIDEO_H265_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_H265;
        } else if (codec == FLV_VIDEO_VP8_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_VP8;
        } else if (codec == FLV_VIDEO_VP9_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_VP9;
        }  else {
            log_errorf("does not support video codec typeid:%d, 0x%02x", cs_ptr->type_id_, p[0]);
            //assert(0);
            return pkt_ptr;
        }

        uint8_t frame_type = p[0] & 0xf0;
        uint8_t nalu_type = p[1];
        if (frame_type == FLV_VIDEO_KEY_FLAG) {
            if (nalu_type == FLV_VIDEO_AVC_SEQHDR) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (nalu_type == FLV_VIDEO_AVC_NALU) {
                pkt_ptr->is_key_frame_ = true;
            } else {
                log_errorf("input flv video error, 0x%02x 0x%02x", p[0], p[1]);
                return pkt_ptr;
            }
        } else if (frame_type == FLV_VIDEO_INTER_FLAG) {
            pkt_ptr->is_key_frame_ = false;
        }

        ts_delta = read_3bytes(p + 2);
    } else if (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_AUDIO) {
        pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;
        uint8_t frame_type = p[0] & 0xf0;

        if (frame_type == FLV_AUDIO_AAC_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_AAC;
            if(p[1] == 0x00) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                pkt_ptr->is_key_frame_ = false;
                pkt_ptr->is_seq_hdr_   = false;
            }
        } else if (frame_type == FLV_AUDIO_OPUS_CODEC) {
            pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
            if(p[1] == 0x00) {
                pkt_ptr->is_seq_hdr_ = true;
            } else if (p[1] == 0x01) {
                pkt_ptr->is_key_frame_ = false;
                pkt_ptr->is_seq_hdr_   = false;
            }
        } else {
            log_errorf("does not support audio codec typeid:%d, 0x%02x", cs_ptr->type_id_, p[0]);
            assert(0);
            return pkt_ptr;
        }
    } else if ((cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA0) || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA3)) {
        pkt_ptr->av_type_ = MEDIA_METADATA_TYPE;
    } else {
        log_warnf("rtmp input unkown media type:%d", cs_ptr->type_id_);
        assert(0);
        return pkt_ptr;
    }

    if (ts_delta > 500) {
        log_warnf("video ts_delta error:%u", ts_delta);
    }
    pkt_ptr->dts_  = cs_ptr->timestamp32_;
    pkt_ptr->pts_  = pkt_ptr->dts_ + ts_delta;
    pkt_ptr->buffer_ptr_->reset();
    pkt_ptr->buffer_ptr_->append_data(cs_ptr->chunk_data_ptr_->data(), cs_ptr->chunk_data_ptr_->data_len());

    pkt_ptr->app_        = req_.app_;
    pkt_ptr->streamname_ = req_.stream_name_;
    pkt_ptr->key_        = req_.key_;
    pkt_ptr->streamid_   = cs_ptr->msg_stream_id_;

    return pkt_ptr;
}

int rtmp_session_base::read_chunk_stream(CHUNK_STREAM_PTR& cs_ptr) {
    int ret = -1;

    if (!fmt_ready_) {
        ret = read_fmt_csid();
        if (ret != 0) {
            return ret;
        }
        fmt_ready_ = true;
    }

    std::unordered_map<uint8_t, CHUNK_STREAM_PTR>::iterator iter = cs_map_.find(csid_);
    if (iter == cs_map_.end()) {
        cs_ptr = std::make_shared<chunk_stream>(this, fmt_, csid_, chunk_size_);
        cs_map_.insert(std::make_pair(csid_, cs_ptr));
    } else {
        cs_ptr =iter->second;
        cs_ptr->chunk_size_ = chunk_size_;
    }

    ret = cs_ptr->read_message_header(fmt_, csid_);
    if ((ret < RTMP_OK) || (ret == RTMP_NEED_READ_MORE)) {
        return ret;
    } else {
        ret = cs_ptr->read_message_payload();
        //cs_ptr->dump_header();
        if (ret == RTMP_OK) {
            fmt_ready_ = false;
            //cs_ptr->dump_payload();
            return ret;
        }
    }

    return ret;
}