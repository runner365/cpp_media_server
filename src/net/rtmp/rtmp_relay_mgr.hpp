#ifndef RTMP_RELAY_MGR_HPP
#define RTMP_RELAY_MGR_HPP
#include "rtmp_relay.hpp"
#include "timer.hpp"
#include <map>
#include <memory>

class rtmp_relay_manager : public timer_interface
{
public:
    rtmp_relay_manager(uv_loop_t* loop);
    virtual ~rtmp_relay_manager();

    int add_new_relay(const std::string& host, const std::string& key);

public:
    virtual void on_timer() override;

private:
    std::map<std::string, std::shared_ptr<rtmp_relay>> relay_map_;
    uv_loop_t* loop_ = nullptr;
};


#endif

