#include "rtmp_server.hpp"
#include "timer.hpp"

rtmp_server::rtmp_server(uv_loop_t* loop, uint16_t port):timer_interface(loop, 5000)
{
    server_ = std::make_shared<tcp_server>(loop, port, this);
    start_timer();
    log_infof("rtmp server is starting, port:%d", port);
}

rtmp_server::~rtmp_server() {
   stop_timer();
}

void rtmp_server::on_close(std::string session_key) {
    log_infof("tcp close key:%s", session_key.c_str());
    const auto iter = session_ptr_map_.find(session_key);
    if (iter != session_ptr_map_.end()) {
        session_ptr_map_.erase(iter);
    }
}

void rtmp_server::on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) {
    if (ret_code == 0) {
        std::shared_ptr<rtmp_server_session> session_ptr = std::make_shared<rtmp_server_session>(loop, handle, this);
        log_infof("get rtmp session key:%s", session_ptr->get_sesson_key().c_str());
        session_ptr_map_.insert(std::make_pair(session_ptr->get_sesson_key(), session_ptr));
    }
}

void rtmp_server::on_timer() {
    on_check_alive();
}

void rtmp_server::on_check_alive() {
    std::vector<std::shared_ptr<rtmp_server_session>> session_vec;

    for (auto item : session_ptr_map_) {
        std::shared_ptr<rtmp_server_session> session_ptr = item.second;
        if (!session_ptr->is_alive()) {
            session_vec.push_back(session_ptr);
        }
    }
    for (auto item : session_vec) {
        item->close();
    }
}
