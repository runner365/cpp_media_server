#ifndef TCP_SERVER_BASE_H
#define TCP_SERVER_BASE_H
#include "logger.hpp"
#include "data_buffer.hpp"
#include "tcp_pub.hpp"
#include "ipaddress.hpp"
#include <uv.h>
#include <memory>
#include <string>
#include <stdint.h>
#include <iostream>
#include <stdio.h>
#include <queue>
#include <sstream>

inline static void on_tcp_close(uv_handle_t* handle);
inline static void on_uv_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf);
inline static void on_uv_read(uv_stream_t* handle,
                       ssize_t nread,
                       const uv_buf_t* buf);
inline static void on_uv_write(uv_write_t* req, int status);

class tcp_session : public tcp_base_session
{
friend void on_tcp_close(uv_handle_t* handle);
friend void on_uv_alloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
friend void on_uv_read(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);
friend void on_uv_write(uv_write_t* req, int status);

public:
    tcp_session(uv_loop_t* loop,
            uv_stream_t* server_uv_handle,
            tcp_session_callbackI* callback):callback_(callback)
    {
        buffer_    = (char*)malloc(buffer_size_);
        uv_handle_ = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        uv_handle_->data = this;

        // Set the UV handle.
        int err = uv_tcp_init(loop, uv_handle_);
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw MediaServerError("uv_tcp_init() failed");
        }
        err = uv_accept(
          reinterpret_cast<uv_stream_t*>(server_uv_handle),
          reinterpret_cast<uv_stream_t*>(uv_handle_));
    
        if (err != 0) {
            delete uv_handle_;
            uv_handle_ = nullptr;
            free(buffer_);
            throw MediaServerError("uv_accept() failed");
        }
        int namelen = (int)sizeof(local_name_);
	    uv_tcp_getsockname(uv_handle_, &local_name_, &namelen);
        uv_tcp_getpeername(uv_handle_, &peer_name_, &namelen);
        close_ = false;
    }
    
    virtual ~tcp_session()
    {
        close();
        if (buffer_) {
            free(buffer_);
        }
    }

public:
    virtual void async_read() override {
        if (close_) {
            return;
        }

        if (!uv_handle_) {
            return;
        }
    
        int err = uv_read_start(
                            reinterpret_cast<uv_stream_t*>(uv_handle_),
                            static_cast<uv_alloc_cb>(on_uv_alloc),
                            static_cast<uv_read_cb>(on_uv_read));
    
        if (err != 0) {
            if (err == UV_EALREADY) {
                return;
            }
            throw MediaServerError("uv_read_start() failed");
        }
    }

    virtual void async_write(const char* data, size_t len) override {
        write_req_t* wr = (write_req_t*) malloc(sizeof(write_req_t));

        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        wr->buf = uv_buf_init(new_data, len);
        if (uv_write((uv_write_t*)wr, reinterpret_cast<uv_stream_t*>(uv_handle_), &wr->buf, 1, on_uv_write)) {
            free(new_data);
            throw MediaServerError("uv_write error");
        }
    }

    virtual void async_write(std::shared_ptr<data_buffer> buffer_ptr) override {
        this->async_write(buffer_ptr->data(), buffer_ptr->data_len());
    }

    virtual void close() override {
        if (close_) {
            return;
        }
        close_ = true;

        int err = uv_read_stop(reinterpret_cast<uv_stream_t*>(uv_handle_));
        if (err != 0) {
            throw MediaServerError("uv_read_stop error");
        }
        uv_close(reinterpret_cast<uv_handle_t*>(uv_handle_), static_cast<uv_close_cb>(on_tcp_close));
    }

    virtual std::string get_remote_endpoint() override {
        std::stringstream ss;
        uint16_t remoteport = 0;
        std::string remoteip = get_ip_str(&peer_name_, remoteport);

        ss << remoteip << ":" << remoteport;
        return ss.str();
    }

    virtual std::string get_local_endpoint() override {
        std::stringstream ss;
        uint16_t localport = 0;
        std::string localip = get_ip_str(&local_name_, localport);

        ss << localip << ":" << localport;
        return ss.str();
    }

private:
    void on_alloc(uv_buf_t* buf) {
        buf->base = buffer_;
        buf->len  = buffer_size_;
    }

    void on_read(ssize_t nread, const uv_buf_t* buf) {
        if (close_) {
            return;
        }
        if (nread <= 0) {
            callback_->on_read(-1, nullptr, 0);
            return;
        }
        callback_->on_read(0, buf->base, nread);
    }

    void on_write(write_req_t* req, int status) {
        write_req_t* wr;
      
        /* Free the read/write buffer and the request */
        wr = (write_req_t*) req;
        if (callback_ && !close_) {
            callback_->on_write(status, wr->buf.len);
        }
        free(wr->buf.base);
        free(wr);
    }

private:
    tcp_session_callbackI* callback_ = nullptr;
    uv_tcp_t* uv_handle_ = nullptr;
    struct sockaddr local_name_;
    struct sockaddr peer_name_;
    char* buffer_ = nullptr;
    size_t buffer_size_ = TCP_DEF_RECV_BUFFER_SIZE;
    bool close_ = false;
};

inline static void on_uv_alloc(uv_handle_t* handle,
                       size_t suggested_size,
                       uv_buf_t* buf) {
    tcp_session* session = (tcp_session*)handle->data;
    if (session) {
        session->on_alloc(buf);
    }
}

inline static void on_uv_read(uv_stream_t* handle, ssize_t nread, const uv_buf_t* buf)
{
    tcp_session* session = (tcp_session*)handle->data;
    if (session) {
        session->on_read(nread, buf);
    }
    return;
}

inline static void on_uv_write(uv_write_t* req, int status) {
    tcp_session* session = static_cast<tcp_session*>(req->handle->data);

    if (session) {
        session->on_write((write_req_t*)req, status);
    }
    return;
}

inline static void on_tcp_close(uv_handle_t* handle) {
    delete handle;
}

#endif //TCP_SERVER_BASE_H