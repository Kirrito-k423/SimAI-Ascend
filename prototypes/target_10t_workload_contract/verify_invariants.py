#!/usr/bin/env python3
"""PROTOTYPE ONLY: deterministic contract invariants, without network access."""

from __future__ import annotations

import json
from pathlib import Path

from contract import Architecture, resolve_contract
from tui import scenario


TARGET_TOTAL = 8_414_884_746_526
TARGET_ACTIVE = {
    "main_blocks_only": 88_950_053_982,
    "main_forward_including_io": 90_803_533_923,
    "training_graph_including_mtp": 92_345_423_134,
}


def main() -> None:
    fixture = json.loads(
        Path(__file__).with_name("official_v4_pro_fixture.json").read_text(
            encoding="utf-8"
        )
    )
    baseline_architecture = Architecture.from_dict(fixture["architecture"])
    results = {
        name: resolve_contract(scenario(name, fixture), fixture["checkpoint_expected"])
        for name in (
            "official-baseline",
            "target",
            "topk-only",
            "gts-limit",
            "gts-overflow",
            "target-with-routing",
            "target-contract-complete",
        )
    }

    baseline = results["official-baseline"]
    target = results["target"]
    topk_only = results["topk-only"]
    limit = results["gts-limit"]
    overflow = results["gts-overflow"]
    routing = results["target-with-routing"]
    complete = results["target-contract-complete"]
    expected = fixture["checkpoint_expected"]

    assert baseline["model"]["baseline_reproduction_gate"]["status"] == "PASS"
    baseline_summary = baseline["model"]["tensor_summary"]
    assert baseline_summary["tensor_count"] == expected["tensor_count"]
    assert baseline_summary["logical_trainable_params"] == expected["logical_trainable_params"]
    assert baseline_summary["checkpoint_storage_bytes"] == expected["storage_bytes"]

    target_summary = target["model"]["tensor_summary"]
    assert target_summary["logical_trainable_params"] == TARGET_TOTAL
    assert target_summary["active_params_per_token"] == TARGET_ACTIVE
    assert sum(target_summary["trainable_logical_params_by_kind"].values()) == TARGET_TOTAL
    assert (
        sum(
            target_summary[
                "logical_trainable_params_by_checkpoint_value_dtype"
            ].values()
        )
        == TARGET_TOTAL
    )

    # This arithmetic is intentionally independent of tensor enumeration.
    added_experts = 2048 - 384
    moe_blocks = baseline_architecture.num_hidden_layers + baseline_architecture.num_mtp_layers
    per_expert = 3 * baseline_architecture.hidden_size * baseline_architecture.moe_intermediate_size
    non_hash_routers_with_bias = (
        baseline_architecture.num_hidden_layers
        - baseline_architecture.num_hash_layers
        + baseline_architecture.num_mtp_layers
    )
    independent_target = (
        expected["logical_trainable_params"]
        + added_experts * moe_blocks * per_expert
        + added_experts * moe_blocks * baseline_architecture.hidden_size
        + added_experts * non_hash_routers_with_bias
    )
    assert independent_target == TARGET_TOTAL

    assert topk_only["model"]["tensor_summary"]["logical_trainable_params"] == expected[
        "logical_trainable_params"
    ]
    assert topk_only["model"]["tensor_summary"]["active_params_per_token"] != baseline_summary[
        "active_params_per_token"
    ]
    assert limit["step"]["configured_gts"] == 500_000_000
    assert "GTS_EXCEEDS_500M" not in limit["errors"]
    assert limit["step"]["configured_routed_assignment_slots_upper_bound"] == 496_000_000_000
    assert overflow["status"] == "INVALID"
    assert "GTS_EXCEEDS_500M" in overflow["errors"]
    assert overflow["model_identity_sha256"] == target["model_identity_sha256"]
    assert overflow["model"]["tensor_summary"]["logical_trainable_params"] == TARGET_TOTAL
    assert routing["incomplete"] == ["MEMORY_MATERIALIZATION_INPUTS_INCOMPLETE"]
    assert complete["status"] == "CONTRACT_COMPLETE_WITH_PROTOTYPE_REFS_NOT_ANALYTICAL_DATA"
    assert complete["memory"]["peak_bytes_per_rank"] is None

    print(
        json.dumps(
            {
                "status": "PASS",
                "scenarios": len(results),
                "official_baseline_gate": "PASS",
                "independent_target_arithmetic": independent_target,
                "target_logical_trainable_params": TARGET_TOTAL,
                "target_active_params_per_token": TARGET_ACTIVE,
                "gts_limit": 500_000_000,
                "prototype_memory_peak_is_unknown": True,
            },
            indent=2,
            sort_keys=True,
        )
    )


if __name__ == "__main__":
    main()
