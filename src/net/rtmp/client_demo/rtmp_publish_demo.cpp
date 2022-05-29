#include "rtmp_client_session.hpp"
#include "rtmp_pub.hpp"
#include "flv_demux.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include "timeex.hpp"
#include <thread>
#include <string>
#include <functional>
#include <iostream>

rtmp_client_session* client_session_p = nullptr;
std::string dst_url;
bool g_closed = false;

class client_media_callback : public rtmp_client_callbackI
{
public:
    client_media_callback() {
    }
    ~client_media_callback() {
    }

public:
    virtual void on_close(int ret_code) override {
        log_infof("client rtmp close notify:%d", ret_code);
        std::cout << "rtmp is closed. exit the rtmp publish.\r\n";
        exit(0);
    }

    virtual void on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) override {
        if (ret_code != 0) {
            log_infof("publish client on message return error:%d", ret_code);
            g_closed = true;
            return;
        }
        return;
    }
};

client_media_callback client_cb;

void client_publish(uv_loop_t* loop) {
    log_infof("rtmp client start publishing: %s", dst_url.c_str());

    client_session_p = new rtmp_client_session(loop, &client_cb);
    client_session_p->start(dst_url, true);

    return;
}

class rtmp_sender : public av_format_callback
{
public:
    rtmp_sender() {
    }
    ~rtmp_sender() {
        stop();
    }

protected:
    void on_work() {
        try {
            uv_loop_t* loop = uv_default_loop();

            client_publish(loop);
            uv_run(loop, UV_RUN_DEFAULT);
        }
        catch(const std::exception& e) {
            std::cerr << e.what() << '\n';
        }
    }

public:
    int start() {
        thread_ptr_ = std::make_shared<std::thread>(&rtmp_sender::on_work, this);
        return 0;
    }
    void stop() {
        if (stop_flag_) {
            return;
        }
        stop_flag_ = true;
        thread_ptr_->join();
    }

public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override {
        if (g_closed) {
            return -1;
        }
        log_debugf("key:%s, av type:%d, codec type:%d, fmt type:%d, \
dts:%ld, pts:%lu, keyframe:%d, seqframe:%d, data len:%lu",
            pkt_ptr->key_.c_str(),
            pkt_ptr->av_type_, pkt_ptr->codec_type_, pkt_ptr->fmt_type_,
            pkt_ptr->dts_, pkt_ptr->pts_,
            pkt_ptr->is_key_frame_, pkt_ptr->is_seq_hdr_,
            pkt_ptr->buffer_ptr_->data_len());
        wait(pkt_ptr);

        int ret = flv_muxer::add_flv_media_header(pkt_ptr);
        if (ret < 0) {
            log_infof("add flv media header error:%d", ret);
            return ret;
        }
        ret = client_session_p->rtmp_write(pkt_ptr);
        if (ret < 0) {
            g_closed = true;
            return ret;
        }
        return ret;
    }

private:
    void wait(MEDIA_PACKET_PTR pkt_ptr) {
        while((client_session_p == nullptr) || (!client_session_p->is_ready())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (pkt_ptr->av_type_ != MEDIA_VIDEO_TYPE) {
            return;
        }
        if (last_dts_ < 0) {
            if (pkt_ptr->is_seq_hdr_) {
                return;
            }
            last_dts_    = pkt_ptr->dts_;
            last_sys_ts_ = now_millisec();
            return;
        }

        if (pkt_ptr->dts_ <= last_dts_) {
            return;
        }
        int64_t diff_t = 0;
        
        do {
            diff_t = now_millisec() - last_sys_ts_ - (pkt_ptr->dts_ - last_dts_);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while(diff_t < 0);
        return;
    }
private:
    std::shared_ptr<std::thread> thread_ptr_;
    int64_t last_dts_    = -1;
    int64_t last_sys_ts_ = -1;
    bool stop_flag_      = false;
};

rtmp_sender sender;

//./rtmp_publish_demp output.flv rtmp://x.x.x.x/live/123456
int main(int argn, char** argv) {
    if (argn < 3) {
        log_errorf("please input rigth parameter...");
        return -1;
    }
    Logger::get_instance()->set_filename("publish.log");

    flv_demuxer flv_dec(&sender);

    std::string filename(argv[1]);
    dst_url = argv[2];

    if ((filename.empty() || (dst_url.size() < strlen("rtmp://x.x.x.x")))) {
        log_errorf("please input rigth parameter...");
        return -1;
    }
    sender.start();
    log_infof("start publishing flv file:%s", filename.c_str());

    FILE* fh_p = fopen(filename.c_str(), "r");
    if (!fh_p) {
        log_errorf("fail to open filename:%s\r\n", filename.c_str());
        return -1;
    }

    char buffer[2048];
    int n = 0;

    do
    {
        if (g_closed) {
            break;
        }
        n = fread(buffer, 1, sizeof(buffer), fh_p);
        MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
        pkt_ptr->buffer_ptr_->append_data(buffer, n);

        flv_dec.input_packet(pkt_ptr);
    } while (n > 0);

    sender.stop();
    fclose(fh_p);
    log_infof("read file eof....");
    return 0;
}