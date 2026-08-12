#!/usr/bin/env python3
"""PROTOTYPE ONLY: deterministic transcript of conservation claims."""

from __future__ import annotations

import json

from logic import (
    compare,
    exact_pair_flow_baseline,
    resolve,
    scale_scenarios,
    scenarios,
    uniform_formula_projection,
    uniform_formula_scale_evidence,
)


REF = "PROTOTYPE_SYNTHETIC_NOT_MEASURED"


def main():
    results = {name: resolve(name, REF, REF, REF) for name in scenarios()}
    for name, result in results.items():
        assert result["comparison"]["hierarchical"]["all_invariants"]
        assert result["simulation_flow_capability"] == "UNSUPPORTED_AGGREGATE_IS_NOT_A_FLOW_PROVIDER"
        assert result["analytical_time_unit"] == "SYNTHETIC_CAPACITY_UNIT_NOT_NS"
    assert results["uniform"]["comparison"]["symmetry_fold"]["all_invariants"]
    assert results["locality"]["comparison"]["symmetry_fold"]["all_invariants"]
    assert not results["hotspot"]["comparison"]["symmetry_fold"]["all_invariants"]
    assert not results["ragged"]["comparison"]["symmetry_fold"]["all_invariants"]
    assert results["uniform"]["comparison"]["representative_flow"]["all_invariants"]
    assert not results["hotspot"]["comparison"]["representative_flow"]["all_invariants"]
    assert not results["locality"]["comparison"]["representative_flow"]["all_invariants"]
    assert not results["ragged"]["comparison"]["representative_flow"]["all_invariants"]
    uniform = scenarios()["uniform"]
    _, uniform_exact = exact_pair_flow_baseline(uniform)
    uniform_formula = uniform_formula_projection(uniform.rank_to_domain, 128)
    assert all(compare(uniform_exact, uniform_formula).values())

    missing = {
        "routing": resolve("hotspot", None, REF, REF)["status"],
        "topology": resolve("hotspot", REF, None, REF)["status"],
        "cost": resolve("hotspot", REF, REF, None)["status"],
    }
    assert missing == {
        "routing": "UNKNOWN_ROUTING_COUNTS",
        "topology": "TRAFFIC_ONLY_TOPOLOGY_UNKNOWN",
        "cost": "RESOURCE_LOAD_PROJECTED_COST_MODEL_UNKNOWN",
    }
    scale = scale_scenarios()
    assert scale["ep2048_two_1024_domains"]["exact_directed_flow_objects"] == 4_192_256
    assert scale["ep2048_two_1024_domains"]["upstream_prev_integer_references"] == 8_581_548_032
    assert scale["ep2048_two_1024_domains"]["hierarchical_total_summary_cells"] == 8_200
    assert scale["target100k_ragged_98_domains"]["exact_directed_flow_objects"] == 9_999_900_000
    assert scale["target100k_ragged_98_domains"]["hierarchical_total_summary_cells"] == 419_208
    formula_scale = uniform_formula_scale_evidence()
    assert formula_scale["ep2048_two_1024_domains"]["method"] == "CLOSED_FORM_NO_PAIR_ENUMERATION"
    assert formula_scale["ep2048_two_1024_domains"]["total_bytes"] == 4_192_256
    assert formula_scale["target100k_ragged_98_domains"]["total_bytes"] == 9_999_900_000
    assert formula_scale["target100k_ragged_98_domains"]["analytical_time"] is None
    print(
        json.dumps(
            {
                "status": "PASS",
                "scenarios": len(results),
                "hierarchical_all_invariants": "4/4",
                "symmetry_exact_only_when_block_symmetric": True,
                "representative_flow_is_not_general": True,
                "uniform_formula_matches_exact": True,
                "fail_closed_statuses": missing,
                "scale": scale,
                "uniform_formula_scale_evidence": formula_scale,
                "performance_data": False,
            },
            indent=2,
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
