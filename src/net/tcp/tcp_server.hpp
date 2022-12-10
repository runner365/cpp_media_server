#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include "tcp_session.hpp"
#include "tcp_pub.hpp"
#include <uv.h>
#include <memory>
#include <string>
#include <stdint.h>
#include <cstdlib>
#include <functional>
#include <iostream>

class tcp_server;

inline void on_uv_connection(uv_stream_t* handle, int status);
inline void on_uv_server_close(uv_handle_t* handle);

class tcp_server
{
friend void on_uv_connection(uv_stream_t* handle, int status);
friend void on_uv_server_close(uv_handle_t* handle);

public:
    tcp_server(uv_loop_t* loop,
        uint16_t local_port,
        tcp_server_callbackI* callback):loop_(loop)
                                    , callback_(callback)
                                    , closed_(false)
    {
        uv_ip4_addr("0.0.0.0", local_port, &server_addr_);
        uv_tcp_init(loop_, &server_handle_);

        uv_tcp_bind(&server_handle_, (const struct sockaddr*)&server_addr_, 0);

        server_handle_.data = this;
        uv_listen((uv_stream_t*)&server_handle_, SOMAXCONN, on_uv_connection);
    }

    virtual ~tcp_server()
    {
        std::cout << "tcp server destruct...\r\n";
    }

    void close() {
        if (closed_) {
            return;
        }
        closed_ = true;
        server_handle_.data = nullptr;

        uv_close(reinterpret_cast<uv_handle_t*>(&server_handle_), static_cast<uv_close_cb>(on_uv_server_close));
    }

private:
    void on_connection(int status, uv_stream_t* handle) {
        if (callback_) {
            callback_->on_accept(status, loop_, handle);
        }
    }

private:
    uv_loop_t* loop_ = nullptr;
    tcp_server_callbackI* callback_ = nullptr;
    uv_tcp_t server_handle_;
    struct sockaddr_in server_addr_;
    bool closed_ = false;
};

inline void on_uv_connection(uv_stream_t* handle, int status) {
    auto* server = static_cast<tcp_server*>(handle->data);
    if (server) {
        server->on_connection(status, handle);
    }
}

inline void on_uv_server_close(uv_handle_t* handle) {
    delete handle;
}

#endif //TCP_SERVER_HPP