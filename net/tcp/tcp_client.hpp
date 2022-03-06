#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H
#include "data_buffer.hpp"
#include <memory>
#include <queue>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>

class tcp_client_callback
{
public:
    virtual void on_connect(int ret_code) = 0;
    virtual void on_write(int ret_code, size_t sent_size) = 0;
    virtual void on_read(int ret_code, const char* data, size_t data_size) = 0;
};

class tcp_client
{
public:
    tcp_client(boost::asio::io_context& io_ctx,
        const std::string& host, uint16_t dst_port,
        tcp_client_callback* callback)
      : io_ctx_(io_ctx),
        socket_(io_ctx),
        callback_(callback)
    {
        boost::asio::ip::tcp::resolver resolver(io_ctx);
        std::string port_str = std::to_string(dst_port);
        endpoint_ = resolver.resolve(host, port_str);

        buffer_ = new char[buffer_size_];
    }

    tcp_client(boost::asio::io_context& io_ctx,
        tcp_client_callback* callback)
      : io_ctx_(io_ctx),
        socket_(io_ctx),
        callback_(callback)
    {
        buffer_ = new char[buffer_size_];
    }

    virtual ~tcp_client() {
        delete[] buffer_;

    }

    void connect(const std::string& host, uint16_t dst_port) {
        boost::asio::ip::tcp::resolver resolver(io_ctx_);
        std::string port_str = std::to_string(dst_port);
        endpoint_ = resolver.resolve(host, port_str);

        boost::asio::async_connect(socket_, endpoint_,
            [this](boost::system::error_code ec, boost::asio::ip::tcp::endpoint) {
                if (!ec) {
                    //boost::asio::ip::tcp::socket socket, tcp_session_callbackI* callback
                    is_connect_ = true;
                    this->callback_->on_connect(0);
                    return;
                }
                is_connect_ = false;
                this->callback_->on_connect(-1);
                return;
            });
    }

    void connect() {
        boost::asio::async_connect(socket_, endpoint_,
            [this](boost::system::error_code ec, boost::asio::ip::tcp::endpoint) {
                if (!ec) {
                    //boost::asio::ip::tcp::socket socket, tcp_session_callbackI* callback
                    is_connect_ = true;
                    this->callback_->on_connect(0);
                    return;
                }
                this->callback_->on_connect(-1);
                return;
            });
    }

    void send(const char* data, size_t len) {
        std::shared_ptr<data_buffer> data_ptr = std::make_shared<data_buffer>();

        data_ptr->append_data(data, len);

        send_buffer_queue_.push(data_ptr);
        do_write();
    }

    void async_read() {
        socket_.async_read_some(boost::asio::buffer(buffer_, buffer_size_),
            [this](boost::system::error_code ec, size_t read_length) {
                if (!ec) {
                    this->callback_->on_read(0, this->buffer_, read_length);
                    return;
                }
                this->callback_->on_read(-1, nullptr, 0);
            });
    }

    void close() {
        if (socket_.is_open()) {
            socket_.close();
        }
        is_connect_ = false;
    }

    bool is_connect() {
        return is_connect_;
    }

private:
    void do_write() {
        if (send_buffer_queue_.empty()) {
            return;
        }

        auto head_ptr = send_buffer_queue_.front();
        if (head_ptr->sent_flag_) {
            return;
        }

        head_ptr->sent_flag_ = true;
        boost::asio::async_write(socket_, boost::asio::buffer(head_ptr->data(), head_ptr->data_len()),
            [this](boost::system::error_code ec, size_t written_size) {
                if (!ec && this->callback_ && (written_size > 0)) {
                    if (!this->send_buffer_queue_.empty()) {
                        this->send_buffer_queue_.pop();
                    }
                    this->callback_->on_write(0, written_size);
                    this->do_write();
                    return;
                }
                log_infof("tcp write callback error:%s", ec.message().c_str());
                if (this->callback_) {
                    this->callback_->on_write(-1, written_size);
                }
            });
    }

private:
    boost::asio::io_context& io_ctx_;
    boost::asio::ip::tcp::socket socket_;
    tcp_client_callback* callback_ = nullptr;
    boost::asio::ip::tcp::resolver::results_type endpoint_;
    std::queue< std::shared_ptr<data_buffer> > send_buffer_queue_;
    char* buffer_ = nullptr;
    size_t buffer_size_ = 2048;
    bool is_connect_ = false;
};

#endif //TCP_CLIENT_H