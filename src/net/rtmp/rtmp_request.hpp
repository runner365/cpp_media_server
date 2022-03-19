#ifndef RTMP_REQUEST_HPP
#define RTMP_REQUEST_HPP
#include "logger.hpp"

class rtmp_request
{
public:
    rtmp_request() {};
    ~rtmp_request() {};

public:
    void dump() {
        std::string method = publish_flag_ ? "publish" : "play";
        log_infof("%s request app:%s, tcurl:%s, flash version:%s, transaction id:%ld, stream name:%s",
            method.c_str(), app_.c_str(), tcurl_.c_str(), flash_ver_.c_str(),
            transaction_id_, stream_name_.c_str());
    }

public:
    bool publish_flag_ = false;//publish: true, play: false
    bool is_ready_     = false;
    std::string app_;
    std::string tcurl_;
    std::string flash_ver_;
    std::string stream_name_;
    std::string key_;//app/streamname
    int64_t transaction_id_ = 1;
    uint32_t stream_id_ = 0;
};

#endif