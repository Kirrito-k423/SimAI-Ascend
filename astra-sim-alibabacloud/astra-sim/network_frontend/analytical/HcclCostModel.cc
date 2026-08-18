/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "HcclCostModel.h"

#include <cmath>
#include <limits>

namespace SimAIContract {

HcclCostModel::HcclCostModel(const HcclCostModelConfig& config)
    : config_(config) {}

AstraSim::CollectiveCostEstimate HcclCostModel::Unsupported(
    const std::string& reason) {
  summary_.unsupported_request = true;
  summary_.unsupported_reason = reason;
  AstraSim::CollectiveCostEstimate estimate;
  estimate.reason = reason;
  return estimate;
}

AstraSim::CollectiveCostEstimate HcclCostModel::Estimate(
    const AstraSim::CollectiveCostRequest& request) {
  if (request.collective != config_.collective) {
    return Unsupported("COLLECTIVE_OUTSIDE_MODEL_DOMAIN");
  }
  if (request.group_type != config_.group_type ||
      request.rank_count != config_.rank_count) {
    return Unsupported("GROUP_OUTSIDE_MODEL_DOMAIN");
  }
  if (request.rank_count < 2) {
    return Unsupported("GROUP_OUTSIDE_MODEL_DOMAIN");
  }
  if (request.topology_domain != config_.topology_domain ||
      request.topology_digest != config_.topology_digest) {
    return Unsupported("TOPOLOGY_OUTSIDE_MODEL_DOMAIN");
  }
  if (request.message_bytes_per_rank < config_.minimum_message_bytes ||
      request.message_bytes_per_rank > config_.maximum_message_bytes) {
    return Unsupported("MESSAGE_OUTSIDE_MODEL_DOMAIN");
  }

  const double transfer_ns =
      static_cast<double>(request.message_bytes_per_rank) * 1000000000.0 /
      config_.bandwidth_Bps;
  const double duration_ns =
      static_cast<double>(config_.startup_ns) + transfer_ns;
  if (!std::isfinite(duration_ns) || duration_ns < 0.0 ||
      duration_ns > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    return Unsupported("MODEL_RESULT_OUT_OF_RANGE");
  }
  const uint64_t ring_multiplier =
      2U * static_cast<uint64_t>(request.rank_count - 1);
  if (request.message_bytes_per_rank >
      std::numeric_limits<uint64_t>::max() / ring_multiplier) {
    return Unsupported("TRAFFIC_RESULT_OUT_OF_RANGE");
  }

  AstraSim::CollectiveCostEstimate estimate;
  estimate.supported = true;
  estimate.duration_ns = static_cast<uint64_t>(std::llround(duration_ns));
  estimate.traffic_bytes = request.message_bytes_per_rank * ring_multiplier;
  if (summary_.total_duration_ns >
          std::numeric_limits<uint64_t>::max() - estimate.duration_ns ||
      summary_.total_traffic_bytes >
          std::numeric_limits<uint64_t>::max() - estimate.traffic_bytes) {
    return Unsupported("MODEL_ACCUMULATION_OUT_OF_RANGE");
  }

  summary_.has_estimate = true;
  summary_.total_duration_ns += estimate.duration_ns;
  summary_.total_traffic_bytes += estimate.traffic_bytes;
  summary_.message_bytes_per_rank = request.message_bytes_per_rank;
  summary_.rank_count = request.rank_count;
  summary_.collective = request.collective;
  summary_.group_type = request.group_type;
  summary_.topology_domain = request.topology_domain;
  return estimate;
}

AstraSim::CollectiveCostSummary HcclCostModel::Summary() const {
  return summary_;
}

}  // namespace SimAIContract
