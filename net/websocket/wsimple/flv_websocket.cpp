#include "flv_websocket.hpp"
#include "media_stream_manager.hpp"
#include "flv_pub.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include "utils/byte_stream.hpp"
#include <sstream>

int av_outputer::output_packet(MEDIA_PACKET_PTR pkt_ptr) {
    int ret = flv_muxer::add_flv_media_header(pkt_ptr);
    if (ret < 0) {
        return ret;
    }
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;

    return media_stream_manager::writer_media_packet(pkt_ptr);
}

flv_websocket::flv_websocket():demuxer_(&outputer_)
{

}

flv_websocket::~flv_websocket()
{

}

void flv_websocket::on_accept() {
    log_infof("flv in websocket is accept....");
}

void flv_websocket::on_read(const char* data, size_t len) {
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

void flv_websocket::on_writen(int len) {

}

void flv_websocket::on_close(int err_code) {
    log_errorf("flv in websocket is closed, err:%d, uri:%s",
            err_code, uri_.c_str());
    if (!uri_.empty()) {
        media_stream_manager::remove_publisher(uri_);
    }
}
