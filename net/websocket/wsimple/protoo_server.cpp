#include "protoo_server.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>

protoo_server::protoo_server() {

}

protoo_server::~protoo_server() {

}

void protoo_server::on_accept() {
    log_infof("protoo server accept...");
}

void protoo_server::on_read(const char* data, size_t len) {
    std::string body(data, len);
    log_infof("protoo server read body:%s", body.c_str());
}

void protoo_server::on_writen(int len) {
    log_infof("protoo server writen len:%d", len);
}

void protoo_server::on_close(int err_code) {
    log_infof("protoo server on close, error code:%d", err_code);
}