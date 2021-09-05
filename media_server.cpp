#include "rtmp_server.hpp"
#include "ws_server.hpp"
#include "httpflv_server.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <stddef.h>

int main(int argn, char** argv) {
    const uint16_t rtmp_def_port = 1935;
    const uint16_t ws_def_port = 1900;
    const uint16_t httpflv_port = 8080;

    boost::asio::io_context io_context;
    boost::asio::io_service::work work(io_context);

    Logger::get_instance()->set_filename("server.log");

    try {
        rtmp_server server(io_context, rtmp_def_port);
        websocket_server ws(io_context, ws_def_port);
        httpflv_server httpflv_serv(io_context, httpflv_port);
        log_infof("rtmp server start:%d", rtmp_def_port);
        log_infof("websocket server start:%d", ws_def_port);
        io_context.run();
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}