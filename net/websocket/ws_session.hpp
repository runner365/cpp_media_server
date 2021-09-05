#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "tcp_server.hpp"
#include "av_outputer.hpp"
#include "flv_demux.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>

class websocket_server_callbackI
{
public:
    virtual void on_close(const std::string& session_key) = 0;
};

class websocket_session : public std::enable_shared_from_this<websocket_session>
{
public:
    websocket_session(boost::asio::ip::tcp::socket socket, websocket_server_callbackI* cb, std::string stream_id);
    ~websocket_session();

public:
    void run();
    void on_accept(boost::beast::error_code ec);
    void do_read();
    void on_read(boost::beast::error_code ec, size_t bytes_transferred);
    void close_session(boost::beast::error_code& ec);

private:
    boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
    websocket_server_callbackI* server_cb_ = nullptr;
    std::string stream_id_;
    boost::beast::flat_buffer buffer_;
    std::string uri_;

private:
   flv_demuxer demuxer_;
   av_outputer outputer_;
};

#endif //WEBSOCKET_SESSION_HPP