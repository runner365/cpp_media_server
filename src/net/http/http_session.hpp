#ifndef HTTP_SESSION_HPP
#define HTTP_SESSION_HPP
#include "tcp_server.hpp"
#include "tcp_session.hpp"
#include "data_buffer.hpp"
#include "stringex.hpp"
#include "logger.hpp"
#include "net_pub.hpp"
#include "session_aliver.hpp"

#include <stdint.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <iostream>
#include <assert.h>


class http_callbackI;
class http_request;
class http_response;

class http_session : public tcp_session_callbackI, public session_aliver
{
friend class http_response;

public:
    http_session(boost::asio::ip::tcp::socket socket, http_callbackI* callback);
    virtual ~http_session();

public:
    void try_read();
    void write(const char* data, size_t len);
    void close();

protected://tcp_session_callbackI
    virtual void on_write(int ret_code, size_t sent_size) override;
    virtual void on_read(int ret_code, const char* data, size_t data_size) override;

private:
    int handle_request(const char* data, size_t data_size, bool& continue_flag);
    int analyze_header();

private:
    http_callbackI* callback_;
    std::shared_ptr<tcp_session> session_ptr_;
    std::shared_ptr<http_response> response_ptr_;
    data_buffer header_data_;
    data_buffer content_data_;
    http_request* request_;
    int content_start_ = -1;

private:
    bool header_is_ready_ = false;
    bool is_closed_ = false;
    bool continue_flag_ = false;
    boost::asio::ip::tcp::endpoint remote_endpoint_;
};

#endif