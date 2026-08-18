/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_A2_GROUND_TRUTH_H__
#define __SIMAI_A2_GROUND_TRUTH_H__

#include <cstdint>
#include <string>
#include <vector>

namespace SimAIContract {

struct A2GroundTruthScenarioSummary {
  std::string id;
  int sample_count = 0;
  double cv = 0.0;
  uint64_t median_step_time_ns = 0;
  uint64_t minimum_step_time_ns = 0;
  uint64_t maximum_step_time_ns = 0;
  uint64_t representative_step_time_ns = 0;
  uint64_t peak_hbm_B = 0;
  std::string representative_statistic = "UNKNOWN";
};

struct A2GroundTruthScenarioInput {
  std::string id;
  std::vector<uint64_t> step_time_ns;
  std::vector<uint64_t> peak_hbm_B;
  uint64_t base_hbm_B = 0;
  int completed_ranks = 0;
  int expected_ranks = 0;
  bool loss_finite = false;
  bool gradients_finite = false;
  bool oom = false;
  uint64_t configured_tokens = 0;
  uint64_t consumed_tokens = 0;
  uint64_t dropped_tokens = 0;
  uint64_t replayed_tokens = 0;
  std::string provenance_digest;
};

enum class A2GroundTruthValidationClass {
  Valid,
  InvalidInput,
  InvalidAccuracyExecution,
};

struct A2GroundTruthScenarioValidation {
  A2GroundTruthValidationClass classification =
      A2GroundTruthValidationClass::InvalidInput;
  std::string reject_code;
  std::string message;
  std::string remediation;
  A2GroundTruthScenarioSummary summary;
};

A2GroundTruthScenarioValidation ValidateA2GroundTruthScenario(
    const A2GroundTruthScenarioInput& input,
    uint64_t expected_configured_tokens,
    const std::string& expected_provenance_digest);

}  // namespace SimAIContract

#endif  // __SIMAI_A2_GROUND_TRUTH_H__
