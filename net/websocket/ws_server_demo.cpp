#include "ws_server.hpp"
#include <iostream>

int main(int argn, char** argv) {
    if (argn < 2) {
        std::cout << "please input port" << std::endl;
        return -1;
    }

    uint16_t ws_port = 9191;
    const std::string ssl_pem_file = "../certs/server.pem";
    const std::string cert_file = "../certs/server.pem";

    ws_port = (uint16_t)atoi(argv[1]);
    boost::asio::io_context io_context;
    boost::asio::io_service::work work(io_context);

    log_infof("websocket server is starting, port:%d", ws_port);

    try {
        websocket_server wss(io_context, ws_port, WEBSOCKET_IMPLEMENT_PROTOO_TYPE, cert_file, ssl_pem_file);

        io_context.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
    
    return 0;
}