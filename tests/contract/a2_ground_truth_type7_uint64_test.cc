#include <cstdint>
#include <iostream>
#include <limits>

#include "astra-sim/network_frontend/analytical/A2GroundTruth.h"

using SimAIContract::A2GroundTruthScenarioInput;
using SimAIContract::A2GroundTruthValidationClass;
using SimAIContract::ValidateA2GroundTruthScenario;

int main() {
  const uint64_t maximum = std::numeric_limits<uint64_t>::max();
  A2GroundTruthScenarioInput input;
  input.id = "type7-uint64-boundary";
  input.step_time_ns = {
      1U, maximum, maximum, maximum, maximum,
      maximum, maximum, maximum, maximum, maximum};
  input.peak_hbm_B.assign(10U, 1U);
  input.base_hbm_B = 100U;
  input.completed_ranks = 8;
  input.expected_ranks = 8;
  input.loss_finite = true;
  input.gradients_finite = true;
  input.configured_tokens = 16U;
  input.consumed_tokens = 16U;
  input.provenance_digest = "sha256:type7-boundary";

  const auto result = ValidateA2GroundTruthScenario(
      input, 16U, "sha256:type7-boundary");
  if (result.classification != A2GroundTruthValidationClass::Valid) {
    std::cerr << "TYPE7_BOUNDARY_REJECTED:" << result.reject_code << "\n";
    return 1;
  }
  if (result.summary.representative_statistic != "LINEAR_TYPE7_P90") {
    std::cerr << "TYPE7_BOUNDARY_WRONG_STATISTIC\n";
    return 2;
  }
  if (result.summary.representative_step_time_ns != maximum) {
    std::cerr << "TYPE7_UINT64_TRUNCATED:"
              << result.summary.representative_step_time_ns << "\n";
    return 3;
  }
  return 0;
}
