#include "co_tcp_server.hpp"
#include "co_tcp_session.hpp"
#include "utils/cpp20_coroutine.hpp"
#include <stdio.h>
#include <iostream>

cpp20_co_task session_handle(CoTcpSession* session) {
    while (true) {
        RecvData data = co_await session->sync_read();
        if (data.data_ == nullptr || data.len_ == 0) {
            delete session;
            std::cout << "session is closed...\r\n";
            break;
        }
    
        std::string recv_str((char*)data.data_, data.len_);
        std::cout << "receive data:" << recv_str << "\r\n";
    
        long ret = co_await session->sync_write(data.data_, data.len_);
        if (ret < 0) {
            delete session;
            std::cout << "session is closed...\r\n";
            break;
        }
        std::cout << "send back len:" << ret << "\r\n";
    }
    
}

cpp20_co_task accept_handle(CoTcpServer& server) {
    while (true) {
        uv_stream_t* stream_handle = co_await server.co_accept();
        CoTcpSession* session = new CoTcpSession(server.get_loop(), stream_handle);
        session_handle(session);
    }
}

int main(int argn, char** argv) {
    int local_port = atoi(argv[1]);
    uv_loop_t* loop = uv_default_loop();

    local_port = local_port <= 0 ? 9000 : local_port;

    try {
        CoTcpServer server(loop, local_port);

        accept_handle(server);
        std::cout << "server is running...\r\n";
        uv_run(loop, UV_RUN_DEFAULT);
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }
    return 0;
}