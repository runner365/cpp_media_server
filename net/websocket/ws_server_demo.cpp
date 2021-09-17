#include "ws_server.hpp"
#include <iostream>

websocket_server* wss_p = nullptr;
websocket_server* ws_p  = nullptr;

void create_wss_server(boost::asio::io_context& io_context, uint16_t ws_port) {
    const std::string ssl_pem_file = "../certs/server.pem";
    const std::string cert_file = "../certs/server.pem";

    log_infof("websocket https server is starting, port:%d", ws_port);
    wss_p = new websocket_server(io_context, ws_port, WEBSOCKET_IMPLEMENT_PROTOO_TYPE, cert_file, ssl_pem_file);
}

void create_ws_server(boost::asio::io_context& io_context, uint16_t ws_port) {
    ws_p = new websocket_server(io_context, ws_port, WEBSOCKET_IMPLEMENT_PROTOO_TYPE);
    log_infof("websocket http server is starting, port:%d", ws_port);
}

int main(int argn, char** argv) {
    if (argn < 2) {
        std::cout << "please input port" << std::endl;
        return -1;
    }

    uint16_t ws_port = 0;

    ws_port = (uint16_t)atoi(argv[1]);
    boost::asio::io_context io_context;
    boost::asio::io_service::work work(io_context);

    try {
        create_ws_server(io_context, ws_port);
        io_context.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    if (wss_p) {
        delete wss_p;
        wss_p = nullptr;
    }
    if (ws_p) {
        delete ws_p;
        ws_p = nullptr;
    }
    return 0;
}