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

private:
    int64_t max_bitrate_;
    int64_t min_bitrate_;
    int64_t target_bitrate_;
    BandwidthUsage last_state_ = BandwidthUsage::kBwNormal;
};

}


#endif