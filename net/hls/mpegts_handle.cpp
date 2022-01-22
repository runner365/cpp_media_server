#include "mpegts_handle.hpp"
#include "media_packet.hpp"
#include "logger.hpp"
#include "format/audio_pub.hpp"
#include "format/h264_header.hpp"
#include "utils/timeex.hpp"
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static const uint8_t H264_AUD_DATA[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xff};

mpegts_handle::mpegts_handle(const std::string& app, const std::string& streamname,
                    const std::string& path, bool rec_enable):rec_enable_(rec_enable)
                        , path_(path)
                        , app_(app)
                        , streamname_(streamname)
                        , muxer_(this)
{
    std::stringstream cmd;
    std::string prefix_path;

    path_ = path;
    if (path.substr(path.size()-1) == "/") {
        path_ = path.substr(0, path.size()-1);
    }
    path_ += "/";
    path_ += app;
    prefix_path = path_;
    path_ += "/";
    path_ += streamname;

    if (rec_enable_) {
        cmd << "mkdir -p " << path_;
        system(cmd.str().c_str());
    }

    live_m3u8_filename_ = prefix_path + "/";
    live_m3u8_filename_ += streamname;
    live_m3u8_filename_ += ".m3u8";

    rec_m3u8_filename_ = prefix_path + "/";
    rec_m3u8_filename_ += streamname;
    rec_m3u8_filename_ += "_record.m3u8";

    log_infof("mpegts_handle construct path:%s", path_.c_str());
}

mpegts_handle::~mpegts_handle()
{
    log_infof("mpegts_handle destruct app:%s, streamname:%s",
            app_.c_str(), streamname_.c_str());
}

void mpegts_handle::set_ts_list_max(size_t list_max) {
    if (list_max < 3) {
        ts_list_max_ = 3;
        return;
    }

    if (list_max > 6) {
        ts_list_max_ = 6;
        return;
    }
    ts_list_max_ = list_max;
    return;
}

void mpegts_handle::set_ts_duration(size_t duration) {
    if (duration > 30) {
        duration = 30;
    }
    if (duration < 2) {
        duration = 2;
    }
    mpegts_duration_ = duration;
    return;
}

void mpegts_handle::handle_media_packet(MEDIA_PACKET_PTR pkt_ptr) {
    if (!ready_ && (!video_ready_ || !audio_ready_) && (wait_queue_.size() < 30)) {
        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            video_ready_ = true;
            muxer_.set_video_codec(pkt_ptr->codec_type_);
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            audio_ready_ = true;
            muxer_.set_audio_codec(pkt_ptr->codec_type_);
        }
        wait_queue_.push(pkt_ptr);
        return;
    }

    ready_ = true;
    
    while (wait_queue_.size() > 0) {
        auto current_pkt_ptr = wait_queue_.front();
        wait_queue_.pop();
        handle_packet(current_pkt_ptr);
    }
    
    keep_alive();

    handle_packet(pkt_ptr);
    return;
}

int mpegts_handle::output_packet(MEDIA_PACKET_PTR pkt_ptr) {
    std::string filename;
    if (rec_enable_) {
        filename = ts_filename_;
    }
    ts_info_ptr_->write(pkt_ptr->dts_, (uint8_t*)pkt_ptr->buffer_ptr_->data(),
                    pkt_ptr->buffer_ptr_->data_len(), filename);
    return 0;
}

bool mpegts_handle::gen_live_m3u8(std::string& m3u8_header) {
    std::stringstream header_ss;
    std::stringstream item_list_ss;
    int64_t max_duration = 0;

    if (ts_list_.size() < ts_list_max_) {
        log_infof("ts list size:%lu, ts_max:%d", ts_list_.size(), ts_list_max_);
        return false;
    }

    int index = 0;
    for (auto iter = ts_list_.begin();
        iter != ts_list_.end();
        iter++) {
        if (index++ == 0) {
            continue;
        }
        
        if (iter->get()->duration > max_duration) {
            max_duration = iter->get()->duration;
        }
        item_list_ss << "#EXTINF:" << iter->get()->duration/1000.0 << ",\n";
        item_list_ss << iter->get()->ts_key << "\n";
    }

    header_ss << "#EXTM3U\n";
    header_ss << "#EXT-X-VERSION:3\n";
    header_ss << "#EXT-X-ALLOW-CACHE:NO\n";
    header_ss << "#EXT-X-TARGETDURATION:" << max_duration << "\n";
    header_ss << "#EXT-X-MEDIA-SEQUENCE:" << seq_ << "\n\n";

    m3u8_header = header_ss.str();
    m3u8_header += item_list_ss.str();

    return true;
}

