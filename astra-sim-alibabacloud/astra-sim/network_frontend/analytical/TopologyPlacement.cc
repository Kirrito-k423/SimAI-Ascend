/******************************************************************************
 * Copyright (c) 2026, SimAI-Ascend contributors.
 * Licensed under the Apache License, Version 2.0.
 ******************************************************************************/

#include "TopologyPlacement.h"

#include <algorithm>
#include <limits>
#include <sstream>

#include "ProjectedA2ATraffic.h"

namespace SimAIContract {
namespace {

bool Fail(TopologyPlacementSummary* summary, const std::string& reason) {
  summary->ready = false;
  summary->failure_reason = reason;
  return false;
}

bool CheckedAdd(uint64_t value, uint64_t* total) {
  if (*total > std::numeric_limits<uint64_t>::max() - value) {
    return false;
  }
  *total += value;
  return true;
}

bool CheckedPositiveProduct3(int first, int second, int third, int64_t* product) {
  if (first <= 0 || second <= 0 || third <= 0) {
    return false;
  }
  const int64_t first_two = static_cast<int64_t>(first) * second;
  if (first_two > std::numeric_limits<int64_t>::max() / third) {
    return false;
  }
  *product = first_two * third;
  return true;
}

bool IsPowerOfTwo(int value) {
  return value > 0 &&
      (static_cast<unsigned int>(value) &
       (static_cast<unsigned int>(value) - 1U)) == 0U;
}

uint64_t GreatestCommonDivisor(uint64_t left, uint64_t right) {
  while (right != 0U) {
    const uint64_t remainder = left % right;
    left = right;
    right = remainder;
  }
  return left;
}

uint64_t FlatRandomMultiplier(
    uint64_t active_ranks,
    uint64_t domain_size_ranks,
    uint64_t seed) {
  const uint64_t base = 769U;
  uint64_t multiplier = seed >
          (std::numeric_limits<uint64_t>::max() - base) / 2U
      ? base
      : base + seed * 2U;
  multiplier %= active_ranks;
  if (multiplier == 0U) {
    multiplier = 1U;
  }
  if (multiplier % 2U == 0U) {
    ++multiplier;
  }
  const uint64_t minimum_stride =
      (domain_size_ranks + 126U) / 127U;
  while (multiplier <= 1U || multiplier >= active_ranks - 1U ||
         GreatestCommonDivisor(multiplier, active_ranks) != 1U ||
         std::min(multiplier, active_ranks - multiplier) < minimum_stride) {
    multiplier += 2U;
    if (multiplier >= active_ranks) {
      multiplier %= active_ranks;
      if (multiplier == 0U) {
        multiplier = 1U;
      }
    }
  }
  return multiplier;
}

int PhysicalRank(
    int logical_rank,
    int active_ranks,
    int domain_size_ranks,
    TopologyPlacementKind placement,
    uint64_t seed) {
  if (placement == TopologyPlacementKind::TopologyAware) {
    return logical_rank;
  }
  const uint64_t ranks = static_cast<uint64_t>(active_ranks);
  const uint64_t multiplier = FlatRandomMultiplier(
      ranks, static_cast<uint64_t>(domain_size_ranks), seed);
  const uint64_t offset = seed % ranks;
  const uint64_t physical =
      (static_cast<uint64_t>(logical_rank) * multiplier + offset) % ranks;
  return static_cast<int>(physical);
}

bool ResolveResourceScenario(
    const TopologyPlacementConfig& config,
    TopologyPlacementSummary* summary) {
  if (config.resource_scenario == "REGULAR_98304") {
    summary->active_ranks = 98304;
    summary->capacity_ranks = 100000;
    summary->spare_ranks = 1696;
  } else if (config.resource_scenario == "EXACT_100000_RAGGED") {
    summary->active_ranks = 100000;
    summary->capacity_ranks = 100000;
    summary->spare_ranks = 0;
  } else if (config.resource_scenario == "PRODUCT_CAPACITY_100352") {
    if (config.topology_identity != "CURRENT_PRODUCT_SUPERPOD_1024") {
      return Fail(summary, "PRODUCT_CAPACITY_TOPOLOGY_NOT_APPLICABLE");
    }
    summary->active_ranks = 100000;
    summary->capacity_ranks = 100352;
    summary->spare_ranks = 352;
  } else {
    return Fail(summary, "RESOURCE_SCENARIO_INVALID");
  }
  return true;
}

bool ValidateConfig(
    const TopologyPlacementConfig& config,
    TopologyPlacementSummary* summary) {
  if (config.topology_identity == "CURRENT_PRODUCT_SUPERPOD_1024") {
    if (config.topology_scope != "CURRENT_PRODUCT" ||
        config.domain_size_ranks != 1024) {
      return Fail(summary, "TOPOLOGY_IDENTITY_SEMANTICS_INVALID");
    }
  } else if (config.topology_identity ==
             "ARCHITECTURE_LIMIT_SUPERNODE_8192") {
    if (config.topology_scope != "ARCHITECTURE_LIMIT" ||
        config.domain_size_ranks != 8192) {
      return Fail(summary, "TOPOLOGY_IDENTITY_SEMANTICS_INVALID");
    }
  } else {
    return Fail(summary, "TOPOLOGY_IDENTITY_INVALID");
  }
  if (!ResolveResourceScenario(config, summary)) {
    return false;
  }
  if (config.spare_semantics != "UNPROVISIONED_SPARE_OR_SERVICE") {
    return Fail(summary, "RESOURCE_SPARE_SEMANTICS_INVALID");
  }
  if (config.routed_experts != 2048 || config.attention_tp <= 0 ||
      config.attention_cp <= 0 || config.attention_pp <= 0 ||
      config.moe_etp <= 0 || config.moe_pp <= 0 ||
      config.attention_pp != config.moe_pp) {
    return Fail(summary, "PARALLEL_GRID_INVALID");
  }
  if (!IsPowerOfTwo(config.attention_tp) || config.hidden_size <= 0 ||
      config.hidden_size % config.attention_tp != 0) {
    return Fail(summary, "ATTENTION_TP_SHARD_INVALID");
  }
  if (!IsPowerOfTwo(config.moe_etp) ||
      config.expert_intermediate_size <= 0 ||
      config.expert_intermediate_size % config.moe_etp != 0) {
    return Fail(summary, "MOE_ETP_SHARD_INVALID");
  }
  int64_t attention_denominator = 0;
  if (!CheckedPositiveProduct3(
          config.attention_tp,
          config.attention_cp,
          config.attention_pp,
          &attention_denominator) ||
      attention_denominator > summary->active_ranks) {
    return Fail(summary, "ATTENTION_GRID_INVALID");
  }
  const int attention_remainder = static_cast<int>(
      summary->active_ranks % attention_denominator);
  if (config.resource_scenario == "REGULAR_98304" &&
      attention_remainder != 0) {
    return Fail(summary, "REGULAR_ATTENTION_GRID_NOT_DIVISIBLE");
  }
  if (attention_remainder != 0 && !config.ragged_groups_supported) {
    return Fail(summary, "EXACT_RAGGED_FRAMEWORK_CAPABILITY_REQUIRED");
  }
  if (config.placement_kinds.size() != 2U ||
      config.placement_kinds[0] != TopologyPlacementKind::FlatRandom ||
      config.placement_kinds[1] != TopologyPlacementKind::TopologyAware) {
    return Fail(summary, "PLACEMENT_PAIR_COVERAGE_INCOMPLETE");
  }
  for (const int ep : config.ep_values) {
    if (ep <= 0 || config.routed_experts % ep != 0) {
      return Fail(summary, "EP_NOT_DIVISOR_OF_ROUTED_EXPERTS");
    }
    int64_t regular_group = 0;
    if (!CheckedPositiveProduct3(
            config.moe_etp, ep, config.moe_pp, &regular_group) ||
        regular_group > summary->active_ranks) {
      return Fail(summary, "MOE_GRID_INVALID");
    }
    const int remainder = static_cast<int>(
        summary->active_ranks % regular_group);
    if (config.resource_scenario == "REGULAR_98304" && remainder != 0) {
      return Fail(summary, "REGULAR_MOE_GRID_NOT_DIVISIBLE");
    }
    if (remainder != 0 && !config.ragged_groups_supported) {
      return Fail(summary, "EXACT_RAGGED_FRAMEWORK_CAPABILITY_REQUIRED");
    }
    const int edp_count = static_cast<int>(
        summary->active_ranks / regular_group);
    for (int edp = 0; edp <= edp_count; ++edp) {
      for (int pp = 0; pp < config.moe_pp; ++pp) {
        for (int etp = 0; etp < config.moe_etp; ++etp) {
          int group_size = 0;
          for (int ep_coordinate = 0; ep_coordinate < ep;
               ++ep_coordinate) {
            const int64_t logical =
                (((static_cast<int64_t>(edp) * config.moe_pp + pp) * ep +
                  ep_coordinate) * config.moe_etp) + etp;
            if (logical < summary->active_ranks) {
              ++group_size;
            }
          }
          if (group_size != 0 &&
              (config.message_bytes_per_rank == 0U ||
               config.message_bytes_per_rank %
                       static_cast<uint64_t>(group_size) !=
                   0U)) {
            return Fail(summary, "PLACEMENT_TRAFFIC_MESSAGE_NOT_DIVISIBLE");
          }
        }
      }
    }
  }
  const std::vector<int> required_ep_values = {128, 256, 512, 1024, 2048};
  if (config.ep_values != required_ep_values) {
    return Fail(summary, "REGULAR_EP_COVERAGE_INCOMPLETE");
  }
  return true;
}

std::string RankMapCanonical(
    const TopologyPlacementConfig& config,
    const TopologyPlacementSummary& summary,
    TopologyPlacementKind placement) {
  std::ostringstream canonical;
  canonical << "simai.rank-map/v1\n"
            << config.topology_identity << "\n"
            << config.resource_scenario << "\n"
            << TopologyPlacementKindName(placement) << "\n"
            << summary.active_ranks << "\n";
  for (int logical = 0; logical < summary.active_ranks; ++logical) {
    canonical << logical << ":"
              << PhysicalRank(
                     logical,
                     summary.active_ranks,
                     config.domain_size_ranks,
                     placement,
                     config.flat_random_seed)
              << "\n";
  }
  return canonical.str();
}

std::string GroupCanonical(
    const TopologyPlacementConfig& config,
    const TopologyPlacementSummary& summary,
    const std::string& rank_map_digest,
    TopologyPlacementKind placement,
    int ep,
    const std::string& axis,
    const std::string& formula,
    int axis_size,
    int64_t full_grid_product,
    int remainder_ranks) {
  std::ostringstream canonical;
  canonical << "simai.folded-communication-group/v1|" << axis << "|"
            << config.target_workload_sha256 << "|"
            << config.topology_identity << "|"
            << config.resource_scenario << "|"
            << TopologyPlacementKindName(placement) << "|"
            << rank_map_digest << "|N=" << summary.active_ranks
            << "|TP=" << config.attention_tp
            << "|CP=" << config.attention_cp
            << "|DP="
            << summary.active_ranks /
                   (static_cast<int64_t>(config.attention_tp) *
                    config.attention_cp * config.attention_pp)
            << "|APP=" << config.attention_pp
            << "|ETP=" << config.moe_etp << "|EP=" << ep
            << "|EDP="
            << summary.active_ranks /
                   (static_cast<int64_t>(config.moe_etp) * ep *
                    config.moe_pp)
            << "|MPP=" << config.moe_pp
            << "|axisSize=" << axis_size
            << "|fullGridProduct=" << full_grid_product
            << "|remainder=" << remainder_ranks
            << "|" << formula;
  return canonical.str();
}

bool BuildCandidate(
    const TopologyPlacementConfig& config,
    TopologyPlacementDigestFunction digest,
    TopologyPlacementSummary* summary,
    TopologyPlacementKind placement,
    const std::string& rank_map_digest,
    int ep,
    TopologyPlacementCandidateSummary* candidate) {
  candidate->placement = placement;
  candidate->flat_random_seed = config.flat_random_seed;
  candidate->rank_map_algorithm = placement == TopologyPlacementKind::FlatRandom
      ? "AFFINE_DOMAIN_MIXING_PRP_V2"
      : "LOGICAL_CONTIGUOUS_DOMAIN_AFFINITY_V1";
  candidate->rank_map_digest = rank_map_digest;
  candidate->rank_map_multiplier =
      placement == TopologyPlacementKind::FlatRandom
      ? FlatRandomMultiplier(
            static_cast<uint64_t>(summary->active_ranks),
            static_cast<uint64_t>(config.domain_size_ranks),
            config.flat_random_seed)
      : 1U;
  candidate->rank_map_offset =
      placement == TopologyPlacementKind::FlatRandom
      ? config.flat_random_seed % static_cast<uint64_t>(summary->active_ranks)
      : 0U;
  candidate->attention_tp = config.attention_tp;
  candidate->attention_cp = config.attention_cp;
  candidate->attention_pp = config.attention_pp;
  int64_t attention_denominator = 0;
  if (!CheckedPositiveProduct3(
          config.attention_tp,
          config.attention_cp,
          config.attention_pp,
          &attention_denominator)) {
    return false;
  }
  candidate->attention_dp = static_cast<int>(
      summary->active_ranks / attention_denominator);
  candidate->attention_full_grid_product =
      attention_denominator * candidate->attention_dp;
  candidate->attention_remainder_ranks = static_cast<int>(
      summary->active_ranks - candidate->attention_full_grid_product);
  candidate->moe_etp = config.moe_etp;
  candidate->moe_ep = ep;
  candidate->moe_pp = config.moe_pp;
  int64_t moe_denominator = 0;
  if (!CheckedPositiveProduct3(
          config.moe_etp, ep, config.moe_pp, &moe_denominator)) {
    return false;
  }
  candidate->moe_edp = static_cast<int>(
      summary->active_ranks / moe_denominator);
  candidate->moe_full_grid_product =
      moe_denominator * candidate->moe_edp;
  candidate->moe_remainder_ranks = static_cast<int>(
      summary->active_ranks - candidate->moe_full_grid_product);
  candidate->ragged = candidate->attention_remainder_ranks != 0 ||
      candidate->moe_remainder_ranks != 0;
  candidate->id = std::string(TopologyPlacementKindName(placement)) +
      "_EP" + std::to_string(ep);
  std::ostringstream attention_formula_stream;
  attention_formula_stream
      << "full:logical=(((dp*PP+pp)*CP+cp)*TP+tp),0<=tp<TP,0<=cp<CP,"
      << "0<=dp<DP,0<=pp<PP; hold all non-axis coordinates; "
      << "ragged-tail:logical in [" << candidate->attention_full_grid_product
      << ",N) as explicit partial group; physical=rank_map(logical)";
  const std::string attention_formula = attention_formula_stream.str();
  std::ostringstream moe_formula_stream;
  moe_formula_stream
      << "full:logical=(((edp*PP+pp)*EP+ep)*ETP+etp),0<=etp<ETP,"
      << "0<=ep<EP,0<=edp<EDP,0<=pp<PP; hold all non-axis coordinates; "
      << "ragged-tail:logical in [" << candidate->moe_full_grid_product
      << ",N) decoded by the same mixed radix with edp=EDP and logical<N; "
      << "physical=rank_map(logical)";
  const std::string moe_formula = moe_formula_stream.str();
  const std::string attention_axes[] = {
      "ATTENTION_TP", "ATTENTION_CP", "ATTENTION_DP", "ATTENTION_PP"};
  const int attention_axis_sizes[] = {
      candidate->attention_tp,
      candidate->attention_cp,
      candidate->attention_dp,
      candidate->attention_pp};
  for (size_t index = 0U; index < 4U; ++index) {
    TopologyPlacementCommunicationGroup group;
    group.axis = attention_axes[index];
    group.representation = "MIXED_RADIX_FORMULA_V1";
    group.membership_formula =
        "axis=" + group.axis + "; " + attention_formula;
    group.membership_digest = digest(GroupCanonical(
        config,
        *summary,
        rank_map_digest,
        placement,
        ep,
        group.axis,
        group.membership_formula,
        attention_axis_sizes[index],
        candidate->attention_full_grid_product,
        candidate->attention_remainder_ranks));
    group.axis_size = attention_axis_sizes[index];
    group.covered_ranks = summary->active_ranks;
    group.full_grid_product = candidate->attention_full_grid_product;
    group.ragged_tail_ranks = candidate->attention_remainder_ranks;
    candidate->communication_groups.push_back(group);
  }
  const std::string moe_axes[] = {
      "MOE_ETP", "MOE_EP", "MOE_EDP", "MOE_PP"};
  const int moe_axis_sizes[] = {
      candidate->moe_etp,
      candidate->moe_ep,
      candidate->moe_edp,
      candidate->moe_pp};
  for (size_t index = 0U; index < 4U; ++index) {
    TopologyPlacementCommunicationGroup group;
    group.axis = moe_axes[index];
    group.representation = "MIXED_RADIX_FORMULA_V1";
    group.membership_formula = "axis=" + group.axis + "; " + moe_formula;
    group.membership_digest = digest(GroupCanonical(
        config,
        *summary,
        rank_map_digest,
        placement,
        ep,
        group.axis,
        group.membership_formula,
        moe_axis_sizes[index],
        candidate->moe_full_grid_product,
        candidate->moe_remainder_ranks));
    group.axis_size = moe_axis_sizes[index];
    group.covered_ranks = summary->active_ranks;
    group.full_grid_product = candidate->moe_full_grid_product;
    group.ragged_tail_ranks = candidate->moe_remainder_ranks;
    candidate->communication_groups.push_back(group);
  }
  const std::string optimizer_axes[] = {"OPTIMIZER_DP", "OPTIMIZER_EDP"};
  const int optimizer_axis_sizes[] = {
      candidate->attention_dp, candidate->moe_edp};
  for (size_t index = 0U; index < 2U; ++index) {
    const bool attention_optimizer = index == 0U;
    TopologyPlacementCommunicationGroup group;
    group.axis = optimizer_axes[index];
    group.representation = "MIXED_RADIX_FORMULA_V1";
    group.membership_formula = "axis=" + group.axis + "; optimizer-state; " +
        (attention_optimizer ? attention_formula : moe_formula);
    const int64_t full_grid_product = attention_optimizer
        ? candidate->attention_full_grid_product
        : candidate->moe_full_grid_product;
    const int remainder_ranks = attention_optimizer
        ? candidate->attention_remainder_ranks
        : candidate->moe_remainder_ranks;
    group.membership_digest = digest(GroupCanonical(
        config,
        *summary,
        rank_map_digest,
        placement,
        ep,
        group.axis,
        group.membership_formula,
        optimizer_axis_sizes[index],
        full_grid_product,
        remainder_ranks));
    group.axis_size = optimizer_axis_sizes[index];
    group.covered_ranks = summary->active_ranks;
    group.full_grid_product = full_grid_product;
    group.ragged_tail_ranks = remainder_ranks;
    candidate->communication_groups.push_back(group);
  }

  const size_t domain_count = static_cast<size_t>(summary->domain_count);
  candidate->domain_matrix_bytes.assign(domain_count * domain_count, 0U);
  for (int edp = 0; edp <= candidate->moe_edp; ++edp) {
    for (int pp = 0; pp < candidate->moe_pp; ++pp) {
      for (int etp = 0; etp < candidate->moe_etp; ++etp) {
        std::vector<int> logical_group;
        logical_group.reserve(static_cast<size_t>(ep));
        for (int ep_coordinate = 0; ep_coordinate < ep; ++ep_coordinate) {
          const int64_t logical =
              (((static_cast<int64_t>(edp) * candidate->moe_pp + pp) * ep +
                ep_coordinate) * candidate->moe_etp) + etp;
          if (logical < summary->active_ranks) {
            logical_group.push_back(static_cast<int>(logical));
          }
        }
        const int group_size = static_cast<int>(logical_group.size());
        if (group_size == 0) {
          continue;
        }
        if (group_size == ep) {
          ++candidate->full_ep_groups;
        } else {
          ++candidate->partial_ep_groups;
          candidate->partial_ep_group_ranks += group_size;
        }
        std::vector<int> counts(domain_count, 0);
        for (int offset = 0; offset < group_size; ++offset) {
          const int physical = PhysicalRank(
              logical_group[static_cast<size_t>(offset)],
              summary->active_ranks,
              config.domain_size_ranks,
              placement,
              config.flat_random_seed);
          const size_t domain = static_cast<size_t>(
              physical / config.domain_size_ranks);
          if (domain >= domain_count) {
            return false;
          }
          ++counts[domain];
        }
        ProjectedA2AConfig projected;
        projected.present = true;
        projected.rank_count = group_size;
        projected.policy = ProjectedA2APolicy::Uniform;
        projected.scenario = candidate->id;
        projected.message_bytes_per_rank = config.message_bytes_per_rank;
        projected.artifact_id = config.artifact_id;
        projected.artifact_sha256 = config.artifact_sha256;
        projected.evidence_level = config.topology_evidence.evidence_class;
        projected.field_readiness = config.topology_evidence.readiness;
        int local_first = 0;
        std::vector<size_t> local_to_global;
        for (size_t domain = 0U; domain < domain_count; ++domain) {
          if (counts[domain] == 0) {
            continue;
          }
          ProjectedA2ADomain projected_domain;
          projected_domain.id = "domain-" + std::to_string(domain);
          projected_domain.first_rank = local_first;
          projected_domain.rank_count = counts[domain];
          projected.domains.push_back(projected_domain);
          local_to_global.push_back(domain);
          local_first += counts[domain];
        }
        ProjectedA2AResource intra;
        intra.id = "intra-topology-domain";
        intra.scope = "INTRA_DOMAIN";
        projected.resources.push_back(intra);
        ProjectedA2AResource inter;
        inter.id = "inter-topology-domain-shared-fabric";
        inter.scope = "INTER_DOMAIN";
        projected.resources.push_back(inter);
        ProjectedA2ASummary projected_summary;
        if (!ProjectA2ATraffic(
                projected,
                group_size,
                config.message_bytes_per_rank,
                false,
                &projected_summary) ||
            !projected_summary.ready ||
            !projected_summary.consumed_by_analytical) {
          return false;
        }
        summary->maximum_projector_rank_state = std::max(
            summary->maximum_projector_rank_state,
            projected_summary.per_rank.size());
        summary->maximum_projector_domain_cells = std::max(
            summary->maximum_projector_domain_cells,
            projected_summary.domain_matrix_bytes.size());
        if (!CheckedAdd(
                projected_summary.global_bytes, &candidate->global_bytes)) {
          return false;
        }
        const size_t local_domains = local_to_global.size();
        for (size_t source = 0U; source < local_domains; ++source) {
          for (size_t destination = 0U; destination < local_domains;
               ++destination) {
            const uint64_t bytes = projected_summary.domain_matrix_bytes[
                source * local_domains + destination];
            const size_t global_source = local_to_global[source];
            const size_t global_destination = local_to_global[destination];
            if (!CheckedAdd(
                    bytes,
                    &candidate->domain_matrix_bytes[
                        global_source * domain_count + global_destination]) ||
                !CheckedAdd(
                    bytes,
                    global_source == global_destination
                        ? &candidate->intra_domain_bytes
                        : &candidate->cross_domain_bytes)) {
              return false;
            }
          }
        }
      }
    }
  }
  if (candidate->global_bytes == 0U ||
      candidate->global_bytes != candidate->intra_domain_bytes +
          candidate->cross_domain_bytes) {
    return false;
  }
  candidate->local_expert_hit =
      static_cast<double>(candidate->intra_domain_bytes) /
      static_cast<double>(candidate->global_bytes);
  std::ostringstream candidate_canonical;
  candidate_canonical << "simai.topology-placement-candidate/v1|"
                      << config.artifact_sha256 << "|"
                      << config.target_workload_sha256 << "|"
                      << candidate->id << "|" << rank_map_digest;
  for (const TopologyPlacementCommunicationGroup& group :
       candidate->communication_groups) {
    candidate_canonical << "|" << group.membership_digest;
  }
  candidate->candidate_digest = digest(candidate_canonical.str());
  return true;
}

const TopologyPlacementCandidateSummary* FindCandidate(
    const TopologyPlacementSummary& summary,
    TopologyPlacementKind placement,
    int ep) {
  for (const TopologyPlacementCandidateSummary& candidate :
       summary.candidates) {
    if (candidate.placement == placement && candidate.moe_ep == ep) {
      return &candidate;
    }
  }
  return nullptr;
}

bool SignedDifference(uint64_t left, uint64_t right, int64_t* difference) {
  const uint64_t maximum =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  if (left > maximum || right > maximum) {
    return false;
  }
  *difference = static_cast<int64_t>(left) - static_cast<int64_t>(right);
  return true;
}

}  // namespace

const char* TopologyPlacementKindName(TopologyPlacementKind kind) {
  switch (kind) {
    case TopologyPlacementKind::FlatRandom:
      return "FLAT_RANDOM";
    case TopologyPlacementKind::TopologyAware:
      return "TOPOLOGY_AWARE";
    case TopologyPlacementKind::Unsupported:
      return "UNSUPPORTED";
  }
  return "UNSUPPORTED";
}

bool AnalyzeTopologyPlacements(
    const TopologyPlacementConfig& config,
    TopologyPlacementDigestFunction digest,
    TopologyPlacementSummary* summary) {
  *summary = TopologyPlacementSummary();
  summary->present = config.present;
  summary->topology_identity = config.topology_identity;
  summary->topology_scope = config.topology_scope;
  summary->domain_size_ranks = config.domain_size_ranks;
  summary->topology_evidence = config.topology_evidence;
  summary->resource_scenario = config.resource_scenario;
  summary->spare_semantics = config.spare_semantics;
  summary->ragged_groups_supported = config.ragged_groups_supported;
  summary->ragged_evidence = config.ragged_evidence;
  summary->artifact_id = config.artifact_id;
  summary->artifact_sha256 = config.artifact_sha256;
  summary->artifact_bytes_read = config.artifact_bytes_read;
  summary->target_workload_sha256 = config.target_workload_sha256;
  if (!config.present || digest == nullptr) {
    return Fail(summary, "TOPOLOGY_PLACEMENT_CONFIG_INVALID");
  }
  if (!ValidateConfig(config, summary)) {
    return false;
  }
  summary->full_domain_count =
      summary->active_ranks / config.domain_size_ranks;
  summary->partial_domain_active_ranks =
      summary->active_ranks % config.domain_size_ranks;
  summary->domain_count = summary->full_domain_count +
      (summary->partial_domain_active_ranks == 0 ? 0 : 1);
  std::ostringstream topology_canonical;
  topology_canonical << "simai.topology-identity/v1|"
                     << config.topology_identity << "|"
                     << config.topology_scope << "|"
                     << config.domain_size_ranks << "|"
                     << config.topology_evidence.ref << "|"
                     << config.topology_evidence.evidence_class << "|"
                     << config.topology_evidence.readiness << "|"
                     << config.topology_evidence.source_revision << "|"
                     << config.topology_evidence.source_sha256 << "|"
                     << config.topology_evidence.claim_sha256 << "|"
                     << (config.topology_evidence.hardware_available
                             ? "hardware=true"
                             : "hardware=false");
  summary->topology_digest = digest(topology_canonical.str());

  for (const TopologyPlacementKind placement : config.placement_kinds) {
    const std::string rank_map_digest = digest(
        RankMapCanonical(config, *summary, placement));
    for (const int ep : config.ep_values) {
      TopologyPlacementCandidateSummary candidate;
      if (!BuildCandidate(
              config,
              digest,
              summary,
              placement,
              rank_map_digest,
              ep,
              &candidate)) {
        return Fail(summary, "TOPOLOGY_PLACEMENT_PROJECTION_FAILED");
      }
      summary->candidates.push_back(candidate);
    }
  }

  for (const int ep : config.ep_values) {
    const TopologyPlacementCandidateSummary* flat = FindCandidate(
        *summary, TopologyPlacementKind::FlatRandom, ep);
    const TopologyPlacementCandidateSummary* aware = FindCandidate(
        *summary, TopologyPlacementKind::TopologyAware, ep);
    if (flat == nullptr || aware == nullptr) {
      return Fail(summary, "PLACEMENT_PAIR_COVERAGE_INCOMPLETE");
    }
    TopologyPlacementPairSummary pair;
    pair.ep = ep;
    pair.flat_candidate = flat->id;
    pair.topology_aware_candidate = aware->id;
    pair.flat_cross_domain_bytes = flat->cross_domain_bytes;
    pair.topology_aware_cross_domain_bytes = aware->cross_domain_bytes;
    if (!SignedDifference(
            flat->cross_domain_bytes,
            aware->cross_domain_bytes,
            &pair.topology_aware_cross_domain_reduction_bytes)) {
      return Fail(summary, "TOPOLOGY_PLACEMENT_DELTA_OUT_OF_RANGE");
    }
    pair.flat_local_expert_hit = flat->local_expert_hit;
    pair.topology_aware_local_expert_hit = aware->local_expert_hit;
    summary->placement_pairs.push_back(pair);
  }
  for (const TopologyPlacementKind placement : config.placement_kinds) {
    const TopologyPlacementCandidateSummary* global = FindCandidate(
        *summary, placement, 2048);
    if (global == nullptr) {
      return Fail(summary, "GLOBAL_EP_CANDIDATE_MISSING");
    }
    for (const int local_ep : std::vector<int>{1024, 512}) {
      const TopologyPlacementCandidateSummary* local = FindCandidate(
          *summary, placement, local_ep);
      if (local == nullptr) {
        return Fail(summary, "LOCAL_EP_CANDIDATE_MISSING");
      }
      TopologyPlacementExpertPairSummary pair;
      pair.placement = placement;
      pair.local_ep = local_ep;
      pair.global_candidate = global->id;
      pair.local_candidate = local->id;
      if (!SignedDifference(
              global->cross_domain_bytes,
              local->cross_domain_bytes,
              &pair.local_cross_domain_reduction_bytes)) {
        return Fail(summary, "TOPOLOGY_PLACEMENT_DELTA_OUT_OF_RANGE");
      }
      pair.global_local_expert_hit = global->local_expert_hit;
      pair.local_local_expert_hit = local->local_expert_hit;
      summary->expert_parallel_pairs.push_back(pair);
    }
  }
  summary->ready = true;
  return true;
}

}  // namespace SimAIContract
