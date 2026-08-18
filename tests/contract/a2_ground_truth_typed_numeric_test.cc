#include <cstdint>
#include <iostream>
#include <limits>

#include "astra-sim/network_frontend/analytical/A2GroundTruth.h"

using SimAIContract::A2GroundTruthScenarioInput;
using SimAIContract::A2GroundTruthValidationClass;
using SimAIContract::ValidateA2GroundTruthScenario;

namespace {

A2GroundTruthScenarioInput ValidInput() {
  A2GroundTruthScenarioInput input;
  input.id = "typed-test";
  input.step_time_ns = {100U, 101U, 99U, 100U, 100U};
  input.peak_hbm_B = {1U, 1U, 1U, 1U, 1U};
  input.base_hbm_B = 100U;
  input.completed_ranks = 8;
  input.expected_ranks = 8;
  input.loss_finite = true;
  input.gradients_finite = true;
  input.configured_tokens = 16U;
  input.consumed_tokens = 16U;
  input.provenance_digest = "sha256:typed";
  return input;
}

}  // namespace

int main() {
  A2GroundTruthScenarioInput zero = ValidInput();
  zero.step_time_ns[0] = 0U;
  const auto zero_result =
      ValidateA2GroundTruthScenario(zero, 16U, "sha256:typed");
  if (zero_result.classification != A2GroundTruthValidationClass::InvalidInput ||
      zero_result.reject_code != "A2_GROUND_TRUTH_STEP_TIME_INVALID") {
    std::cerr << "ZERO_STEP_ACCEPTED:" << zero_result.reject_code << "\n";
    return 1;
  }

  A2GroundTruthScenarioInput maximum = ValidInput();
  maximum.base_hbm_B = std::numeric_limits<uint64_t>::max();
  maximum.peak_hbm_B.assign(
      5U, std::numeric_limits<uint64_t>::max());
  const auto maximum_result =
      ValidateA2GroundTruthScenario(maximum, 16U, "sha256:typed");
  if (maximum_result.classification !=
          A2GroundTruthValidationClass::InvalidAccuracyExecution ||
      maximum_result.reject_code != "A2_HBM_LIMIT_REACHED") {
    std::cerr << "UINT64_HBM_OVERFLOW:" << maximum_result.reject_code << "\n";
    return 2;
  }
  return 0;
}
