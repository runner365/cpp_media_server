#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "tcp/tcp_session.hpp"
#include "utils/data_buffer.hpp"
#include "utils/uuid.hpp"
#include "ws_format.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

#define WS_RET_READ_MORE 100
#define WS_RET_OK        0

class websocket_server;
class websocket_server_callbackI;

class av_format_callback;
class flv_demuxer;

class websocket_session : public tcp_session_callbackI
{
public:
    websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                    websocket_server*, websocket_server_callbackI*);
    websocket_session(uv_loop_t* loop, uv_stream_t* handle,
                    websocket_server*, websocket_server_callbackI*,
                    const std::string& key_file, const std::string& cert_file);
    virtual ~websocket_session();

public:
    std::string method();
    websocket_server* get_server();
    void set_close(bool flag);
    bool is_close();
    std::string path();
    void set_uri(const std::string& uri);
    std::string get_uri();
    std::string get_uuid();
    std::string remote_address();

public:
    void send_data_text(const char* data, size_t len);
    
public:
    void incr_die_count() { die_count_++; }
    int get_die_count() { return die_count_; }

public:
    void* get_user_data() { return user_data_; }
    void set_user_data(void* output) { user_data_ = output; }

protected:
    virtual void on_write(int ret_code, size_t sent_size);
    virtual void on_read(int ret_code, const char* data, size_t data_size);

private:
    int on_handle_http_request();
    int send_http_response();
    void send_error_response();
    void gen_hashcode();
    void on_handle_frame(uint8_t* data, size_t len);
    void send_ws_frame(uint8_t* data, size_t len, uint8_t op_code);

private:
    void handle_ws_ping();
    void handle_ws_text(uint8_t* data, size_t len);
    void handle_ws_bin(uint8_t* data, size_t len);
    void handle_ws_close(uint8_t* data, size_t len);

private:
    void send_close(uint16_t code, const char *reason, size_t reason_len = 0);

private:
    bool http_request_ready_ = false;
    data_buffer http_recv_buffer_;
    std::unordered_map<std::string, std::string> headers_;
	std::string method_;
    std::string path_;
    std::string uri_;
    int sec_ws_ver_ = -1;
    std::string sec_ws_key_;
    std::string sec_ws_protocol_;
    std::string hash_code_;
    bool close_ = false;
    int die_count_ = 0;

private:
    std::string uuid_;
    websocket_server* server_ = nullptr;
    websocket_server_callbackI* cb_ = nullptr;
    std::unique_ptr<tcp_session> session_ptr_;
    std::vector<std::shared_ptr<data_buffer>> recv_buffer_vec_;

    websocket_frame frame_;

private:
    void* user_data_ = nullptr;
};

#endif //WEBSOCKET_SESSION_HPP
