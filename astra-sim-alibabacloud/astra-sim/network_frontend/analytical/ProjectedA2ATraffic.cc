/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "ProjectedA2ATraffic.h"

#include <algorithm>
#include <chrono>
#include <limits>

namespace SimAIContract {
namespace {

bool CheckedAdd(uint64_t value, uint64_t* total) {
  if (*total > std::numeric_limits<uint64_t>::max() - value) {
    return false;
  }
  *total += value;
  return true;
}

bool CheckedMultiply(uint64_t left, uint64_t right, uint64_t* product) {
  if (left != 0U && right > std::numeric_limits<uint64_t>::max() / left) {
    return false;
  }
  *product = left * right;
  return true;
}

uint64_t ElapsedNanoseconds(
    const std::chrono::steady_clock::time_point& start) {
  const std::chrono::steady_clock::duration elapsed =
      std::chrono::steady_clock::now() - start;
  const std::chrono::nanoseconds nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
  return nanoseconds.count() < 0
      ? 0U
      : static_cast<uint64_t>(nanoseconds.count());
}

bool Fail(ProjectedA2ASummary* summary, const std::string& reason) {
  summary->ready = false;
  summary->failure_reason = reason;
  return false;
}

}  // namespace

const char* ProjectedA2APolicyName(ProjectedA2APolicy policy) {
  switch (policy) {
    case ProjectedA2APolicy::Uniform:
      return "UNIFORM";
    case ProjectedA2APolicy::DenseCounts:
      return "DENSE_COUNTS";
    case ProjectedA2APolicy::Unsupported:
      return "UNSUPPORTED";
  }
  return "UNSUPPORTED";
}

bool ProjectA2ATraffic(
    const ProjectedA2AConfig& config,
    int runtime_rank_count,
    uint64_t runtime_message_bytes_per_rank,
    bool runtime_alltoallv,
    ProjectedA2ASummary* summary) {
  const std::chrono::steady_clock::time_point started =
      std::chrono::steady_clock::now();
  *summary = ProjectedA2ASummary();
  summary->present = config.present;
  summary->consumed_by_analytical = true;
  summary->policy = ProjectedA2APolicyName(config.policy);
  summary->scenario = config.scenario;
  summary->artifact_bytes_read = config.artifact_bytes_read;
  summary->parse_time_ns = config.parse_time_ns;
  summary->artifact_id = config.artifact_id;
  summary->artifact_sha256 = config.artifact_sha256;
  summary->evidence_level = config.evidence_level;
  summary->field_readiness = config.field_readiness;
  if (!config.present || config.rank_count != runtime_rank_count ||
      runtime_rank_count < 2) {
    return Fail(summary, "PROJECTED_A2A_RUNTIME_RANK_MISMATCH");
  }
  if ((runtime_alltoallv && config.policy != ProjectedA2APolicy::DenseCounts) ||
      (!runtime_alltoallv && config.policy != ProjectedA2APolicy::Uniform)) {
    return Fail(summary, "PROJECTED_A2A_COLLECTIVE_POLICY_MISMATCH");
  }
  if (config.policy == ProjectedA2APolicy::Uniform &&
      config.message_bytes_per_rank != runtime_message_bytes_per_rank) {
    return Fail(summary, "PROJECTED_A2A_MESSAGE_MISMATCH");
  }

  const size_t ranks = static_cast<size_t>(config.rank_count);
  const size_t domains = config.domains.size();
  summary->per_rank.resize(ranks);
  summary->per_domain.resize(domains);
  summary->domain_matrix_bytes.assign(domains * domains, 0U);
  std::vector<size_t> rank_domain(ranks, domains);
  for (size_t domain = 0U; domain < domains; ++domain) {
    summary->per_domain[domain].domain = config.domains[domain].id;
    const size_t first = static_cast<size_t>(config.domains[domain].first_rank);
    const size_t count = static_cast<size_t>(config.domains[domain].rank_count);
    for (size_t offset = 0U; offset < count; ++offset) {
      rank_domain[first + offset] = domain;
    }
  }
  for (size_t rank = 0U; rank < ranks; ++rank) {
    if (rank_domain[rank] == domains) {
      return Fail(summary, "PROJECTED_A2A_MEMBERSHIP_INCOMPLETE");
    }
    summary->per_rank[rank].rank = static_cast<int>(rank);
    summary->per_rank[rank].domain =
        config.domains[rank_domain[rank]].id;
  }

  if (config.policy == ProjectedA2APolicy::Uniform) {
    if (config.message_bytes_per_rank % ranks != 0U) {
      return Fail(summary, "PROJECTED_A2A_UNIFORM_NOT_DIVISIBLE");
    }
    const uint64_t pair_bytes = config.message_bytes_per_rank / ranks;
    uint64_t per_rank_network_bytes = 0U;
    if (!CheckedMultiply(pair_bytes, ranks - 1U, &per_rank_network_bytes) ||
        !CheckedMultiply(per_rank_network_bytes, ranks, &summary->global_bytes)) {
      return Fail(summary, "PROJECTED_A2A_OVERFLOW");
    }
    if (!CheckedMultiply(
            static_cast<uint64_t>(ranks),
            static_cast<uint64_t>(ranks - 1U),
            &summary->directed_pairs_represented)) {
      return Fail(summary, "PROJECTED_A2A_OVERFLOW");
    }
    summary->directed_pairs_materialized = 0U;
    for (size_t rank = 0U; rank < ranks; ++rank) {
      summary->per_rank[rank].send_bytes = per_rank_network_bytes;
      summary->per_rank[rank].receive_bytes = per_rank_network_bytes;
    }
    for (size_t source = 0U; source < domains; ++source) {
      for (size_t destination = 0U; destination < domains; ++destination) {
        uint64_t pairs = 0U;
        const uint64_t source_ranks =
            static_cast<uint64_t>(config.domains[source].rank_count);
        const uint64_t destination_ranks =
            static_cast<uint64_t>(config.domains[destination].rank_count);
        if (!CheckedMultiply(source_ranks, destination_ranks, &pairs)) {
          return Fail(summary, "PROJECTED_A2A_OVERFLOW");
        }
        if (source == destination) {
          pairs -= source_ranks;
        }
        uint64_t bytes = 0U;
        if (!CheckedMultiply(pairs, pair_bytes, &bytes)) {
          return Fail(summary, "PROJECTED_A2A_OVERFLOW");
        }
        summary->domain_matrix_bytes[source * domains + destination] = bytes;
      }
    }
    summary->routing_records_read = 0U;
  } else if (config.policy == ProjectedA2APolicy::DenseCounts) {
    if (config.dense_send_counts.size() != ranks * ranks) {
      return Fail(summary, "PROJECTED_A2A_DENSE_SHAPE_INVALID");
    }
    for (size_t source = 0U; source < ranks; ++source) {
      for (size_t destination = 0U; destination < ranks; ++destination) {
        const uint64_t bytes =
            config.dense_send_counts[source * ranks + destination];
        if (source == destination && bytes != 0U) {
          return Fail(summary, "PROJECTED_A2A_SELF_TRAFFIC_INVALID");
        }
        if (!CheckedAdd(bytes, &summary->global_bytes) ||
            !CheckedAdd(bytes, &summary->per_rank[source].send_bytes) ||
            !CheckedAdd(bytes, &summary->per_rank[destination].receive_bytes) ||
            !CheckedAdd(
                bytes,
                &summary->domain_matrix_bytes[
                    rank_domain[source] * domains + rank_domain[destination]])) {
          return Fail(summary, "PROJECTED_A2A_OVERFLOW");
        }
      }
    }
    summary->routing_records_read = config.dense_send_counts.size();
  } else {
    return Fail(summary, "PROJECTED_A2A_POLICY_UNSUPPORTED");
  }

  uint64_t rank_send_total = 0U;
  uint64_t rank_receive_total = 0U;
  for (size_t rank = 0U; rank < ranks; ++rank) {
    if (!CheckedAdd(summary->per_rank[rank].send_bytes, &rank_send_total) ||
        !CheckedAdd(summary->per_rank[rank].receive_bytes, &rank_receive_total)) {
      return Fail(summary, "PROJECTED_A2A_OVERFLOW");
    }
    const size_t domain = rank_domain[rank];
    if (!CheckedAdd(
            summary->per_rank[rank].send_bytes,
            &summary->per_domain[domain].send_bytes) ||
        !CheckedAdd(
            summary->per_rank[rank].receive_bytes,
            &summary->per_domain[domain].receive_bytes)) {
      return Fail(summary, "PROJECTED_A2A_OVERFLOW");
    }
    if (summary->per_rank[rank].receive_bytes >
        summary->maximum_rank_receive_bytes) {
      summary->maximum_rank_receive_bytes =
          summary->per_rank[rank].receive_bytes;
      summary->hottest_receive_rank = static_cast<int>(rank);
    }
  }
  summary->mean_rank_receive_bytes =
      static_cast<double>(summary->global_bytes) /
      static_cast<double>(ranks);
  summary->maximum_to_mean_receive_ratio =
      summary->mean_rank_receive_bytes == 0.0
      ? 0.0
      : static_cast<double>(summary->maximum_rank_receive_bytes) /
            summary->mean_rank_receive_bytes;
  uint64_t matrix_total = 0U;
  uint64_t intra_load = 0U;
  uint64_t inter_load = 0U;
  for (size_t source = 0U; source < domains; ++source) {
    for (size_t destination = 0U; destination < domains; ++destination) {
      const uint64_t bytes =
          summary->domain_matrix_bytes[source * domains + destination];
      if (!CheckedAdd(bytes, &matrix_total) ||
          !CheckedAdd(
              bytes, source == destination ? &intra_load : &inter_load)) {
        return Fail(summary, "PROJECTED_A2A_OVERFLOW");
      }
    }
  }
  uint64_t resource_total = 0U;
  for (const ProjectedA2AResource& resource : config.resources) {
    ProjectedA2AResourceSummary load;
    load.id = resource.id;
    load.scope = resource.scope;
    load.offered_load_bytes = resource.scope == "INTRA_DOMAIN"
        ? intra_load
        : inter_load;
    if (!CheckedAdd(load.offered_load_bytes, &resource_total)) {
      return Fail(summary, "PROJECTED_A2A_OVERFLOW");
    }
    summary->resource_loads.push_back(load);
  }
  summary->global_equals_rank_send = summary->global_bytes == rank_send_total;
  summary->global_equals_rank_receive =
      summary->global_bytes == rank_receive_total;
  summary->global_equals_domain_matrix = summary->global_bytes == matrix_total;
  summary->global_equals_resource_loads =
      summary->global_bytes == resource_total;
  summary->domain_rows_equal_send = true;
  summary->domain_columns_equal_receive = true;
  for (size_t domain = 0U; domain < domains; ++domain) {
    uint64_t row_total = 0U;
    uint64_t column_total = 0U;
    for (size_t peer = 0U; peer < domains; ++peer) {
      if (!CheckedAdd(
              summary->domain_matrix_bytes[domain * domains + peer],
              &row_total) ||
          !CheckedAdd(
              summary->domain_matrix_bytes[peer * domains + domain],
              &column_total)) {
        return Fail(summary, "PROJECTED_A2A_OVERFLOW");
      }
    }
    summary->domain_rows_equal_send =
        summary->domain_rows_equal_send &&
        row_total == summary->per_domain[domain].send_bytes;
    summary->domain_columns_equal_receive =
        summary->domain_columns_equal_receive &&
        column_total == summary->per_domain[domain].receive_bytes;
  }
  summary->ready = summary->global_equals_rank_send &&
      summary->global_equals_rank_receive &&
      summary->global_equals_domain_matrix &&
      summary->global_equals_resource_loads &&
      summary->domain_rows_equal_send &&
      summary->domain_columns_equal_receive;
  if (!summary->ready) {
    return Fail(summary, "PROJECTED_A2A_CONSERVATION_FAILED");
  }
  summary->projection_time_ns = ElapsedNanoseconds(started);
  return true;
}

}  // namespace SimAIContract
