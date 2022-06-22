#include "base_player.hpp"
#include "format/h264_header.hpp"
#include "format/audio_pub.hpp"

base_player::base_player()
{
    filter_ = new stream_filter();
}

base_player::~base_player()
{
    if (dec_) {
        delete dec_;
        dec_ = nullptr;
    }

    if (filter_) {
        delete filter_;
        filter_ = nullptr;
    }

    if (player_) {
        delete player_;
        player_ = nullptr;
    }
}

void base_player::on_video_packet_callback(MEDIA_PACKET_PTR pkt_ptr, bool flv_flag) {
    if (!dec_) {
        dec_ = new decode_oper(this);
    }

    if (flv_flag) {
        pkt_ptr->buffer_ptr_->consume_data(2);
    
        uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->data();
        uint32_t cts = 0;
        
        cts += ((uint32_t)*p) << 16 ;
        p++;
        cts += ((uint32_t)*p) << 8 ;
        p++;
        cts += *p;
    
        pkt_ptr->pts_ = pkt_ptr->dts_ + cts;
        pkt_ptr->buffer_ptr_->consume_data(3);
    }

    if (pkt_ptr->is_seq_hdr_) {
        handle_video_extra_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                            pkt_ptr->buffer_ptr_->data_len());
        return;
    }

    std::vector<std::shared_ptr<data_buffer>> nalus;
    bool ret = annexb_to_nalus((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                            pkt_ptr->buffer_ptr_->data_len(), nalus);
    if (!ret || nalus.empty()) {
        return;
    }
    for (std::shared_ptr<data_buffer> nalu : nalus) {
        MEDIA_PACKET_PTR nalu_pkt_ptr = std::make_shared<MEDIA_PACKET>(nalu->data_len() + 1024);
        nalu_pkt_ptr->copy_properties(pkt_ptr);
        nalu_pkt_ptr->buffer_ptr_->append_data(nalu->data(), nalu->data_len());
        uint8_t nalu_type = ((uint8_t*)nalu->data())[4];

        log_debugf("nalu type:0x%02x, nalu len:%lu", GET_H264_NALU_TYPE(nalu_type), nalu->data_len());
        nalu_pkt_ptr->is_key_frame_ = H264_IS_KEYFRAME(nalu_type);
        nalu_pkt_ptr->is_seq_hdr_   = H264_IS_SEQ(nalu_type);
        if (H264_IS_SPS(nalu_type)) {
            updata_sps((uint8_t*)nalu_pkt_ptr->buffer_ptr_->data(),
                    nalu_pkt_ptr->buffer_ptr_->data_len());
            continue;
        }
        if (H264_IS_PPS(nalu_type)) {
            updata_pps((uint8_t*)nalu_pkt_ptr->buffer_ptr_->data(),
                    nalu_pkt_ptr->buffer_ptr_->data_len());
            continue;
        }

        AVPacket* pkt = get_avpacket(nalu_pkt_ptr, 0);
        
        if (nalu_pkt_ptr->is_key_frame_ && !sps_.empty() && !pps_.empty()) {
            if (av_grow_packet(pkt, sizeof(H264_START_CODE) * 2 + sps_.size() + pps_.size()) < 0) {
                av_packet_free(&pkt);
                log_errorf("packet grow error");
                return;
            }
            size_t data_len = 0;

            memcpy(pkt->data + data_len, H264_START_CODE, sizeof(H264_START_CODE));
            data_len += sizeof(H264_START_CODE);
            memcpy(pkt->data + data_len, sps_.data(), sps_.size());
            data_len += sps_.size();

            memcpy(pkt->data + data_len, H264_START_CODE, sizeof(H264_START_CODE));
            data_len += sizeof(H264_START_CODE);
            memcpy(pkt->data + data_len, pps_.data(), pps_.size());
            data_len += pps_.size();

            memcpy(pkt->data + data_len, nalu_pkt_ptr->buffer_ptr_->data(), nalu_pkt_ptr->buffer_ptr_->data_len());
        }
    
        if (pkt && dec_) {
            dec_->input_avpacket(pkt, pkt_ptr->codec_type_, false);
        }
    }
    return;
}

