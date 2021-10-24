#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "tcp_server.hpp"
#include "ws_session_pub.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <queue>

class websocket_server_callbackI
{
public:
    virtual void on_close(const std::string& session_key) = 0;
};

class websocket_session : public std::enable_shared_from_this<websocket_session>
{
public:
    websocket_session(boost::asio::io_context& io_ctx, boost::asio::ip::tcp::socket&& socket, websocket_server_callbackI* cb, std::string stream_id);
    websocket_session(boost::asio::io_context& io_ctx, boost::asio::ip::tcp::socket&& socket, boost::asio::ssl::context& ctx,
            websocket_server_callbackI* cb, std::string stream_id);
    ~websocket_session();

    void set_websocket_callback(ws_session_callback* cb);
    void run();
    void async_write(const char* data, int len);

private:
    void on_handshake(boost::beast::error_code ec);
    void on_accept(boost::beast::error_code ec);
    void do_read();
    void do_write();
    void on_read(boost::beast::error_code ec, size_t bytes_transferred);
    void on_write(boost::beast::error_code ec, size_t bytes_transferred);
    void close_session(boost::beast::error_code& ec);

private:
    boost::asio::io_context& io_ctx_;
    boost::beast::websocket::stream<boost::beast::tcp_stream>* ws_ = nullptr;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>* wss_ = nullptr;
    websocket_server_callbackI* server_cb_ = nullptr;
    ws_session_callback* ws_cb_            = nullptr;
    std::string stream_id_;
    bool closed_flag_ = false;
    bool sending_flag_   = false;

private:
    boost::beast::flat_buffer recv_buffer_;
    std::queue<boost::beast::flat_buffer> send_buffer_queue_;
};

#endif //WEBSOCKET_SESSION_HPP