std::shared_ptr<ts_item_info> mpegts_handle::get_mpegts_item(const std::string& ts_name) {
    std::shared_ptr<ts_item_info> ts_ptr;
    for (auto item : ts_list_) {
        size_t pos = item->ts_filename.find(ts_name);
        if (pos == std::string::npos) {
            continue;
        }
        ts_ptr = item;
        break;
    }

    return ts_ptr;
}

void mpegts_handle::write_live_m3u8() {
    std::string live_m3u8;
    bool ok = this->gen_live_m3u8(live_m3u8);
    if (ok) {
        FILE* file_p = fopen(live_m3u8_filename_.c_str(), "w");
        if (file_p) {
            fwrite(live_m3u8.c_str(), live_m3u8.length(), 1, file_p);
            fclose(file_p);
        }
    }
}

void mpegts_handle::write_record_m3u8() {
    if (!ts_info_ptr_) {
        return;
    }

    if (!record_init_) {
        std::stringstream header_ss;
        int64_t duration_max = ts_info_ptr_->duration/1000 + 2;

        record_init_ = true;

        header_ss << "#EXTM3U\n";
        header_ss << "#EXT-X-VERSION:3\n";
        header_ss << "#EXT-X-ALLOW-CACHE:NO\n";
        header_ss << "#EXT-X-TARGETDURATION:" << duration_max << "\n";
        header_ss << "\n";

        FILE* file_p = fopen(rec_m3u8_filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(header_ss.str().c_str(), header_ss.str().length(), 1, file_p);
            fclose(file_p);
        }
    }

    std::stringstream iter_ss;

    iter_ss << "#EXTINF:" << ts_info_ptr_->duration/1000.0 << ",\n";
    iter_ss << ts_info_ptr_->ts_key << "\n";

    FILE* file_p = fopen(rec_m3u8_filename_.c_str(), "ab+");
    if (file_p) {
        fwrite(iter_ss.str().c_str(), iter_ss.str().length(), 1, file_p);
        fclose(file_p);
    }
}

void mpegts_handle::flush() {
    const std::string endlist_str = "#EXT-X-ENDLIST";

    if (!record_init_) {
        return;
    }
    FILE* file_p = fopen(rec_m3u8_filename_.c_str(), "ab+");
    if (file_p) {
        fwrite(endlist_str.c_str(), endlist_str.length(), 1, file_p);
        fclose(file_p);
    }
}

void mpegts_handle::handle_packet(MEDIA_PACKET_PTR pkt_ptr) {
    int64_t now_sec = now_millisec()/1000;
    if (((pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) && (pkt_ptr->is_key_frame_) && ((now_sec - last_ts_) > mpegts_duration_)) || (ts_filename_.empty())) {
        std::stringstream ss;
        last_ts_ = now_sec;
        ss << path_ << "/" << last_ts_ << ".ts";
        ts_filename_ = ss.str();

        audio_first_flag_ = true;
        pat_pmt_flag_ = true;
        seq_++;
        
        if (!ts_info_ptr_) {
            ts_info_ptr_ = std::make_shared<ts_item_info>();
            ts_info_ptr_->reset();
            ts_info_ptr_->set_ts_filename(ts_filename_);
        } else {
            log_infof("ts duration:%ld", ts_info_ptr_->duration);
            ts_list_.push_back(ts_info_ptr_);
            if (ts_list_.size() > ts_list_max_) {
                log_infof("pop mpegts filename:%s, file key:%s",
                    ts_list_.front()->ts_filename.c_str(),
                    ts_list_.front()->ts_key.c_str());
                ts_list_.pop_front();
            }
            write_live_m3u8();
            write_record_m3u8();

            ts_info_ptr_ = std::make_shared<ts_item_info>();
            ts_info_ptr_->reset();
            ts_info_ptr_->set_ts_filename(ts_filename_);
        }
        log_infof("ts_filename_:%s, ts key:%s, seq:%ld",
            ts_filename_.c_str(), ts_info_ptr_->ts_key.c_str(), seq_);
    }

    if (pat_pmt_flag_ || ((pkt_ptr->dts_ - last_patpmt_ts_) > 1000)) {
        pat_pmt_flag_ = false;
        last_patpmt_ts_ = pkt_ptr->dts_;
        muxer_.write_pat();
        muxer_.write_pmt();
    }

    if (pkt_ptr->fmt_type_ == MEDIA_FORMAT_FLV) {
        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            pkt_ptr->buffer_ptr_->consume_data(5);//0x17 00 00 00 00
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            pkt_ptr->buffer_ptr_->consume_data(2);//0xaf 01
        } else {
            log_errorf("not support media type:%d", pkt_ptr->av_type_);
            return;
        }
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_RAW;
    }

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
            handle_video_h264(pkt_ptr);
        } else {
            log_errorf("not support video codec type:%d", pkt_ptr->codec_type_);
        }
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
            handle_audio_aac(pkt_ptr);
        } else if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
            handle_audio_opus(pkt_ptr);
        } else {
            log_errorf("not support audio codec type:%d", pkt_ptr->codec_type_);
        }
    } else {
        //not handle
    }
    return;
}

