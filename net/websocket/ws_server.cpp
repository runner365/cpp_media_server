#include "ws_server.hpp"
#include "tcp_server.hpp"
#include "ws_session.hpp"
#include "ws_service_imp.hpp"
#include "ws_session_pub.hpp"
#include "wsimple/flv_websocket.hpp"
#include "wsimple/protoo_server.hpp"
#include "server_certificate.hpp"
#include "logger.hpp"
#include "net_pub.hpp"

websocket_server::websocket_server(boost::asio::io_context& io_context, uint16_t port, int imp_type):io_ctx_(io_context)
    , imp_type_(imp_type)
    , ssl_ctx_(boost::asio::ssl::context::tlsv12) {
    server_ = std::make_shared<tcp_server>(io_context, port, this);
    server_->accept();
    ssl_enable_ = false;
}

websocket_server::websocket_server(boost::asio::io_context& io_context, uint16_t port, int imp_type,
    const std::string& cert_file, const std::string& key_file):io_ctx_(io_context)
                                                            , imp_type_(imp_type)
                                                            , ssl_ctx_(boost::asio::ssl::context::tlsv12) {
    
    log_infof("websocket open ssl certificate file:%s, private key file:%s, tls version:%d",
        cert_file.c_str(), key_file.c_str(), (int)boost::asio::ssl::context::tlsv12);

    ssl_ctx_.use_certificate_file(cert_file, boost::asio::ssl::context::file_format::pem);
    ssl_ctx_.use_private_key_file(key_file, boost::asio::ssl::context::file_format::pem);
    
    ssl_enable_ = true;

    server_ = std::make_shared<tcp_server>(io_context, port, this);
    server_->accept();
}

websocket_server::~websocket_server()
{

}

void websocket_server::on_accept(int ret_code, boost::asio::ip::tcp::socket socket) {
    if (ret_code == 0) {
        std::string key;
        make_endpoint_string(socket.remote_endpoint(), key);
        log_infof("tcp accept key:%s", key.c_str());

        std::shared_ptr<websocket_session> session_ptr;
        if (ssl_enable_) {
            session_ptr = std::make_shared<websocket_session>(io_ctx_, std::move(socket), ssl_ctx_, this, key);
        } else {
            session_ptr = std::make_shared<websocket_session>(io_ctx_, std::move(socket), this, key);
        }
        session_ptr->run();
        sessions_.insert(std::make_pair(key, session_ptr));

        ws_session_callback* cb = create_websocket_implement(imp_type_, session_ptr);
        if (cb) {
            session_ptr->set_websocket_callback(cb);
        }
    }
    server_->accept();
}

void websocket_server::on_close(const std::string& session_key) {
    auto iter = sessions_.find(session_key);
    if (iter == sessions_.end()) {
        log_warnf("the session key doesn't exist:%s", session_key.c_str());
        return;
    }
    log_infof("remove websocket session id:%s", session_key.c_str());
    sessions_.erase(iter);
}