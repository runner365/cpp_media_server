#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP
#include "tcp/tcp_server.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <stdint.h>
#include <uv.h>

class websocket_session;

class websocket_server_callbackI
{
public:
    virtual void on_accept(std::shared_ptr<websocket_session> session) = 0;
    virtual void on_read(std::shared_ptr<websocket_session> session, const char* data, size_t len) = 0;
    virtual void on_close(std::shared_ptr<websocket_session> session) = 0;
};

class websocket_server : public tcp_server_callbackI
{
public:
    websocket_server(uv_loop_t* loop, uint16_t port, websocket_server_callbackI* cb);
    websocket_server(uv_loop_t* loop, uint16_t port, websocket_server_callbackI* cb
                , const std::string& key_file, const std::string& cert_file);
    virtual ~websocket_server();

public:
    void session_close(websocket_session* session);

private:
    virtual void on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle);

private:
    std::unique_ptr<tcp_server> tcp_svr_ptr_;
    std::unordered_map<std::string, std::shared_ptr<websocket_session>> sessions_;
    websocket_server_callbackI* cb_ = nullptr;
    std::string key_file_;
    std::string cert_file_;
    bool ssl_enable_ = false;
};

#endif