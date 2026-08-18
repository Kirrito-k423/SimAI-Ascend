#include <iostream>
#include <limits>

#include "astra-sim/network_frontend/analytical/A2GroundTruth.h"

int main() {
  SimAIContract::A2GroundTruthScenarioInput input;
  input.id = "typed-nonfinite-test";
  input.step_time_ns = {
      std::numeric_limits<double>::infinity(), 101.0, 99.0, 100.0, 100.0};
  input.peak_hbm_B = {1U, 1U, 1U, 1U, 1U};
  input.base_hbm_B = 100U;
  input.completed_ranks = 8;
  input.expected_ranks = 8;
  input.loss_finite = true;
  input.gradients_finite = true;
  input.configured_tokens = 16U;
  input.consumed_tokens = 16U;
  input.provenance_digest = "sha256:typed";
  const auto result = SimAIContract::ValidateA2GroundTruthScenario(
      input, 16U, "sha256:typed");
  if (result.classification !=
          SimAIContract::A2GroundTruthValidationClass::InvalidInput ||
      result.reject_code != "A2_GROUND_TRUTH_STEP_TIME_INVALID") {
    std::cerr << "NONFINITE_STEP_ACCEPTED:" << result.reject_code << "\n";
    return 1;
  }
  return 0;
}
