#include "transcode.hpp"
#include "utils/logger.hpp"
#include <chrono>
#include <functional>

transcode::transcode()
{
    dec_ = new decode_oper(this);
    output_audio_fmt_ = "libfdk_aac";
    log_infof("transcode construct...");
}

transcode::~transcode()
{
    stop();
    if (dec_) {
        delete dec_;
        dec_ = nullptr;
    }
    if (venc_) {
        delete venc_;
        venc_ = nullptr;
    }
    if (aenc_) {
        delete aenc_;
        aenc_ = nullptr;
    }
    clear();
    log_infof("transcode destruct...");
}

void transcode::start() {
    if (run_) {
        return;
    }
    run_ = true;
    thread_ptr_ = std::make_shared<std::thread>(std::bind(&transcode::on_work, this));
}

void transcode::stop() {
    if (!run_) {
        return;
    }
    run_ = false;
    thread_ptr_->join();
    clear();
    return;
}

void transcode::clear() {
    do {
        std::unique_lock<std::mutex> locker(send_mutex_);
        while(!send_queue_.empty()) {
            rtp_packet_info info = send_queue_.front();
            send_queue_.pop();
            av_packet_free(&info.pkt);
        }
    } while(0);
    
    do {
        std::unique_lock<std::mutex> locker(recv_mutex_);
        while(!recv_queue_.empty()) {
            recv_queue_.pop();
        }
    } while(0);
    return;
}

void transcode::set_output_audio_fmt(const std::string& fmt) {
    output_audio_fmt_ = fmt;
}

void transcode::set_output_audio_samplerate(int samplerate) {
    audio_samplerate_ = samplerate;
}

void transcode::set_output_format(MEDIA_FORMAT_TYPE format) {
    output_format_ = format;
}

void transcode::set_video_bitrate(int bitrate) {
    video_bitrate_ = bitrate;
}

void transcode::set_audio_bitrate(int bitrate) {
    audio_bitrate_ = bitrate;
}

void transcode::insert_send_queue(rtp_packet_info info) {
    std::unique_lock<std::mutex> locker(send_mutex_);
    send_queue_.push(info);
}

AVPacket* transcode::get_send_queue(MEDIA_CODEC_TYPE& codec_type) {
    std::unique_lock<std::mutex> locker(send_mutex_);
    if (send_queue_.empty()) {
        return nullptr;
    }
    rtp_packet_info info = send_queue_.front();
    send_queue_.pop();
    
    codec_type = info.codec_type;
    return info.pkt;
}

void transcode::insert_recv_queue(MEDIA_PACKET_PTR pkt_ptr) {
    std::unique_lock<std::mutex> locker(recv_mutex_);
    
    recv_queue_.push(pkt_ptr);
}


MEDIA_PACKET_PTR transcode::get_recv_queue() {
    std::unique_lock<std::mutex> locker(recv_mutex_);
    MEDIA_PACKET_PTR pkt_ptr;
    
    if (recv_queue_.empty()) {
        return pkt_ptr;
    }
    pkt_ptr = recv_queue_.front();
    recv_queue_.pop();
    
    return pkt_ptr;
}


