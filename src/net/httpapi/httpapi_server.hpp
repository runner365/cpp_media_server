#ifndef HTTPAPI_H
#define HTTPAPI_H
#include "net/http/http_server.hpp"

class httpapi_server
{
public:
    httpapi_server(boost::asio::io_context& io_ctx, uint16_t port);
    ~httpapi_server();

private:
    void run();

private:
    http_server server_;
};

#endif

