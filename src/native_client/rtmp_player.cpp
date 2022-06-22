#include "rtmp_player.hpp"
#include "logger.hpp"
#include "format/audio_pub.hpp"
#include "format/h264_header.hpp"
#include "transcode/transcode_pub.hpp"
#include "transcode/filter.hpp"

rtmp_player::rtmp_player(uv_loop_t* loop):loop_(loop)
{
}

rtmp_player::~rtmp_player()
{
    if (session_) {
        delete session_;
        session_ = nullptr;
    }
}

void rtmp_player::run() {
    player_->run();
}

int rtmp_player::open_url(const std::string& url) {
    if (!url_.empty()) {
        return -1;
    }
    if (session_) {
        return -1;
    }

    url_ = url;

    player_ = new sdl_player(url);

    session_ = new rtmp_client_session(loop_, this);

    session_->start(url_, false);

    return 0;
}

void rtmp_player::on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) {
    log_infof("code:%d, key:%s, av type:%s, codec type:%s, fmt type:%s, \
dts:%ld, pts:%lu, key:%d, seq:%d, data len:%lu",
            ret_code, pkt_ptr->key_.c_str(),
            avtype_tostring(pkt_ptr->av_type_).c_str(),
            codectype_tostring(pkt_ptr->codec_type_).c_str(),
            formattype_tostring(pkt_ptr->fmt_type_).c_str(),
            pkt_ptr->dts_, pkt_ptr->pts_,
            pkt_ptr->is_key_frame_, pkt_ptr->is_seq_hdr_,
            pkt_ptr->buffer_ptr_->data_len());

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        on_video_packet_callback(pkt_ptr, true);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        on_audio_packet_callback(pkt_ptr, true);
    } else {
        log_errorf("unkown av type:%d", pkt_ptr->av_type_);
    }
}

void rtmp_player::on_close(int ret_code) {
    log_infof("rtmp on close code:%d", ret_code);
}
