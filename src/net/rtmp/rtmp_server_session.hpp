#ifndef RTMP_SESSION_HPP
#define RTMP_SESSION_HPP
#include "tcp_session.hpp"
#include "data_buffer.hpp"
#include "logger.hpp"
#include "rtmp_pub.hpp"
#include "rtmp_handshake.hpp"
#include "chunk_stream.hpp"
#include "rtmp_control_handler.hpp"
#include "rtmp_request.hpp"
#include "media_stream_manager.hpp"
#include "amf/amf0.hpp"
#include "rtmp_writer.hpp"
#include "media_packet.hpp"
#include "session_aliver.hpp"
#include "rtmp_session_base.hpp"
#include <memory>
#include <stdint.h>
#include <unordered_map>
#include <vector>

class rtmp_server_callbackI
{
public:
    virtual void on_close(std::string session_key) = 0;
};

class rtmp_server_session : public tcp_session_callbackI, public session_aliver, public rtmp_session_base
{
friend class rtmp_control_handler;
friend class rtmp_server_handshake;
friend class media_stream_manager;
friend class rtmp_writer;

public:
    rtmp_server_session(boost::asio::ip::tcp::socket socket, rtmp_server_callbackI* callback, std::string session_key);
    virtual ~rtmp_server_session();

public:
    void try_read();
    std::string get_sesson_key();

public:
    void close();

protected://implement rtmp_session_base
    data_buffer* get_recv_buffer() override;
    int rtmp_send(char* data, int len) override;
    int rtmp_send(std::shared_ptr<data_buffer> data_ptr) override;

protected://implement tcp_session_callbackI
    virtual void on_write(int ret_code, size_t sent_size) override;
    virtual void on_read(int ret_code, const char* data, size_t data_size) override;

private:
    int receive_chunk_stream();
    int handle_request();
    int send_rtmp_ack(uint32_t size);

private:
    std::string session_key_;
    std::shared_ptr<tcp_session> session_ptr_;
    rtmp_server_callbackI* callback_ = nullptr;

private:
    rtmp_server_handshake hs_;

private:
    rtmp_writer* play_writer_ = nullptr;
    bool closed_flag_ = false;
    rtmp_control_handler ctrl_handler_;
};

#endif