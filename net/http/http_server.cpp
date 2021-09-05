#include "http_server.hpp"


http_server::http_server(boost::asio::io_context& io_context, uint16_t port):timer_interface(io_context, 5000) {
    server_ = std::make_shared<tcp_server>(io_context, port, this);
    server_->accept();
    start_timer();
}

http_server::~http_server() {
}

void http_server::add_get_handle(const std::string uri, HTTP_HANDLE_Ptr handle_func) {
    get_handle_map_.insert(std::make_pair(uri, handle_func));
}

void http_server::add_post_handle(const std::string uri, HTTP_HANDLE_Ptr handle_func) {
    post_handle_map_.insert(std::make_pair(uri, handle_func));
}

void http_server::on_timer() {
    auto iter = session_ptr_map_.begin();
    while (iter != session_ptr_map_.end()) {
        std::shared_ptr<http_session> session_ptr = iter->second;
        
        if (!session_ptr->is_alive()) {
            log_infof("remove http session %s", iter->first.c_str());
            session_ptr_map_.erase(iter++);
            continue;
        }
        iter++;
    }
}

void http_server::on_accept(int ret_code, boost::asio::ip::tcp::socket socket) {
    if (ret_code == 0) {
        std::string key;
        make_endpoint_string(socket.remote_endpoint(), key);
        std::shared_ptr<http_session> session_ptr = std::make_shared<http_session>(std::move(socket), this);
        session_ptr_map_.insert(std::make_pair(key, session_ptr));
    }
    server_->accept();
    return;
}

void http_server::on_close(boost::asio::ip::tcp::endpoint endpoint) {
    std::string key;
    make_endpoint_string(endpoint, key);

    log_infof("http server remove key:%s", key.c_str());
    auto iter = session_ptr_map_.find(key);
    if (iter != session_ptr_map_.end()) {
        session_ptr_map_.erase(iter);
    }
    return;
}

HTTP_HANDLE_Ptr http_server::get_handle(const http_request* request) {
    HTTP_HANDLE_Ptr handle_func = nullptr;
    std::unordered_map< std::string, HTTP_HANDLE_Ptr >::iterator iter;

    if (request->method_ == "GET") {
        iter = get_handle_map_.find(request->uri_);
        if (iter != get_handle_map_.end()) {
            handle_func = iter->second;
            return handle_func;
        }
    } else if (request->method_ == "POST") {
        iter = post_handle_map_.find(request->uri_);
        if (iter != post_handle_map_.end()) {
            handle_func = iter->second;
            return handle_func;
        }
    }
    
    iter = get_handle_map_.find("/");
    if (iter != post_handle_map_.end()) {
        handle_func = iter->second;
        return handle_func;
    }
    return handle_func;
}