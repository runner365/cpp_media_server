#include "httpflv_player.hpp"
#include "utils/url_analyze.hpp"
#include "utils/logger.hpp"

httpflv_player::httpflv_player(uv_loop_t* loop):loop_(loop)
                                            , flvdemux_(this)
{
}

httpflv_player::~httpflv_player()
{
}

int httpflv_player::open_url(const std::string& url) {
    std::string host;
    uint16_t port = 80;
    std::string subpath;
    std::map<std::string, std::string> parameters;
    std::map<std::string, std::string> headers;

    int ret = get_hostport_by_url(url, host, port, subpath, parameters);
    if (ret != 0) {
        log_infof("url format is error, url:%s", url.c_str());
        return ret;
    }

    size_t pos = subpath.find(".flv");
    if (pos != std::string::npos) {
        key_ = subpath.substr(0, pos);
    } else {
        key_ = subpath;
    }

    log_infof("httpflv host:%s, port:%d, subpath:%s", host.c_str(), port, subpath.c_str());
    for (auto& item : parameters) {
        log_infof("parameter %s=%s", item.first.c_str(), item.second.c_str());
    }

    hc_ptr_ = std::make_unique<http_client>(loop_, host, port, this);
    player_ = new sdl_player(url);

    ret =  hc_ptr_->get(subpath, headers);
    if (ret != 0) {
        return ret;
    }
    return 0;
}

void httpflv_player::run() {
    player_->run();
}

int httpflv_player::output_packet(MEDIA_PACKET_PTR pkt_ptr) {
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        on_video_packet_callback(pkt_ptr, false);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        on_audio_packet_callback(pkt_ptr, false);
    } else {
        log_errorf("unkown av type:%d", pkt_ptr->av_type_);
    }
    return 0;
}

void httpflv_player::on_http_read(int ret, std::shared_ptr<http_client_response> resp_ptr) {
    if (ret < 0) {
        if (hc_ptr_) {
            hc_ptr_->close();
            hc_ptr_ = nullptr;
        }
        return;
    }

    flvdemux_.input_packet((uint8_t*)resp_ptr->data_.data(), resp_ptr->data_.data_len(), key_);

    resp_ptr->data_.consume_data(resp_ptr->data_.data_len());
}
