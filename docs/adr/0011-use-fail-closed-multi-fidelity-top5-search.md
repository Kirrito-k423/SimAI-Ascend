# Use Fail-closed Multi-fidelity Search for Slice-local Top-5 Results

## Status

Accepted on 2026-08-13 after user HITL review of the five decisions in “定义多保真配置搜索与 Top-5 输出契约”.

## Context

The 100k search combines discrete parallel grids, topology-aware placement, micro-batch and gradient-accumulation boundaries, recompute choices, three active-rank identities and three uncalibrated A5 sensitivity bundles. Exhaustive packet simulation is out of scope, while ranking a configuration that lacks a valid grid, memory proof, routing evidence or complete time model would create a false champion.

The search must preserve separate evidence levels. Upstream SimAI-Analytical can rank candidates only when the selected Ascend Profile, collective cost model and workload artifacts are complete. A future small-scale SimAI-Simulation run can audit mechanisms and ordering tendencies, but cannot become a measurement of the 100k target. Fault goodput inputs and the exact Simulation smoke topology remain decisions of separate tickets.

## Decision

### Four fail-closed fidelity stages

Every candidate carries one highest completed `Search Fidelity Stage`; stages are evidence labels, not interchangeable scores.

1. `F0_STATIC_VALIDATED` checks schema and content identities, topology identity, parallel-group divisibility and coverage, rank placement, tensor and optimizer placement, `GTS ≤ 500,000,000`, peak HBM, routing conservation, and required Profile/cost readiness. Each failure has a stable rejection code. A rejected candidate never enters a ranking.
2. `F1_ANALYTICAL_SCREENED` evaluates every F0-valid candidate with SimAI-Analytical under one complete report slice. Results are deterministically cached by the full content identity of workload, topology, placement, Profile, cost model and search settings.
3. `F2_ANALYTICAL_REFINED` performs the more detailed overlap, memory-timeline, routing-skew and sensitivity evaluation for the promoted set. Per report slice, promotion takes the Useful Throughput Top-20 plus the communication-exposure, peak-HBM and recompute Pareto boundary and deterministic knee/diversity representatives. The refined set is capped at 100 candidates per slice. When truncation occurs, the report publishes enumeration and promotion coverage and does not claim a global optimum.
4. `F3_SIMULATION_AUDITED` is assigned only after the separate Simulation smoke contract audits selected representatives through an independent `CollectiveFlowProvider`. A scaled Simulation audit never silently replaces a 100k Analytical score.

F0, F1 and F2 can be implemented before F3. Missing F3 evidence is visible but does not permit a simulated 100k claim.

### Independent report slices

A ranking slice is one compatible combination of:

- current-product 1,024-rank SuperPod or architecture-limit 8,192-rank topology identity;
- `REGULAR_98304`, supported `EXACT_100000_RAGGED`, or the applicable `PRODUCT_CAPACITY_100352` resource interpretation;
- A5 low, nominal or high Sensitivity Envelope bundle.

Different topology, active-rank and sensitivity identities are never blended into one Top-5. An unsupported combination is reported as infeasible or not applicable instead of being filled from another slice.

### Deterministic ranking and uncertainty language

Within a complete slice, rank feasible candidates by descending Useful Throughput. Exact ties are broken in this order: lower exposed communication, greater HBM headroom, lower recompute FLOPs, then canonical configuration digest. Candidates without a complete time estimate remain in a separately labelled traffic-only result and cannot enter the time-ranked Top-5.

The A5 bundles remain non-probabilistic. A candidate is a Robust A5 Candidate only when it is feasible and in the Top-5 in low, nominal and high bundles of the same compatible topology/resource slice. Reports show rank reversals and dominant sensitivity inputs. Point ordering without a valid uncertainty model is not described as a confidence-qualified winner.

### Top-5 and named representatives

Each valid slice publishes its five highest-ranked feasible configurations and their gaps. The aggregate Typical Configuration Set additionally identifies:

- highest Useful Throughput;
- lowest exposed communication;
- highest fault-tolerant goodput;
- best Robust A5 Candidate.

The goodput representative consumes the future fault-sensitivity contract. Until that contract supplies complete inputs, its value and representative remain `UNKNOWN`; Useful Throughput is not substituted for goodput.

Every published candidate includes TP/CP/DP/PP and ETP/EP/EDP, MBS, gradient accumulation, GTS, sequence length, routing/recompute/precision/optimizer identity, placement digest, HBM breakdown and headroom, step-time breakdown, Useful Throughput, exposed communication, report-slice identity, fidelity stage and complete provenance.

### Simulation authority and provenance

The Simulation smoke ticket chooses the exact rank count, topology, messages and mismatch gate. If a selected audit exceeds that accepted gate, the affected configuration family is labelled `SIMULATION_DISAGREEMENT`; its champion claim is withheld until the Analytical or flow model is revised and revalidated. The result is not silently back-fitted.

Every acceptance, rejection, score and rank is reproducible from content-addressed source revisions, workload/step/routing/memory resources, topology, Profile, cost model, placement, search settings, rejection codes, extrapolation chain and ranking method. Public provenance contains no private machine identity, credentials or raw private logs.

## Consequences

- Static validity, Analytical ranking and Simulation audit can progress independently without allowing a lower-fidelity result to masquerade as a higher-fidelity one.
- A deterministic Top-20 plus Pareto/diversity promotion policy bounds refined work while retaining non-throughput trade-offs. The 100-candidate cap makes any loss of exhaustive coverage explicit.
- Top-5 results are comparable only within the same report slice. Cross-scenario robustness is an intersection property, not an averaged score.
- The highest-throughput and highest-goodput representatives may differ. The latter remains unknown until fault assumptions are accepted.
- Small-scale Simulation can invalidate a configuration-family conclusion but cannot directly measure or re-rank the 100k target.

## Revisit conditions

Revisit promotion limits when measured search-size or recall evidence shows that Top-20 plus Pareto/diversity representatives systematically misses important candidates. Revisit tie-breakers only through a versioned ranking contract. Add fault-goodput and Simulation gates through their dedicated decisions; do not infer them from this ADR.
