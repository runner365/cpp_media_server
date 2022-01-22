#include "hls_worker.hpp"
#include "logger.hpp"
#include "stringex.hpp"

static hls_worker* s_worker = nullptr;

/*
void response_error(std::shared_ptr<http_response> response) {
    std::string err_str = "403 Forbidden";
    //StatusForbidden:                    ,
    response->set_status_code(403);
    response->set_status("Forbidden");
    response->write(err_str.c_str(), err_str.length());
    return;
}

void hls_handle(const http_request* request, std::shared_ptr<http_response> response) {
    size_t pos = request->uri_.find(".m3u8");
    if (pos != std::string::npos) {
        std::string m3u8_header;
        std::string key = request->uri_.substr(0, pos);
        if (key[0] == '/') {
            key = key.substr(1);
        }
        log_infof("http request m3u8 uri:%s, key:%s", request->uri_.c_str(), key.c_str());
        std::shared_ptr<mpegts_handle> handle_ptr = s_worker->get_mpegts_handle(key);
        if (!handle_ptr) {
            log_errorf("fail to get mpegts handle by key:%s", key.c_str());
            response_error(response);
            return;
        }
        bool ret = handle_ptr->gen_live_m3u8(m3u8_header);
        if (!ret) {
            log_infof("live m3u8 is not ready");
            response_error(response);
            return;
        }
        log_infof("hls response m3u8_header:%s", m3u8_header.c_str());

        response->add_header("Access-Control-Allow-Origin", "*");
        response->add_header("Cache-Control", "no-cache");
        response->add_header("Content-Type", "application/x-mpegURL");
        response->write(m3u8_header.c_str(), m3u8_header.size());
    } else {
        pos = request->uri_.find(".ts");
        if (pos == std::string::npos) {
            log_errorf("http request error uri:%s", request->uri_.c_str());
            response_error(response);
            return;
        }
        std::string key = request->uri_.substr(0, pos);
        std::string handle_key;
        std::vector<std::string> output_vec;
        log_infof("http request ts file uri:%s, key:%s", request->uri_.c_str(), key.c_str());

        string_split(key, "/", output_vec);
        if (output_vec.size() >= 2) {
            if (output_vec[0].empty() && output_vec.size() >= 2) {
                handle_key = output_vec[1];
                handle_key += "/";
                handle_key += output_vec[2];
            } else {
                handle_key = output_vec[0];
                handle_key += "/";
                handle_key += output_vec[1];
            }
        } else {
            handle_key = key;
        }
        std::shared_ptr<mpegts_handle> handle_ptr = s_worker->get_mpegts_handle(handle_key);
        if (!handle_ptr) {
            log_errorf("fail to get mpegts handle by key:%s", handle_key.c_str());
            response_error(response);
            return;
        }
        std::shared_ptr<ts_item_info> ts_item = handle_ptr->get_mpegts_item(key);
        if (!ts_item) {
            log_errorf("fail to get mpegts file item by key:%s", key.c_str());
            response_error(response);
            return;
        }
        log_infof("get http mpegts filename:%s, file key:%s, data len:%lu",
                ts_item->ts_filename.c_str(), ts_item->ts_key.c_str(),
                ts_item->ts_buffer.data_len());
		response->add_header("Access-Control-Allow-Origin", "*");
		response->add_header("Content-Type", "video/mp2ts");
        response->write(ts_item->ts_buffer.data(), ts_item->ts_buffer.data_len());
    }

}
*/
hls_worker::hls_worker(boost::asio::io_context& io_context, uint16_t port):timer_interface(io_context, 5000)
                        , io_context_(io_context)
{
    run();
    start_timer();
}

hls_worker::~hls_worker()
{
    stop();
    stop_timer();
}

void hls_worker::run() {
    if (run_flag_) {
        return;
    }
    run_flag_ = true;
    //server_.add_get_handle("/", hls_handle);
    s_worker = this;
    run_thread_ptr_ = std::make_shared<std::thread>(&hls_worker::on_work, this);
}

void hls_worker::stop() {
    if (!run_flag_) {
        return;
    }
    run_flag_ = false;
    run_thread_ptr_->join();
}

void hls_worker::insert_packet(MEDIA_PACKET_PTR pkt_ptr) {
    io_context_.post(std::bind(&hls_worker::on_handle_packet, this, pkt_ptr));
    return;
}

void hls_worker::on_handle_packet(MEDIA_PACKET_PTR pkt_ptr) {
    if (!pkt_ptr) {
        return;
    }
    
    std::shared_ptr<mpegts_handle> handle = get_mpegts_handle(pkt_ptr);
    if (!handle) {
        log_errorf("get mpegts handle error, app:%s, streamname:%s, key:%s",
            pkt_ptr->app_.c_str(), pkt_ptr->streamname_.c_str(), pkt_ptr->key_.c_str());
        return;
    }
    handle->handle_media_packet(pkt_ptr);
    return;
}

std::shared_ptr<mpegts_handle> hls_worker::get_mpegts_handle(MEDIA_PACKET_PTR pkt_ptr) {
    auto iter = mpegts_handles_.find(pkt_ptr->key_);
    if (iter != mpegts_handles_.end()) {
        return iter->second;
    }
    std::shared_ptr<mpegts_handle> handle_ptr = std::make_shared<mpegts_handle>(pkt_ptr->app_,
                                                                            pkt_ptr->streamname_,
                                                                            path_,
                                                                            rec_enable_);
    mpegts_handles_[pkt_ptr->key_] = handle_ptr;

    return handle_ptr;
}

std::shared_ptr<mpegts_handle> hls_worker::get_mpegts_handle(const std::string& key) {
    std::shared_ptr<mpegts_handle> handle_ptr;
    auto iter = mpegts_handles_.find(key);
    if (iter == mpegts_handles_.end()) {
        return handle_ptr;
    }
    handle_ptr = iter->second;

    return handle_ptr;
}

void hls_worker::on_work() {
    log_infof("http hls is running...");
    io_context_.run();
}

void hls_worker::on_timer() {
    check_timeout();
}

void hls_worker::check_timeout() {
    for (auto iter = mpegts_handles_.begin();
        iter != mpegts_handles_.end();
        ) {
        if (iter->second->is_alive()) {
            iter++;
            continue;
        }
        log_infof("mpegts handle is timeout, key:%s", iter->first.c_str());
        iter->second->flush();
        iter = mpegts_handles_.erase(iter);
    }
}
    