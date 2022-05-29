/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "logger.hpp"
#include "timeex.hpp"

#include <math.h>
#include <algorithm>

namespace webrtc {

enum {
  // MS_NOTE: kAbsSendTimeFraction taken from RTPHeaderExtension::kAbsSendTimeFraction.
  kAbsSendTimeFraction = 18,
  kTimestampGroupLengthMs = 5,
  kAbsSendTimeInterArrivalUpshift = 8,
  kInterArrivalShift = kAbsSendTimeFraction +
                       kAbsSendTimeInterArrivalUpshift,
  kInitialProbingIntervalMs = 2000,
  kMinClusterSize = 4,
};

static const double kTimestampToMs =
    1000.0 / static_cast<double>(1 << kInterArrivalShift);

template <typename K, typename V>
std::vector<K> Keys(const std::map<K, V>& map) {
  std::vector<K> keys;
  keys.reserve(map.size());
  for (typename std::map<K, V>::const_iterator it = map.begin();
       it != map.end(); ++it) {
    keys.push_back(it->first);
  }
  return keys;
}

RemoteBitrateEstimatorAbsSendTime::~RemoteBitrateEstimatorAbsSendTime() =
    default;

RemoteBitrateEstimatorAbsSendTime::RemoteBitrateEstimatorAbsSendTime(
    RemoteBitrateObserver* observer)
    : observer_(observer),
      inter_arrival_(),
      estimator_(),
      detector_() {
  log_infof("RemoteBitrateEstimatorAbsSendTime: Instantiating.");
}

void RemoteBitrateEstimatorAbsSendTime::IncomingPacket(
    int64_t arrival_time_ms, size_t payload_size, uint32_t ssrc, const uint32_t send_time_24bits) {
  IncomingPacketInfo(arrival_time_ms, send_time_24bits, payload_size, ssrc);
}

void RemoteBitrateEstimatorAbsSendTime::IncomingPacketInfo(
    int64_t arrival_time_ms,
    uint32_t send_time_24bits,
    size_t payload_size,
    uint32_t ssrc) {
  // Shift up send time to use the full 32 bits that inter_arrival works with,
  // so wrapping works properly.
  uint32_t timestamp = send_time_24bits << kAbsSendTimeInterArrivalUpshift;
  //int64_t send_time_ms = static_cast<int64_t>(timestamp) * kTimestampToMs;

  int64_t now_ms = now_millisec();
  // TODO(holmer): SSRCs are only needed for REMB, should be broken out from
  // here.

  incoming_bitrate_.update(payload_size, arrival_time_ms);

  uint32_t ts_delta = 0;
  int64_t t_delta = 0;
  int size_delta = 0;
  bool update_estimate = false;
  uint32_t target_bitrate_bps = 0;
  std::vector<uint32_t> ssrcs;
  {
    TimeoutStreams(now_ms);
    ssrcs_[ssrc] = now_ms;

    if (inter_arrival_->ComputeDeltas(timestamp, arrival_time_ms, now_ms,
                                      payload_size, &ts_delta, &t_delta,
                                      &size_delta)) {
      double ts_delta_ms = (1000.0 * ts_delta) / (1 << kInterArrivalShift);
      estimator_->Update(t_delta, ts_delta_ms, size_delta, detector_.State(),
                         arrival_time_ms);
      detector_.Detect(estimator_->offset(), ts_delta_ms,
                       estimator_->num_of_deltas(), arrival_time_ms);
    }

    int64_t incoming_rate = -1;
    if (last_update_ms_ <= 0) {
        last_update_ms_ = now_ms;
    } else if ((now_ms - last_update_ms_) > 2000) {
        last_update_ms_ = now_ms;

        size_t count_per_second = 0;
        incoming_rate = 8 * incoming_bitrate_.bytes_per_second(arrival_time_ms, count_per_second);
        if (avg_bitrate_ <= 0) {
          avg_bitrate_ = incoming_rate;
        } else {
          avg_bitrate_ += (incoming_rate - avg_bitrate_)/5;
        }
    }

    update_estimate = rate_ctrl_.update(avg_bitrate_, detector_.State());

    if (update_estimate) {
      target_bitrate_bps = rate_ctrl_.get_target_bitrate();
      if (observer_ != nullptr) {
        auto ssrcs = Keys(ssrcs_);
        observer_->OnRembServerAvailableBitrate(
               this,
               ssrcs,
               target_bitrate_bps);
      }
    }
  }
  return;
}

void RemoteBitrateEstimatorAbsSendTime::TimeoutStreams(int64_t now_ms) {
  for (Ssrcs::iterator it = ssrcs_.begin(); it != ssrcs_.end();) {
    if ((now_ms - it->second) > kStreamTimeOutMs) {
      ssrcs_.erase(it++);
    } else {
      ++it;
    }
  }
  if (ssrcs_.empty()) {
    // We can't update the estimate if we don't have any active streams.
    inter_arrival_.reset(
        new InterArrival((kTimestampGroupLengthMs << kInterArrivalShift) / 1000,
                         kTimestampToMs, true));
    estimator_.reset(new OveruseEstimator(OverUseDetectorOptions()));
  }
}

void RemoteBitrateEstimatorAbsSendTime::RemoveStream(uint32_t ssrc) {
  ssrcs_.erase(ssrc);
}

bool RemoteBitrateEstimatorAbsSendTime::LatestEstimate(
    std::vector<uint32_t>* ssrcs,
    uint32_t* bitrate_bps) const {
  // Currently accessed from both the process thread (see
  // ModuleRtpRtcpImpl::Process()) and the configuration thread (see
  // Call::GetStats()). Should in the future only be accessed from a single
  // thread.
  #if 0
  if (!remote_rate_.ValidEstimate()) {
    return false;
  }
  *ssrcs = Keys(ssrcs_);
  if (ssrcs_.empty()) {
    *bitrate_bps = 0;
  } else {
    *bitrate_bps = remote_rate_.LatestEstimate().bps<uint32_t>();
  }
  #endif
  return true;
}

void RemoteBitrateEstimatorAbsSendTime::SetMinBitrate(int min_bitrate_bps) {
  // Called from both the configuration thread and the network thread. Shouldn't
  // be called from the network thread in the future.
  // remote_rate_.SetMinBitrate(DataRate::bps(min_bitrate_bps));
}
}  // namespace webrtc
