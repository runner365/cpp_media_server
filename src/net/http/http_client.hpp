#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP
#include "http_common.hpp"
#include "net/tcp/tcp_client.hpp"
#include "net/tcp/tcp_pub.hpp"
#include "utils/data_buffer.hpp"
#include <string>
#include <memory>
#include <map>
#include <uv.h>

typedef enum {
    HTTP_GET,
    HTTP_POST
} HTTP_METHOD;

class http_client_response
{
public:
    std::string status_  = "OK";  // e.g. "OK"
    int status_code_     = 200;     // e.g. 200
    std::string proto_   = "HTTP";   // e.g. "HTTP"
    std::string version_ = "1.1"; // e.g. "1.1"
    std::map<std::string, std::string> headers_; //headers
    int content_length_  = 0;
    data_buffer data_;
    bool header_ready_ = false;
    bool body_ready_   = false;
};

class http_client_callbackI
{
public:
    virtual void on_http_read(int ret, std::shared_ptr<http_client_response> resp_ptr) = 0;
};

class http_client : public tcp_client_callback
{
public:
    http_client(uv_loop_t* loop, const std::string& host, uint16_t port, http_client_callbackI* cb);
    virtual ~http_client();

public:
    int get(const std::string& subpath, std::map<std::string, std::string> headers);
    int post(const std::string& subpath, std::map<std::string, std::string> headers, const std::string& data);
    void close();
    
private:
    virtual void on_connect(int ret_code) override;
    virtual void on_write(int ret_code, size_t sent_size) override;
    virtual void on_read(int ret_code, const char* data, size_t data_size) override;

private:
    tcp_client* client_ = nullptr;
    std::string host_;
    uint16_t port_ = 0;
    HTTP_METHOD method_ = HTTP_GET;
    std::string subpath_;
    http_client_callbackI* cb_;
    std::string post_data_;
    std::shared_ptr<http_client_response> resp_ptr_;
};

#endif