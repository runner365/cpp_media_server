#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP
#include "ws28/Server.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <stdint.h>
#include <uv.h>

class websocket_session;

void on_connected_callback(ws28::Client* client, ws28::HTTPRequest& req, void* user_data);
void on_disconnect_callback(ws28::Client* client, void* user_data);
void on_client_data_callback(ws28::Client* client, char* data, size_t len, int opcode, void* user_data);

class websocket_server_callbackI
{
public:
    virtual void on_accept(websocket_session* session) = 0;
    virtual void on_read(websocket_session* session, const char* data, size_t len) = 0;
    virtual void on_close(websocket_session* session) = 0;
};

class websocket_server
{
friend void on_connected_callback(ws28::Client* client, ws28::HTTPRequest& req, void* user_data);
friend void on_disconnect_callback(ws28::Client* client, void* user_data);
friend void on_client_data_callback(ws28::Client* client, char* data, size_t len, int opcode, void* user_data);

public:
    websocket_server(uv_loop_t* loop, uint16_t port, websocket_server_callbackI* cb);
    virtual ~websocket_server();

private:
    ws28::Server ws_server_;
    websocket_server_callbackI* cb_ = nullptr;
};

#endif