/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH
#define ASTRA_SIM_WORKLOAD_WORKLOAD_COLLECTIVE_DECODER_HH

#include <string>

namespace AstraSim {

enum class WorkloadAllToAllVToken {
  NotAllToAllV,
  Default,
  ExpertParallel,
  DataAndExpertParallel,
  InvalidVariant,
};

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