int mpegts_handle::handle_video_vp8(MEDIA_PACKET_PTR pkt_ptr) {
    muxer_.input_packet(pkt_ptr);
    return 0;
}

int mpegts_handle::handle_video_h264(MEDIA_PACKET_PTR pkt_ptr) {
    if (pkt_ptr->is_seq_hdr_) {
        uint8_t sps[1024];
        size_t sps_len = 0;
        uint8_t pps[1024];
        size_t pps_len = 0;
        uint8_t* data   = (uint8_t*)pkt_ptr->buffer_ptr_->data();
        size_t data_len = pkt_ptr->buffer_ptr_->data_len();

        int ret = get_sps_pps_from_extradata(pps, pps_len, sps, sps_len, 
                            data, data_len);
        if (ret == 0) {
            memcpy(sps_, H264_START_CODE, sizeof(H264_START_CODE));
            memcpy(sps_ + sizeof(H264_START_CODE), sps, sps_len);
            sps_len_ = sizeof(H264_START_CODE) + sps_len;

            memcpy(pps_, H264_START_CODE, sizeof(H264_START_CODE));
            memcpy(pps_ + sizeof(H264_START_CODE), pps, pps_len);
            pps_len_ = sizeof(H264_START_CODE) + pps_len;
        }
        return 0;
    }

    std::vector<std::shared_ptr<data_buffer>> nalus;
    bool ret = annexb_to_nalus((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                        pkt_ptr->buffer_ptr_->data_len(), nalus);
    if (!ret) {
        log_errorf("mpegts handle h264 error, dump:%s", pkt_ptr->dump().c_str());
        log_info_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                    pkt_ptr->buffer_ptr_->data_len(), "wrong h264 data");
        return -1;
    }

    MEDIA_PACKET_PTR nalu_pkt_ptr = std::make_shared<MEDIA_PACKET>();
    for (std::shared_ptr<data_buffer> item : nalus) {
        uint8_t* data = (uint8_t*)item->data();
        size_t data_len = (size_t)item->data_len();

        uint8_t nalu_type = data[4] & 0x1f;
        if (H264_IS_AUD(nalu_type)) {
            continue;
        }
        if (H264_IS_PPS(nalu_type)) {
            pps_len_ = data_len;
            memcpy(pps_, data, pps_len_);
            continue;
        }
        if (H264_IS_SPS(nalu_type)) {
            sps_len_ = data_len;
            memcpy(sps_, data, sps_len_);
            continue;
        }
        nalu_pkt_ptr->copy_properties(pkt_ptr);
        nalu_pkt_ptr->buffer_ptr_->reset();
        
        nalu_pkt_ptr->buffer_ptr_->append_data((char*)H264_AUD_DATA, sizeof(H264_AUD_DATA));
        if (H264_IS_KEYFRAME(nalu_type)) {
            nalu_pkt_ptr->buffer_ptr_->append_data((char*)sps_, sps_len_);
            nalu_pkt_ptr->buffer_ptr_->append_data((char*)pps_, pps_len_);
        }
        nalu_pkt_ptr->buffer_ptr_->append_data((char*)data, data_len);
        
        muxer_.input_packet(nalu_pkt_ptr);
    }
    
    return 0;
}

int mpegts_handle::handle_audio_aac(MEDIA_PACKET_PTR pkt_ptr) {
    if (pkt_ptr->is_seq_hdr_) {
        log_info_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
            pkt_ptr->buffer_ptr_->data_len(), "audio data");
        bool ret = get_audioinfo_by_asc((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                                    pkt_ptr->buffer_ptr_->data_len(), aac_type_, sample_rate_, channel_);
        if (!ret) {
            log_errorf("audio asc decode error");
            return -1;
        }
        log_infof("audio asc decode aac type:%d, sample rate:%d, channel:%d",
                aac_type_, sample_rate_, channel_);
        return 0;
    }
    if ((aac_type_ == 0) || (sample_rate_ == 0) || (channel_ == 0)) {
        log_infof("audio config is not ready");
        return 0;
    }

    uint8_t adts_data[32];
    int adts_len = make_adts(adts_data, aac_type_,
            sample_rate_, channel_, pkt_ptr->buffer_ptr_->data_len());
    assert(adts_len == 7);

    pkt_ptr->buffer_ptr_->consume_data(0 - adts_len);
    uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->data();
    memcpy(p, adts_data, adts_len);

    muxer_.input_packet(pkt_ptr);
    return 0;
}

int mpegts_handle::handle_audio_opus(MEDIA_PACKET_PTR pkt_ptr) {
    muxer_.input_packet(pkt_ptr);
    return 0;
}
