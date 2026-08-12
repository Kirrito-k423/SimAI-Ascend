# Separate 100k Topology Identities and Folded Placement

## Status

Accepted on 2026-08-12 after user HITL review of the five decisions in “定义 100k SuperPoD 拓扑与并行放置搜索空间”.

## Context

The 100,000-rank target does not divide evenly by 1,024, 8,192 or 2,048. Public evidence also describes different physical identities: a current 1,024-NPU Atlas 950 product configuration, an 8,192-NPU architecture limit, and a historical 8,192-NPU roadmap. Combining their cabinet counts, link ceilings or performance assumptions would create a topology that no source describes.

The Target 10T Workload has 2,048 routed experts and TopK 16. Its attention and MoE dimensions need not use the same process-group factorization, so the search must represent MoE Parallel Folding explicitly. Upstream SimAI's regular Cartesian assumptions cannot by themselves prove that a ragged 100,000-rank training grid is executable.

## Decision

### Independent topology identities

1. The primary current-product search uses independent 1,024-rank SuperPod domains. The 8,192-rank architecture-limit search is a separate sensitivity scenario. The historical 8,192-rank roadmap remains evidence only and never enters current-product ranking.
2. Every topology identity has its own topology digest, hierarchy, cabinet and fault domains, link and shared-resource definitions, and collective cost-model references. No performance parameter is copied between the 1,024 and 8,192 identities without an explicit `EXTRAPOLATED` transformation.

### Three resource-count scenarios

The search represents all three of the following without conflating capacity with active ranks:

| Scenario | Active ranks | Physical capacity / idle ranks | Current-product topology | Architecture-limit topology |
|---|---:|---:|---|---|
| `REGULAR_98304` | 98,304 | 1,696 spare/service from the nominal 100,000 pool | 96 full × 1,024 | 12 full × 8,192 |
| `EXACT_100000_RAGGED` | 100,000 | no inactive rank in the 100,000 target | 97 full × 1,024 + one 672-rank domain | 12 full × 8,192 + one 1,696-rank domain |
| `PRODUCT_CAPACITY_100352` | 100,000 | 98 × 1,024 capacity, 352 spare | 97 full active domains plus one partially active 672/1,024 domain | not the primary meaning of this product-capacity scenario |

`REGULAR_98304` is the regular-grid performance baseline. Reports must call it 98,304 active ranks, never “100,000 simultaneously training”. `EXACT_100000_RAGGED` remains a required feasibility and performance result where supported.

### Folded parallel-grid legality

Each candidate proves both group identities over the same active world:

`N = TP × CP × DP × PP`

`N = ETP × EP × EDP × PP`

EP must divide the 2,048 routed experts evenly. Every tensor shard, pipeline stage, attention group, expert group and optimizer-state group must provide an integer divisibility and membership proof.

For `REGULAR_98304`, the initial search uses powers of two for TP, CP and ETP; `EP ∈ {128, 256, 512, 1024, 2048}`; PP is limited to factors that yield non-empty, memory-feasible and balanceable stages; DP and EDP are derived from the two identities.

For a regular Cartesian `EXACT_100000_RAGGED` world, the power-of-two factor available to `ETP × EP × PP` is constrained by `100000 = 2^5 × 5^5`. With odd PP the largest power-of-two `ETP × EP` is 32, and an even PP consumes part of that factor. If this cannot hold the model in memory, the regular exact-100,000 lane is honestly infeasible. An experimental ragged lane is enabled only when the selected training framework explicitly supports non-uniform process groups, ragged tensor/expert shards and consistent optimizer semantics; topology expressibility alone is insufficient.

### Placement contrasts and SuperPod affinity

For every legal parallel grid, generate paired placements with the same workload and active-rank count:

- flat/random rank order and topology-aware rank order;
- global EP=2048 and SuperPod-local EP=1024 or 512 plus EDP;
- optional two-level dispatch or hotspot-expert replication only with explicit routing, state-consistency and memory semantics.

TP and ETP prefer the smallest high-bandwidth domain. EP and CP prefer one SuperPod. PP boundaries may align with domain boundaries only while maintaining stage balance and minimizing activation P2P. DP and EDP are the primary candidates for cross-SuperPod bucket communication; this is a searched hypothesis, not a fixed performance claim.

Every placement publishes a rank-map digest, complete group memberships, cross-domain bytes, domain-pair matrix, shared-resource offered load and local-expert-hit statistics. Cross-domain AlltoAll uses Projected A2A Traffic. Missing routing artifacts, topology paths or matching cost models yields traffic/load or UNKNOWN, never a fabricated time.

### Batch, memory and candidate validity

- Micro-batch size searches powers of two from 1 upward and the largest integer near the boundary `peak HBM ≤ 95% × Scenario Usable HBM Budget`.
- Gradient accumulation searches 1, powers of two and boundary values near the GTS limit. Every candidate proves `GTS = sequence × MBS × DP × GA ≤ 500,000,000` tokens per optimizer step.
- Reaching 500M is not an objective. Useful Throughput is the objective under GTS, memory, routing and correctness constraints.
- No recompute, selective tensor recompute, attention activation recompute and MoE activation recompute remain Pareto choices. A fuller memory allocation is not retained when it reduces throughput without another named benefit.
- Divisibility, group coverage, rank mapping, HBM, GTS, routing conservation and required cost-model dependencies fail closed independently.

Reports compare regular 98,304 and exact 100,000 feasibility and explain changes through cross-domain bytes, collective startup count, local expert hit, overlap, pipeline bubble and GEMM shape rather than only total speedup.

## Consequences

- The current-product 100,000-active topology contains a partially active SuperPod and cannot be treated as 98 identical full domains.
- The 98,304 lane provides clean 2,048-expert and 1,024/8,192-domain divisibility, but its 1,696 inactive devices affect capacity and later goodput analysis.
- A result may be topology-valid but training-grid-infeasible. This distinction prevents ragged topology support from overstating framework support.
- SuperPod advantage is evaluated through paired placement contrasts and attributable resource metrics, not assumed from physical peak bandwidth.

## Revisit conditions

Revisit topology identities when the target BOM or an authoritative current-product configuration changes. Revisit exact-100,000 feasibility when the selected Ascend training stack demonstrates non-uniform groups and ragged shard/optimizer semantics. Never merge current-product, architecture-limit and historical-roadmap parameters merely to recover divisibility.
