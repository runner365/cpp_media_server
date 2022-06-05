#ifndef RATE_CONTROL_H
#define RATE_CONTROL_H
#include "include/bwe_defines.h"
#include <stdint.h>
#include <stddef.h>

namespace webrtc {
class RateControl
{
public:
    RateControl();
    ~RateControl();

public:
    bool update(int64_t rate, BandwidthUsage state);
    int64_t get_target_bitrate() { return target_bitrate_; }
    void set_min_bitrate(int64_t min_bitrate) { min_bitrate_ = min_bitrate; }
    void set_max_bitrate(int64_t max_bitrate) { max_bitrate_ = max_bitrate; }
    void set_start_bitrate(int64_t start_bitrate) { target_bitrate_ = start_bitrate; }

private:
    int64_t max_bitrate_;
    int64_t min_bitrate_;
    int64_t target_bitrate_;
    BandwidthUsage last_state_ = BandwidthUsage::kBwNormal;
    int64_t normal2normal_count_ = 0;
};

}


#endif