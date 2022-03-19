#ifndef UDP_SERVER_HPP
#define UDP_SERVER_HPP
#include "logger.hpp"
#include "data_buffer.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <queue>
#include <boost/asio.hpp>

#define UDP_DATA_BUFFER_MAX 1500

class udp_tuple
{
public:
    udp_tuple() {
    }
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
    uint16_t    port = 0;
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
    udp_server(boost::asio::io_context& io_context, uint16_t port, udp_session_callbackI* cb):io_ctx_(io_context)
        , cb_(cb)
        , socket_(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {
        buffer_ = new char[UDP_DATA_BUFFER_MAX];
        try_read();
    }
    ~udp_server() {
        delete[] buffer_;
    }

public:
    boost::asio::io_context& get_io_context() {return io_ctx_;}

    void write(char* data, size_t len, udp_tuple remote_address) {
        std::shared_ptr<data_buffer> buffer_ptr = std::make_shared<data_buffer>();
        buffer_ptr->append_data(data, len);
        buffer_ptr->dst_ip_    = remote_address.ip_address;
        buffer_ptr->dst_port_  = remote_address.port;
        buffer_ptr->sent_flag_ = false;

        send_buffer_queue_.push(buffer_ptr);

        do_write();
    }

private:
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
                char* cb_data = new char[bytes_recvd + 1];
                memcpy(cb_data, buffer_, bytes_recvd);
                cb_->on_read(cb_data, bytes_recvd, recv_address);
                delete[] cb_data;
                this->try_read();
            });
    }

    void do_write() {
        if (send_buffer_queue_.empty()) {
            return;
        }

        auto head_ptr = send_buffer_queue_.front();
        if (head_ptr->sent_flag_) {
            return;
        }

        head_ptr->sent_flag_ = true;

        remote_ep_ = boost::asio::ip::udp::endpoint(boost::asio::ip::address::from_string(head_ptr->dst_ip_), head_ptr->dst_port_);
        socket_.async_send_to(
            boost::asio::buffer(head_ptr->data(), head_ptr->data_len()), remote_ep_,
            [this](boost::system::error_code ec, std::size_t bytes_sent)
            {
                udp_tuple send_address(remote_ep_.address().to_string(), remote_ep_.port());

                send_buffer_queue_.pop();
                if (ec) {
                    log_errorf("udp send error:%s", ec.message().c_str());
                    cb_->on_write(0, send_address);
                } else {
                    cb_->on_write(bytes_sent, send_address);
                }
                
                this->do_write();
            });
    }

private:
    boost::asio::io_context& io_ctx_;
    udp_session_callbackI* cb_       = nullptr;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_ep_;
    std::queue< std::shared_ptr<data_buffer> > send_buffer_queue_;
    char* buffer_ = nullptr;
};

#endif