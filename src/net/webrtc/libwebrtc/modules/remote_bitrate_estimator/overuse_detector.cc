/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include <math.h>
#include <stdio.h>
#include <algorithm>
#include <string>


namespace webrtc {

const double kMaxAdaptOffsetMs = 15.0;
const double kOverUsingTimeThreshold = 150;
const int kMaxNumDeltas = 60;

OveruseDetector::OveruseDetector()
    // Experiment is on by default, but can be disabled with finch by setting
    // the field trial string to "WebRTC-AdaptiveBweThreshold/Disabled/".
    : in_experiment_(true),
      k_up_(0.0087),
      k_down_(0.039),
      overusing_time_threshold_(150),
      threshold_(12.5),
      last_update_ms_(-1),
      prev_offset_(0.0),
      time_over_using_(-1),
      overuse_counter_(0),
      hypothesis_(BandwidthUsage::kBwNormal) {
    InitializeExperiment();
}

OveruseDetector::~OveruseDetector() {}

BandwidthUsage OveruseDetector::State() const {
  return hypothesis_;
}

std::string OveruseDetector::GetState() const {
  if (hypothesis_ == BandwidthUsage::kBwNormal) {
    return "normal";
  } else if (hypothesis_ == BandwidthUsage::kBwOverusing) {
    return "overusing";
  } else if (hypothesis_ == BandwidthUsage::kBwUnderusing) {
    return "underusing";
  }
  return "unkown";
}

BandwidthUsage OveruseDetector::Detect(double offset,
                                       double ts_delta,
                                       int num_of_deltas,
                                       int64_t now_ms) {
  if (num_of_deltas < 2) {
    return BandwidthUsage::kBwNormal;
  }
  const double T = std::min(num_of_deltas, kMaxNumDeltas) * offset;
  if (T > threshold_) {
    if (time_over_using_ == -1) {
      // Initialize the timer. Assume that we've been
      // over-using half of the time since the previous
      // sample.
      time_over_using_ = ts_delta / 2;
    } else {
      // Increment timer
      time_over_using_ += ts_delta;
    }

    overuse_counter_++;
    if (time_over_using_ > overusing_time_threshold_ && overuse_counter_ > 3) {
        time_over_using_ = 0;
        overuse_counter_ = 0;
        hypothesis_ = BandwidthUsage::kBwOverusing;
    }else{
        hypothesis_ = BandwidthUsage::kBwNormal;
    }
  } else if (T < -threshold_) {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwUnderusing;
  } else {
    time_over_using_ = -1;
    overuse_counter_ = 0;
    hypothesis_ = BandwidthUsage::kBwNormal;
  }
  prev_offset_ = offset;

  UpdateThreshold(T, now_ms);
  return hypothesis_;
}

void OveruseDetector::UpdateThreshold(double modified_offset, int64_t now_ms) {
  if (!in_experiment_)
    return;

  if (last_update_ms_ == -1)
    last_update_ms_ = now_ms;

  if (fabs(modified_offset) > threshold_ + kMaxAdaptOffsetMs) {
    // Avoid adapting the threshold to big latency spikes, caused e.g.,
    // by a sudden capacity drop.
    last_update_ms_ = now_ms;
    return;
  }

  const double k = fabs(modified_offset) < threshold_ ? k_down_ : k_up_;
  const int64_t kMaxTimeDeltaMs = 100;
  int64_t time_delta_ms = std::min(now_ms - last_update_ms_, kMaxTimeDeltaMs);
  threshold_ += k * (fabs(modified_offset) - threshold_) * time_delta_ms;
  if (threshold_ < 6.f) {
    threshold_ = 6.f;
  }
  if (threshold_ > 600.f) {
    threshold_ = 600.f;
  }

  last_update_ms_ = now_ms;
}

void OveruseDetector::InitializeExperiment() {
  // RTC_DCHECK(in_experiment_);
  overusing_time_threshold_ = kOverUsingTimeThreshold;
}
}  // namespace webrtc
