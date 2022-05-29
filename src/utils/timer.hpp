#ifndef ASIO_TIMER_HPP
#define ASIO_TIMER_HPP
#include "logger.hpp"
#include <uv.h>
#include <stdint.h>

inline void on_uv_timer_callback(uv_timer_t *handle);

class timer_interface
{
public:
    timer_interface(uv_loop_t* loop, uint32_t timeout_ms):timeout_ms_(timeout_ms)
    {
        uv_timer_init(loop, &timer_);
        timer_.data = this;

        
    }

    virtual ~timer_interface() {
        log_infof("timer base destruct...");
        stop_timer();
    }

public:
    virtual void on_timer() = 0;

public:
    void start_timer() {
        if(running_) {
            return;
        }
        running_ = true;
        uv_timer_start(&timer_, on_uv_timer_callback, timeout_ms_, timeout_ms_);
    }

    void stop_timer() {
        if (!running_) {
            return;
        }
        running_ = false;
        uv_timer_stop(&timer_);
    }

private:
    uv_timer_t timer_;
    uint32_t timeout_ms_;
    bool running_ = false;
};

inline void on_uv_timer_callback(uv_timer_t *handle) {
    timer_interface* timer = (timer_interface*)handle->data;
    if (timer) {
        timer->on_timer();
    }
}

#endif