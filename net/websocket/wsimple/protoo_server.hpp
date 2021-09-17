#ifndef PROTOO_SERVER_HPP
#define PROTOO_SERVER_HPP
#include "net/websocket/ws_session_pub.hpp"
#include "protoo_pub.hpp"
#include "json.hpp"
#include <stddef.h>
#include <memory>
#include <string>

using json = nlohmann::json;

class websocket_session;
class protoo_server : public ws_session_callback, public protoo_request_interface
{
public:
    protoo_server(websocket_session* ws_session);
    virtual ~protoo_server();

public:
    virtual void on_accept();
    virtual void on_read(const char* data, size_t len);
    virtual void on_writen(int len);
    virtual void on_close(int err_code);

public:
    virtual void accept(const std::string& id, const std::string& data);
    virtual void reject(const std::string& id, int err_code, const std::string& err);
    virtual void notification(const std::string& method, const std::string& data);

private:
    void on_request(json& protooBodyJson);
    void on_reponse(json& protooBodyJson);
    void on_notification(json& protooBodyJson);

private:
    std::string roomId_;
    std::string uid_;
    std::shared_ptr<protoo_event_callback> ev_cb_ptr_;
    websocket_session* ws_session_ = nullptr;
};

#endif