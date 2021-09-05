#include "http_session.hpp"
#include "http_server.hpp"
#include "http_common.hpp"
#include <boost/asio.hpp>

http_session::http_session(boost::asio::ip::tcp::socket socket, http_callbackI* callback) : callback_(callback)
{
    request_ = new http_request(this);
    session_ptr_ = std::make_shared<tcp_session>(std::move(socket), this);
    remote_endpoint_ = session_ptr_->get_remote_endpoint();

    try_read();
}

http_session::~http_session() {
    close();
    log_infof("http session destruct......")
}

void http_session::try_read() {
    session_ptr_->async_read();
}

void http_session::write(const char* data, size_t len) {
    session_ptr_->async_write(data, len);
}


void http_session::close() {
    if (is_closed_) {
        return;
    }
    is_closed_ = true;
    log_infof("http session close....");
    
    if (response_ptr_.get()) {
        response_ptr_->set_close(true);
    }
    return;
}

void http_session::on_write(int ret_code, size_t sent_size) {
    if (ret_code != 0) {
        log_warnf("http session write callback return code:%d", ret_code);
        close();
    }
    keep_alive();
    return;
}

void http_session::on_read(int ret_code, const char* data, size_t data_size) {
    if (ret_code != 0) {
        log_warnf("http session read callback return code:%d", ret_code);
        close();
    }

    keep_alive();

    if (!header_is_ready_) {
        header_data_.append_data(data, data_size);
        int ret = analyze_header();
        if (ret != 0) {
            callback_->on_close(remote_endpoint_);
            return;
        }

        if (!header_is_ready_) {
            try_read();
            return;
        }

        char* start = header_data_.data() + content_start_;
        int len     = header_data_.data_len() - content_start_;
        content_data_.append_data(start, len);
    } else {
        content_data_.append_data(data, data_size);
    }

    if ((request_->content_length_ == 0) || (request_->method_ == "GET")) {
        HTTP_HANDLE_Ptr handle_ptr = callback_->get_handle(request_);
        if (!handle_ptr) {
            close();
            return;
        }
        //call handle
        response_ptr_ = std::make_shared<http_response>(this);
        handle_ptr(request_, response_ptr_);
        return;
    }

    if ((int)content_data_.data_len() >= request_->content_length_) {
        HTTP_HANDLE_Ptr handle_ptr = callback_->get_handle(request_);
        if (!handle_ptr) {
            close();
            return;
        }
        //call handle
        response_ptr_ = std::make_shared<http_response>(this);
        request_->content_body_ = content_data_.data();
        handle_ptr(request_, response_ptr_);
        return;
    }

    try_read();
    return;
}

int http_session::analyze_header() {
    std::string info(header_data_.data(), header_data_.data_len());
    size_t pos = info.find("\r\n\r\n");
    if (pos == info.npos) {
        header_is_ready_ = false;
        return 0;
    }

    content_start_ = (int)pos + 4;
    header_is_ready_ = true;
    std::string header_str(header_data_.data(), pos);
    std::vector<std::string> header_vec;

    string_split(header_str, "\r\n", header_vec);
    if (header_vec.empty()) {
        return -1;
    }

    const std::string& first_line = header_vec[0];
    std::vector<std::string> version_vec;
    string_split(first_line, " ", version_vec);
    if (version_vec.size() != 3) {
        log_errorf("version line error:%s", first_line.c_str());
        return -1;
    }

    request_->method_ = version_vec[0];
    request_->uri_ = version_vec[1];
    const std::string& version_info = version_vec[2];
    pos = version_info.find("HTTP");
    if (pos == version_info.npos) {
        log_errorf("http version error:%s", version_info.c_str());
        return -1;
    }

    request_->version_ = version_info.substr(pos+5);
    for (size_t index = 1; index < header_vec.size(); index++) {
        const std::string& info_line = header_vec[index];
        pos = info_line.find(":");
        if (pos == info_line.npos) {
            log_errorf("header line error:%s", info_line.c_str());
            return -1;
        }
        std::string key = info_line.substr(0, pos);
        std::string value = info_line.substr(pos+2);
        if (key == "Content-Length") {
            request_->content_length_ = atoi(value.c_str());
        }
        request_->headers_.insert(std::make_pair(key, value));
    }

    return 0;
}
