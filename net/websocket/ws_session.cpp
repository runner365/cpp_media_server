#include "ws_server.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "media_stream_manager.hpp"
#include <stdio.h>

websocket_session::websocket_session(boost::asio::io_context& io_ctx,
    boost::asio::ip::tcp::socket&& socket,
    websocket_server_callbackI* cb,
    std::string id):io_ctx_(io_ctx)
    , ws_(std::move(socket))
    , server_cb_(cb)
    , stream_id_(id)
{
}

websocket_session::~websocket_session() {
    if (ws_cb_) {
        delete ws_cb_;
        ws_cb_ = nullptr;
    }
}

void websocket_session::run() {
    // Accept the websocket handshake
    ws_.async_accept(
        boost::beast::bind_front_handler(
            &websocket_session::on_accept,
            shared_from_this()));
}

void websocket_session::set_websocket_callback(ws_session_callback* cb) {
    ws_cb_ = cb;
}

void websocket_session::on_accept(boost::beast::error_code ec) {
    if (ec) {
        log_errorf("websocket accept error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }
    log_infof("websocket accepted...");

    if (ws_cb_) {
        ws_cb_->on_accept();
    }
    do_read();
}

void websocket_session::async_write(const char* data, int len) {
    auto output_data = send_buffer_.prepare((size_t)len);

    memcpy(output_data.data(), data, len);

    io_ctx_.post(std::bind(&websocket_session::do_write, this));
    return;
}

void websocket_session::do_write() {
    ws_.async_write(send_buffer_.data(),
        boost::beast::bind_front_handler(&websocket_session::on_write, this));
    return;
}

void websocket_session::do_read() {
    // Read a message into our buffer
    ws_.async_read(recv_buffer_,
        boost::beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void websocket_session::close_session(boost::beast::error_code& ec) {
    if (closed_flag_) {
        return;
    }
    closed_flag_ = true;
    ws_.close(ws_.reason(), ec);
    server_cb_->on_close(stream_id_);
    if (ws_cb_) {
        ws_cb_->on_close(ec.value());
    }
}

void websocket_session::on_write(boost::beast::error_code ec, size_t bytes_transferred) {
    if (ec) {
        log_errorf("websocket write error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }
    send_buffer_.consume(bytes_transferred);
}

void websocket_session::on_read(boost::beast::error_code ec, size_t bytes_transferred) {
    if (ec) {
        log_errorf("websocket read error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }

    if (ws_cb_) {
        ws_cb_->on_read((char*)recv_buffer_.data().data(), bytes_transferred);
    }
    recv_buffer_.consume(bytes_transferred);
    do_read();
}