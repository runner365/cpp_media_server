#ifndef RTMP_SERVER_HPP
#define RTMP_SERVER_HPP
#include "rtmp_server_session.hpp"
#include "rtmp_pub.hpp"
#include "tcp_server.hpp"
#include "logger.hpp"
#include "timer.hpp"
#include <unordered_map>
#include <ctime>
#include <chrono>

class rtmp_server : public tcp_server_callbackI, public rtmp_server_callbackI, public timer_interface
{
public:
    rtmp_server(boost::asio::io_context& io_context, uint16_t port);
    virtual ~rtmp_server();

public:
    virtual void on_timer() override;

protected:
    virtual void on_close(std::string session_key) override;

protected:
    virtual void on_accept(int ret_code, boost::asio::ip::tcp::socket socket) override;
    virtual void on_accept_ssl(int ret_code, boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket) override;

private:
    void on_check_alive();

private:
    std::shared_ptr<tcp_server> server_;
    std::unordered_map< std::string, std::shared_ptr<rtmp_server_session> > session_ptr_map_;
};

#endif //RTMP_SERVER_HPP