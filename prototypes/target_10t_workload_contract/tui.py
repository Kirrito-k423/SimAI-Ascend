#!/usr/bin/env python3
"""PROTOTYPE ONLY: TUI shell for the pure Target 10T Workload contract."""

from __future__ import annotations

import argparse
import json
from dataclasses import replace
from pathlib import Path

from contract import Architecture, ContractState, StepConfig, resolve_contract, target_architecture


BOLD = "\x1b[1m"
DIM = "\x1b[2m"
RESET = "\x1b[0m"


def load_fixture():
    path = Path(__file__).with_name("official_v4_pro_fixture.json")
    return json.loads(path.read_text(encoding="utf-8"))


def compact_int(value):
    if value is None:
        return "UNKNOWN"
    if value >= 1_000_000_000_000:
        return f"{value / 1_000_000_000_000:.6f}T ({value:,})"
    if value >= 1_000_000_000:
        return f"{value / 1_000_000_000:.6f}B ({value:,})"
    return f"{value:,}"


def field(name, value):
    print(f"{BOLD}{name}{RESET}: {value}")


def complete_refs(state, enabled):
    value = "PROTOTYPE_REF_NOT_CALIBRATED" if enabled else None
    return replace(
        state,
        precision_policy_ref=value,
        optimizer_policy_ref=value,
        placement_ref=value,
        checkpoint_policy_ref=value,
        runtime_profile_ref=value,
    )


def initial_state(fixture):
    baseline = Architecture.from_dict(fixture["architecture"])
    return ContractState(
        architecture=target_architecture(baseline),
        step=StepConfig(1, 2048, 8, 1, nonpadding_tokens=16384),
    )


def scenario(name, fixture):
    baseline = Architecture.from_dict(fixture["architecture"])
    state = initial_state(fixture)
    if name == "official-baseline":
        state = replace(state, architecture=baseline)
    elif name == "target":
        pass
    elif name == "topk-only":
        state = replace(state, architecture=target_architecture(baseline, experts=384, topk=16))
    elif name == "gts-limit":
        state = replace(state, step=StepConfig(1, 1_000_000, 500, 1, nonpadding_tokens=500_000_000))
    elif name == "gts-overflow":
        state = replace(state, step=StepConfig(1, 1_000_000, 501, 1, nonpadding_tokens=501_000_000))
    elif name == "target-with-routing":
        state = replace(state, routing_artifact_ref="PROTOTYPE_ROUTING_NOT_MEASURED")
    elif name == "target-contract-complete":
        state = replace(state, routing_artifact_ref="PROTOTYPE_ROUTING_NOT_MEASURED")
        state = complete_refs(state, True)
    else:
        raise ValueError("unknown scenario: " + name)
    return state


def render(state, fixture, clear):
    if clear:
        print("\x1b[2J\x1b[H", end="")
    result = resolve_contract(state, fixture["checkpoint_expected"])
    summary = result["model"]["tensor_summary"]
    active = summary.get("active_params_per_token", {})
    print(f"{BOLD}PROTOTYPE — Target 10T Workload Contract{RESET}")
    print(f"{DIM}Question: can exact tensor, active, GTS, routing and memory semantics coexist without hidden defaults?{RESET}\n")
    field("status", result["status"])
    field("modelIdentity", result["model_identity_sha256"][:16])
    field("routedExperts / TopK", f"{state.architecture.n_routed_experts} / {state.architecture.topk}")
    field("main / hash / MTP blocks", f"{state.architecture.num_hidden_layers} / {state.architecture.num_hash_layers} / {state.architecture.num_mtp_layers}")
    field("configuredGTS", compact_int(result["step"]["configured_gts"]))
    field(
        "configuredAssignmentSlotsUpperBound",
        compact_int(result["step"]["configured_routed_assignment_slots_upper_bound"]),
    )
    print()
    field("tensorCount", compact_int(summary.get("tensor_count")))
    field("tensorManifest", summary.get("tensor_manifest_sha256", "UNKNOWN")[:16])
    field("logicalTrainableParams", compact_int(summary.get("logical_trainable_params")))
    field("checkpointStorageBytes", compact_int(summary.get("checkpoint_storage_bytes")))
    field("quantScaleElements", compact_int(summary.get("quant_scale_elements")))
    field("routingTableElements", compact_int(summary.get("routing_table_elements")))
    field("active.mainBlocksOnly", compact_int(active.get("main_blocks_only")))
    field("active.mainForwardIncludingIO", compact_int(active.get("main_forward_including_io")))
    field("active.trainingGraphIncludingMTP", compact_int(active.get("training_graph_including_mtp")))
    gate = result["model"]["baseline_reproduction_gate"]
    if gate is not None:
        field("officialBaselineGate", gate["status"])
    field("routingArtifact", result["routing"]["ref"] or "MISSING")
    field("memoryPeakBytesPerRank", "UNKNOWN — symbolic events only")
    if result["errors"]:
        field("errors", ", ".join(result["errors"]))
    if result["incomplete"]:
        field("incomplete", ", ".join(result["incomplete"]))
    print(f"\n{BOLD}Actions{RESET}")
    print("[e] experts 384/2048  [k] TopK 6/16  [g] GTS small/limit/overflow")
    print("[r] routing ref  [m] memory refs  [z] reset target  [q] quit")
    return result


def scripted(name, fixture, emit_json):
    names = [
        "official-baseline",
        "target",
        "topk-only",
        "gts-limit",
        "gts-overflow",
        "target-with-routing",
        "target-contract-complete",
    ] if name == "all" else [name]
    for item in names:
        state = scenario(item, fixture)
        result = resolve_contract(state, fixture["checkpoint_expected"])
        if emit_json:
            print(json.dumps({"scenario": item, "result": result}, sort_keys=True))
        else:
            print(f"\n===== scenario: {item} =====")
            render(state, fixture, False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    fixture = load_fixture()
    if args.scenario:
        scripted(args.scenario, fixture, args.json)
        return
    state = initial_state(fixture)
    gts_mode = 0
    memory_enabled = False
    while True:
        render(state, fixture, True)
        action = input().strip()
        if action == "q":
            return
        if action == "e":
            experts = 384 if state.architecture.n_routed_experts == 2048 else 2048
            state = replace(state, architecture=replace(state.architecture, n_routed_experts=experts))
        elif action == "k":
            topk = 6 if state.architecture.topk == 16 else 16
            state = replace(state, architecture=replace(state.architecture, topk=topk))
        elif action == "g":
            gts_mode = (gts_mode + 1) % 3
            steps = (
                StepConfig(1, 2048, 8, 1, nonpadding_tokens=16384),
                StepConfig(1, 1_000_000, 500, 1, nonpadding_tokens=500_000_000),
                StepConfig(1, 1_000_000, 501, 1, nonpadding_tokens=501_000_000),
            )
            state = replace(state, step=steps[gts_mode])
        elif action == "r":
            ref = None if state.routing_artifact_ref else "PROTOTYPE_ROUTING_NOT_MEASURED"
            state = replace(state, routing_artifact_ref=ref)
        elif action == "m":
            memory_enabled = not memory_enabled
            state = complete_refs(state, memory_enabled)
        elif action == "z":
            state = initial_state(fixture)
            gts_mode = 0
            memory_enabled = False


if __name__ == "__main__":
    main()
