/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_ABS_SEND_TIME_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_ABS_SEND_TIME_H_

#include "modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "modules/remote_bitrate_estimator/inter_arrival.h"
#include "modules/remote_bitrate_estimator/overuse_detector.h"
#include "modules/remote_bitrate_estimator/overuse_estimator.h"
#include "stream_statics.hpp"
#include <stddef.h>
#include <stdint.h>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace webrtc {

class RemoteBitrateEstimatorAbsSendTime : public RemoteBitrateEstimator {
 public:
  RemoteBitrateEstimatorAbsSendTime(RemoteBitrateObserver* observer);
  ~RemoteBitrateEstimatorAbsSendTime();

  void IncomingPacket(
      int64_t arrivalTimeMs, size_t payloadSize, uint32_t ssrc, uint32_t absSendTime) override;
  // This class relies on Process() being called periodically (at least once
  // every other second) for streams to be timed out properly. Therefore it
  // shouldn't be detached from the ProcessThread except if it's about to be
  // deleted.
  // void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override;
  void RemoveStream(uint32_t ssrc) override;
  bool LatestEstimate(std::vector<uint32_t>* ssrcs,
                      uint32_t* bitrate_bps) const override;
  void SetMinBitrate(int min_bitrate_bps) override;

 private:
  typedef std::map<uint32_t, int64_t> Ssrcs;

  void IncomingPacketInfo(int64_t arrival_time_ms,
                          uint32_t send_time_24bits,
                          size_t payload_size,
                          uint32_t ssrc);

  void TimeoutStreams(int64_t now_ms);

  RemoteBitrateObserver* const observer_;
  std::unique_ptr<InterArrival> inter_arrival_;
  std::unique_ptr<OveruseEstimator> estimator_;
  OveruseDetector detector_;
  stream_statics incoming_bitrate_;
  int64_t avg_bitrate_ = -1;

  Ssrcs ssrcs_;
  int64_t last_update_ms_ = -1;
  //AimdRateControl remote_rate_;
};

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_REMOTE_BITRATE_ESTIMATOR_ABS_SEND_TIME_H_