void transcode::on_work() {
    log_infof("transcode work thread is running...");
    while(run_) {
        if (!dec_) {
            break;
        }
        MEDIA_CODEC_TYPE codec_type = MEDIA_CODEC_UNKOWN;
        AVPacket* pkt = get_send_queue(codec_type);
        if (!pkt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        
        dec_->input_avpacket(pkt, codec_type);
        av_packet_free(&pkt);
    }
    log_infof("transcode work thread is over...");
    return;
}

void transcode::on_avpacket_callback(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type,
                                     uint8_t* extra_data, size_t extra_data_size) {
    MEDIA_PACKET_PTR pkt_ptr = get_media_packet(pkt, codec_type);
    if (!pkt_ptr) {
        log_errorf("make media packet error");
        return;
    }

    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        if (first_audio_ && extra_data && (extra_data_size > 0)) {
            uint8_t data[2] = {0xaf, 0x00};
            first_audio_ = false;

            MEDIA_PACKET_PTR first_pkt_ptr = std::make_shared<MEDIA_PACKET>();
            first_pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;
            first_pkt_ptr->codec_type_ = codec_type;
            first_pkt_ptr->dts_        = pkt_ptr->dts_;
            first_pkt_ptr->pts_        = pkt_ptr->pts_;
            first_pkt_ptr->fmt_type_   = MEDIA_FORMAT_FLV;
            first_pkt_ptr->is_seq_hdr_ = true;

            if (output_format_ == MEDIA_FORMAT_FLV) {
                first_pkt_ptr->buffer_ptr_->append_data((char*)data, sizeof(data));
            }
            first_pkt_ptr->buffer_ptr_->append_data((char*)extra_data, extra_data_size);
            insert_recv_queue(first_pkt_ptr);
        }
        pkt_ptr->is_seq_hdr_ = false;
        if (output_format_ == MEDIA_FORMAT_FLV) {
            pkt_ptr->buffer_ptr_->consume_data(-2);
            uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->data();
            data[0] = 0xaf;
            data[1] = 0x01;
        }
    } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        if (first_video_) {
            uint8_t data[5] = {0x17, 0, 0, 0, 0};
            first_video_ = false;
            
            MEDIA_PACKET_PTR first_pkt_ptr = std::make_shared<MEDIA_PACKET>();
            first_pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
            first_pkt_ptr->codec_type_ = codec_type;
            first_pkt_ptr->dts_        = pkt_ptr->dts_;
            first_pkt_ptr->pts_        = pkt_ptr->pts_;
            first_pkt_ptr->fmt_type_   = MEDIA_FORMAT_FLV;
            first_pkt_ptr->buffer_ptr_->append_data((char*)data, sizeof(data));
            first_pkt_ptr->buffer_ptr_->append_data((char*)extra_data, extra_data_size);
            insert_recv_queue(first_pkt_ptr);
        }
        pkt_ptr->buffer_ptr_->consume_data(-5);
        uint8_t* data = (uint8_t*)pkt_ptr->buffer_ptr_->data();
        data[0] = 0x17;
        data[1] = 0x01;
        data[2] = data[3] = data[4] = 0;
    } else {
        log_errorf("unkown av type:%d", pkt_ptr->av_type_);
        return;
    }
 
    insert_recv_queue(pkt_ptr);
}

void transcode::on_avframe_callback(int stream_index, AVFrame* frame) {
    if (stream_index == VIDEO_STREAM_ID) {
        if (venc_ == nullptr) {
            venc_ = new video_encode(this);
            int ret = ((video_encode*)venc_)->init_video("libx264", frame->width, frame->height,
                                                              "baseline", "veryfast", 1000, 15);
            if (ret < 0) {
                log_errorf("video encode init error");
                delete venc_;
                venc_ = nullptr;
                return;
            }
        }
        venc_->send_frame(frame);
    } else if (stream_index == AUDIO_STREAM_ID) {
        if (aenc_ == nullptr) {
            aenc_ = new audio_encode(this);
            int ret =  ((audio_encode*)aenc_)->init_audio(output_audio_fmt_, audio_samplerate_, 2, 64);
            if (ret < 0) {
                log_errorf("audio encode init error");
                delete aenc_;
                aenc_ = nullptr;
                return;
            }
        }
        aenc_->send_frame(frame);
    } else {
        log_errorf("unkown avtype index:%d", stream_index);
    }
}

MEDIA_PACKET_PTR transcode::get_media_packet(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type) {
    MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
    if (pkt->stream_index == VIDEO_STREAM_ID) {
        pkt_ptr->av_type_ = MEDIA_VIDEO_TYPE;
    } else if (pkt->stream_index == AUDIO_STREAM_ID) {
        pkt_ptr->av_type_ = MEDIA_AUDIO_TYPE;
    } else {
        MEDIA_PACKET_PTR null_pkt_ptr;
        return null_pkt_ptr;
    }

    pkt_ptr->codec_type_ = codec_type;
    pkt_ptr->dts_        = pkt->dts;
    pkt_ptr->pts_        = pkt->pts;
    pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;

    pkt_ptr->buffer_ptr_->append_data((char*)pkt->data, pkt->size);
    return pkt_ptr;
}

int transcode::send_transcode(MEDIA_PACKET_PTR pkt_ptr, int pos) {
    AVPacket* pkt = nullptr;
    
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        pkt = get_avpacket(pkt_ptr, pos);
    } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        pkt = get_avpacket(pkt_ptr, pos);
    }
    
    if (pkt == nullptr) {
        return -1;
    }
    rtp_packet_info info;
    info.pkt = pkt;
    info.codec_type = pkt_ptr->codec_type_;
    
    insert_send_queue(info);
    return 0;
}

MEDIA_PACKET_PTR transcode::recv_transcode() {
    return get_recv_queue();
}


