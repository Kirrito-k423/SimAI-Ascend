/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__
#define __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__

#include <cstdint>
#include <string>

#include "astra-sim/system/CollectiveCostModel.hh"

namespace SimAIContract {

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
  std::string topology_domain;
  std::string topology_digest;
};

class HcclCostModel : public AstraSim::CollectiveCostModel {
 public:
  explicit HcclCostModel(const HcclCostModelConfig& config);

  AstraSim::CollectiveCostEstimate Estimate(
      const AstraSim::CollectiveCostRequest& request) override;
  AstraSim::CollectiveCostSummary Summary() const override;

 private:
  AstraSim::CollectiveCostEstimate Unsupported(const std::string& reason);

  HcclCostModelConfig config_;
  AstraSim::CollectiveCostSummary summary_;
};

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_HCCL_COST_MODEL_H__
