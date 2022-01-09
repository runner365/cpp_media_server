#include "hls_worker.hpp"
#include "logger.hpp"

static hls_worker* s_worker = nullptr;

void hls_handle(const http_request* request, std::shared_ptr<http_response> response) {
    auto pos = request->uri_.find(".m3u8");
    if (pos == std::string::npos) {
        log_errorf("hls request uri error:%s", request->uri_.c_str());
        response->close();
        return;
    }
    std::string key = request->uri_.substr(0, pos);
    if (key[0] == '/') {
        key = key.substr(1);
    }

    std::shared_ptr<mpegts_handle> handle_ptr = s_worker->get_mpegts_handle(key);
    if (!handle_ptr) {
        log_errorf("fail to get mpegts handle by key:%s", key.c_str());
        return;
    }
}

hls_worker::hls_worker(boost::asio::io_context& io_context, uint16_t port):timer_interface(io_context, 30)
                        , io_context_(io_context)
                        , server_(io_context, port)
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
    run_thread_ptr_ = std::make_shared<std::thread>(&hls_worker::on_work, this);

    s_worker = this;

    server_.add_get_handle("/", hls_handle);
}

void hls_worker::stop() {
    if (!run_flag_) {
        return;
    }
    run_flag_ = false;
    run_thread_ptr_->join();
}

int hls_worker::insert_packet(MEDIA_PACKET_PTR pkt_ptr) {
    std::unique_lock<std::mutex> lock(mutex_);

    const int MEDIA_QUEUE_MAX = 500;

    if (!run_flag_) {
        return -1;
    }

	while (pkt_queue_.size() > MEDIA_QUEUE_MAX) {
        log_infof("hls media queue is overflow, queue length:%d", pkt_queue_.size());
		pkt_queue_.pop();
	}

    pkt_queue_.push(pkt_ptr);
    
    return pkt_queue_.size();
}

MEDIA_PACKET_PTR hls_worker::get_packet() {
    std::unique_lock<std::mutex> lock(mutex_);
    MEDIA_PACKET_PTR pkt_ptr;

    if (!run_flag_) {
        return pkt_ptr;
    }

    if (pkt_queue_.empty()) {
        return pkt_ptr;
    }

    pkt_ptr = pkt_queue_.front();
    pkt_queue_.pop();
    return pkt_ptr;
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
    io_context_.run();
}

void hls_worker::on_timer() {
    while (true) {
        check_timeout();

        MEDIA_PACKET_PTR pkt_ptr = get_packet();
        if (!pkt_ptr) {
            break;
        }
        
        std::shared_ptr<mpegts_handle> handle = get_mpegts_handle(pkt_ptr);
        if (!handle) {
            log_errorf("get mpegts handle error, app:%s, streamname:%s, key:%s",
                pkt_ptr->app_.c_str(), pkt_ptr->streamname_.c_str(), pkt_ptr->key_.c_str());
            continue;
        }
        handle->handle_media_packet(pkt_ptr);
    }
}

void hls_worker::check_timeout() {

}
    