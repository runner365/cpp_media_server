#include "rtmp_server.hpp"
#include "net_pub.hpp"
#include "timer.hpp"

rtmp_server::rtmp_server(boost::asio::io_context& io_context, uint16_t port):timer_interface(io_context, 5000)
{
    server_ = std::make_shared<tcp_server>(io_context, port, this);
    server_->accept();
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

void rtmp_server::on_accept(int ret_code, boost::asio::ip::tcp::socket socket) {
    if (ret_code == 0) {
        std::string key;
        make_endpoint_string(socket.remote_endpoint(), key);
        log_infof("tcp accept key:%s", key.c_str());
        std::shared_ptr<rtmp_server_session> session_ptr = std::make_shared<rtmp_server_session>(std::move(socket), this, key);
        session_ptr_map_.insert(std::make_pair(key, session_ptr));
    }
    server_->accept();
}

void rtmp_server::on_accept_ssl(int ret_code, boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket) {

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
