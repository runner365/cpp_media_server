#include "rtmp_relay.hpp"
#include "utils/logger.hpp"
#include "utils/av/media_stream_manager.hpp"


rtmp_relay::rtmp_relay(const std::string& host, const std::string& key, boost::asio::io_context& io_context):host_(host)
    , key_(key)
{
    client_ptr_ = std::make_shared<rtmp_client_session>(io_context, this);
    alive_cnt_ = 0;
    log_infof("rtmp relay construct host:%s, key:%s", host_.c_str(), key_.c_str());
}

rtmp_relay::~rtmp_relay() {
    log_infof("rtmp relay destruct host:%s, key:%s", host_.c_str(), key_.c_str());
    client_ptr_ = nullptr;
    media_stream_manager::remove_publisher(key_);
}

void rtmp_relay::on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) {
    if (ret_code != 0) {
        log_errorf("rtmp receive callback error:%d", ret_code);
        return;
    }

    if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE) {
        log_infof("discard av type is metadata, key:%s", key_.c_str());
        return;
    }
    if (pkt_ptr->codec_type_ == MEDIA_CODEC_UNKOWN) {
        log_infof("discard codec type is unkown, key:%s", key_.c_str());
        return;
    }

    int ret = media_stream_manager::writer_media_packet(pkt_ptr);
    if (ret > 0) {
        alive_cnt_ = 0;
    }
    return;
}

void rtmp_relay::on_close(int ret_code) {
    log_infof("rtmp play on close:%d", ret_code);
    alive_cnt_ = 4;
    client_ptr_ = nullptr;
}


bool rtmp_relay::is_alive() {
    if (alive_cnt_ > 3) {
        return false;
    }
    alive_cnt_++;
    return true;
}

void rtmp_relay::start() {
    std::string url = "rtmp://";

    url += host_;
    url += "/";
    url += key_;

    log_infof("rtmp relay start url:%s", url.c_str());
    client_ptr_->start(url, false);

    return;
}
