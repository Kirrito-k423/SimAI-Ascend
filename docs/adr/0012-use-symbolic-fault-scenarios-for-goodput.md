# Use Symbolic Fault Scenarios for Goodput

## Status

Accepted on 2026-08-17 after user HITL review of the five decisions in “定义故障 goodput 敏感性场景”.

## Context

The target has no measured A5 failure rate, recovery distribution or checkpoint path. The three 100k resource interpretations also have materially different spare capacity: 98,304 active ranks leave 1,696 ranks outside the active set, exact 100,000 has no spare in the nominal pool, and a 100,352-capacity product interpretation has 352 inactive ranks. None of those inactive ranks is automatically provisioned, compatible or ready as a hot spare.

A single invented MTBF would create a false highest-goodput configuration. Healthy Useful Throughput also cannot represent rollback, replay, checkpoint contention, communicator repair or state redistribution. The search therefore needs a symbolic and fail-closed fault contract that can produce break-even regions now and numerical rankings only after evidence or user inputs exist.

## Decision

### Fault Goodput and ranking scope

**Fault Goodput** is the expected number of committed, non-dropped and non-replayed useful training tokens divided by total wall-clock time. Wall-clock time includes healthy execution, checkpoint work, detection, quiescence, remapping, communicator rebuild, state reload or redistribution, warmup and replay before the last committed point.

A Goodput Ranking Slice is exactly one Analytical Ranking Slice plus one complete Fault Scenario. Fault scenarios are never averaged together and healthy Useful Throughput is never substituted for an incomplete goodput result. `NO_FAILURE_UPPER_BOUND` remains a named upper bound rather than a failure prediction.

### Fault event contract

Fault classes use the topology/Profile identities `RANK`, `BOARD`, `HOST`, `CABINET`, `SUPERPOD`, `LINK` and `CLUSTER`. Each consumed class supplies:

- an MTBF, event rate or content-addressed event trace;
- affected members, impact scope and correlation semantics;
- detection, quiescence, remap, communicator rebuild, reload, redistribution and warmup durations;
- the recovery policy and any required checkpoint or spare resource;
- evidence identity, source date/version and readiness.

Rates are non-negative, durations are non-negative, affected ranks are within the active world, and checkpoint intervals are positive integer optimizer steps or `OFF`. Available spares satisfy `0 ≤ available spares ≤ physical capacity − active ranks`. Independence is never a default: it must be explicitly asserted with provenance. A missing field required by a policy yields `UNKNOWN`, not zero or an adjacent-domain value.

### Five policy scenarios

The report preserves five distinct policies:

1. `NO_FAILURE_UPPER_BOUND`: healthy training with no failure overhead.
2. `ROLLBACK_RESTART`: the affected training world stops and reloads a valid checkpoint.
3. `HOT_SPARE_REMAP`: failed ranks are replaced while the requested active-rank count is retained.
4. `RAGGED_CONTINUE`: training continues with fewer or non-uniform ranks after group and state reconstruction.
5. `CORRELATED_DOMAIN_LOSS`: a host, cabinet, SuperPod, link domain or larger correlated set fails together.

Without numeric A5 evidence or user inputs, these policies produce symbolic response surfaces and pairwise break-even boundaries over event rate, recovery duration, checkpoint cost and spare availability. No policy receives a fabricated nominal point. When complete inputs later exist, low/nominal/high fault bundles may be formed with explicit provenance; their names remain non-probabilistic sensitivity scenarios.

### Checkpoint, spare and ragged semantics

A recoverable checkpoint covers the complete training state: parameters, optimizer, RNG, router, data position and scheduler state. Inference-checkpoint storage is not a training checkpoint cost. Blocking checkpoint time enters wall time directly; asynchronous checkpointing records storage, H2D/D2H and compute/communication interference and is never treated as free.

Checkpoint interval is searched in optimizer steps using `OFF`, powers of two and boundary values implied by the accepted loss/recovery constraints. Lost work is counted from the last committed training state in useful tokens.

Spare inventory is stateful. Capacity, eligibility, activation time, repair/replenishment time, compatibility and depletion are separate fields. The 1,696 and 352 inactive-capacity counts are upper bounds, not hot-spare claims. After depletion, the selected policy must explicitly transition to restart, ragged continuation or an unavailable state.

`RAGGED_CONTINUE` is enabled only after the training framework proves non-uniform group membership, tensor/expert/optimizer shard semantics and deterministic state redistribution. Otherwise it is `UNSUPPORTED`, not a preferred recovery shortcut.

### Sampling and robust ranking

All candidates in a complete stochastic slice consume common event traces or common random seeds. Begin with five traces. If Goodput CV is at most 10%, rank by the median. If CV exceeds 10%, extend to ten total traces and rank by adverse-tail P10 Goodput using `linear_type7`; higher remains better. Always publish median, P10, minimum, maximum and CV.

A **Fault-Robust Candidate** is feasible and remains in the Top-5 of every complete Fault Scenario within the same topology, resource and A5 slice. If any required scenario input is incomplete, the highest-fault-goodput representative remains `UNKNOWN` and the report publishes only the available surface or break-even result.

## Consequences

- Configuration search can compare recovery policies without disguising guessed A5 reliability as measured truth.
- Checkpoint interval, hot-spare provisioning and ragged continuation become candidate decisions with explicit costs and support proofs.
- The healthy-throughput winner and adverse-tail goodput winner may differ, while their reason is attributable to failure exposure, restart work and spare behavior.
- The Cartesian product of A5 and fault assumptions remains slice-local; no averaged score hides a scenario reversal.

## Revisit conditions

Add immutable failure observations when A5 or a sufficiently homologous system becomes available. Revisit event classes if the target BOM exposes a different fault-containment boundary. Revisit sampling only with evidence that the five/ten common-trace rule cannot distinguish candidates; never convert an unsupported ragged policy or capacity count into an implicit recovery capability.
