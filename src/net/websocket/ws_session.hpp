#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "tcp/tcp_session.hpp"
#include "utils/data_buffer.hpp"
#include "utils/uuid.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <memory>
#include <unordered_map>

#define WS_RET_READ_MORE 100
#define WS_RET_OK        0

class websocket_server;

class websocket_session : public tcp_session_callbackI
{
public:
    websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                    websocket_server*);
    websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                    websocket_server*,
                    const std::string& key_file, const std::string& cert_file);
    virtual ~websocket_session();

public:
    std::string method();
    websocket_server* get_server();
    void set_close(bool flag);
    bool is_close();
    std::string get_uri();
    std::string get_uuid();
    std::string remote_address();

protected:
    virtual void on_write(int ret_code, size_t sent_size);
    virtual void on_read(int ret_code, const char* data, size_t data_size);

private:
    int on_handle_http_request();
    int send_http_response();
    void send_error_response();
    void gen_hashcode();

private:
    bool http_request_ready_ = false;
    std::unordered_map<std::string, std::string> headers_;
	std::string method_;
    std::string uri_;
    int sec_ws_ver_ = -1;
    std::string sec_ws_key_;
    std::string sec_ws_protocol_;
    std::string hash_code_;
    bool close_ = false;

private:
    std::string uuid_;
    websocket_server* server_ = nullptr;
    std::unique_ptr<tcp_session> session_ptr_;
    data_buffer recv_buffer_;
};

#endif //WEBSOCKET_SESSION_HPP
