#include "flv_websocket.hpp"
#include "media_stream_manager.hpp"
#include "flv_pub.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include "utils/byte_stream.hpp"
#include "format/flv/flv_demux.hpp"
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

flv_websocket::flv_websocket(uv_loop_t* loop,
                        uint16_t port):server_(loop, port, this)
{

}

flv_websocket::flv_websocket(uv_loop_t* loop, uint16_t port,
                const std::string& key_file,
                const std::string& cert_file):server_(loop, port, this, key_file, cert_file)
{
}

flv_websocket::~flv_websocket()
{

}

void flv_websocket::on_accept(websocket_session* session) {
    log_infof("flv in websocket is accept....");
}

void flv_websocket::on_read(websocket_session* session, const char* data, size_t len) {
    if (session->get_uri().empty()) {
        std::string uri = session->path();
        size_t pos = uri.find(".flv");
        if (pos != std::string::npos) {
            uri = uri.substr(0, pos);
        }
        if (uri[0] == '/') {
            uri = uri.substr(1);
        }
        log_infof("websocket uri:%s, path:%s", uri.c_str(), session->path().c_str());

        session->set_uri(uri);
        media_stream_manager::add_publisher(uri);
    }
    
    if (session->get_output() == nullptr) {
        session->set_output(new av_outputer());
    }

    if (session->get_media_demuxer() == nullptr) {
        session->set_media_demuxer(new flv_demuxer(session->get_output()));
    }

    MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
    pkt_ptr->buffer_ptr_->append_data(data, len);
    pkt_ptr->key_ = session->get_uri();
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    session->get_media_demuxer()->input_packet(pkt_ptr);
}

void flv_websocket::on_close(websocket_session* session) {
    log_errorf("flv in websocket is closed, uri:%s", session->get_uri().c_str());
    if (!session->get_uri().empty()) {
        media_stream_manager::remove_publisher(session->get_uri());
    }
    outputer_.release();
}
