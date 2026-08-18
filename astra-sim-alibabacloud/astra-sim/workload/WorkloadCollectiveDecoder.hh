/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH
#define ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH

#include <cstdint>
#include <istream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace AstraSim {

enum class WorkloadAllToAllVToken {
  NotAllToAllV,
  Default,
  ExpertParallel,
  DataAndExpertParallel,
  InvalidVariant,
};

enum class WorkloadLayerRecordFormat {
  Standard12,
  HybridCustomized13,
  TargetBound17,
  HybridCustomizedTargetBound18,
};

struct TargetWorkloadEventBinding {
  bool present = false;
  std::string model_sha256;
  std::string step_sha256;
  std::string routing_sha256;
  std::string memory_event_plan_sha256;
  std::string target_workload_sha256;
};

struct DecodedWorkloadLayerRecord {
  std::string id;
  int dependency = 0;
  uint64_t forward_compute_time = 0;
  std::string forward_collective;
  uint64_t forward_collective_bytes = 0;
  uint64_t input_gradient_compute_time = 0;
  std::string input_gradient_collective;
  uint64_t input_gradient_collective_bytes = 0;
  uint64_t weight_gradient_compute_time = 0;
  std::string weight_gradient_collective;
  uint64_t weight_gradient_collective_bytes = 0;
  uint64_t weight_update_time = 0;
  std::string specific_parallelism;
  TargetWorkloadEventBinding target_binding;
};

