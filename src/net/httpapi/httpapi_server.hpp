#ifndef HTTPAPI_H
#define HTTPAPI_H
#include "net/http/http_server.hpp"
#include <string>

class httpapi_server
{
public:
    httpapi_server(uv_loop_t* loop, uint16_t port);
    //httpapi_server(boost::asio::io_context& io_ctx, uint16_t port, const std::string& cert_file, const std::string& key_file);
    ~httpapi_server();

private:
    void run();

private:
    http_server server_;
};

#endif

