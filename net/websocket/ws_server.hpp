#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP
#include "ws_session.hpp"
#include "tcp_server.hpp"
#include "ws_service_imp.hpp"
#include <unordered_map>
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>

class websocket_server : public tcp_server_callbackI, public websocket_server_callbackI
{
public:
    websocket_server(boost::asio::io_context& io_context, uint16_t port, int imp_type);
    websocket_server(boost::asio::io_context& io_context, uint16_t port, int imp_type, const std::string& cert_file, const std::string& key_file);
    virtual ~websocket_server();

public://implement tcp_server_callbackI
    virtual void on_accept(int ret_code, boost::asio::ip::tcp::socket socket) override;

public://implement websocket_server_callbackI
    virtual void on_close(const std::string& session_key) override;

private:
    std::shared_ptr<tcp_server> server_;
    boost::asio::io_context& io_ctx_;
    int imp_type_ = 0;
    boost::asio::ssl::context ssl_ctx_;
    std::unordered_map< std::string, std::shared_ptr<websocket_session> > sessions_;
    bool ssl_enable_ = false;
};

#endif