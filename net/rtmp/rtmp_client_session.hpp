#ifndef RTMP_CLIENT_SESSION_HPP
#define RTMP_CLIENT_SESSION_HPP
#include "tcp_client.hpp"
#include "rtmp_pub.hpp"
#include "rtmp_handshake.hpp"
#include "rtmp_session_base.hpp"
#include "data_buffer.hpp"
#include "media_packet.hpp"
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <boost/asio.hpp>

class rtmp_client_callbackI
{
public:
    virtual void on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) = 0;
    virtual void on_close(int ret_code) = 0;
};

class rtmp_control_handler;
class media_stream_manager;
class rtmp_server_handshake;
class rtmp_writer;

class rtmp_client_session : public tcp_client_callback, public rtmp_session_base
{
friend class rtmp_control_handler;
friend class media_stream_manager;
friend class rtmp_client_handshake;
friend class rtmp_writer;

public:
    rtmp_client_session(boost::asio::io_context& io_context, rtmp_client_callbackI* callback);
    virtual ~rtmp_client_session();

public://tcp client callback implement
    virtual void on_connect(int ret_code) override;
    virtual void on_write(int ret_code, size_t sent_size) override;
    virtual void on_read(int ret_code, const char* data, size_t data_size) override;

public:
    void try_read();
    void close();
    bool is_ready();

public:
    int start(const std::string& url, bool is_publish);
    int rtmp_write(MEDIA_PACKET_PTR pkt_ptr);

protected://implement rtmp_session_base
    data_buffer* get_recv_buffer() override;
    int rtmp_send(char* data, int len) override;
    int rtmp_send(std::shared_ptr<data_buffer> data_ptr) override;

private://rtmp client behavior
    int rtmp_connect();
    int rtmp_createstream();
    int rtmp_play();
    int rtmp_publish();
    int receive_resp_message();
    int handle_message();

private:
    tcp_client conn_;
    rtmp_client_callbackI* cb_;
    rtmp_client_handshake hs_;

private:
    std::string host_;
    uint16_t    port_ = 1935;

private:
    rtmp_control_handler ctrl_handler_;
    boost::asio::io_context& io_ctx_;
};

#endif