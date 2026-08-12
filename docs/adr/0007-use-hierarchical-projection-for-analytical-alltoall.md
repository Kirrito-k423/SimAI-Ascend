# Use Hierarchical Projection for Analytical AlltoAll

## Status

Accepted on 2026-08-12 after user HITL review of the throwaway prototype at `prototype/hierarchical-a2a-projection@691346f`.

## Context

Upstream SimAI is the starting point for the Ascend adaptation. Its current `MockNcclGroup::genAlltoallFlowModels` materializes one directed flow for every non-self rank pair and copies the other source ranks into each flow's dependency list. A compiled P=4 probe against the fixed Upstream source produced 12 unique directed flows, 24 duplicated rank-view entries and 36 unique copied `prev` integer references.

The Upstream `data_size` argument is the total collective input per rank. Each non-self pair therefore carries `data_size / P`, and the collective's total network bytes are `P × (P−1) × (data_size / P) = (P−1) × data_size`. This semantic must remain explicit; interpreting `data_size` as bytes per pair would overcount traffic by P.

At EP=2048, the current representation implies 4,192,256 directed flow objects and 8,581,548,032 copied `prev` references. At 100,000 ranks it implies 9,999,900,000 directed pairs before configuration search even begins. Run C006 compared exact pair flows with symmetry folding, representative flows and hierarchical projection over uniform, hotspot, locality and ragged-domain scenarios. Only hierarchical projection preserved total bytes, per-rank ingress/egress, domain-pair bytes, resource load and the deterministic synthetic bottleneck in all four scenarios.

## Decision

1. The primary Analytical representation is a content-addressed `ProjectedA2ATraffic` resource. It records source identities and digests, total bytes, per-rank send and receive vectors, a domain-pair byte matrix, topology-resource offered loads, conservation results and readiness state. It does not retain resident endpoint flow objects.
2. Uniform AlltoAll is projected by a closed form with O(P+D²+R) time and state, where P is ranks, D is placement domains and R is topology resources. Arbitrary dense A2AV may be streamed with O(P+D²+R) resident state, but reading its full counts remains O(P²). No lossless subquadratic claim is made for an arbitrary dense routing matrix.
3. A2AV consumes either an immutable dense or sparse counts artifact identified by content hash, or a routing-derived stream with equivalent provenance. A summary-only input cannot support hotspot- or path-aware completion-time estimates and therefore fails closed for those uses.
4. Missing routing, topology or a matching HCCL cost model produces distinct UNKNOWN readiness states. Synthetic capacities may prove conservation and ordering only; their output is not nanoseconds, calibrated performance or an HCCL prediction.
5. `ProjectedA2ATraffic` is Analytical-only. Simulation and NS-3 require an independent HCCL-aware `CollectiveFlowProvider` that produces real endpoint flows at bounded scale. The aggregate projection must never masquerade as a packet-flow provider.

## Consequences

- EP=2048 uniform traffic across two 1,024-rank domains is represented by 8,200 summary cells instead of 4,192,256 directed flow objects. A ragged 100,000-rank placement over 98 domains uses 419,208 summary cells for the uniform closed form without enumerating pairs. These are representation counts, not memory or runtime benchmarks.
- The 100,000-card placement work must supply rank-to-domain placement and topology paths before resource load can be interpreted. The A2/A3 Accuracy Gate must supply a matching measured or fitted HCCL cost model before projected load can become time.
- Small-scale Simulation smoke tests expand or generate real endpoint flows through the separate flow-provider capability; they do not consume the aggregate as if it were a flow list.
- Symmetry folding may remain an optimization for proven block-symmetric cases, but it is not the general contract. A weighted representative flow is rejected as the primary representation because it loses rank and resource hotspots.

## Revisit conditions

Revisit the sufficient statistics if measured HCCL algorithms require information not preserved by the rank, domain-pair and resource-load surfaces. Revisit complexity claims if routing-artifact or topology semantics change. The separation between Analytical projection and Simulation endpoint flows remains required unless an Upstream provider ABI proves both semantics without fabricating either one.
