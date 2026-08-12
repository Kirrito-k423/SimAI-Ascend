#!/usr/bin/env python3
"""PROTOTYPE ONLY: interactive shell for the hierarchical A2A projection."""

from __future__ import annotations

import argparse
import json

from logic import resolve, scale_scenarios, scenarios, uniform_formula_scale_evidence


BOLD = "\x1b[1m"
DIM = "\x1b[2m"
RESET = "\x1b[0m"
PROTOTYPE_REF = "PROTOTYPE_SYNTHETIC_NOT_MEASURED"


def field(name, value):
    print(f"{BOLD}{name}{RESET}: {value}")


def render(name, routing, topology, cost, clear=True):
    if clear:
        print("\x1b[2J\x1b[H", end="")
    result = resolve(name, routing, topology, cost)
    print(f"{BOLD}PROTOTYPE — Hierarchical A2A Projection{RESET}")
    print(f"{DIM}Question: which compact Analytical representation preserves exact traffic and bottleneck state?{RESET}\n")
    field("scenario", name)
    field("status", result["status"])
    field("routing / topology / cost refs", f"{bool(routing)} / {bool(topology)} / {bool(cost)}")
    if result["traffic_projection"] is not None:
        field("exactPairFlows", result["exact_pair_flow_count"])
        for representation, comparison in result["comparison"].items():
            field(f"conservation.{representation}", "PASS" if comparison["all_invariants"] else "FAIL")
        projection = result["traffic_projection"]
        field("total / intra / inter bytes", f"{projection['total_bytes']} / {projection['intra_domain_bytes']} / {projection['inter_domain_bytes']}")
        field("syntheticBottleneck", result["analytical_time"] or "UNKNOWN")
    field("simulation", result["simulation_flow_capability"])
    print(f"\n{BOLD}Scale formulas{RESET}")
    for scale_name, scale in scale_scenarios().items():
        field(
            scale_name,
            f"exact={scale['exact_directed_flow_objects']:,}, projectionCells={scale['hierarchical_total_summary_cells']:,}",
        )
    field(
        "uniformFormula",
        "closed-form at 2048 and ragged 100k; no pair enumeration",
    )
    print(f"\n{BOLD}Actions{RESET}")
    print("[s] scenario  [r] routing ref  [t] topology ref  [c] cost ref  [q] quit")
    return result


def scripted(name, emit_json):
    names = list(scenarios()) if name == "all" else [name]
    for scenario_name in names:
        result = resolve(scenario_name, PROTOTYPE_REF, PROTOTYPE_REF, PROTOTYPE_REF)
        if emit_json:
            print(json.dumps(result, sort_keys=True))
        else:
            print(f"\n===== scenario: {scenario_name} =====")
            render(scenario_name, PROTOTYPE_REF, PROTOTYPE_REF, PROTOTYPE_REF, False)
    if name == "all":
        for missing_name, refs in (
            ("missing-routing", (None, PROTOTYPE_REF, PROTOTYPE_REF)),
            ("missing-topology", (PROTOTYPE_REF, None, PROTOTYPE_REF)),
            ("missing-cost", (PROTOTYPE_REF, PROTOTYPE_REF, None)),
        ):
            result = resolve("hotspot", *refs)
            if emit_json:
                print(json.dumps({"scenario": missing_name, "result": result}, sort_keys=True))
            else:
                print(f"\n===== scenario: {missing_name} =====")
                field("status", result["status"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", choices=["all", *scenarios()])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    if args.scenario:
        scripted(args.scenario, args.json)
        return
    names = list(scenarios())
    index = 0
    routing = topology = cost = PROTOTYPE_REF
    while True:
        render(names[index], routing, topology, cost)
        action = input().strip()
        if action == "q":
            return
        if action == "s":
            index = (index + 1) % len(names)
        elif action == "r":
            routing = None if routing else PROTOTYPE_REF
        elif action == "t":
            topology = None if topology else PROTOTYPE_REF
        elif action == "c":
            cost = None if cost else PROTOTYPE_REF


if __name__ == "__main__":
    main()
