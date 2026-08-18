/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_RUN_CONTRACT_H__
#define __SIMAI_ANALYTICAL_RUN_CONTRACT_H__

#include <cstdint>
#include <string>
#include <vector>

#include "A2GroundTruth.h"
#include "HcclCostModel.h"
#include "TopologyPlacement.h"
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
  std::string workload_snapshot;
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
  std::string profile_evidence_ref = "UNKNOWN";
  std::string ascend_profile_id = "UNKNOWN";
  bool profile_hardware_available = false;
  std::string raw_observation_evidence_level = "UNKNOWN";
  std::string raw_observation_field_readiness = "UNKNOWN";
  std::string raw_observation_evidence_ref = "UNKNOWN";
  bool raw_observation_hardware_available = false;
  std::string cost_model_evidence_level = "UNKNOWN";
  std::string cost_model_field_readiness = "UNKNOWN";
  std::string cost_model_evidence_ref = "UNKNOWN";
  bool cost_model_hardware_available = false;
  std::string hccl_group_id = "UNKNOWN";
  std::string hccl_group_membership_sha256 = "UNKNOWN";
  std::vector<int> hccl_group_members;
  bool target_workload_present = false;
  bool target_workload_ready = false;
  std::string target_workload_sha256 = "UNKNOWN";
  std::string target_runtime_record_format = "UNKNOWN";
  std::vector<std::string> target_runtime_specific_parallelism;
  bool target_model_ready = false;
  std::string target_model_sha256 = "UNKNOWN";
  std::string target_model_evidence_level = "UNKNOWN";
  std::string target_model_field_readiness = "UNKNOWN";
  bool target_step_ready = false;
  std::string target_step_sha256 = "UNKNOWN";
  std::string target_step_evidence_level = "UNKNOWN";
  std::string target_step_field_readiness = "UNKNOWN";
  uint64_t target_sequence_tokens = 0;
  uint64_t target_micro_batch_sequences = 0;
  uint64_t target_data_parallel_replicas = 0;
  uint64_t target_gradient_accumulation = 0;
  uint64_t target_configured_gts = 0;
  uint64_t target_routed_assignment_slots = 0;
  bool target_routing_ready = false;
  std::string target_routing_sha256 = "UNKNOWN";
  std::string target_routing_evidence_level = "UNKNOWN";
  std::string target_routing_field_readiness = "UNKNOWN";
  bool target_memory_event_plan_ready = false;
  std::string target_memory_event_plan_sha256 = "UNKNOWN";
  std::string target_memory_evidence_level = "UNKNOWN";
  std::string target_memory_field_readiness = "UNKNOWN";
  bool target_memory_symbolic = false;
  bool target_memory_materialized = false;
  bool target_memory_gate_failed = false;
  std::string target_precision_policy_sha256 = "UNKNOWN";
  std::string target_optimizer_policy_sha256 = "UNKNOWN";
  std::string target_placement_sha256 = "UNKNOWN";
  std::string target_recomputation_policy_sha256 = "UNKNOWN";
  std::string target_runtime_profile_sha256 = "UNKNOWN";
  uint64_t target_memory_parameters_B = 0;
  uint64_t target_memory_gradients_B = 0;
  uint64_t target_memory_optimizer_states_B = 0;
  uint64_t target_memory_activations_B = 0;
  uint64_t target_memory_communication_buffers_B = 0;
  uint64_t target_memory_expert_placement_B = 0;
  uint64_t target_memory_recomputation_B = 0;
  uint64_t target_memory_peak_B = 0;
  uint64_t target_memory_base_hbm_B = 0;
  uint64_t target_memory_reserve_hbm_B = 0;
  uint64_t target_memory_scenario_usable_hbm_B = 0;
  uint64_t target_memory_search_limit_B = 0;
  bool target_memory_execution_peak_known = false;
  uint64_t target_memory_execution_peak_B = 0;
  uint64_t target_memory_execution_boundary_B = 0;
  uint64_t target_memory_execution_maximum_accepted_B = 0;
  std::string target_memory_search_gate = "UNKNOWN";
  std::string target_memory_execution_gate = "UNKNOWN";
  uint64_t target_logical_trainable_parameters = 0;
  uint64_t target_checkpoint_auxiliary_elements = 0;
  uint64_t target_checkpoint_quant_scale_elements = 0;
  uint64_t target_checkpoint_routing_table_elements = 0;
  uint64_t target_checkpoint_storage_bytes = 0;
  uint64_t target_active_main_blocks_parameters = 0;
  uint64_t target_active_main_forward_parameters = 0;
  uint64_t target_active_training_graph_parameters = 0;
  int target_routed_experts = 0;
  int target_top_k = 0;
  int target_expert_intermediate_size = 0;
  int target_shared_experts = 0;
  bool a2_ground_truth_present = false;
  bool a2_ground_truth_ready = false;
  bool a2_calibration_eligible = false;
  std::string a2_ground_truth_run_sha256 = "UNKNOWN";
  std::string a2_ground_truth_result_sha256 = "UNKNOWN";
  std::string a2_ground_truth_status = "UNKNOWN";
  std::string a2_run_evidence_level = "UNKNOWN";
  std::string a2_run_field_readiness = "UNKNOWN";
  std::string a2_run_evidence_ref = "UNKNOWN";
  bool a2_run_hardware_available = false;
  std::string a2_ground_truth_evidence_level = "UNKNOWN";
  std::string a2_ground_truth_field_readiness = "UNKNOWN";
  std::string a2_result_evidence_ref = "UNKNOWN";
  bool a2_result_hardware_available = false;
  std::string a2_bound_profile_id = "UNKNOWN";
  std::string a2_bound_profile_sha256 = "UNKNOWN";
  std::string a2_bound_profile_evidence_level = "UNKNOWN";
  std::string a2_bound_profile_field_readiness = "UNKNOWN";
  std::string a2_bound_profile_evidence_ref = "UNKNOWN";
  std::string a2_bound_workload_sha256 = "UNKNOWN";
  std::string a2_bound_topology_sha256 = "UNKNOWN";
  std::string a2_bound_group_id = "UNKNOWN";
  std::string a2_bound_group_type = "UNKNOWN";
  int a2_bound_group_rank_count = 0;
  std::vector<int> a2_bound_group_members;
  std::string a2_bound_group_membership_sha256 = "UNKNOWN";
  std::string a2_derived_cost_model_sha256 = "UNKNOWN";
  int a2_raw_observation_count = 0;
  std::vector<A2GroundTruthScenarioSummary> a2_scenarios;
  TopologyPlacementConfig topology_placement;
  TopologyPlacementSummary topology_placement_summary;
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
    const HcclCostModel* cost_model = nullptr);

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_RUN_CONTRACT_H__
