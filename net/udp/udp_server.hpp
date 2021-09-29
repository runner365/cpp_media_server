#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP
#include "logger.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>

#define UDP_DATA_BUFFER_MAX (10*1024)

class udp_tuple
{
public:
    udp_tuple(const std::string& ip, uint16_t udp_port): ip_address(ip)
        , port(udp_port)
    {
    }
    ~udp_tuple(){
    }

    std::string to_string() const {
        std::string ret = ip_address;

        ret += ":";
        ret += std::to_string(port);
        return ret;
    }

public:
    std::string ip_address;
    uint16_t    port;
};

class udp_session_callbackI
{
public:
    virtual void on_write(size_t sent_size, udp_tuple address) = 0;
    virtual void on_read(const char* data, size_t data_size, udp_tuple address) = 0;
};

class udp_server
{
public:
    udp_server(boost::asio::io_context& io_context, uint16_t port, udp_session_callbackI* cb):cb_(cb)
        , socket_(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {
        try_read();
    }
    ~udp_server() {
    }

public:
    void try_read() {
        socket_.async_receive_from(
            boost::asio::buffer(buffer_, UDP_DATA_BUFFER_MAX), remote_ep_,
            [this](boost::system::error_code ec, std::size_t bytes_recvd)
            {
                if (ec) {
                    log_errorf("udp receive error:%s", ec.message().c_str());
                    return;
                }
                udp_tuple recv_address(remote_ep_.address().to_string(), remote_ep_.port());
                cb_->on_read(buffer_, bytes_recvd, recv_address);
            });
    }

    void write(char* data, size_t len, udp_tuple remote_address) {
        remote_ep_ = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(remote_address.ip_address), remote_address.port);

        socket_.async_send_to(
            boost::asio::buffer(data, len), remote_ep_,
            [this](boost::system::error_code ec, std::size_t bytes_sent)
            {
                udp_tuple send_address(remote_ep_.address().to_string(), remote_ep_.port());
                if (ec || (bytes_sent == 0)) {
                    log_errorf("udp send error:%s", ec.message().c_str());
                    cb_->on_write(0, send_address);
                    return;
                }
                cb_->on_write(bytes_sent, send_address);
                this->try_read();
            });
    }
private:
    udp_session_callbackI* cb_ = nullptr;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_ep_;
    char buffer_[UDP_DATA_BUFFER_MAX];
};

#endif