void base_player::on_audio_packet_callback(MEDIA_PACKET_PTR pkt_ptr, bool flv_flag) {
    if (!dec_) {
        dec_ = new decode_oper(this);
    }
    if (flv_flag) {
        pkt_ptr->buffer_ptr_->consume_data(2);
    }

    if (pkt_ptr->is_seq_hdr_) {
        bool ok = get_audioinfo_by_asc((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                                    pkt_ptr->buffer_ptr_->data_len(),
                                    aac_type_, sample_rate_, channel_);
        if (!ok) {
            log_errorf("get audio info error");
            return;
        }
        log_infof("get audio info from aac asc, aac type:%d, sample rate:%d, channel:%d",
                aac_type_, sample_rate_, channel_);
        return;
    }
    if ((aac_type_ == 0) || (sample_rate_ == 0) || (channel_ == 0)) {
        log_infof("audio config is not ready");
        return;
    }

    uint8_t adts_data[32];
    int adts_len = make_adts(adts_data, aac_type_,
            sample_rate_, channel_, pkt_ptr->buffer_ptr_->data_len());
    assert(adts_len == 7);

    pkt_ptr->buffer_ptr_->consume_data(0 - adts_len);
    uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->data();
    memcpy(p, adts_data, adts_len);

    AVPacket* pkt = get_avpacket(pkt_ptr, 0);

    if (pkt && dec_) {
        dec_->input_avpacket(pkt, pkt_ptr->codec_type_, false);
    }
    av_packet_free(&pkt);
}

void base_player::handle_video_extra_data(uint8_t* extra_data, size_t extra_len) {
    uint8_t sps[1024];
    uint8_t pps[1024];
    size_t sps_len = 0;
    size_t pps_len = 0;
    int ret = 0;

    ret = get_sps_pps_from_extradata(pps, pps_len,
                        sps, sps_len,
                        extra_data, extra_len);
    if ((ret < 0) || (pps_len == 0) || (sps_len == 0)) {
        log_errorf("receive wrong h264 extra data");
        log_info_data(extra_data, extra_len, "wrong video h264 seq data");
        return;
    }
    log_info_data(extra_data, extra_len, "video extra data");
    log_info_data(sps, sps_len, "live user udpate sps");
    log_info_data(pps, pps_len, "live user udpate pps");

    updata_pps(pps, pps_len);
    updata_sps(sps, sps_len);

    return;
}

void base_player::updata_sps(uint8_t* sps, size_t sps_len) {
    if (sps_.empty()) {
        sps_.reserve(sps_len);
    } else {
        sps_.resize(sps_len);
        sps_.clear();
    }

    sps_.insert(sps_.end(), (uint8_t*)sps, (uint8_t*)sps + sps_len);
    return;
}

void base_player::updata_pps(uint8_t* pps, size_t pps_len) {
    if (pps_.empty()) {
        pps_.reserve(pps_len);
    } else {
        pps_.resize(pps_len);
        pps_.clear();
    }

    pps_.insert(pps_.end(), (uint8_t*)pps, (uint8_t*)pps + pps_len);
    return;
}

void base_player::handle_video_avframe(AVFrame* frame) {
    log_infof("+++ get video avframe stream, format:%s, %dx%d, pts:%ld",
            av_get_pix_fmt_name((enum AVPixelFormat)frame->format),
            frame->width, frame->height, frame->pts);
    player_->insertVideoFrame(frame);
    return;
}

void base_player::handle_audio_avframe(AVFrame* frame) {
    int ret;
    AVFrame* filtered_frame = nullptr;
    char dscr[512] = {0};
    char layout_name[256];

    av_get_channel_layout_string(layout_name, sizeof(layout_name),
                                frame->channels, frame->channel_layout);
    snprintf(dscr, sizeof(dscr),
        "aformat=sample_fmts=%s:channel_layouts=%s:sample_rates=%d",
        av_get_sample_fmt_name(AV_SAMPLE_FMT_S16), layout_name, frame->sample_rate);
    filter_->set_outputlink_request_samples(AUDIO_STREAM_ID, frame->nb_samples);
    filter_->set_inputlink_request_samples(AUDIO_STREAM_ID, frame->nb_samples);
    filter_->init_audio_filter((enum AVSampleFormat)frame->format, frame->sample_rate,
                            frame->channels, (uint64_t)frame->channel_layout,
                            AV_SAMPLE_FMT_S16, frame->sample_rate,
                            frame->channels, (uint64_t)frame->channel_layout,
                            frame->nb_samples, dscr);
    
    AVFrame* input_frame_p = frame;
    while (true) {
        ret = filter_->filter_write_frame(input_frame_p, AUDIO_STREAM_ID, &filtered_frame);
        if ((ret  < 0) || (filtered_frame == nullptr)) {
            if (ret == AVERROR_EOF) {
                filter_->release_audio();
            }
            return;
        }
        //AVRational filter_tb = filter_->get_outputlink_timebase(AUDIO_STREAM_ID);

        input_frame_p = nullptr;//set nullptr for only reading in next time
        //filtered_frame->pts = av_rescale_q(filtered_frame->pts, filter_tb, standard_tb);

        //display audio frame
        log_infof("--- get audio avframe stream, format:%s, nb_samples:%d, channels:%d, pts:%ld",
            av_get_sample_fmt_name((enum AVSampleFormat)filtered_frame->format),
            filtered_frame->nb_samples, filtered_frame->channels, filtered_frame->pts);

        player_->insertAudioFrame(filtered_frame);
        av_frame_free(&filtered_frame);
    }

    return;
}

void base_player::on_avframe_callback(int stream_index, AVFrame* frame) {
    if (stream_index == VIDEO_STREAM_ID) {
        handle_video_avframe(frame);
    } else if (stream_index == AUDIO_STREAM_ID) {
        handle_audio_avframe(frame);
    } else {
        CMS_ASSERT(0, "unkown streamindex:%d", stream_index);
    }
}
