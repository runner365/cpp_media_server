#include "flv_websocket.hpp"
#include "media_stream_manager.hpp"
#include "flv_pub.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include "utils/byte_stream.hpp"
#include "transcode/transcode.hpp"
#include "net/websocket/ws_session.hpp"
#include <sstream>
#include <assert.h>

av_outputer::av_outputer()
{
}

av_outputer::~av_outputer()
{
    release();
}

void av_outputer::release() {
    if (trans_) {
        trans_->stop();
        delete trans_;
        trans_ = nullptr;
    }
}

int av_outputer::output_packet(MEDIA_PACKET_PTR pkt_ptr) {
    int ret = 0;

    if ((pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) && (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS)) {
        if (trans_ == nullptr) {
            trans_ = new transcode();
            trans_->set_output_audio_fmt("libfdk_aac");
            trans_->set_output_format(MEDIA_FORMAT_FLV);
            trans_->set_output_audio_samplerate(44100);
            trans_->start();
        }
        trans_->send_transcode(pkt_ptr, 0);
        while(true) {
            MEDIA_PACKET_PTR ret_pkt_ptr = trans_->recv_transcode();
            if (!ret_pkt_ptr) {
                break;
            }
            ret_pkt_ptr->key_        = pkt_ptr->key_;
            ret_pkt_ptr->app_        = pkt_ptr->app_;
            if (ret_pkt_ptr->is_seq_hdr_) {
                log_infof("audio seq dts:%ld, pts:%ld",
                        ret_pkt_ptr->dts_, ret_pkt_ptr->pts_);
                log_info_data((uint8_t*)ret_pkt_ptr->buffer_ptr_->data(),
                        ret_pkt_ptr->buffer_ptr_->data_len(),
                        "audio seq data");
            }           
            ret_pkt_ptr->streamname_ = pkt_ptr->streamname_;
            ret = media_stream_manager::writer_media_packet(ret_pkt_ptr);
        }
    } else {
        ret = flv_muxer::add_flv_media_header(pkt_ptr);
        if (ret < 0) {
            return ret;
        }
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
        ret = media_stream_manager::writer_media_packet(pkt_ptr);
    }

    return ret;
}

flv_websocket::flv_websocket(uv_loop_t* loop, uint16_t port):server_(loop, port, this)
                        , demuxer_(&outputer_)
{

}

flv_websocket::~flv_websocket()
{

}

void flv_websocket::on_accept(websocket_session* session) {
    log_infof("flv in websocket is accept....");
}

void flv_websocket::on_read(websocket_session* session, const char* data, size_t len) {
    if (uri_.empty()) {
        const char* uri_data = data;
        assert(uri_data[0] == 0x02);
        uint16_t str_len = read_2bytes((uint8_t*)uri_data + 1);
        std::string str(uri_data + 3, str_len);
        std::size_t pos = str.find(".flv");
        if (pos != str.npos) {
            uri_ = str.substr(0, pos);
        }
        if (uri_[0] == '/') {
            uri_ = uri_.substr(1);
        }
        log_infof("websocket uri:%s", uri_.c_str());
        media_stream_manager::add_publisher(uri_);
        return;
    }

    MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
    pkt_ptr->buffer_ptr_->append_data(data, len);
    pkt_ptr->key_ = uri_;
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    demuxer_.input_packet(pkt_ptr);
}


void flv_websocket::on_close(websocket_session* session) {
    log_errorf("flv in websocket is closed, uri:%s", uri_.c_str());
    if (!uri_.empty()) {
        media_stream_manager::remove_publisher(uri_);
    }
    outputer_.release();
}
