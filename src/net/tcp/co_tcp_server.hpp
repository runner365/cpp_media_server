#ifndef CO_TCP_SERVER_HPP
#define CO_TCP_SERVER_HPP
#include "uv.h"
#include "cpp20_coroutine.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <iostream>
#include <queue>

class CoTcpServer;

class CoServerBase
{
public:
    virtual uv_stream_t* get_stream_handle() = 0;
    #ifdef __APPLE__
    virtual void set_continuation(std::experimental::coroutine_handle<>) = 0;
    virtual std::experimental::coroutine_handle<> get_continuation() = 0;
    #else
    virtual void set_continuation(std::coroutine_handle<>) = 0;
    virtual std::coroutine_handle<> get_continuation() = 0;
    #endif
};


struct tcp_conn_awaitable_res {
    tcp_conn_awaitable_res(CoServerBase* server):server_(server)
    {
    }

    auto operator co_await() {
        struct uio_awaiter {
            uio_awaiter(CoServerBase* server) : server_(server)
            {
            }
            constexpr bool await_ready() const noexcept { return false; }

#ifdef __APPLE__
            void await_suspend(std::experimental::coroutine_handle<> continuation) noexcept {
#else
            void await_suspend(std::coroutine_handle<> continuation) noexcept {
#endif
                server_->set_continuation(continuation);
            }

            uv_stream_t* await_resume() {
                return server_->get_stream_handle();
            }

            CoServerBase* server_ = nullptr;
        };
        return uio_awaiter(this->server_);
    }

    CoServerBase* server_ = nullptr;
};

class CoTcpServer : public CoServerBase
{
public:
    CoTcpServer(uv_loop_t* loop,
        uint16_t local_port):loop_(loop)
    {
        uv_ip4_addr("0.0.0.0", local_port, &server_addr_);
        uv_tcp_init(loop_, &server_handle_);

        uv_tcp_bind(&server_handle_, (const struct sockaddr*)&server_addr_, 0);
    }
    virtual ~CoTcpServer()
    {
    }

public:
    virtual uv_stream_t* get_stream_handle() override {
        if (this->conn_streams_.empty()) {
            return nullptr;
        }
        uv_stream_t* ret = this->conn_streams_.front();
        this->conn_streams_.pop();
        return ret;
    }
    #ifdef __APPLE__
    virtual void set_continuation(std::experimental::coroutine_handle<> continuation) override {
    #else
    virtual void set_continuation(std::coroutine_handle<> continuation) override {
    #endif
        this->continuation_ = continuation;
    }

    #ifdef __APPLE__
    virtual std::experimental::coroutine_handle<> get_continuation() override {
    #else
    virtual std::coroutine_handle<> get_continuation() override {
    #endif
        return this->continuation_;
    }

public:
    uv_loop_t* get_loop() {
        return loop_;
    }

    static void on_uv_connection(uv_stream_t* handle, int status) {
        CoTcpServer* server = static_cast<CoTcpServer*>(handle->data);
        server->conn_streams_.push(handle);

        if (server->suspend_flag_) {
            server->suspend_flag_ = false;
            server->continuation_.resume();
        }
        
        return;
    }

    tcp_conn_awaitable_res co_accept() {
        if (!listen_flag_) {
            listen_flag_ = true;
            server_handle_.data = this;
            ::uv_listen((uv_stream_t*)&server_handle_, SOMAXCONN,
                    &CoTcpServer::on_uv_connection);
        }

        suspend_flag_ = true;
        return tcp_conn_awaitable_res(this);
    }

public:
#ifdef __APPLE__
    std::experimental::coroutine_handle<> continuation_;
#else
    std::coroutine_handle<> continuation_;
#endif
    bool suspend_flag_ = false;
    std::queue<uv_stream_t*> conn_streams_;

private:
    uv_loop_t* loop_ = nullptr;
    uv_tcp_t server_handle_;
    struct sockaddr_in server_addr_;
    bool listen_flag_ = false;
};

#endif
