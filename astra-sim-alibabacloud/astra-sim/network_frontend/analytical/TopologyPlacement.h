/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_TOPOLOGY_PLACEMENT_H__
#define __SIMAI_ANALYTICAL_TOPOLOGY_PLACEMENT_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SimAIContract {

enum class TopologyPlacementKind {
  FlatRandom,
  TopologyAware,
  Unsupported
};

struct TopologyPlacementEvidence {
  std::string ref = "UNKNOWN";
  std::string evidence_class = "UNKNOWN";
  std::string readiness = "UNKNOWN";
};

struct TopologyPlacementConfig {
  bool present = false;
  std::string artifact_id;
  std::string artifact_sha256 = "UNKNOWN";
  size_t artifact_bytes_read = 0U;
  std::string target_workload_sha256 = "UNKNOWN";
  int routed_experts = 0;
  std::string topology_identity;
  std::string topology_scope;
  int domain_size_ranks = 0;
  TopologyPlacementEvidence topology_evidence;
  std::string resource_scenario;
  std::string spare_semantics;
  int attention_tp = 0;
  int attention_cp = 0;
  int attention_pp = 0;
  int moe_etp = 0;
  int moe_pp = 0;
  std::vector<int> ep_values;
  bool ragged_groups_supported = false;
  TopologyPlacementEvidence ragged_evidence;
  std::vector<TopologyPlacementKind> placement_kinds;
  uint64_t flat_random_seed = 0U;
  uint64_t message_bytes_per_rank = 0U;
};

struct TopologyPlacementCommunicationGroup {
  std::string grid;
  std::string representation;
  std::string membership_formula;
  std::string membership_digest = "UNKNOWN";
};

struct TopologyPlacementCandidateSummary {
  std::string id;
  std::string candidate_digest = "UNKNOWN";
  TopologyPlacementKind placement = TopologyPlacementKind::Unsupported;
  std::string rank_map_algorithm;
  std::string rank_map_digest = "UNKNOWN";
  uint64_t flat_random_seed = 0U;
  int attention_tp = 0;
  int attention_cp = 0;
  int attention_dp = 0;
  int attention_pp = 0;
  int moe_etp = 0;
  int moe_ep = 0;
  int moe_edp = 0;
  int moe_pp = 0;
  bool ragged = false;
  int full_ep_groups = 0;
  int partial_ep_group_ranks = 0;
  std::vector<TopologyPlacementCommunicationGroup> communication_groups;
  std::vector<uint64_t> domain_matrix_bytes;
  uint64_t global_bytes = 0U;
  uint64_t cross_domain_bytes = 0U;
  uint64_t intra_domain_bytes = 0U;
  double local_expert_hit = 0.0;
};

struct TopologyPlacementPairSummary {
  int ep = 0;
  std::string flat_candidate;
  std::string topology_aware_candidate;
  uint64_t flat_cross_domain_bytes = 0U;
  uint64_t topology_aware_cross_domain_bytes = 0U;
  int64_t topology_aware_cross_domain_reduction_bytes = 0;
  double flat_local_expert_hit = 0.0;
  double topology_aware_local_expert_hit = 0.0;
};

struct TopologyPlacementExpertPairSummary {
  TopologyPlacementKind placement = TopologyPlacementKind::Unsupported;
  int global_ep = 2048;
  int local_ep = 0;
  std::string global_candidate;
  std::string local_candidate;
  int64_t local_cross_domain_reduction_bytes = 0;
  double global_local_expert_hit = 0.0;
  double local_local_expert_hit = 0.0;
};

struct TopologyPlacementSummary {
  bool present = false;
  bool ready = false;
  std::string failure_reason;
  std::string topology_identity;
  std::string topology_scope;
  std::string topology_digest = "UNKNOWN";
  int domain_size_ranks = 0;
  int domain_count = 0;
  int full_domain_count = 0;
  int partial_domain_active_ranks = 0;
  TopologyPlacementEvidence topology_evidence;
  std::string resource_scenario;
  int active_ranks = 0;
  int capacity_ranks = 0;
  int spare_ranks = 0;
  std::string spare_semantics;
  bool ragged_groups_supported = false;
  TopologyPlacementEvidence ragged_evidence;
  std::string artifact_id;
  std::string artifact_sha256 = "UNKNOWN";
  size_t artifact_bytes_read = 0U;
  std::string target_workload_sha256 = "UNKNOWN";
  std::vector<TopologyPlacementCandidateSummary> candidates;
  std::vector<TopologyPlacementPairSummary> placement_pairs;
  std::vector<TopologyPlacementExpertPairSummary> expert_parallel_pairs;
  size_t maximum_projector_rank_state = 0U;
  size_t maximum_projector_domain_cells = 0U;
};

typedef std::string (*TopologyPlacementDigestFunction)(
    const std::string& bytes);

bool AnalyzeTopologyPlacements(
    const TopologyPlacementConfig& config,
    TopologyPlacementDigestFunction digest,
    TopologyPlacementSummary* summary);

const char* TopologyPlacementKindName(TopologyPlacementKind kind);

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_TOPOLOGY_PLACEMENT_H__
