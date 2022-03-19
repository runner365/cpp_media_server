#include "ws_server.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "media_stream_manager.hpp"
#include <stdio.h>

websocket_session::websocket_session(boost::asio::io_context& io_ctx,
    boost::asio::ip::tcp::socket&& socket,
    websocket_server_callbackI* cb,
    std::string id):io_ctx_(io_ctx)
    , server_cb_(cb)
    , stream_id_(id)
{
    ws_  = new boost::beast::websocket::stream<boost::beast::tcp_stream>(std::move(socket));
    wss_ = nullptr;
}

websocket_session::websocket_session(boost::asio::io_context& io_ctx, boost::asio::ip::tcp::socket&& socket, boost::asio::ssl::context& ctx,
            websocket_server_callbackI* cb, std::string id):io_ctx_(io_ctx)
    , server_cb_(cb)
    , stream_id_(id)
{
    ws_  = nullptr;
    wss_ = new boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>(std::move(socket), ctx);
}

websocket_session::~websocket_session() {
    if (ws_cb_) {
        delete ws_cb_;
        ws_cb_ = nullptr;
    }
    if (ws_) {
        delete ws_;
    }
    if (wss_) {
        delete wss_;
    }
}

void websocket_session::run() {
    // Accept the websocket handshake
    if (ws_) {
        log_infof("websocket try to accept....");
        //boost::beast::get_lowest_layer(*ws_).expires_after(std::chrono::seconds(30));
        boost::beast::get_lowest_layer(*ws_).expires_never();
        ws_->set_option(
            boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));
        // Set a decorator to change the Server of the handshake
        ws_->set_option(boost::beast::websocket::stream_base::decorator(
            [](boost::beast::websocket::response_type& res)
            {
                res.set(boost::beast::http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " websocket-server-async");
            }));
        ws_->async_accept(
            boost::beast::bind_front_handler(
                &websocket_session::on_accept,
                shared_from_this()));
    } else {
        // Set the timeout.
        log_infof("websocket ssl accept....");
        boost::beast::get_lowest_layer(*wss_).expires_never();
        //boost::beast::get_lowest_layer(*wss_).expires_after(std::chrono::seconds(30));
        std::string hostname = boost::asio::ip::host_name();
        log_infof("get hostname:%s", hostname.c_str());
        if(! SSL_set_tlsext_host_name((*wss_).next_layer().native_handle(), hostname.c_str()))
        {
           boost::system::error_code ec{static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category()};
           throw boost::system::system_error{ec};
        }
        log_infof("SSL_set_tlsext_host_name ok, hostname:%s", hostname.c_str());
         // Perform the SSL handshake
        wss_->next_layer().async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::beast::bind_front_handler(
                &websocket_session::on_handshake,
                shared_from_this()));
    }
}

void websocket_session::set_websocket_callback(ws_session_callback* cb) {
    ws_cb_ = cb;
}

void websocket_session::on_handshake(boost::beast::error_code ec) {
    if(ec) {
        log_errorf("websocket ssl handshake error:%s", ec.message().c_str());
        ws_cb_->on_close(ec.value());
        return;
    }

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    boost::beast::get_lowest_layer(*wss_).expires_never();

    // Set suggested timeout settings for the websocket
    wss_->set_option(
        boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    wss_->set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res)
        {
            res.set(boost::beast::http::field::server,
                std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-server-async-ssl");
        }));

    log_infof("ssl handle ok...");
    // Accept the websocket handshake
    wss_->async_accept(
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

    if (ws_cb_) {
        ws_cb_->on_accept();
    }
    do_read();
}

void websocket_session::async_write(const char* data, int len) {
    boost::beast::flat_buffer send_buffer;
    auto output_data = send_buffer.prepare((size_t)len);
    memcpy(output_data.data(), data, len);
    send_buffer.commit(len);

    send_buffer_queue_.push(send_buffer);
    io_ctx_.post(std::bind(&websocket_session::do_write, this));
    return;
}

void websocket_session::do_write() {
    if (send_buffer_queue_.empty()) {
        return;
    }

    if (sending_flag_) {
        return;
    }

    sending_flag_ = true;
    boost::beast::flat_buffer send_buffer = send_buffer_queue_.front();
    if (ws_) {
        ws_->async_write(send_buffer.data(),
            boost::beast::bind_front_handler(&websocket_session::on_write, this));
    } else {
        wss_->async_write(send_buffer.data(),
            boost::beast::bind_front_handler(&websocket_session::on_write, this));
    }
    return;
}

void websocket_session::do_read() {
    if (ws_) {
        ws_->async_read(recv_buffer_,
            boost::beast::bind_front_handler(
                &websocket_session::on_read,
                shared_from_this()));
        return;
    } else {
        wss_->async_read(recv_buffer_,
            boost::beast::bind_front_handler(
                &websocket_session::on_read,
                shared_from_this()));
        return;
    }
}

void websocket_session::close_session(boost::beast::error_code& ec) {
    if (closed_flag_) {
        return;
    }
    closed_flag_ = true;

    if (ws_) {
        ws_->close(ws_->reason(), ec);
    } else if (wss_) {
        wss_->close(wss_->reason(), ec);
    }
    
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
    sending_flag_ = false;
    send_buffer_queue_.pop();

    if (ws_cb_) {
        ws_cb_->on_writen((int)bytes_transferred);
    }
    do_write();
}

void websocket_session::on_read(boost::beast::error_code ec, size_t bytes_transferred) {
    if (ec) {
        log_errorf("websocket read error:%s", ec.message().c_str());
        close_session(ec);
        return;
    }

    if (ws_cb_) {
        try
        {
            ws_cb_->on_read((char*)recv_buffer_.data().data(), bytes_transferred);
        }
        catch(const std::exception& e)
        {
            log_errorf("call websocket on_read exception:%s", e.what());
            close_session(ec);
            return;
        }
    }
    recv_buffer_.consume(bytes_transferred);
    do_read();
}
