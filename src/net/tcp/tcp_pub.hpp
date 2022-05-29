#ifndef TCP_PUB_HPP
#define TCP_PUB_HPP
#include "uv.h"
#include <stdint.h>
#include <stddef.h>
#include <string>

#define TCP_DEF_RECV_BUFFER_SIZE (8*1024)

typedef struct {
  uv_write_t req;
  uv_buf_t buf;
} write_req_t;

class tcp_client_callback
{
public:
    virtual void on_connect(int ret_code) = 0;
    virtual void on_write(int ret_code, size_t sent_size) = 0;
    virtual void on_read(int ret_code, const char* data, size_t data_size) = 0;
};

class tcp_server_callbackI
{
public:
    virtual void on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) = 0;
};

class tcp_session_callbackI
{
public:
    virtual void on_write(int ret_code, size_t sent_size) = 0;
    virtual void on_read(int ret_code, const char* data, size_t data_size) = 0;
};

class tcp_base_session
{
public:
    virtual void async_write(const char* data, size_t data_size) = 0;
    virtual void async_write(std::shared_ptr<data_buffer> buffer_ptr) = 0;
    virtual void async_read() = 0;
    virtual void close() = 0;
    virtual std::string get_remote_endpoint() = 0;
    virtual std::string get_local_endpoint() = 0;
};

#endif