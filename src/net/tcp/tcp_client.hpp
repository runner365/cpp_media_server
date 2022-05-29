#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include "data_buffer.hpp"
#include "utils/logger.hpp"
#include "tcp_pub.hpp"
#include <uv.h>
#include <memory>
#include <queue>
#include <string>
#include <stdint.h>
#include <sstream>

inline void on_uv_client_connected(uv_connect_t *conn, int status);
inline void on_uv_client_write(uv_write_t* req, int status);
inline void on_uv_client_alloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
inline void on_uv_client_read(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);

class tcp_client
{
friend void on_uv_client_connected(uv_connect_t *conn, int status);
friend void on_uv_client_write(uv_write_t* req, int status);
friend void on_uv_client_alloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
friend void on_uv_client_read(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf);
public:
    tcp_client(uv_loop_t* loop,
        tcp_client_callback* callback) : callback_(callback)
    {   
        client_  = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
        connect_ = (uv_connect_t*)malloc(sizeof(uv_connect_t));

        uv_tcp_init(loop, client_);

        buffer_ = (char*)malloc(buffer_size_);
    }

    virtual ~tcp_client() {
        free(buffer_);
        buffer_ = nullptr;
    }

    void connect(const std::string& host, uint16_t dst_port) {
        int r = 0;

        if ((r = uv_ip4_addr(host.c_str(), dst_port, &dst_addr_)) != 0) {
            throw MediaServerError("connect address error");
        }
        connect_->data = this;

        if ((r = uv_tcp_connect(connect_, client_,
                            (const struct sockaddr *)&dst_addr_,
                            on_uv_client_connected)) != 0) {
            throw MediaServerError("connect address error");
        }
        return;
    }

    void send(const char* data, size_t len) {
        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);

        write_req_t *req = (write_req_t*) malloc(sizeof(write_req_t));
        req->buf = uv_buf_init(new_data, len);

        connect_->handle->data = this;
        if (uv_write((uv_write_t*)req, connect_->handle, &req->buf, 1, on_uv_client_write)) {
            free(new_data);
            throw MediaServerError("uv_write error");
        }
        return;
    }

    void async_read() {
        int r = 0;

        if ((r = uv_read_start(connect_->handle, on_uv_client_alloc, on_uv_client_read)) != 0) {
            if (r == UV_EALREADY) {
                return;
            }
            std::stringstream err_ss;
            err_ss << "uv_read_start error:" << r;
            throw MediaServerError(err_ss.str().c_str());
        }
    }

    void close() {
        is_connect_ = false;
    }

    bool is_connect() {
        return is_connect_;
    }

private:
    void on_connect(int status) {
        if (status == 0) {
            is_connect_ = true;
        }
        if (callback_) {
            callback_->on_connect(status);
        }
    }

    void on_alloc(uv_buf_t* buf) {
        buf->base = buffer_;
        buf->len  = buffer_size_;
    }

    void on_write(write_req_t* req, int status) {
        write_req_t* wr;
      
        /* Free the read/write buffer and the request */
        wr = (write_req_t*) req;
        if (callback_) {
            callback_->on_write(status, wr->buf.len);
        }
        free(wr->buf.base);
        free(wr);
    }

    void on_read(ssize_t nread, const uv_buf_t* buf) {
        if (nread <= 0) {
            callback_->on_read(-1, nullptr, 0);
            return;
        }
        callback_->on_read(0, buf->base, nread);
    }

private:
    struct sockaddr_in dst_addr_;
    uv_tcp_t* client_ = nullptr;
    uv_connect_t* connect_ = nullptr;
    tcp_client_callback* callback_ = nullptr;
    
    char* buffer_ = nullptr;
    size_t buffer_size_ = 2048;
    bool is_connect_ = false;
};

inline void on_uv_client_connected(uv_connect_t *conn, int status) {
    tcp_client* client = (tcp_client*)conn->data;
    if (client) {
        client->on_connect(status);
    }
}

inline void on_uv_client_write(uv_write_t* req, int status) {
    tcp_client* client = static_cast<tcp_client*>(req->handle->data);

    log_infof("uv write callback status:%d, client:%p", status, client);
    if (client) {
        client->on_write((write_req_t*)req, status);
    }
    return;
}

inline void on_uv_client_alloc(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf)
{
    tcp_client* client = (tcp_client*)handle->data;
    if (client) {
        client->on_alloc(buf);
    }
}

inline void on_uv_client_read(uv_stream_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf)
{
    tcp_client* client = (tcp_client*)handle->data;
    if (client) {
        client->on_read(nread, buf);
    }
    return;
}
#endif //TCP_CLIENT_H