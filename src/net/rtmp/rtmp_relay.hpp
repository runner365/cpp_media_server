#ifndef RTMP_RELAY_HPP
#define RTMP_RELAY_HPP
#include "rtmp_client_session.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <memory>

class rtmp_relay : public rtmp_client_callbackI
{
public:
    rtmp_relay(const std::string& host, const std::string& key, uv_loop_t* loop);
    virtual ~rtmp_relay();

    void start();

    bool is_alive();

public:
    virtual void on_message(int ret_code, MEDIA_PACKET_PTR pkt_ptr) override;
    virtual void on_close(int ret_code) override;

private:
    std::string host_;
    std::string key_;
    std::shared_ptr<rtmp_client_session> client_ptr_;
    int alive_cnt_ = 0;
};

#endif
