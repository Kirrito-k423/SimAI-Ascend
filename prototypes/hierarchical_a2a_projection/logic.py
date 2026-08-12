"""PROTOTYPE ONLY: compare exact and compact A2A traffic representations.

The pure logic asks whether a streaming hierarchical projection is a sufficient
input to an Analytical contention functional.  It is deliberately not an HCCL
algorithm, a calibrated cost model, or a Simulation flow provider.
"""

from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction
from typing import Dict, Iterable, Iterator, List, Optional, Sequence, Tuple


Pair = Tuple[int, int, Fraction]


@dataclass(frozen=True)
class TrafficScenario:
    name: str
    description: str
    rank_to_domain: Tuple[int, ...]
    matrix: Tuple[Tuple[int, ...], ...]

    @property
    def ranks(self) -> int:
        return len(self.rank_to_domain)

    @property
    def domains(self) -> int:
        return max(self.rank_to_domain) + 1

    def pairs(self) -> Iterator[Pair]:
        for src, row in enumerate(self.matrix):
            for dst, value in enumerate(row):
                if src != dst and value:
                    yield src, dst, Fraction(value)


@dataclass(frozen=True)
class ExplicitFlow:
    src: int
    dst: int
    size: Fraction


@dataclass(frozen=True)
class ProjectionSummary:
    total_bytes: Fraction
    rank_egress: Tuple[Fraction, ...]
    rank_ingress: Tuple[Fraction, ...]
    domain_pair_bytes: Tuple[Tuple[Fraction, ...], ...]
    resource_load: Tuple[Tuple[str, Fraction], ...]
    synthetic_bottleneck_units: Optional[Fraction]


def _resource_path(src: int, dst: int, rank_to_domain: Sequence[int]) -> Tuple[str, ...]:
    src_domain = rank_to_domain[src]
    dst_domain = rank_to_domain[dst]
    resources = [f"rank/{src}/egress", f"domain/{src_domain}/fabric"]
    if src_domain != dst_domain:
        resources.append(f"inter/{src_domain}->{dst_domain}")
        resources.append(f"domain/{dst_domain}/fabric")
    resources.append(f"rank/{dst}/ingress")
    return tuple(resources)


def _capacity(resource: str) -> int:
    if resource.startswith("rank/"):
        return 512
    if resource.startswith("inter/"):
        return 256
    return 1024


def _empty_domain_matrix(domains: int) -> List[List[Fraction]]:
    return [[Fraction(0) for _ in range(domains)] for _ in range(domains)]


def summarize_explicit_flows(
    flows: Sequence[ExplicitFlow],
    rank_to_domain: Sequence[int],
    materialize_time: bool = True,
) -> ProjectionSummary:
    """Baseline: first materialize every pair flow, then reduce it."""
    ranks = len(rank_to_domain)
    domains = max(rank_to_domain) + 1
    egress = [Fraction(0) for _ in range(ranks)]
    ingress = [Fraction(0) for _ in range(ranks)]
    domain_pairs = _empty_domain_matrix(domains)
    resources: Dict[str, Fraction] = {}
    total = Fraction(0)
    for flow in flows:
        total += flow.size
        egress[flow.src] += flow.size
        ingress[flow.dst] += flow.size
        source_domain = rank_to_domain[flow.src]
        destination_domain = rank_to_domain[flow.dst]
        domain_pairs[source_domain][destination_domain] += flow.size
        for resource in _resource_path(flow.src, flow.dst, rank_to_domain):
            resources[resource] = resources.get(resource, Fraction(0)) + flow.size
    bottleneck = (
        max((load / _capacity(resource) for resource, load in resources.items()), default=Fraction(0))
        if materialize_time
        else None
    )
    return ProjectionSummary(
        total,
        tuple(egress),
        tuple(ingress),
        tuple(tuple(row) for row in domain_pairs),
        tuple(sorted(resources.items())),
        bottleneck,
    )


