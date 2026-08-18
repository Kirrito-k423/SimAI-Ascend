/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__
#define __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__

#include <cstdint>
#include <string>
#include <vector>

#include "astra-sim/system/CollectiveCostModel.hh"
#include "ProjectedA2ATraffic.h"

namespace SimAIContract {

struct HcclCostSegment {
  uint64_t minimum_message_bytes = 0;
  uint64_t maximum_message_bytes = 0;
  bool maximum_inclusive = false;
  uint64_t startup_ns = 0;
  double bandwidth_Bps = 0.0;
};

struct HcclCostModelConfig {
  std::string model_id;
  AstraSim::CostedCollective collective =
      AstraSim::CostedCollective::Unsupported;
  AstraSim::CostedGroupType group_type =
      AstraSim::CostedGroupType::Unsupported;
  int rank_count = 0;
  uint64_t minimum_message_bytes = 0;
  uint64_t maximum_message_bytes = 0;
  uint64_t startup_ns = 0;
  double bandwidth_Bps = 0.0;
  std::vector<HcclCostSegment> segments;
  std::string payload_semantics;
  std::string traffic_algorithm;
  std::string source_adapter = "NONE";
  std::string routing_digest = "NOT_REQUIRED";
  uint64_t routing_total_traffic_bytes = 0;
  uint64_t routing_max_receive_bytes = 0;
  std::vector<uint64_t> routing_send_counts;
  std::string topology_domain;
  std::string topology_digest;
  ProjectedA2AConfig projected_a2a;
};

class HcclCostModel : public AstraSim::CollectiveCostModel {
 public:
  explicit HcclCostModel(const HcclCostModelConfig& config);

  AstraSim::CollectiveCostEstimate Estimate(
      const AstraSim::CollectiveCostRequest& request) override;
  AstraSim::CollectiveCostSummary Summary() const override;
  const ProjectedA2ASummary& ProjectedSummary() const;

 private:
  AstraSim::CollectiveCostEstimate Unsupported(const std::string& reason);

  HcclCostModelConfig config_;
  AstraSim::CollectiveCostSummary summary_;
  ProjectedA2ASummary projected_summary_;
};

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__
