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

    virtual ~timer_interface()
    {
    }

public:
    virtual void on_timer() = 0;

public:
    void start_timer() {
        timer_.expires_from_now(boost::posix_time::millisec(timeout_ms_));
        timer_.async_wait([this](const boost::system::error_code& ec) {
            if(!ec) {
                this->on_timer();
            } else {
                log_errorf("timer error:%s, value:%d",
                    ec.message().c_str(), ec.value())
            }
            this->start_timer();
        });
    }

private:
    boost::asio::deadline_timer timer_;
    uint32_t timeout_ms_;
};

#endif