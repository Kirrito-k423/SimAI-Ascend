# Use a 16-rank Two-domain Simulation Smoke

## Status

Accepted on 2026-08-17 after user HITL review of the five decisions in “决定 Simulation smoke test 的最小拓扑与验收”. The decision state model is preserved on `prototype/simulation-smoke-contract@b9c3297`; the prototype does not enter `main`.

## Context

The Ascend Vertical Slice is Analytical-first. Upstream SimAI-Simulation currently obtains NVIDIA-oriented flows through `MockNcclGroup`, while the accepted provider boundary requires an independent HCCL `CollectiveFlowProvider`. Analytical aggregate traffic cannot be passed to NS-3 as endpoint flows.

The first Simulation objective is to prove that the Ascend-specific path is structurally connected and then separately test whether its timing agrees well enough to audit an Analytical candidate family. It is not a 100k packet simulation or an A5 measurement. A one-domain test would miss the cross-domain paths that distinguish topology-aware from striped expert placement.

## Decision

### Synthetic topology and execution boundary

The minimum smoke uses 16 ranks arranged as two logical domains of eight ranks. The two-level topology exposes an intra-domain path, an inter-domain shared bottleneck and two rank placements. Its canonical identity is `SYNTHETIC_ASCEND_SMOKE_TOPOLOGY`; it must not be called an A5, Ascend 950DT or measured SuperPod topology.

The authoritative run uses Linux CPU execution and a single Simulation thread. It does not require or occupy an NPU. Two identical runs are used for deterministic event/provenance comparison. The complete suite has a 60-minute bound; exceeding it produces `SMOKE_RESOURCE_BLOCKED` and does not silently remove a case.

### Seventeen collective micro cases

Run AllReduce, AllGather, ReduceScatter, AllToAll and uniform AllToAllV at per-rank buffer sizes 4 KiB, 1 MiB and 64 MiB, for 15 cases. Add 1 MiB locality and hotspot AllToAllV, for 17 total micro cases.

Every operation uses its exact HCCL buffer/count semantics. The flow output preserves endpoint, group, dependency, rank-send/receive, domain-pair and topology-resource evidence. A2AV counts and displacements remain content-addressed inputs.

### Two reduced end-to-end cases

Use `GT-TARGET-SEMANTIC-v1`: four active layers, 32 routed experts, TopK 16, expert width 3,072 and the established 16-rank A3 shape. The attention grid is `TP1/CP1/DP8/PP2`; the MoE grid is `ETP1/EP8/EDP1/PP2`.

Run the same workload, routing artifact and Profile with two rank maps:

- topology-aware placement that keeps EP traffic within a logical domain where possible;
- striped placement that crosses the two logical domains.

Both cases publish step time, collective and pipeline P2P bytes, domain-pair load, exposed communication, workload identity and placement digest.

### Structural support gate

The Simulation backend must use the HCCL-specific `CollectiveFlowProvider`. Falling back to `MockNcclGroup`, NCCL, NVLS, PXN or another NVIDIA behavior fails closed.

All 19 cases must complete and satisfy exact endpoint/group membership, dependency, global/per-rank/per-domain byte conservation and single collective completion. The two runs must have identical event and provenance digests. A complete structural pass is `FLOW_SMOKE_PASS`. Failure to establish the build or runtime is `BLOCKED_ENVIRONMENT`; exceeding the resource bound is `SMOKE_RESOURCE_BLOCKED`. Neither is a timing disagreement or a support pass.

### F3 timing gate

For every micro and end-to-end case, compare Simulation and the matching Analytical result using:

`APE = abs(T_simulation − T_analytical) / T_simulation`

Every case must have APE at most 30%. Within an operation and traffic pattern, completion time must be non-decreasing from 4 KiB to 1 MiB to 64 MiB. Analytical and Simulation must also agree on the ordering direction of topology-aware versus striped end-to-end placement.

Only a complete structural and timing pass is `F3_SIMULATION_AUDITED`. If any timing case exceeds 30%, violates monotonicity or reverses placement ordering, structural support may remain `FLOW_SMOKE_PASS`, but the affected family becomes `SIMULATION_DISAGREEMENT` and cannot support a champion claim. The model is not silently back-fitted.

## Consequences

- Ascend Simulation support can be brought up and reported before its timing audit passes, matching the project's support-first priority without overstating accuracy.
- Sixteen ranks are sufficient to exercise two domains and the existing A3 reduced shape while keeping real endpoint flows bounded.
- The suite covers startup, middle and large-message behavior plus uniform, locality and hotspot MoE traffic without expanding to 100k ranks.
- The synthetic topology validates provider mechanics and ordering only; it cannot calibrate or measure A5/950DT.

## Revisit conditions

Revisit the topology only if 16 ranks cannot express the production provider's smallest two-domain group. Revisit message points if an accepted HCCL measurement domain excludes one of them; replacement must remain a preregistered small/middle/large point. A larger Simulation campaign belongs after this smoke passes and requires a separate implementation plan, not a reinterpretation of this gate.