def stream_hierarchical_projection(
    pairs: Iterable[Pair],
    rank_to_domain: Sequence[int],
    materialize_time: bool = True,
) -> ProjectionSummary:
    """Candidate seam: reduce a pair/count stream without retaining flows."""
    ranks = len(rank_to_domain)
    domains = max(rank_to_domain) + 1
    egress = [Fraction(0) for _ in range(ranks)]
    ingress = [Fraction(0) for _ in range(ranks)]
    domain_pairs = _empty_domain_matrix(domains)
    resources: Dict[str, Fraction] = {}
    total = Fraction(0)
    for src, dst, size in pairs:
        total += size
        egress[src] += size
        ingress[dst] += size
        source_domain = rank_to_domain[src]
        destination_domain = rank_to_domain[dst]
        domain_pairs[source_domain][destination_domain] += size
        for resource in _resource_path(src, dst, rank_to_domain):
            resources[resource] = resources.get(resource, Fraction(0)) + size
    bottleneck = (
        max((load / _capacity(resource) for resource, load in resources.items()), default=Fraction(0))
        if materialize_time
        else None
    )
    return ProjectionSummary(
        total,
        tuple(egress),
        tuple(ingress),
        tuple(tuple(row) for row in domain_pairs),
        tuple(sorted(resources.items())),
        bottleneck,
    )


def uniform_formula_projection(
    rank_to_domain: Sequence[int],
    per_peer_bytes: int,
    materialize_time: bool = True,
) -> ProjectionSummary:
    """Closed-form uniform A2A projection without enumerating rank pairs."""
    ranks = len(rank_to_domain)
    domains = max(rank_to_domain) + 1
    domain_sizes = tuple(rank_to_domain.count(domain) for domain in range(domains))
    unit = Fraction(per_peer_bytes)
    rank_total = (ranks - 1) * unit
    egress = tuple(rank_total for _ in range(ranks))
    ingress = tuple(rank_total for _ in range(ranks))
    domain_pairs = _empty_domain_matrix(domains)
    resources: Dict[str, Fraction] = {}
    for domain, size in enumerate(domain_sizes):
        domain_pairs[domain][domain] = size * (size - 1) * unit
        fabric_load = (
            size * (size - 1) + 2 * size * (ranks - size)
        ) * unit
        resources[f"domain/{domain}/fabric"] = fabric_load
    for source_domain, source_size in enumerate(domain_sizes):
        for destination_domain, destination_size in enumerate(domain_sizes):
            if source_domain == destination_domain:
                continue
            value = source_size * destination_size * unit
            domain_pairs[source_domain][destination_domain] = value
            resources[f"inter/{source_domain}->{destination_domain}"] = value
    for rank in range(ranks):
        resources[f"rank/{rank}/egress"] = rank_total
        resources[f"rank/{rank}/ingress"] = rank_total
    total = ranks * (ranks - 1) * unit
    bottleneck = (
        max((load / _capacity(resource) for resource, load in resources.items()), default=Fraction(0))
        if materialize_time
        else None
    )
    return ProjectionSummary(
        total,
        egress,
        ingress,
        tuple(tuple(row) for row in domain_pairs),
        tuple(sorted(resources.items())),
        bottleneck,
    )


def exact_pair_flow_baseline(scenario: TrafficScenario) -> Tuple[List[ExplicitFlow], ProjectionSummary]:
    flows = [ExplicitFlow(src, dst, size) for src, dst, size in scenario.pairs()]
    return flows, summarize_explicit_flows(flows, scenario.rank_to_domain)


def _eligible_pairs(
    scenario: TrafficScenario, source_domain: int, destination_domain: int
) -> Iterator[Tuple[int, int]]:
    sources = [rank for rank, domain in enumerate(scenario.rank_to_domain) if domain == source_domain]
    destinations = [rank for rank, domain in enumerate(scenario.rank_to_domain) if domain == destination_domain]
    for src in sources:
        for dst in destinations:
            if src != dst:
                yield src, dst


def symmetry_fold_projection(scenario: TrafficScenario) -> Tuple[int, ProjectionSummary]:
    """Block-average orbit classes; exact only for domain-block symmetry."""
    domain_totals = _empty_domain_matrix(scenario.domains)
    for src, dst, size in scenario.pairs():
        domain_totals[scenario.rank_to_domain[src]][scenario.rank_to_domain[dst]] += size
    reconstructed: List[Pair] = []
    classes = 0
    for source_domain in range(scenario.domains):
        for destination_domain in range(scenario.domains):
            eligible = list(_eligible_pairs(scenario, source_domain, destination_domain))
            total = domain_totals[source_domain][destination_domain]
            if not eligible or not total:
                continue
            classes += 1
            average = total / len(eligible)
            reconstructed.extend((src, dst, average) for src, dst in eligible)
    return classes, stream_hierarchical_projection(reconstructed, scenario.rank_to_domain)


def representative_flow_projection(scenario: TrafficScenario) -> Tuple[int, ProjectionSummary]:
    """One globally weighted representative; useful as a counterexample."""
    pairs = [(src, dst) for src in range(scenario.ranks) for dst in range(scenario.ranks) if src != dst]
    total = sum((size for _, _, size in scenario.pairs()), Fraction(0))
    average = total / len(pairs)
    return 1, stream_hierarchical_projection(
        ((src, dst, average) for src, dst in pairs), scenario.rank_to_domain
    )


