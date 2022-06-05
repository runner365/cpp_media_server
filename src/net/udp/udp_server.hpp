#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP
#include "logger.hpp"
#include "data_buffer.hpp"
#include "ipaddress.hpp"
#include <sstream>
#include <memory>
#include <string>
#include <stdint.h>
#include <queue>
#include <uv.h>

#define UDP_DATA_BUFFER_MAX (10*1500)

typedef struct udp_req_info_s
{
    uv_udp_send_t handle;
    uv_buf_t buf;
    char ip[80];
    uint16_t port;
} udp_req_info_t;

inline void udp_alloc_callback(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
inline void udp_read_callback(uv_udp_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf,
                    const struct sockaddr* addr,
                    unsigned flags);

inline void udp_send_callback(uv_udp_send_t* req, int status);

class udp_tuple
{
public:
    udp_tuple() {
    }
    udp_tuple(const std::string& ip, uint16_t udp_port): ip_address(ip)
        , port(udp_port)
    {
    }
    ~udp_tuple(){
    }

    std::string to_string() const {
        std::string ret = ip_address;

        ret += ":";
        ret += std::to_string(port);
        return ret;
    }

public:
    std::string ip_address;
    uint16_t    port = 0;
};

class udp_session_callbackI
{
public:
    virtual void on_write(size_t sent_size, udp_tuple address) = 0;
    virtual void on_read(const char* data, size_t data_size, udp_tuple address) = 0;
};

class udp_server
{
friend void udp_alloc_callback(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf);
friend void udp_read_callback(uv_udp_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf,
                    const struct sockaddr* addr,
                    unsigned flags);
friend void udp_send_callback(uv_udp_send_t* req, int status);

public:
    udp_server(uv_loop_t* loop, uint16_t port, udp_session_callbackI* cb):cb_(cb)
                                                                        , loop_(loop)
    {
        recv_buffer_ = new char[UDP_DATA_BUFFER_MAX];

        uv_udp_init(loop, &udp_handle_);
        struct sockaddr_in recv_addr;
        uv_ip4_addr("0.0.0.0", port, &recv_addr);
        uv_udp_bind(&udp_handle_, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);
        udp_handle_.data = this;

        try_read();
    }

    ~udp_server() {
        if (recv_buffer_) {
            delete[] recv_buffer_;
            recv_buffer_ = nullptr;
        }
    }

public:
    uv_loop_t* get_loop() { return loop_; }

    void write(char* data, size_t len, udp_tuple remote_address) {
        struct sockaddr_in send_addr;
        udp_req_info_t* req = (udp_req_info_t*)malloc(sizeof(udp_req_info_t));
        uv_ip4_addr(remote_address.ip_address.c_str(), ntohs(remote_address.port), &send_addr);

        req->handle.data = this;

        char* new_data = (char*)malloc(len);
        memcpy(new_data, data, len);
        req->buf = uv_buf_init(new_data, len);

        memset(req->ip, 0, sizeof(req->ip));
        memcpy(req->ip, remote_address.ip_address.c_str(), remote_address.ip_address.size());
        req->port = remote_address.port;

        uv_udp_send((uv_udp_send_t*)req, &udp_handle_, &req->buf, 1,
                (const struct sockaddr *)&send_addr, udp_send_callback);
    }

private:
    void try_read() {
        int ret = 0;

        ret = uv_udp_recv_start(&udp_handle_, udp_alloc_callback, udp_read_callback);
        if (ret != 0) {
            if (ret == UV_EALREADY) {
                return;
            }
            std::stringstream err_ss;
            err_ss << "uv_udp_recv_start error:" << ret;
            throw MediaServerError(err_ss.str().c_str());
        }
    }

    void on_alloc(uv_buf_t* buf) {
        buf->base = recv_buffer_;
        buf->len  = UDP_DATA_BUFFER_MAX;
    }

    void on_read(uv_udp_t* handle,
            ssize_t nread,
            const uv_buf_t* buf,
            const struct sockaddr* addr,
            unsigned flags) {
        if (cb_) {
            if (nread > 0) {
                uint16_t remote_port = 0;
                std::string remote_ip = get_ip_str(addr, remote_port);
                udp_tuple addr_tuple(remote_ip, remote_port);
                cb_->on_read(buf->base, nread, addr_tuple);
            }
        }
        try_read();
    }

    void on_write(uv_udp_send_t* req, int status) {
        udp_tuple addr;
        if (status != 0) {
            if (cb_) {
                cb_->on_write(0, addr);
            }
            udp_req_info_t* wr = (udp_req_info_t*)req;
            if (wr) {
                free(wr);
                if (wr->buf.base) {
                    free(wr->buf.base);
                }
            }
            return;
        }
        
        udp_req_info_t* wr = (udp_req_info_t*)req;
        if (cb_) {
            addr.ip_address = wr->ip;
            addr.port       = wr->port;
            cb_->on_write(wr->buf.len, addr);
        }
        if (wr) {
            if (wr->buf.base) {
                free(wr->buf.base);
            }
            free(wr);
        }
    }

private:
    udp_session_callbackI* cb_ = nullptr;
    uv_loop_t* loop_ = nullptr;
    uv_udp_t udp_handle_;
    char* recv_buffer_ = nullptr;
};

inline void udp_alloc_callback(uv_handle_t* handle,
                    size_t suggested_size,
                    uv_buf_t* buf) {
    udp_server* server = (udp_server*)handle->data;
    if (server) {
        server->on_alloc(buf);
    }
}

inline void udp_read_callback(uv_udp_t* handle,
                    ssize_t nread,
                    const uv_buf_t* buf,
                    const struct sockaddr* addr,
                    unsigned flags) {
    udp_server* server = (udp_server*)handle->data;
    if (server) {
        server->on_read(handle, nread, buf, addr, flags);
    }
}

inline void udp_send_callback(uv_udp_send_t* req, int status) {
    udp_server* server = (udp_server*)req->data;
    if (server) {
        server->on_write(req, status);
    }
}

#endif