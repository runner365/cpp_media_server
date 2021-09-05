#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include "tcp_session.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>

class tcp_session;

class tcp_server_callbackI
{
public:
    virtual void on_accept(int ret_code, boost::asio::ip::tcp::socket socket) = 0;
};

class tcp_server : public std::enable_shared_from_this<tcp_server>
{
public:
    tcp_server(boost::asio::io_context& io_context, uint16_t local_port, tcp_server_callbackI* callback):
        acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), local_port))
        , callback_(callback)
    {
    }
    virtual ~tcp_server()
    {
    }

public:
    void accept() {
        auto self(shared_from_this());
        acceptor_.async_accept(
            [self](boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
            {
                if (!ec) {
                    self->callback_->on_accept(0, std::move(socket));
                    return;
                }
                self->accept();
            }
        );
    }

private:
    boost::asio::ip::tcp::acceptor acceptor_;
    tcp_server_callbackI* callback_;
};

#endif //TCP_SERVER_HPP