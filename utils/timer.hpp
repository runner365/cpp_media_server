#ifndef ASIO_TIMER_HPP
#define ASIO_TIMER_HPP
#include "logger.hpp"
#include <boost/asio.hpp>
#include <stdint.h>

class timer_interface
{
public:
    timer_interface(boost::asio::io_context& io_ctx, uint32_t timeout_ms):timer_(io_ctx)
        , timeout_ms_(timeout_ms)
    {
    }

    virtual ~timer_interface() {
        log_infof("timer base destruct...");
        stop_timer();
    }

public:
    virtual void on_timer() = 0;

public:
    void start_timer() {
        running_ = true;
        timer_.expires_from_now(boost::posix_time::millisec(timeout_ms_));
        timer_.async_wait([this](const boost::system::error_code& ec) {
            if (!this->running_) {
                return;
            }
            if(!ec) {
                this->on_timer();
            } else {
                log_errorf("timer error:%s, value:%d",
                    ec.message().c_str(), ec.value())
                return;
            }
            this->start_timer();
        });
    }

    void stop_timer() {
        if (!running_) {
            return;
        }
        running_ = false;
        boost::system::error_code ec;
        timer_.cancel(ec);
    }
    void update_timeout(uint32_t timeout_ms) {
        timeout_ms_ = timeout_ms;
    }

private:
    boost::asio::deadline_timer timer_;
    uint32_t timeout_ms_;
    bool running_ = false;
};

#endif