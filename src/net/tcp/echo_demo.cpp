#include "tcp_server.hpp"
#include "tcp_client.hpp"
#include "logger.hpp"
#include <iostream>

class echo_server : public tcp_server_callbackI, public tcp_session_callbackI
{
public:
    echo_server(uv_loop_t* loop, uint16_t local_port)
    {
        server_ = std::make_shared<tcp_server>(loop, local_port, this);
    }
    virtual ~echo_server()
    {
    }

public://implement tcp_server_callbackI
    virtual void on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) override {
        log_infof("on accept return %d", ret_code);
        session_ptr_ = std::make_shared<tcp_session>(loop, handle, this);
        session_ptr_->async_read();
    }

public://implement tcp_session_callbackI
    virtual void on_write(int ret_code, size_t sent_size) override {
        log_infof("data length:%lu is sent", sent_size);
    }

    virtual void on_read(int ret_code, const char* data, size_t data_size) override {
        log_infof("receive data length:%lu, message:%s", data_size, data);
        std::string back_msg(data, data_size);
        back_msg += " is back.";
        session_ptr_->async_write(back_msg.c_str(), back_msg.length());
    }

private:
    std::shared_ptr<tcp_server> server_;
    std::shared_ptr<tcp_session> session_ptr_;
};

class echo_client : public tcp_client_callback
{
public:
    echo_client(uv_loop_t* loop)
    {
        client_ = std::make_shared<tcp_client>(loop, this);
    }
    virtual ~echo_client()
    {
        log_infof("echo_client destructed...");
    }

public:
    void connnect(const std::string host, uint16_t port) {
        client_->connect(host, port);
    }

public:
    virtual void on_connect(int ret_code) override {
        log_infof("on connect return:%d", ret_code);
        if (ret_code != 0) {
            return;
        }
        std::string message("hello world.");
        client_->send(message.c_str(), message.length());
    }

public:
    virtual void on_write(int ret_code, size_t sent_size) override {
        log_infof("client send return:%d, sent length:%lu", ret_code, sent_size);
        client_->async_read();
    }

    virtual void on_read(int ret_code, const char* data, size_t data_size) override {
        std::string recvStr(data, data_size);
        log_infof("client return code::%d, receive message:%s", ret_code, recvStr.c_str());
    }

private:
    std::shared_ptr<tcp_client> client_;
};

int main(int argn, char** argv) {
    if (argn != 2) {
        std::cout << "input parameter error" << std::endl;
        return -1;
    }

    int local_port = atoi(argv[1]);
    uv_loop_t* loop = uv_default_loop();

    try {
        echo_server echoSevice(loop, local_port);
        echo_client echoClient(loop);

        echoClient.connnect("127.0.0.1", local_port);
        
        uv_run(loop, UV_RUN_DEFAULT);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}