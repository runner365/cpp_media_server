#ifndef TCP_SERVER_BASE_H
#define TCP_SERVER_BASE_H
#include "logger.hpp"
#include "data_buffer.hpp"
#include <memory>
#include <string>
#include <stdint.h>
#include <boost/asio.hpp>
#include <iostream>
#include <stdio.h>
#include <queue>

#define TCP_DEF_RECV_BUFFER_SIZE (8*1024)

class tcp_session_callbackI
{
public:
    virtual void on_write(int ret_code, size_t sent_size) = 0;
    virtual void on_read(int ret_code, const char* data, size_t data_size) = 0;
};


class tcp_session : public std::enable_shared_from_this<tcp_session>
{
public:
    tcp_session(boost::asio::ip::tcp::socket socket, tcp_session_callbackI* callback):socket_(std::move(socket))
        ,callback_(callback)
    {
        buffer_ = new char[buffer_size_];
    }
    
    virtual ~tcp_session()
    {
        close();
        delete[] buffer_;
    }

private:
    void do_write() {
        if (send_buffer_queue_.empty()) {
            return;
        }

        auto head_ptr = send_buffer_queue_.front();
        if (head_ptr->sent_flag_ > 0) {
            return;
        }

        auto self(shared_from_this());

        head_ptr->sent_flag_ = true;
        boost::asio::async_write(socket_, boost::asio::buffer(head_ptr->data(), head_ptr->data_len()),
            [self](boost::system::error_code ec, size_t written_size) {
                if (!ec && self->callback_) {
                    int64_t remain = (int64_t)written_size;

                    while(remain > 0) {
                        auto current = self->send_buffer_queue_.front();
                        int64_t current_len = current->data_len();
                        if (current_len > remain) {
                            current->consume_data(remain);
                            remain = 0;
                        } else {
                            self->send_buffer_queue_.pop();
                            remain -= current_len;
                        }
                    }

                    self->callback_->on_write(0, written_size);
                    self->do_write();
                    return;
                }
                log_infof("tcp write callback error:%s", ec.message().c_str());
                if (self->callback_) {
                    self->callback_->on_write(-1, written_size);
                }
            });

    }
public:
    void async_write(const char* data, size_t data_size) {
        if (!socket_.is_open()) {
            return;
        }
        std::shared_ptr<data_buffer> buffer_ptr = std::make_shared<data_buffer>();
        buffer_ptr->append_data(data, data_size);
        
        async_write(buffer_ptr);
        return;
    }

    void async_write(std::shared_ptr<data_buffer> buffer_ptr) {
        if (!socket_.is_open()) {
            return;
        }
        send_buffer_queue_.push(buffer_ptr);

        do_write();
    }

    void async_read() {
        if (!socket_.is_open()) {
            return;
        }
        auto self(shared_from_this());
        try
        {
            socket_.async_read_some(boost::asio::buffer(buffer_, buffer_size_),
                [self](boost::system::error_code ec, size_t read_length) {
                    if (!ec && self->callback_) {
                        self->callback_->on_read(0, self->buffer_, read_length);
                        return;
                    }

                    if (self->callback_) {
                        self->callback_->on_read(-1, nullptr, read_length);
                    }
                });
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }

        return;
    }

    void close() {
        if (socket_.is_open()) {
            log_infof("tcp close...");
            socket_.close();
        }
        callback_ = nullptr;
    }

    boost::asio::ip::tcp::endpoint get_remote_endpoint() {
        return socket_.remote_endpoint();
    }

    boost::asio::ip::tcp::endpoint get_local_endpoint() {
        return socket_.local_endpoint();
    }

private:
    boost::asio::ip::tcp::socket socket_;
    tcp_session_callbackI* callback_ = nullptr;
    char* buffer_ = nullptr;
    size_t buffer_size_ = TCP_DEF_RECV_BUFFER_SIZE;
    std::queue< std::shared_ptr<data_buffer> > send_buffer_queue_;
};

#endif //TCP_SERVER_BASE_H