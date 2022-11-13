#include "co_tcp_server.hpp"
#include "co_tcp_session.hpp"
#include "cpp20_coroutine.hpp"
#include <stdio.h>
#include <iostream>

cpp20_co_task session_handle(CoTcpSession* session) {
    long send_total = 0;
    long recv_total = 0;
    while (true) {
        std::cout << "sync_read...\r\n";
        RecvData data = co_await session->sync_read();
        if (data.data_ == nullptr || data.len_ == 0) {
            delete session;
            std::cout << "sync read error, session is closed...\r\n";
            break;
        }
    
        //std::string recv_str((char*)data.data_, data.len_);
        //std::cout << "receive data:" << recv_str << "\r\n";
        recv_total += data.len_;
        std::cout << "sync_read get len:" << data.len_  << "\r\n";
    
        long ret = co_await session->sync_write(data.data_, data.len_);
        if (ret < 0) {
            delete session;
            std::cout << "sync write error, session is closed...\r\n";
            break;
        }
        send_total += ret;
        std::cout << "send total:" << send_total
                << ", recv total:" << recv_total << "\r\n";

        free(data.data_);
        data.data_ = nullptr;
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