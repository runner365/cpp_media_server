#include "http_server.hpp"
#include "logger.hpp"
#include <iostream>
#include <stdio.h>
#include <memory>
#include <sstream>

std::string make_response() {
    std::stringstream ss;

    ss << "{" << "\r\n";
    ss << "\"code\":" << "0," << "\r\n";
    ss << "\"desc\":" << "\"ok\"" << "\r\n";
    ss << "}";

    return ss.str();
}

void GetDemo(const http_request* request, std::shared_ptr<http_response> response) {
    std::stringstream ss;
    ss << "---------- handle /api/v1/getdemo ------------" << std::endl;
    ss << "version:" << request->version_ << std::endl;
    ss << "method:" << request->method_ << std::endl;
    ss << "uri:" << request->uri_ << std::endl;
    ss << "content_length_:" << request->content_length_ << std::endl;

    for (const auto& header : request->headers_) {
        ss << header.first << ": " << header.second << std::endl;
    }

    log_infof("%s", ss.str().c_str());
    std::string ret_str = make_response();
    response->write(ret_str.c_str(), ret_str.length());

    return;
}

void PostDemo(const http_request* request, std::shared_ptr<http_response> response) {
    std::stringstream ss;
    ss << "---------- handle /api/v1/postdemo ------------" << std::endl;
    ss << "version:" << request->version_ << std::endl;
    ss << "method:" << request->method_ << std::endl;
    ss << "uri:" << request->uri_ << std::endl;
    ss << "content_length_:" << request->content_length_ << std::endl;

    for (const auto& header : request->headers_) {
        ss << header.first << ": " << header.second << std::endl;
    }

    std::string content_str(request->content_body_, request->content_length_);
    ss << "data body:\r\n" << content_str << std::endl;

    log_infof("%s", ss.str().c_str());
    std::string ret_str = make_response();
    response->write(ret_str.c_str(), ret_str.length());
    return;
}

int main(int argn, char** argv)
{
    std::string cert_file;
    std::string key_file;

    if (argn < 4) {
        std::cout << "input parameter error" << std::endl;
        return -1;
    }

    int port = atoi(argv[1]);

    if (argn >= 4) {
        cert_file = argv[2];
        key_file  = argv[3];
        std::cout << "ssl cert file:" << cert_file << "\r\n";
        std::cout << "ssl key file:" << key_file << "\r\n";
    }

    boost::asio::io_context io_context;
    boost::asio::io_service::work work(io_context);

    try {
        http_server server(io_context, port, cert_file, key_file);
        
        server.add_get_handle("/api/v1/getdemo", GetDemo);
        server.add_post_handle("/api/v1/postdemo", PostDemo);
        io_context.run();
    }
    catch(const std::exception& e) {
        std::cerr << "http server exception:" << e.what() << "\r\n";
    }
    
    return 0;
}