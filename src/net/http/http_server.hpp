#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP
#include "tcp_server.hpp"
#include "tcp_session.hpp"
#include "data_buffer.hpp"
#include "http_common.hpp"
#include "stringex.hpp"
#include "logger.hpp"
#include "timer.hpp"
#include <uv.h>
#include <stdint.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <assert.h>

class http_session;
class http_callbackI
{
public:
    virtual void on_close(const std::string& endpoint) = 0;
    virtual HTTP_HANDLE_Ptr get_handle(http_request* request) = 0;
};

class http_server : public tcp_server_callbackI, public http_callbackI, public timer_interface
{
public:
    http_server(uv_loop_t* loop, uint16_t port);
    virtual ~http_server();

public:
    void add_get_handle(std::string uri, HTTP_HANDLE_Ptr handle_func);
    void add_post_handle(std::string uri, HTTP_HANDLE_Ptr handle_func);

public:
    virtual void on_timer() override;

protected://tcp_server_callbackI
    virtual void on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) override;

protected://http_callbackI
    virtual void on_close(const std::string& endpoint) override;
    virtual HTTP_HANDLE_Ptr get_handle(http_request* request) override;

private:
    std::shared_ptr<tcp_server> server_;
    std::unordered_map< std::string, std::shared_ptr<http_session> > session_ptr_map_;
    std::unordered_map< std::string, HTTP_HANDLE_Ptr > get_handle_map_;
    std::unordered_map< std::string, HTTP_HANDLE_Ptr > post_handle_map_;
};

#endif //HTTP_SERVER_HPP