/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_RUN_CONTRACT_H__
#define __SIMAI_ANALYTICAL_RUN_CONTRACT_H__

#include <string>

#include "HcclCostModel.h"
#include "astra-sim/system/Common.hh"

namespace SimAIContract {

struct LegacyGpuRunConfig {
  int gpu_count = 0;
  int gpus_per_server = 0;
  int nics_per_server = 0;
  double nvlink_bandwidth_GBps = 0.0;
  double nic_bandwidth_GBps = 0.0;
  GPUType gpu_type = GPUType::NONE;
};

struct AnalyticalRunContract {
  bool enabled = false;
  bool accepted = false;
  int exit_code = 2;
  std::string status = "INVALID_INPUT";
  std::string reject_code = "RUN_CONTRACT_CLI_INVALID";
  std::string message = "The Run Contract invocation is invalid.";
  std::string remediation =
      "Provide --run-manifest <file> and --result-manifest <file>.";

  std::string run_manifest_path;
  std::string result_manifest_path;
  std::string binary_path;
  std::string schema_version = "UNKNOWN";
  std::string run_id = "UNKNOWN";
  std::string backend = "analytical";
  std::string workload_path;
  std::string run_manifest_sha256 = "UNKNOWN";
  std::string workload_sha256 = "UNKNOWN";
  std::string binary_sha256 = "UNKNOWN";
  std::string device_profile_sha256 = "UNKNOWN";
  std::string cost_model_sha256 = "UNKNOWN";
  std::string raw_observation_sha256 = "UNKNOWN";
  std::string routing_sha256 = "UNKNOWN";
  std::string routing_evidence_level = "UNKNOWN";
  std::string routing_field_readiness = "UNKNOWN";
  bool routing_required = false;
  std::string topology_readiness = "UNKNOWN";
  bool topology_required = false;
  bool device_profile_present = false;
  bool ascend_profiled = false;
  int ascend_rank_count = 0;
  std::string topology_domain;
  std::string topology_digest;
  std::string profile_evidence_level = "UNKNOWN";
  std::string profile_field_readiness = "UNKNOWN";
  std::string raw_observation_evidence_level = "UNKNOWN";
  std::string raw_observation_field_readiness = "UNKNOWN";
  std::string cost_model_evidence_level = "UNKNOWN";
  std::string cost_model_field_readiness = "UNKNOWN";
  HcclCostModelConfig hccl_cost_model;
  bool workload_digest_verified = false;
  LegacyGpuRunConfig legacy_gpu;
};

// Returns enabled=false when the legacy CLI should remain authoritative.
AnalyticalRunContract LoadAnalyticalRunContract(int argc, char* argv[]);

// Result Manifests never include input paths or raw process logs.
bool WriteAnalyticalResultManifest(
    const AnalyticalRunContract& contract,
    bool execution_succeeded,
    const AstraSim::CollectiveCostModel* cost_model = nullptr);

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_RUN_CONTRACT_H__
