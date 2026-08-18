/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "A2GroundTruth.h"

#include <algorithm>
#include <cmath>

namespace SimAIContract {
namespace {

uint64_t MedianOfSorted(const std::vector<uint64_t>& sorted) {
  const size_t middle = sorted.size() / 2U;
  if ((sorted.size() % 2U) != 0U) {
    return sorted[middle];
  }
  return sorted[middle - 1U] / 2U + sorted[middle] / 2U +
      ((sorted[middle - 1U] % 2U + sorted[middle] % 2U) / 2U);
}

double SampleCv(const std::vector<uint64_t>& samples, size_t count) {
  long double sum = 0.0L;
  for (size_t index = 0U; index < count; ++index) {
    sum += static_cast<long double>(samples[index]);
  }
  const long double mean = sum / static_cast<long double>(count);
  long double squared_deviation = 0.0L;
  for (size_t index = 0U; index < count; ++index) {
    const long double difference =
        static_cast<long double>(samples[index]) - mean;
    squared_deviation += difference * difference;
  }
  const long double deviation = std::sqrt(
      squared_deviation / static_cast<long double>(count - 1U));
  return static_cast<double>(deviation / mean);
}

bool HbmLimitReached(uint64_t peak_hbm_B, uint64_t base_hbm_B) {
  if (base_hbm_B == 0U) {
    return true;
  }
  // ceil(base * 17 / 20), expressed without an overflowing multiplication.
  const uint64_t quotient = base_hbm_B / 20U;
  const uint64_t remainder = base_hbm_B % 20U;
  const uint64_t threshold =
      quotient * 17U + (remainder * 17U + 19U) / 20U;
  return peak_hbm_B >= threshold;
}

uint64_t LinearType7P90(std::vector<uint64_t> sorted) {
  std::sort(sorted.begin(), sorted.end());
  const long double position =
      static_cast<long double>(sorted.size() - 1U) * 0.9L;
  const size_t lower = static_cast<size_t>(std::floor(position));
  const size_t upper = std::min(lower + 1U, sorted.size() - 1U);
  const long double fraction = position - static_cast<long double>(lower);
  return static_cast<uint64_t>(std::llround(
      static_cast<long double>(sorted[lower]) +
      fraction * static_cast<long double>(sorted[upper] - sorted[lower])));
}

A2GroundTruthScenarioValidation InvalidAccuracy(
    const std::string& code) {
  A2GroundTruthScenarioValidation validation;
  validation.classification =
      A2GroundTruthValidationClass::InvalidAccuracyExecution;
  validation.reject_code = code;
  validation.message =
      "The A2 scenario is invalid and cannot enter calibration.";
  validation.remediation =
      "Correct the execution and repeat the unchanged frozen scenario.";
  return validation;
}

}  // namespace

A2GroundTruthScenarioValidation ValidateA2GroundTruthScenario(
    const A2GroundTruthScenarioInput& input,
    uint64_t expected_configured_tokens,
    const std::string& expected_provenance_digest) {
  if (input.oom) {
    return InvalidAccuracy("A2_OOM");
  }
  if ((input.step_time_ns.size() != 5U &&
       input.step_time_ns.size() != 10U) ||
      input.peak_hbm_B.size() != input.step_time_ns.size()) {
    A2GroundTruthScenarioValidation validation;
    validation.classification = A2GroundTruthValidationClass::InvalidInput;
    validation.reject_code = "A2_GROUND_TRUTH_SAMPLE_COUNT_INVALID";
    validation.message =
        "The A2 scenario must contain exactly five or ten paired samples.";
    validation.remediation =
        "Capture five complete pairs, extending the unchanged run to ten only when required.";
    return validation;
  }
  for (const uint64_t sample : input.step_time_ns) {
    if (sample == 0U) {
      A2GroundTruthScenarioValidation validation;
      validation.classification = A2GroundTruthValidationClass::InvalidInput;
      validation.reject_code = "A2_GROUND_TRUTH_STEP_TIME_INVALID";
      validation.message =
          "A2 step-time samples must be positive integral nanoseconds.";
      validation.remediation =
          "Provide nonzero canonical uint64 nanosecond samples.";
      return validation;
    }
  }
  const uint64_t maximum_hbm =
      *std::max_element(input.peak_hbm_B.begin(), input.peak_hbm_B.end());
  if (HbmLimitReached(maximum_hbm, input.base_hbm_B)) {
    return InvalidAccuracy("A2_HBM_LIMIT_REACHED");
  }
  if (input.completed_ranks != input.expected_ranks ||
      input.expected_ranks != 8) {
    return InvalidAccuracy("A2_RANK_LOSS");
  }
  if (!input.loss_finite || !input.gradients_finite) {
    return InvalidAccuracy("A2_NON_FINITE");
  }
  if (input.configured_tokens != expected_configured_tokens ||
      input.consumed_tokens != input.configured_tokens ||
      input.dropped_tokens != 0U) {
    return InvalidAccuracy("A2_TOKEN_LOSS");
  }
  if (input.replayed_tokens != 0U) {
    return InvalidAccuracy("A2_TOKEN_REPLAY");
  }
  if (input.provenance_digest != expected_provenance_digest) {
    return InvalidAccuracy("A2_PROVENANCE_DRIFT");
  }
  const double initial_cv = SampleCv(input.step_time_ns, 5U);
  if (!std::isfinite(initial_cv)) {
    A2GroundTruthScenarioValidation validation;
    validation.classification = A2GroundTruthValidationClass::InvalidInput;
    validation.reject_code = "A2_GROUND_TRUTH_STEP_TIME_INVALID";
    validation.message = "The A2 step-time CV is not finite.";
    validation.remediation = "Provide finite positive uint64 samples.";
    return validation;
  }
  const bool high_variation = initial_cv > 0.1;
  if ((!high_variation && input.step_time_ns.size() != 5U) ||
      (high_variation && input.step_time_ns.size() != 10U)) {
    A2GroundTruthScenarioValidation validation;
    validation.classification = A2GroundTruthValidationClass::InvalidInput;
    validation.reject_code = "A2_GROUND_TRUTH_SAMPLE_RULE_VIOLATION";
    validation.message =
        "The A2 sample count does not follow the fixed 5/10 CV rule.";
    validation.remediation =
        "Use five samples when initial CV is at most 10 percent; otherwise extend to ten.";
    return validation;
  }
  std::vector<uint64_t> sorted = input.step_time_ns;
  std::sort(sorted.begin(), sorted.end());
  A2GroundTruthScenarioValidation validation;
  validation.classification = A2GroundTruthValidationClass::Valid;
  validation.summary.id = input.id;
  validation.summary.sample_count =
      static_cast<int>(input.step_time_ns.size());
  validation.summary.cv = SampleCv(input.step_time_ns, input.step_time_ns.size());
  if (!std::isfinite(validation.summary.cv)) {
    validation.classification = A2GroundTruthValidationClass::InvalidInput;
    validation.reject_code = "A2_GROUND_TRUTH_STEP_TIME_INVALID";
    validation.message = "The complete A2 step-time CV is not finite.";
    validation.remediation = "Provide finite positive uint64 samples.";
    return validation;
  }
  validation.summary.median_step_time_ns = MedianOfSorted(sorted);
  validation.summary.minimum_step_time_ns = sorted.front();
  validation.summary.maximum_step_time_ns = sorted.back();
  validation.summary.peak_hbm_B = maximum_hbm;
  validation.summary.representative_statistic = high_variation
      ? "LINEAR_TYPE7_P90"
      : "MEDIAN";
  validation.summary.representative_step_time_ns = high_variation
      ? LinearType7P90(sorted)
      : validation.summary.median_step_time_ns;
  return validation;
}

}  // namespace SimAIContract
