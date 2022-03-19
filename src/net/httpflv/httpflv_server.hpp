#ifndef HTTP_FLV_SERVER_HPP
#define HTTP_FLV_SERVER_HPP
#include "http_server.hpp"
#include "timer.hpp"

class httpflv_server : public timer_interface
{
public:
    httpflv_server(boost::asio::io_context& io_ctx, uint16_t port);
    ~httpflv_server();

    static void httpflv_writer_close(const std::string& id);

public:
    virtual void on_timer() override;

private:
    void run();
    void on_check_alive();

private:
    http_server server_;
};

#endif //HTTP_FLV_SERVER_HPP