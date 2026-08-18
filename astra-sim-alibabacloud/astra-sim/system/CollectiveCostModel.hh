/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __COLLECTIVE_COST_MODEL_HH__
#define __COLLECTIVE_COST_MODEL_HH__

#include <cstdint>
#include <string>

namespace AstraSim {

enum class CostedCollective {
  AllReduce,
  AllGather,
  ReduceScatter,
  AllToAll,
  AllToAllV,
  Unsupported,
};

enum class CostedGroupType {
  TP,
  DP,
  EP,
  DP_EP,
  Unsupported,
};

struct CollectiveCostRequest {
  CostedCollective collective = CostedCollective::Unsupported;
  CostedGroupType group_type = CostedGroupType::Unsupported;
  uint64_t message_bytes_per_rank = 0;
  int rank_count = 0;
  int tp_size = 0;
  int ep_size = 0;
  std::string topology_domain;
  std::string topology_digest;
};

struct CollectiveCostEstimate {
  bool supported = false;
  uint64_t duration_ns = 0;
  uint64_t traffic_bytes = 0;
  std::string reason;
};

struct CollectiveCostSummary {
  bool has_estimate = false;
  bool unsupported_request = false;
  uint64_t total_duration_ns = 0;
  uint64_t total_traffic_bytes = 0;
  uint64_t message_bytes_per_rank = 0;
  int rank_count = 0;
  CostedCollective collective = CostedCollective::Unsupported;
  CostedGroupType group_type = CostedGroupType::Unsupported;
  std::string topology_domain;
  std::string payload_semantics;
  uint64_t input_bytes_per_rank = 0;
  uint64_t output_bytes_per_rank = 0;
  std::string routing_digest = "NOT_REQUIRED";
  std::string unsupported_reason;
};

class CollectiveCostModel {
 public:
  virtual ~CollectiveCostModel() = default;
  virtual CollectiveCostEstimate Estimate(
      const CollectiveCostRequest& request) = 0;
  virtual CollectiveCostSummary Summary() const = 0;
};

const char* CostedCollectiveName(CostedCollective collective);
const char* CostedGroupTypeName(CostedGroupType group_type);

}  // namespace AstraSim

#endif  // __COLLECTIVE_COST_MODEL_HH__
