#include "rtmp_client_session.hpp"
#include "rtmp_pub.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include <thread>
#include <functional>
#include <iostream>
#include <boost/asio.hpp>

rtmp_client_session* client_session_p = nullptr;
std::string dst_url;

class flv_output_callback : public av_format_callback
{
public:
    flv_output_callback(const std::string& filename):filename_(filename) {

    }

    ~flv_output_callback() {

    }
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override {
        FILE* file_p = fopen(filename_.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->data(), 1, pkt_ptr->buffer_ptr_->data_len(), file_p);
            fclose(file_p);
        }
        return 0;
    }

private:
    std::string filename_;
};

flv_output_callback flv_cb("output.flv");

class client_media_callback : public rtmp_client_callbackI
{
public:
    client_media_callback():muxer_(true, true, &flv_cb) {

    }
    ~client_media_callback() {

    }

public:
    virtual void on_close(int ret_code) override {
        log_infof("client rtmp close notify:%d", ret_code);
        exit(0);
    }

    virtual void on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) override {
        log_debugf("return code:%d, key:%s, av type:%d, codec type:%d, fmt type:%d, \
dts:%ld, pts:%lu, key:%d, seq:%d, data len:%lu",
            ret_code, pkt_ptr->key_.c_str(),
            pkt_ptr->av_type_, pkt_ptr->codec_type_, pkt_ptr->fmt_type_,
            pkt_ptr->dts_, pkt_ptr->pts_,
            pkt_ptr->is_key_frame_, pkt_ptr->is_seq_hdr_,
            pkt_ptr->buffer_ptr_->data_len());
        
        if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            pkt_ptr->buffer_ptr_->consume_data(2);
        } else if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            pkt_ptr->buffer_ptr_->consume_data(5);
        } else {
            return;
        }

        muxer_.input_packet(pkt_ptr);
    }

private:
    flv_muxer muxer_;
};

client_media_callback media_cb;

void client_work(boost::asio::io_context& io_context) {
    log_infof("rtmp client is starting url:%s", dst_url.c_str());

    client_session_p = new rtmp_client_session(io_context, &media_cb);

    client_session_p->start(dst_url, false);

    return;
}

//./rtmp_play_demo rtmp://172.29.66.121/live/1000
// save as output.flv
int main(int argn, char** argv) {
    if (argn < 2) {
        log_errorf("please input rigth parameter...");
        return -1;
    }
    dst_url = argv[1];

    Logger::get_instance()->set_filename("play.log");

    boost::asio::io_context io_context;
    boost::asio::io_service::work work(io_context);

    try {
        client_work(io_context);

        io_context.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}