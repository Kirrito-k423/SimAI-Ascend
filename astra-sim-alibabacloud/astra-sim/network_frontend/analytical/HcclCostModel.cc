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
  if (summary_.has_estimate) {
    return Unsupported("MULTIPLE_REQUESTS_UNSUPPORTED");
  }
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

  uint64_t startup_ns = config_.startup_ns;
  double bandwidth_Bps = config_.bandwidth_Bps;
  if (!config_.segments.empty()) {
    bool matched_segment = false;
    for (const HcclCostSegment& segment : config_.segments) {
      const bool below_upper = segment.maximum_inclusive
          ? request.message_bytes_per_rank <= segment.maximum_message_bytes
          : request.message_bytes_per_rank < segment.maximum_message_bytes;
      if (request.message_bytes_per_rank >= segment.minimum_message_bytes &&
          below_upper) {
        startup_ns = segment.startup_ns;
        bandwidth_Bps = segment.bandwidth_Bps;
        matched_segment = true;
        break;
      }
    }
    if (!matched_segment) {
      return Unsupported("MESSAGE_OUTSIDE_SEGMENT_DOMAIN");
    }
  }
  const double transfer_ns =
      static_cast<double>(request.message_bytes_per_rank) * 1000000000.0 /
      bandwidth_Bps;
  const double duration_ns =
      static_cast<double>(startup_ns) + transfer_ns;
  const double llround_upper_exclusive = std::ldexp(
      1.0, std::numeric_limits<long long>::digits);
  if (!std::isfinite(duration_ns) || duration_ns < 0.0 ||
      duration_ns >= llround_upper_exclusive) {
    return Unsupported("MODEL_RESULT_OUT_OF_RANGE");
  }
  uint64_t traffic_multiplier = 0;
  uint64_t output_multiplier = 1;
  uint64_t output_divisor = 1;
  bool variable_counts = false;
  switch (request.collective) {
    case AstraSim::CostedCollective::AllReduce:
      traffic_multiplier =
          2U * static_cast<uint64_t>(request.rank_count - 1);
      break;
    case AstraSim::CostedCollective::AllGather:
      traffic_multiplier = static_cast<uint64_t>(request.rank_count) *
          static_cast<uint64_t>(request.rank_count - 1);
      output_multiplier = static_cast<uint64_t>(request.rank_count);
      break;
    case AstraSim::CostedCollective::ReduceScatter:
      traffic_multiplier = static_cast<uint64_t>(request.rank_count - 1);
      output_divisor = static_cast<uint64_t>(request.rank_count);
      break;
    case AstraSim::CostedCollective::AllToAll:
      traffic_multiplier = static_cast<uint64_t>(request.rank_count - 1);
      output_divisor = 1;
      break;
    case AstraSim::CostedCollective::AllToAllV:
      variable_counts = true;
      break;
    case AstraSim::CostedCollective::Unsupported:
      return Unsupported("COLLECTIVE_OUTSIDE_MODEL_DOMAIN");
  }
  if ((request.collective == AstraSim::CostedCollective::ReduceScatter ||
       request.collective == AstraSim::CostedCollective::AllToAll) &&
      request.message_bytes_per_rank %
              static_cast<uint64_t>(request.rank_count) !=
          0U) {
    return Unsupported("PAYLOAD_OUTSIDE_COLLECTIVE_DOMAIN");
  }
  if (!variable_counts &&
      (request.message_bytes_per_rank >
           std::numeric_limits<uint64_t>::max() / traffic_multiplier ||
       request.message_bytes_per_rank >
           std::numeric_limits<uint64_t>::max() / output_multiplier)) {
    return Unsupported("TRAFFIC_RESULT_OUT_OF_RANGE");
  }
  if (config_.projected_a2a.present &&
      !ProjectA2ATraffic(
          config_.projected_a2a,
          request.rank_count,
          request.message_bytes_per_rank,
          variable_counts,
          &projected_summary_)) {
    return Unsupported(projected_summary_.failure_reason);
  }
  const uint64_t modeled_traffic_bytes = variable_counts
      ? config_.routing_total_traffic_bytes
      : request.message_bytes_per_rank * traffic_multiplier;
  if (config_.projected_a2a.present &&
      projected_summary_.global_bytes != modeled_traffic_bytes) {
    return Unsupported("PROJECTED_A2A_COST_TRAFFIC_MISMATCH");
  }

  AstraSim::CollectiveCostEstimate estimate;
  estimate.supported = true;
  estimate.duration_ns = static_cast<uint64_t>(std::llround(duration_ns));
  estimate.traffic_bytes = modeled_traffic_bytes;
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
  summary_.payload_semantics = config_.payload_semantics;
  summary_.input_bytes_per_rank = request.message_bytes_per_rank;
  summary_.output_bytes_per_rank = variable_counts
      ? config_.routing_max_receive_bytes
      : request.message_bytes_per_rank * output_multiplier / output_divisor;
  summary_.routing_digest = config_.routing_digest;
  return estimate;
}

AstraSim::CollectiveCostSummary HcclCostModel::Summary() const {
  return summary_;
}

const ProjectedA2ASummary& HcclCostModel::ProjectedSummary() const {
  return projected_summary_;
}

}  // namespace SimAIContract
