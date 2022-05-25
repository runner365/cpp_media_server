#include "rate_control.h"

#define BITRATE_MAX (3000*1000)
#define BITRATE_MIN (150*1000)
#define DECR_RATE 0.85
#define INCR_RATE 1.10

namespace webrtc {
RateControl::RateControl():max_bitrate_(BITRATE_MAX)
                        , min_bitrate_(BITRATE_MIN)
                        , target_bitrate_(BITRATE_MAX)
                        , last_state_(BandwidthUsage::kBwNormal)
{
}

RateControl::~RateControl()
{
}

/*
  "overusing"-->"normal": get min rate; keep hold
  "underusing"-->"normal": get max rate; keep hold
  "normal"-->"normal": increase a littel;

  "overusing"-->"overusing": decrease a lot;
  "underusing"-->"overusing": descrease;
  "normal"-->"overusing": descrease;

  "overusing"-->"underusing": increase;
  "underusing"-->"underusing": increase a lot;
  "normal"-->"underusing": increase;
*/
bool RateControl::update(int64_t rate, BandwidthUsage state) {
    bool ret = false;
    int64_t min_rate = (target_bitrate_ > rate) ? rate : target_bitrate_;
    int64_t max_rate = (target_bitrate_ < rate) ? rate : target_bitrate_;

    if (state == BandwidthUsage::kBwNormal) {
        if (last_state_ == BandwidthUsage::kBwOverusing) {
            min_bitrate_    = target_bitrate_;//update min rate when "overusing" --> "normal"
            target_bitrate_ = target_bitrate_ * 1.0;//keep hold
        } else if (last_state_ == BandwidthUsage::kBwUnderusing) {
            min_bitrate_    = (target_bitrate_ > rate) ? rate : target_bitrate_;
            target_bitrate_ = target_bitrate_ * 1.0;//keep hold
        } else {
            //"normal" --> "normal", increase a little
            target_bitrate_ = max_rate + 10*1000;
            ret = true;
        }
    } else if (state == BandwidthUsage::kBwOverusing) {
        if (last_state_ == BandwidthUsage::kBwOverusing) {
            target_bitrate_ = min_rate * (DECR_RATE - 0.05);
        } else if (last_state_ == BandwidthUsage::kBwUnderusing) {
            target_bitrate_ = min_rate * DECR_RATE;
        } else {
            target_bitrate_ = min_rate * DECR_RATE;
        }
        ret = true;
    } else {//kBwUnderusing
        if (last_state_ == BandwidthUsage::kBwOverusing) {
            target_bitrate_ = max_rate * INCR_RATE;
            ret = true;
        } else if (last_state_ == BandwidthUsage::kBwUnderusing) {
            target_bitrate_ = max_rate * INCR_RATE + 40*1000;
            ret = true;
        } else {
            target_bitrate_ = max_rate * INCR_RATE;
        }
    }
    
    target_bitrate_ = (target_bitrate_ > max_bitrate_) ? max_bitrate_ : target_bitrate_;
    target_bitrate_ = (target_bitrate_ < min_bitrate_) ? min_bitrate_ : target_bitrate_;

    last_state_ = state;
    return ret;
}

}