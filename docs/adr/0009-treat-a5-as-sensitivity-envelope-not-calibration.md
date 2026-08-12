# Treat A5 as a Sensitivity Envelope, Not Calibration

## Status

Accepted on 2026-08-12 after user HITL review of the five decisions in “定义 A5 Estimated Profile 输入与敏感性协议”.

## Context

The project has no A5/Ascend 950DT machine. Public primary sources describe multiple 950DT compute and memory SKUs, product-family physical ceilings, port reuse, and architecture-scale topology limits, but they do not provide the user's exact BOM, allocator-usable memory, workload-specific kernel efficiency, HCCL latency curves, or the final 100,000-rank topology.

A2/A3 measurements can identify efficiency factors and failure modes, but directly scaling their step time by a peak-TFLOP ratio would conflate accelerator generation, operator shape, memory behavior, collective algorithms and overlap. The A5 path therefore needs an explicit scenario contract that remains useful for configuration search without pretending to be calibration or statistical confidence.

## Decision

### Required A5 baseline inputs

Every complete A5 Estimated Profile binds a source version/date and a selected target SKU or a discrete set of SKU scenarios. Each SKU scenario provides or cites:

- training ranks per physical chip and the applicable scope;
- peak FLOP/s for every training dtype consumed by the workload, including BF16/FP16 and any selected lower-precision path;
- installed HBM per training-rank scope and HBM bandwidth;
- either a user-supplied per-rank usable-HBM budget or an explicit reserve/guard ratio used to derive a scenario budget;
- for each in-SuperPod link level: direction, bandwidth, bandwidth semantics, latency, oversubscription, endpoint scope and shared-port/shared-resource constraints;
- the same physical fields for cross-SuperPod links, while the number and arrangement of domains remain a separate 100,000-rank placement decision;
- canonical units, scope and immutable evidence for every value.

Primary-source values need not be re-entered by the user, but the target SKU must be selected. If it is not selected, the published 36/32/28-core and memory variants remain separate scenarios; maxima from different variants must never be combined into a synthetic SKU.

### Conditional inputs

- H2D/D2H inputs are optional when a candidate does not use optimizer/activation offload, host staging or host-mediated checkpoint movement. A candidate that uses one of those paths requires direction, host-memory type, message-size domain, concurrency and bandwidth/curve; missing data invalidates only that candidate.
- Power, temperature and throttling may remain UNKNOWN for the current throughput objective. Energy or throttling claims require their matching fields and otherwise remain UNKNOWN.
- Physical link ceilings without a matching collective cost model allow traffic and offered-load results only. They cannot produce HCCL time, exposed communication time or end-to-end step time.

### Transferable efficiency scenarios

A2/A3 observations may produce dimensionless A5 scenario factors only when keyed by the relevant domain:

- compute efficiency by operator, dtype and shape class;
- effective HBM-bandwidth ratio by access pattern and concurrency;
- link payload efficiency by topology level, traffic class and direction;
- overlap/exposed-communication ratio by execution phase;
- explicit routing, congestion and startup-latency corrections.

They remain `EXTRAPOLATED + FIELD_UNVERIFIED`. Compute, HBM, communication and overlap do not share a global efficiency scalar. A2/A3 step time must not be scaled by a peak-TFLOP ratio and relabeled as A5 time.

### Low, nominal and high sensitivity bundles

For at least five valid same-domain A2/A3 samples, each transferable efficiency factor uses P10 for `low`, P50 for `nominal` and P90 for `high`, calculated with the fixed `linear_type7` quantile convention and capped at 1. These names describe performance capability: `low` is conservative and `high` is optimistic.

With fewer than five valid same-domain samples, the factor remains UNKNOWN unless the user explicitly supplies low/nominal/high assumptions with provenance. A vendor-published range is preserved as a vendor range and is not transformed by the A2/A3 quantiles.

The correlated low, nominal and high bundles form a **Sensitivity Envelope**. They are scenario bounds, not probabilities, confidence intervals, coverage guarantees or calibrated error bars. In addition to the three bundles, one-factor-at-a-time sensitivity identifies whether compute, HBM, links, overlap or another input dominates the result.

### Consumption and reporting

Missing fields fail closed at the point of consumption:

- missing usable HBM prevents memory feasibility and any “fill memory” ranking;
- missing required dtype capability or compute efficiency prevents complete step-time output;
- missing a used topology link or matching collective cost model permits traffic/load only, not communication or step time;
- missing H2D/D2H excludes only candidates that depend on those paths;
- UNKNOWN must never be replaced by zero, a physical peak, an adjacent SKU or a default constant.

The search reports separate Top-5 rankings for low, nominal and high. A configuration is a **Robust A5 Candidate** only if it is feasible and remains in the Top-5 in all three bundles. Other configurations report rank reversals and their dominant sensitivity fields. No result is labeled measured, calibrated or confidence-qualified.

## Consequences

- A reserve-ratio-derived usable-HBM budget enables a planning scenario, but it remains `DERIVED/USER_INPUT + FIELD_UNVERIFIED`; it is not allocator-usable A5 memory.
- A complete A5 step-time estimate still requires a matching collective-cost scenario. Physical bandwidth alone is sufficient for topology pressure analysis, not duration.
- Selecting among public 950DT SKUs becomes explicit rather than silently combining the fastest compute, largest memory and highest bandwidth values.
- The later Top-5 search can distinguish robust configurations from configurations whose rank depends on optimistic A5 assumptions.

## Revisit conditions

When an A5 machine becomes available, create immutable A5 RawObservations and new DerivedCostModels rather than rewriting this scenario evidence as `MEASURED`. Revisit factor domains and bundle construction if the A2/A3 Accuracy Gate disproves transfer or if the target SKU/BOM, software stack or topology changes.