def compare(reference: ProjectionSummary, candidate: ProjectionSummary) -> Dict[str, bool]:
    return {
        "total_bytes": candidate.total_bytes == reference.total_bytes,
        "rank_egress": candidate.rank_egress == reference.rank_egress,
        "rank_ingress": candidate.rank_ingress == reference.rank_ingress,
        "domain_pair_bytes": candidate.domain_pair_bytes == reference.domain_pair_bytes,
        "resource_load": candidate.resource_load == reference.resource_load,
        "synthetic_bottleneck_units": (
            candidate.synthetic_bottleneck_units == reference.synthetic_bottleneck_units
        ),
    }


def _matrix(ranks: int, value_fn) -> Tuple[Tuple[int, ...], ...]:
    return tuple(
        tuple(0 if src == dst else int(value_fn(src, dst)) for dst in range(ranks))
        for src in range(ranks)
    )


def scenarios() -> Dict[str, TrafficScenario]:
    two_domains = (0, 0, 0, 0, 1, 1, 1, 1)
    uniform = TrafficScenario(
        "uniform",
        "Upstream-like equal pair chunks over two equal domains",
        two_domains,
        _matrix(8, lambda _src, _dst: 128),
    )
    hotspot_matrix = [list(row) for row in _matrix(8, lambda _src, _dst: 32)]
    hotspot_matrix[7][0] += 2048
    hotspot = TrafficScenario(
        "hotspot",
        "One remote destination receives a large routed-expert hotspot",
        two_domains,
        tuple(tuple(row) for row in hotspot_matrix),
    )
    locality = TrafficScenario(
        "locality",
        "Most traffic stays inside each four-rank domain",
        two_domains,
        _matrix(8, lambda src, dst: 256 if two_domains[src] == two_domains[dst] else 16),
    )
    ragged_domains = (0, 0, 0, 0, 1, 1, 1, 2, 2)
    ragged_matrix = [
        list(row)
        for row in _matrix(
            9,
            lambda src, dst: 192 if ragged_domains[src] == ragged_domains[dst] else 24,
        )
    ]
    ragged_matrix[8][0] += 777
    ragged_matrix[4][8] += 333
    ragged = TrafficScenario(
        "ragged",
        "Domains of 4/3/2 ranks plus two asymmetric routes",
        ragged_domains,
        tuple(tuple(row) for row in ragged_matrix),
    )
    return {scenario.name: scenario for scenario in (uniform, hotspot, locality, ragged)}


def _fraction_json(value: Optional[Fraction]):
    if value is None:
        return None
    if value.denominator == 1:
        return value.numerator
    return {"numerator": value.numerator, "denominator": value.denominator}


def summary_json(summary: ProjectionSummary) -> Dict[str, object]:
    intra = sum(
        summary.domain_pair_bytes[index][index]
        for index in range(len(summary.domain_pair_bytes))
    )
    return {
        "total_bytes": _fraction_json(summary.total_bytes),
        "rank_egress": [_fraction_json(value) for value in summary.rank_egress],
        "rank_ingress": [_fraction_json(value) for value in summary.rank_ingress],
        "intra_domain_bytes": _fraction_json(intra),
        "inter_domain_bytes": _fraction_json(summary.total_bytes - intra),
        "domain_pair_bytes": [
            [_fraction_json(value) for value in row] for row in summary.domain_pair_bytes
        ],
        "resource_count": len(summary.resource_load),
        "max_resource_load": _fraction_json(
            max((load for _, load in summary.resource_load), default=Fraction(0))
        ),
        "synthetic_bottleneck_units": _fraction_json(summary.synthetic_bottleneck_units),
    }


def representation_scale(ranks: int, domain_sizes: Sequence[int]) -> Dict[str, int]:
    if sum(domain_sizes) != ranks:
        raise ValueError("domain sizes must sum to ranks")
    domains = len(domain_sizes)
    directed_flows = ranks * (ranks - 1)
    resource_cells = 2 * ranks + domains + domains * (domains - 1)
    return {
        "ranks": ranks,
        "domains": domains,
        "exact_directed_flow_objects": directed_flows,
        "upstream_rank_view_entries": 2 * directed_flows,
        "upstream_prev_integer_references": directed_flows * (ranks - 1),
        "hierarchical_rank_marginal_cells": 2 * ranks,
        "hierarchical_domain_pair_cells": domains * domains,
        "hierarchical_resource_cells": resource_cells,
        "hierarchical_total_summary_cells": 2 * ranks + domains * domains + resource_cells,
    }


