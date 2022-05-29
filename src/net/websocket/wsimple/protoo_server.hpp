#ifndef PROTOO_SERVER_HPP
#define PROTOO_SERVER_HPP
#include "protoo_pub.hpp"
#include "net/websocket/ws_server.hpp"
#include "json.hpp"
#include <stddef.h>
#include <memory>
#include <string>

using json = nlohmann::json;

class websocket_session;
class room_service;
class protoo_server : public websocket_server_callbackI, public protoo_request_interface
{
public:
    protoo_server(uv_loop_t* loop, uint16_t port);
    virtual ~protoo_server();

public://websocket_server_callbackI
    virtual void on_accept(websocket_session* session) override;
    virtual void on_read(websocket_session* session, const char* data, size_t len) override;
    virtual void on_close(websocket_session* session) override;

public://protoo_request_interface
    virtual void accept(const std::string& id, const std::string& data, void* ws_session) override;
    virtual void reject(const std::string& id, int err_code, const std::string& err, void* ws_session) override;
    virtual void notification(const std::string& method, const std::string& data, void* ws_session) override;

private:
    void on_request(websocket_session* session, json& protooBodyJson);
    void on_reponse(websocket_session* session, json& protooBodyJson);
    void on_notification(websocket_session* session, json& protooBodyJson);

private:
    std::string roomId_;
    std::string uid_;
    std::shared_ptr<room_service> ev_cb_ptr_;
    websocket_server server_;
};

#endif