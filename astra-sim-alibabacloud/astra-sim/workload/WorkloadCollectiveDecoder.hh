/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH
#define ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH

#include <cstdint>
#include <istream>
#include <sstream>
#include <string>

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
};

inline WorkloadLayerRecordFormat DecodeWorkloadLayerRecordFormat(
    const std::string& header) {
  std::istringstream header_tokens(header);
  std::string run_type;
  header_tokens >> run_type;
  return run_type == "HYBRID_CUSTOMIZED"
      ? WorkloadLayerRecordFormat::HybridCustomized13
      : WorkloadLayerRecordFormat::Standard12;
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
  return format != WorkloadLayerRecordFormat::HybridCustomized13 ||
      static_cast<bool>(input >> record->specific_parallelism);
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
