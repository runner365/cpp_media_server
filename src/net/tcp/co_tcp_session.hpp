#ifndef CO_TCP_SESSION_HPP
#define CO_TCP_SESSION_HPP
#include "uv.h"
#include "cpp20_coroutine.hpp"

#include <stdint.h>
#include <stddef.h>
#include <string>
#include <queue>
#include <cstring>

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} co_write_req_t;

class RecvData {
public:
    uint8_t* data_ = nullptr;
    size_t len_ = 0;
};

class CoSessionBase {
public:
    #ifdef __APPLE__
    virtual void set_send_continuation(std::experimental::coroutine_handle<>) = 0;
    virtual std::experimental::coroutine_handle<> get_send_continuation() = 0;
    virtual void set_recv_continuation(std::experimental::coroutine_handle<>) = 0;
    virtual std::experimental::coroutine_handle<> get_recv_continuation() = 0;
    #else
    virtual void set_send_continuation(std::coroutine_handle<>) = 0;
    virtual std::coroutine_handle<> get_send_continuation() = 0;
    virtual void set_recv_continuation(std::coroutine_handle<>) = 0;
    virtual std::coroutine_handle<> get_recv_continuation() = 0;
    #endif
    virtual size_t recv_queue_size() = 0;
    virtual RecvData get_recvdata() = 0;
    virtual long sent_length() = 0;
};

struct tcp_recvdata_awaitable_res
{
    tcp_recvdata_awaitable_res(CoSessionBase* server):server_(server)
    {
    }

    auto operator co_await() {
        struct uio_awaiter {
            uio_awaiter(CoSessionBase* server) : server_(server)
            {
            }
            constexpr bool await_ready() const noexcept { return server_->recv_queue_size() > 0; }

#ifdef __APPLE__
            void await_suspend(std::experimental::coroutine_handle<> continuation) noexcept {
#else
            void await_suspend(std::coroutine_handle<> continuation) noexcept {
#endif
                server_->set_recv_continuation(continuation);
            }

            RecvData await_resume() {
                return server_->get_recvdata();
            }

            CoSessionBase* server_ = nullptr;
        };
        return uio_awaiter(this->server_);
    }

    CoSessionBase* server_ = nullptr;
};

struct tcp_sendreturn_awaitable_res
{
    tcp_sendreturn_awaitable_res(CoSessionBase* server):server_(server)
    {
    }

    auto operator co_await() {
        struct uio_awaiter {
            uio_awaiter(CoSessionBase* server) : server_(server)
            {
            }
            constexpr bool await_ready() const noexcept { return false; }

#ifdef __APPLE__
            void await_suspend(std::experimental::coroutine_handle<> continuation) noexcept {
#else
            void await_suspend(std::coroutine_handle<> continuation) noexcept {
#endif
                server_->set_send_continuation(continuation);
            }

            long await_resume() {
                return server_->sent_length();
            }

            CoSessionBase* server_ = nullptr;
        };
        return uio_awaiter(this->server_);
    }

    CoSessionBase* server_ = nullptr;
};

class CoTcpSession : public CoSessionBase
{
public:
    CoTcpSession(uv_loop_t* loop, uv_stream_t* server_uv_handle)
    {
        uv_handle_ = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_handle_->data = this;

        // Set the UV handle.
        int err = uv_tcp_init(loop, uv_handle_);
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            throw MediaServerError("uv_tcp_init() failed");
        }
        err = uv_accept(
          reinterpret_cast<uv_stream_t*>(server_uv_handle),
          reinterpret_cast<uv_stream_t*>(uv_handle_));
    
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            throw MediaServerError("uv_accept() failed");
        }
        int namelen = (int)sizeof(local_name_);
	    uv_tcp_getsockname(uv_handle_, &local_name_, &namelen);
        uv_tcp_getpeername(uv_handle_, &peer_name_, &namelen);
        close_ = false;
    }
    virtual ~CoTcpSession()
    {
        close();
    }

public:
    tcp_recvdata_awaitable_res sync_read() {
        if (close_) {
            recv_suspend_flag_ = true;
            return tcp_recvdata_awaitable_res(this);
        }

        if (!uv_handle_) {
            recv_suspend_flag_ = true;
            return tcp_recvdata_awaitable_res(this);
        }
        
        if (!read_start_) {
            read_start_ = true;
            int err = uv_read_start(reinterpret_cast<uv_stream_t*>(uv_handle_),
                                static_cast<uv_alloc_cb>(&CoTcpSession::on_co_uv_alloc),
                                static_cast<uv_read_cb>(&CoTcpSession::on_co_uv_read));

            if (err != 0) {
                if (err == UV_EALREADY) {
                    recv_suspend_flag_ = true;
                    return tcp_recvdata_awaitable_res(this);
                }
                throw MediaServerError("uv_read_start() failed");
            }
        }
        std::cout << "sync_read data queue:" << recv_queue_.size() << "\r\n";

        recv_suspend_flag_ = true;
        return tcp_recvdata_awaitable_res(this);
    }

    tcp_sendreturn_awaitable_res sync_write(const uint8_t* data, size_t len) {
        co_write_req_t* wr = (co_write_req_t*) malloc(sizeof(co_write_req_t));

        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        wr->buf = uv_buf_init(new_data, len);
        if (uv_write((uv_write_t*)wr, reinterpret_cast<uv_stream_t*>(uv_handle_), &wr->buf, 1, &CoTcpSession::on_co_uv_write)) {
            free(new_data);
            throw MediaServerError("uv_write error");
        }
        send_suspend_flag_ = true;
        return tcp_sendreturn_awaitable_res(this);
    }