inline bool IsWorkloadSha256Identifier(const std::string& digest) {
  if (digest.size() != 71U || digest.compare(0U, 7U, "sha256:") != 0) {
    return false;
  }
  for (size_t index = 7U; index < digest.size(); ++index) {
    const char character = digest[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

inline bool TargetWorkloadBindingsEqual(
    const TargetWorkloadEventBinding& left,
    const TargetWorkloadEventBinding& right) {
  return left.present == right.present &&
      left.model_sha256 == right.model_sha256 &&
      left.step_sha256 == right.step_sha256 &&
      left.routing_sha256 == right.routing_sha256 &&
      left.memory_event_plan_sha256 == right.memory_event_plan_sha256 &&
      left.target_workload_sha256 == right.target_workload_sha256;
}

inline bool TargetWorkloadBindingIsComplete(
    const TargetWorkloadEventBinding& binding) {
  return binding.present &&
      IsWorkloadSha256Identifier(binding.model_sha256) &&
      IsWorkloadSha256Identifier(binding.step_sha256) &&
      IsWorkloadSha256Identifier(binding.routing_sha256) &&
      IsWorkloadSha256Identifier(binding.memory_event_plan_sha256) &&
      IsWorkloadSha256Identifier(binding.target_workload_sha256);
}

inline bool WorkloadLayerRecordFormatIsTargetBound(
    WorkloadLayerRecordFormat format) {
  return format == WorkloadLayerRecordFormat::TargetBound17 ||
      format == WorkloadLayerRecordFormat::HybridCustomizedTargetBound18;
}

inline bool WorkloadLayerRecordFormatIsCustomized(
    WorkloadLayerRecordFormat format) {
  return format == WorkloadLayerRecordFormat::HybridCustomized13 ||
      format == WorkloadLayerRecordFormat::HybridCustomizedTargetBound18;
}

inline bool DecodeWorkloadHeader(
    const std::string& header,
    WorkloadLayerRecordFormat* format,
    TargetWorkloadEventBinding* binding) {
  std::istringstream input(header);
  std::vector<std::string> tokens;
  std::string token;
  while (input >> token) {
    tokens.push_back(token);
  }
  if (tokens.empty()) {
    return false;
  }

  const bool customized = tokens.front() == "HYBRID_CUSTOMIZED";
  const std::map<std::string, size_t> target_fields = {
      {"target_model_sha256:", 0U},
      {"target_step_sha256:", 1U},
      {"target_routing_sha256:", 2U},
      {"target_memory_event_plan_sha256:", 3U},
      {"target_workload_sha256:", 4U},
  };
  std::vector<std::string> values(5U);
  size_t target_token_count = 0U;
  for (size_t index = 1U; index < tokens.size(); ++index) {
    const auto target = target_fields.find(tokens[index]);
    if (target != target_fields.end()) {
      if (index + 1U >= tokens.size() || !values[target->second].empty()) {
        return false;
      }
      values[target->second] = tokens[++index];
      ++target_token_count;
    } else if (tokens[index].compare(0U, 7U, "target_") == 0U) {
      return false;
    }
  }

  *binding = TargetWorkloadEventBinding();
  if (target_token_count == 0U) {
    *format = customized ? WorkloadLayerRecordFormat::HybridCustomized13
                         : WorkloadLayerRecordFormat::Standard12;
    return true;
  }
  if (target_token_count != values.size()) {
    return false;
  }
  binding->present = true;
  binding->model_sha256 = values[0];
  binding->step_sha256 = values[1];
  binding->routing_sha256 = values[2];
  binding->memory_event_plan_sha256 = values[3];
  binding->target_workload_sha256 = values[4];
  if (!TargetWorkloadBindingIsComplete(*binding)) {
    return false;
  }
  *format = customized
      ? WorkloadLayerRecordFormat::HybridCustomizedTargetBound18
      : WorkloadLayerRecordFormat::TargetBound17;
  return true;
}

inline WorkloadLayerRecordFormat DecodeWorkloadLayerRecordFormat(
    const std::string& header) {
  WorkloadLayerRecordFormat format = WorkloadLayerRecordFormat::Standard12;
  TargetWorkloadEventBinding binding;
  DecodeWorkloadHeader(header, &format, &binding);
  return format;
}

inline bool DecodeWorkloadLayerRecord(
    std::istream& input,
    WorkloadLayerRecordFormat format,
    DecodedWorkloadLayerRecord* record) {
  if (!(input >> record->id >> record->dependency >>
        record->forward_compute_time >> record->forward_collective >>
        record->forward_collective_bytes >>
        record->input_gradient_compute_time >>
        record->input_gradient_collective >>
        record->input_gradient_collective_bytes >>
        record->weight_gradient_compute_time >>
        record->weight_gradient_collective >>
        record->weight_gradient_collective_bytes >>
        record->weight_update_time)) {
    return false;
  }
  record->specific_parallelism.clear();
  record->target_binding = TargetWorkloadEventBinding();
  if (WorkloadLayerRecordFormatIsCustomized(format) &&
      !(input >> record->specific_parallelism)) {
    return false;
  }
  if (WorkloadLayerRecordFormatIsTargetBound(format)) {
    record->target_binding.present = true;
    if (!(input >> record->target_binding.model_sha256 >>
          record->target_binding.step_sha256 >>
          record->target_binding.routing_sha256 >>
          record->target_binding.memory_event_plan_sha256 >>
          record->target_binding.target_workload_sha256) ||
        !TargetWorkloadBindingIsComplete(record->target_binding)) {
      return false;
    }
  }
  return true;
}

inline WorkloadAllToAllVToken DecodeWorkloadAllToAllVToken(
    const std::string& token) {
  if (token == "ALLTOALLV") {
    return WorkloadAllToAllVToken::Default;
  }
  if (token == "ALLTOALLV_EP") {
    return WorkloadAllToAllVToken::ExpertParallel;
  }
  if (token == "ALLTOALLV_DP_EP") {
    return WorkloadAllToAllVToken::DataAndExpertParallel;
  }
  if (token.compare(0U, 9U, "ALLTOALLV") == 0) {
    return WorkloadAllToAllVToken::InvalidVariant;
  }
  return WorkloadAllToAllVToken::NotAllToAllV;
}

inline bool IsSupportedWorkloadAllToAllVToken(
    WorkloadAllToAllVToken token) {
  return token == WorkloadAllToAllVToken::Default ||
      token == WorkloadAllToAllVToken::ExpertParallel ||
      token == WorkloadAllToAllVToken::DataAndExpertParallel;
}

}  // namespace AstraSim

#endif  // ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH
