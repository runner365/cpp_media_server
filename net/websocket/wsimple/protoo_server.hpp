#ifndef PROTOO_SERVER_HPP
#define PROTOO_SERVER_HPP
#include "net/websocket/ws_session_pub.hpp"
#include <stddef.h>

class protoo_server : public ws_session_callback
{
public:
    protoo_server();
    virtual ~protoo_server();

public:
    virtual void on_accept();
    virtual void on_read(const char* data, size_t len);
    virtual void on_writen(int len);
    virtual void on_close(int err_code);
};

#endif