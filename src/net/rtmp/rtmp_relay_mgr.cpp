#include "rtmp_relay_mgr.hpp"
#include "rtmp_relay.hpp"

#include "utils/logger.hpp"
#include <vector>

rtmp_relay_manager::rtmp_relay_manager(boost::asio::io_context& io_context):timer_interface(io_context, 5000)
                                                                            , io_context_(io_context)
{
    start_timer();
}

rtmp_relay_manager::~rtmp_relay_manager()
{
    stop_timer();
}

int rtmp_relay_manager::add_new_relay(const std::string& host, const std::string& key) {
    auto iter = relay_map_.find(key);
    if (iter != relay_map_.end()) {
        log_infof("rtmp key(%s) has been in relay list", key.c_str());
        return -1;
    }

    auto relay_ptr = std::make_shared<rtmp_relay>(host, key, io_context_);
    relay_ptr->start();

    relay_map_[key] = relay_ptr;

    return 0;
}

void rtmp_relay_manager::on_timer() {
    std::vector<std::string> rm_keys;

    for(auto& item : relay_map_) {
        auto relay_ptr = item.second;
        if (!relay_ptr->is_alive()) {
            rm_keys.push_back(item.first);
        }
    }

    for(auto key : rm_keys) {
        log_infof("remove rtmp relay(%s)", key.c_str());
        relay_map_.erase(key);
    }
}


