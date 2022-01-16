#ifndef HTTP_COMMOM_HPP
#define HTTP_COMMOM_HPP
#include "tcp_session.hpp"
#include "data_buffer.hpp"
#include "http_session.hpp"
#include "logger.hpp"
#include <string>
#include <unordered_map>
#include <stdint.h>
#include <sstream>

class http_request
{
friend class http_session;
public:
    http_request(http_session* session) {
        session_ = session;
    }
    ~http_request() {
    }

public:
    void try_read(tcp_session_callbackI* callback) {
        callback_ = callback;
        session_->try_read();
        return;
    }

public:
    std::string method_;
    std::string uri_;
    std::string version_;
    char* content_body_ = nullptr;
    int content_length_ = 0;
    std::unordered_map<std::string, std::string> headers_;

private:
    http_session* session_;
    tcp_session_callbackI* callback_;
};

class http_response
{
public:
    http_response(http_session* session) {
        session_ = session;
    }
    ~http_response() {
    }

public:
    void set_status_code(int status_code) { status_code_ = status_code; }
    void set_status(const std::string& status) { status_ = status; }
    void add_header(const std::string& key, const std::string& value) { headers_[key] = value; }
    
    int write(const char* data, size_t len, bool continue_flag = false) {
        std::stringstream ss;

        if (is_close_ || session_ == nullptr) {
            return -1;
        }

        if (!written_header_) {
            ss << proto_ << "/" << version_ << " " << status_code_ << " " << status_ << "\r\n";
            if (!continue_flag) {
                ss << "Content-Length:" << len  << "\r\n";
            }
            
            for (const auto& header : headers_) {
                ss << header.first << ": " << header.second << "\r\n";
            }
            ss << "\r\n";
            session_->write(ss.str().c_str(), ss.str().length());
            written_header_ = true;
        }

        if (data && len > 0) {
            session_->write(data, len);
        }
        
        return 0;
    }

    void close() {
        if (is_close_) {
            return;
        }
        is_close_ = true;
        session_->close();
        session_ = nullptr;
    }

    void set_close(bool flag) {
        is_close_ = flag;
        session_ = nullptr;
    }

private:
    bool is_close_ = false;
    bool written_header_ = false;
    std::string status_  = "OK";  // e.g. "OK"
    int status_code_     = 200;     // e.g. 200
    std::string proto_   = "HTTP";   // e.g. "HTTP"
    std::string version_ = "1.1"; // e.g. "1.1"
    std::unordered_map<std::string, std::string> headers_; //headers

public:
    http_session* session_;
};

using HTTP_HANDLE_Ptr = void (*)(const http_request* request, std::shared_ptr<http_response> response_ptr);

#endif