def resolve(
    scenario_name: str,
    routing_ref: Optional[str],
    topology_ref: Optional[str],
    cost_model_ref: Optional[str],
) -> Dict[str, object]:
    scenario = scenarios()[scenario_name]
    if routing_ref is None:
        return {
            "status": "UNKNOWN_ROUTING_COUNTS",
            "scenario": scenario_name,
            "traffic_projection": None,
            "analytical_time": None,
            "simulation_flow_capability": "UNSUPPORTED_AGGREGATE_IS_NOT_A_FLOW_PROVIDER",
        }

    flows, exact = exact_pair_flow_baseline(scenario)
    hierarchical = stream_hierarchical_projection(scenario.pairs(), scenario.rank_to_domain)
    symmetry_classes, symmetry = symmetry_fold_projection(scenario)
    representative_classes, representative = representative_flow_projection(scenario)
    comparisons = {
        "hierarchical": compare(exact, hierarchical),
        "symmetry_fold": compare(exact, symmetry),
        "representative_flow": compare(exact, representative),
    }
    if topology_ref is None:
        status = "TRAFFIC_ONLY_TOPOLOGY_UNKNOWN"
        analytical_time = None
    elif cost_model_ref is None:
        status = "RESOURCE_LOAD_PROJECTED_COST_MODEL_UNKNOWN"
        analytical_time = None
    else:
        status = "PROTOTYPE_EQUIVALENCE_NOT_CALIBRATED"
        analytical_time = _fraction_json(hierarchical.synthetic_bottleneck_units)
    return {
        "status": status,
        "scenario": scenario_name,
        "description": scenario.description,
        "refs": {
            "routing": routing_ref,
            "topology": topology_ref,
            "cost_model": cost_model_ref,
        },
        "exact_pair_flow_count": len(flows),
        "representation_records": {
            "exact_pair_flows": len(flows),
            "symmetry_classes": symmetry_classes,
            "representative_classes": representative_classes,
            "hierarchical_summary_cells": representation_scale(
                scenario.ranks,
                tuple(scenario.rank_to_domain.count(domain) for domain in range(scenario.domains)),
            )["hierarchical_total_summary_cells"],
        },
        "comparison": {
            name: {"all_invariants": all(fields.values()), "fields": fields}
            for name, fields in comparisons.items()
        },
        "traffic_projection": summary_json(hierarchical),
        "analytical_time": analytical_time,
        "analytical_time_unit": "SYNTHETIC_CAPACITY_UNIT_NOT_NS",
        "simulation_flow_capability": "UNSUPPORTED_AGGREGATE_IS_NOT_A_FLOW_PROVIDER",
        "input_contract": {
            "uniform_a2a": "formula descriptor may be evaluated without pair materialization",
            "a2av": "immutable counts artifact or routing-derived stream plus hash",
            "arbitrary_dense_100k": "REJECT_UNLESS_STREAMABLE; no lossless subquadratic encoding is promised",
        },
    }


def scale_scenarios() -> Dict[str, Dict[str, int]]:
    target_2048 = representation_scale(2048, (1024, 1024))
    target_100k_domains = (1024,) * 97 + (672,)
    target_100k = representation_scale(100_000, target_100k_domains)
    return {"ep2048_two_1024_domains": target_2048, "target100k_ragged_98_domains": target_100k}


def uniform_formula_scale_evidence() -> Dict[str, Dict[str, object]]:
    cases = {
        "ep2048_two_1024_domains": (2048, (1024, 1024)),
        "target100k_ragged_98_domains": (100_000, (1024,) * 97 + (672,)),
    }
    result: Dict[str, Dict[str, object]] = {}
    for name, (ranks, domain_sizes) in cases.items():
        rank_to_domain = tuple(
            domain
            for domain, size in enumerate(domain_sizes)
            for _ in range(size)
        )
        summary = uniform_formula_projection(
            rank_to_domain, per_peer_bytes=1, materialize_time=False
        )
        result[name] = {
            "method": "CLOSED_FORM_NO_PAIR_ENUMERATION",
            "per_peer_bytes": 1,
            "total_bytes": summary.total_bytes.numerator,
            "rank_marginals": len(summary.rank_egress) + len(summary.rank_ingress),
            "domain_pair_cells": len(summary.domain_pair_bytes) ** 2,
            "resource_cells": len(summary.resource_load),
            "analytical_time": None,
        }
    return result
