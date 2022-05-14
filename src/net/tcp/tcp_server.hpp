#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP
#include "tcp_session.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>

#include <boost/asio/ssl.hpp>

class tcp_session;

class tcp_server_callbackI
{
public:
    virtual void on_accept(int ret_code, boost::asio::ip::tcp::socket socket) = 0;
    virtual void on_accept_ssl(int ret_code, boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket) = 0;
};

class tcp_server : public std::enable_shared_from_this<tcp_server>
{
public:
    tcp_server(boost::asio::io_context& io_context,
        uint16_t local_port,
        tcp_server_callbackI* callback):ssl_enable_(false)
        , context_(boost::asio::ssl::context::sslv23)
        , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), local_port))
        , callback_(callback)
    {
    }
    tcp_server(boost::asio::io_context& io_context,
            uint16_t local_port,
            const std::string& chain_file,
            const std::string& key_file,
            tcp_server_callbackI* callback):ssl_enable_(true)
            , context_(boost::asio::ssl::context::sslv23)
            , acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), local_port))
            , callback_(callback)
    {
        context_.set_options(
            boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);
        //context_.set_password_callback(std::bind(&tcp_server::get_password, this));
        context_.use_certificate_chain_file(chain_file);
        context_.use_private_key_file(key_file, boost::asio::ssl::context::pem);
        //context_.use_tmp_dh_file("dh2048.pem");
    }
    virtual ~tcp_server()
    {
    }

public:
    void accept() {
        auto self(shared_from_this());
        if (!ssl_enable_) {
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
        } else {
            acceptor_.async_accept(
                [self](const boost::system::error_code& ec, boost::asio::ip::tcp::socket socket)
                {
                    if (!ec) {
                        self->callback_->on_accept_ssl(0, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(std::move(socket), self->context_));
                        return;
                    }
                    self->accept();
                });
        }

    }

private:
    std::string get_password() const
    {
       return "test";
    }
private:
    bool ssl_enable_ = false;
    boost::asio::ssl::context context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    tcp_server_callbackI* callback_;
};

#endif //TCP_SERVER_HPP