#include "http_client.hpp"
#include "utils/logger.hpp"
#include "utils/stringex.hpp"

#include <string>
#include <sstream>
#include <uv.h>

http_client::http_client(uv_loop_t* loop,
                        const std::string& host,
                        uint16_t port,
                        http_client_callbackI* cb): host_(host)
                                            , port_(port)
                                            , cb_(cb)
{
    client_ = new tcp_client(loop, this);
}

http_client::~http_client()
{
    if (client_) {
        delete client_;
        client_ = nullptr;
    }
}

int http_client::get(const std::string& subpath, std::map<std::string, std::string> headers) {
    method_  = HTTP_GET;
    subpath_ = subpath;

    log_infof("http get connect host:%s, port:%d, subpath:%s", host_.c_str(), port_, subpath.c_str());
    client_->connect(host_, port_);
    return 0;
}

int http_client::post(const std::string& supath, std::map<std::string, std::string> headers, const std::string& data) {
    method_    = HTTP_POST;
    subpath_   = supath;
    post_data_ = data;

    client_->connect(host_, port_);
    return 0;
}

void http_client::close() {
    log_infof("http close...");
    client_->close();
}

void http_client::on_connect(int ret_code) {
    std::stringstream http_stream;

    log_infof("on connect code:%d", ret_code);
    if (method_ == HTTP_GET) {
        http_stream << "GET " << subpath_ << " HTTP/1.1\r\n";
    } else if (method_ == HTTP_POST) {
        http_stream << "POST " << subpath_ << " HTTP/1.1\r\n";
    } else {
        MS_THROW_ERROR("unkown http method:%d", method_);
    }
    http_stream << "Accept: */*\r\n";
    http_stream << "Host: " << host_ << "\r\n";
    if (method_ == HTTP_POST) {
        http_stream << "Content-Length: " << post_data_.length() << "\r\n";
    }
    http_stream << "\r\n";
    if (method_ == HTTP_POST) {
        http_stream << post_data_;
    }
    client_->send(http_stream.str().c_str(), http_stream.str().length());
}

void http_client::on_write(int ret_code, size_t sent_size) {
    if (ret_code == 0) {
        log_infof("on write sent size:%lu", sent_size);
        client_->async_read();
    }
}

void http_client::on_read(int ret_code, const char* data, size_t data_size) {
    if (ret_code < 0) {
        cb_->on_http_read(ret_code, resp_ptr_);
        return;
    }

    if (data_size == 0) {
        cb_->on_http_read(-2, resp_ptr_);
        return;
    }

    if (!resp_ptr_) {
        resp_ptr_ = std::make_shared<http_client_response>();
    }

    resp_ptr_->data_.append_data(data, data_size);

    if (!resp_ptr_->header_ready_) {
        std::string header_str(resp_ptr_->data_.data(), resp_ptr_->data_.data_len());
        size_t pos = header_str.find("\r\n\r\n");
        if (pos != std::string::npos) {
            std::vector<std::string> lines_vec;
            resp_ptr_->header_ready_ = true;
            header_str = header_str.substr(0, pos);
            resp_ptr_->data_.consume_data(pos + 4);

            string_split(header_str, "\r\n", lines_vec);
            for (size_t i = 0; i < lines_vec.size(); i++) {
                if (i == 0) {
                    std::vector<std::string> item_vec;
                    string_split(lines_vec[0], " ", item_vec);
                    CMS_ASSERT(item_vec.size() == 3, "http response header error: %s", lines_vec[0].c_str());

                    pos = item_vec[0].find("/");
                    CMS_ASSERT(pos != std::string::npos, "http response header error: %s", lines_vec[0].c_str());
                    resp_ptr_->proto_   = item_vec[0].substr(0, pos);
                    resp_ptr_->version_ = item_vec[0].substr(pos+1);
                    resp_ptr_->status_code_ = atoi(item_vec[1].c_str());
                    resp_ptr_->status_      = item_vec[2];
                    continue;
                }
                pos = lines_vec[i].find(":");
                CMS_ASSERT(pos != std::string::npos, "http response header error: %s", lines_vec[i].c_str());
                std::string key   = lines_vec[i].substr(0, pos);
                std::string value = lines_vec[i].substr(pos + 1);

                if (key == "Content-Length") {
                    resp_ptr_->content_length_ = atoi(value.c_str());
                }
                resp_ptr_->headers_[key] = value;
                log_infof("header: %s: %s", key.c_str(), value.c_str());
            }
        } else {
            log_infof("header not ready, read more");
            client_->async_read();
            return;
        }
    }

    if (resp_ptr_->content_length_ > 0) {
        log_infof("recv data len:%lu, content len:%d", resp_ptr_->data_.data_len(), resp_ptr_->content_length_);
        if (resp_ptr_->data_.data_len() >= resp_ptr_->content_length_) {
            resp_ptr_->body_ready_ = true;
            cb_->on_http_read(0, resp_ptr_);
        } else {
            log_infof("content not ready, read more");
            client_->async_read();
        }
    } else {
        cb_->on_http_read(0, resp_ptr_);
        client_->async_read();
    }
}
