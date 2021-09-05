#include "ws_server.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "media_stream_manager.hpp"
#include <stdio.h>

websocket_session::websocket_session(boost::asio::ip::tcp::socket socket,
    websocket_server_callbackI* cb, std::string id):ws_(std::move(socket))
    , server_cb_(cb)
    , stream_id_(id)
    , demuxer_(&outputer_)
{
}

websocket_session::~websocket_session() {
}

void websocket_session::run() {
    // Accept the websocket handshake
    ws_.async_accept(
        boost::beast::bind_front_handler(
            &websocket_session::on_accept,
            shared_from_this()));
}

void websocket_session::on_accept(boost::beast::error_code ec) {
    if (ec) {
        log_errorf("websocket accept error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }
    log_infof("websocket accepted...");
    do_read();
}

void websocket_session::do_read() {
    // Read a message into our buffer
    ws_.async_read(buffer_,
        boost::beast::bind_front_handler(
            &websocket_session::on_read,
            shared_from_this()));
}

void websocket_session::close_session(boost::beast::error_code& ec) {
    if (!uri_.empty()) {
        media_stream_manager::remove_publisher(uri_);
    }
    ws_.close(ws_.reason(), ec);
    server_cb_->on_close(stream_id_);
}

void websocket_session::on_read(boost::beast::error_code ec, size_t bytes_transferred) {
    if (ec) {
        log_errorf("websocket read error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }

    if (uri_.empty()) {
        char* uri_data = static_cast<char*>(buffer_.data().data());
        assert(uri_data[0] == 0x02);
        uint16_t str_len = read_2bytes((uint8_t*)uri_data + 1);
        std::string str(uri_data + 3, str_len);
        std::size_t pos = str.find(".flv");
        if (pos != str.npos) {
            uri_ = str.substr(0, pos);
        }
        if (uri_[0] == '/') {
            uri_ = uri_.substr(1);
        }
        log_infof("websocket uri:%s", uri_.c_str());
        media_stream_manager::add_publisher(uri_);
        buffer_.consume(bytes_transferred);
        do_read();
        return;
    }

    //read the buffer
    //log_infof("read buffer len:%lu", bytes_transferred);
    //log_info_data(static_cast<uint8_t*>(buffer_.data().data()), bytes_transferred, "input data");
    MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
    pkt_ptr->buffer_ptr_->append_data(static_cast<char*>(buffer_.data().data()), bytes_transferred);
    pkt_ptr->key_ = uri_;
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
    demuxer_.input_packet(pkt_ptr);

    buffer_.consume(bytes_transferred);
    do_read();
}