    void close() {
        if (close_) {
            return;
        }
        close_ = true;

        int err = uv_read_stop(reinterpret_cast<uv_stream_t*>(uv_handle_));
        if (err != 0) {
            throw MediaServerError("uv_read_stop error");
        }
        uv_close(reinterpret_cast<uv_handle_t*>(uv_handle_), static_cast<uv_close_cb>(CoTcpSession::on_co_tcp_close));
    }

    void on_alloc(uv_buf_t* buf) {
        const size_t BUFF_SIZE = 10*1024;
        buf->base = (char*)malloc(BUFF_SIZE);
        buf->len  = BUFF_SIZE;
    }

    void on_read(ssize_t nread, const uv_buf_t* buf) {
        RecvData recv_data;

        if (close_) {
            recv_data.data_ = nullptr;
            recv_data.len_  = 0;
            recv_queue_.push(recv_data);
            if (recv_suspend_flag_) {
                recv_suspend_flag_ = false;
                recv_continuation_.resume();
            }
            return;
        }
        if (nread == 0) {
            recv_data.data_ = nullptr;
            recv_data.len_  = 0;
            recv_queue_.push(recv_data);
            if (recv_suspend_flag_) {
                recv_suspend_flag_ = false;
                recv_continuation_.resume();
            }
            return;
        }
        if (nread < 0) {
            recv_data.data_ = nullptr;
            recv_data.len_  = 0;
            recv_queue_.push(recv_data);
            if (recv_suspend_flag_) {
                recv_suspend_flag_ = false;
                recv_continuation_.resume();
            }
            return;
        }
        recv_total_ += nread;
        std::cout << "session recv total:" << recv_total_ << "\r\n";
        recv_data.len_  = nread;
        recv_data.data_ = (uint8_t*)malloc(nread);
        memcpy(recv_data.data_, (uint8_t*)(buf->base), nread);
        recv_queue_.push(recv_data);

        free(buf->base);
        if (recv_suspend_flag_) {
            recv_suspend_flag_ = false;
            recv_continuation_.resume();
        }
    }

    void on_write(co_write_req_t* req, int status) {
        co_write_req_t* wr = (co_write_req_t*) req;

        if (close_) {
            sent_length_ = 0;
            if (send_suspend_flag_) {
                send_suspend_flag_ = false;
                send_continuation_.resume();
            }
            return;
        }
        sent_length_ = wr->buf.len;

        free(wr->buf.base);
        free(wr);
        if (send_suspend_flag_) {
            send_suspend_flag_ = false;
            send_continuation_.resume();
        }
        return;
    }

public:
    #ifdef __APPLE__
    virtual void set_send_continuation(std::experimental::coroutine_handle<> continuation) override {
    #else
    virtual void set_send_continuation(std::coroutine_handle<> continuation) override {
    #endif
        send_continuation_ = continuation;
    }

    #ifdef __APPLE__
    virtual std::experimental::coroutine_handle<> get_send_continuation() override {
    #else
    virtual std::coroutine_handle<> get_send_continuation() override {
    #endif
        return send_continuation_;
    }

    #ifdef __APPLE__
    virtual void set_recv_continuation(std::experimental::coroutine_handle<> continuation) override {
    #else
    virtual void set_recv_continuation(std::coroutine_handle<> continuation) override {
    #endif
        recv_continuation_ = continuation;
    }

    #ifdef __APPLE__
    virtual std::experimental::coroutine_handle<> get_recv_continuation() override {
    #else
    virtual std::coroutine_handle<> get_recv_continuation() override {
    #endif
        return recv_continuation_;
    }

    virtual size_t recv_queue_size() override {
        return recv_queue_.size();
    }

    virtual RecvData get_recvdata() override {
        RecvData recv_data;
        if (recv_queue_.empty()) {
            return recv_data;
        }
        recv_data = recv_queue_.front();
        recv_queue_.pop();

        return recv_data;
    }

    virtual long sent_length() override {
        return sent_length_;
    }

private:
    static void on_co_tcp_close(uv_handle_t* handle) {
        delete handle;
    }
    
    static void on_co_uv_alloc(uv_handle_t* handle,
                           size_t suggested_size,
                           uv_buf_t* buf) {
        CoTcpSession* session = (CoTcpSession*)handle->data;
        if (session) {
            session->on_alloc(buf);
        }
    }

    static void on_co_uv_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf) {
        CoTcpSession* session = (CoTcpSession*)handle->data;
        if (session) {
            if (nread == 0) {
                return;
            }
            session->on_read(nread, buf);
        }
        return;
    }

    static void on_co_uv_write(uv_write_t* req, int status) {
        CoTcpSession* session = static_cast<CoTcpSession*>(req->handle->data);
    
        if (session) {
            session->on_write((co_write_req_t*)req, status);
        }
        return;
    }

private:
    uv_tcp_t* uv_handle_ = nullptr;
    struct sockaddr local_name_;
    struct sockaddr peer_name_;
    bool close_          = false;

private:
#ifdef __APPLE__
    std::experimental::coroutine_handle<> send_continuation_;
    std::experimental::coroutine_handle<> recv_continuation_;
#else
    std::coroutine_handle<> send_continuation_;
    std::coroutine_handle<> recv_continuation_;
#endif
    bool recv_suspend_flag_ = false;
    bool send_suspend_flag_ = false;
    std::queue<RecvData> recv_queue_;
    long sent_length_ = -1;
    bool read_start_ = false;

private:
    size_t recv_total_ = 0;
    
};

#endif