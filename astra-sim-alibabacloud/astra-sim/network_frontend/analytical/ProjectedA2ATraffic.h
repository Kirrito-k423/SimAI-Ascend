/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#ifndef __SIMAI_ANALYTICAL_PROJECTED_A2A_TRAFFIC_H__
#define __SIMAI_ANALYTICAL_PROJECTED_A2A_TRAFFIC_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SimAIContract {

enum class ProjectedA2APolicy { Uniform, DenseCounts, Unsupported };

struct ProjectedA2ADomain {
  std::string id;
  int first_rank = 0;
  int rank_count = 0;
};

struct ProjectedA2AResource {
  std::string id;
  std::string scope;
};

struct ProjectedA2AConfig {
  bool present = false;
  int rank_count = 0;
  ProjectedA2APolicy policy = ProjectedA2APolicy::Unsupported;
  std::string scenario;
  uint64_t message_bytes_per_rank = 0;
  std::vector<uint64_t> dense_send_counts;
  std::vector<ProjectedA2ADomain> domains;
  std::vector<ProjectedA2AResource> resources;
  std::string artifact_id;
  std::string artifact_sha256 = "UNKNOWN";
  std::string evidence_level = "UNKNOWN";
  std::string field_readiness = "UNKNOWN";
  size_t artifact_bytes_read = 0U;
  uint64_t parse_time_ns = 0U;
};

struct ProjectedA2ARankSummary {
  int rank = 0;
  std::string domain;
  uint64_t send_bytes = 0;
  uint64_t receive_bytes = 0;
};

struct ProjectedA2ADomainSummary {
  std::string domain;
  uint64_t send_bytes = 0;
  uint64_t receive_bytes = 0;
};

struct ProjectedA2AResourceSummary {
  std::string id;
  std::string scope;
  uint64_t offered_load_bytes = 0;
};

struct ProjectedA2ASummary {
  bool present = false;
  bool consumed_by_analytical = false;
  bool ready = false;
  std::string failure_reason;
  std::string policy;
  std::string scenario;
  uint64_t global_bytes = 0;
  std::vector<ProjectedA2ARankSummary> per_rank;
  std::vector<ProjectedA2ADomainSummary> per_domain;
  std::vector<uint64_t> domain_matrix_bytes;
  std::vector<ProjectedA2AResourceSummary> resource_loads;
  bool global_equals_rank_send = false;
  bool global_equals_rank_receive = false;
  bool global_equals_domain_matrix = false;
  bool global_equals_resource_loads = false;
  bool domain_rows_equal_send = false;
  bool domain_columns_equal_receive = false;
  int hottest_receive_rank = 0;
  uint64_t maximum_rank_receive_bytes = 0;
  double mean_rank_receive_bytes = 0.0;
  double maximum_to_mean_receive_ratio = 0.0;
  size_t routing_records_read = 0U;
  size_t artifact_bytes_read = 0U;
  uint64_t parse_time_ns = 0U;
  uint64_t projection_time_ns = 0U;
  uint64_t directed_pairs_represented = 0U;
  uint64_t directed_pairs_materialized = 0U;
  std::string artifact_id;
  std::string artifact_sha256 = "UNKNOWN";
  std::string evidence_level = "UNKNOWN";
  std::string field_readiness = "UNKNOWN";
};

// This is an Analytical aggregation. It never creates endpoint flow objects.
bool ProjectA2ATraffic(
    const ProjectedA2AConfig& config,
    int runtime_rank_count,
    uint64_t runtime_message_bytes_per_rank,
    bool runtime_alltoallv,
    ProjectedA2ASummary* summary);

const char* ProjectedA2APolicyName(ProjectedA2APolicy policy);

}  // namespace SimAIContract

#endif  // __SIMAI_ANALYTICAL_PROJECTED_A2A_TRAFFIC_H__
