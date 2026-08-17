# SimAI-Ascend `/to-spec` 输入

## Ready state

Wayfinder 的六项验收均已通过，所有必要决策均有用户 HITL、ADR 或 prototype/研究证据。此文件把已接受的“做什么、边界是什么、如何判定”交给 `/to-spec`；它不是实现完成声明。

## Product objective

从完整保留历史的 Upstream SimAI 1.6 基线开发 SimAI-Ascend，优先打通 `AICB → Ascend workload/Profile/HCCL → SimAI-Analytical → A2校准/A3留出`，随后增加有界的 SimAI-Simulation smoke。最终用无真机 A5/Ascend 950DT 敏感性输入搜索约100k ranks、10T-scale MoE（2048 routed experts、TopK16）、每 optimizer step GTS≤500M 的 slice-local Top-5 与典型代表配置。

## Normative decision set

| Concern | Normative source |
|---|---|
| Upstream history and source identity | ADR-0001 |
| 10T-scale target interpretation | ADR-0002, ADR-0004, ADR-0006 |
| Public/private evidence boundary | ADR-0003 |
| Analytical cost vs Simulation flow seam | ADR-0005 |
| 2048-EP Projected A2A Traffic | ADR-0007 |
| A2 calibration and strict A3 30% gate | ADR-0008 |
| A5 Estimated Profile and Sensitivity Envelope | ADR-0009 |
| 100k topology identities and folded placement | ADR-0010 |
| F0–F3 search and slice-local Top-5 | ADR-0011 |
| Fault Goodput policies and symbolic uncertainty | ADR-0012 |
| 16-rank two-domain Simulation smoke | ADR-0013 |

Domain terms in `CONTEXT.md` are normative. Research documents under `docs/research/` provide fixed primary-source evidence; Goal runs C001～C011 record decision provenance.

## Required implementation sequence

1. **Baseline and schemas:** preserve upstream/GPU behavior; productionize versioned Profile, RawObservation, DerivedCostModel, workload resources, Projected A2A Traffic and validators.
2. **Analytical vertical slice:** inject an optional Ascend `CollectiveCostModel`; null retains legacy GPU. Implement the 10T/reduced workload adapters without putting hardware facts in model identity.
3. **Ground Truth:** establish A2/A3 pinned stacks; collect A2 L0–L3 and HCCL curves; freeze the predictor and then execute the three strict A3 cases.
4. **A5 planning model:** ingest user/vendor SKU, compute, HBM, transfer and link inputs; produce low/nominal/high non-probabilistic sensitivity bundles and explicit UNKNOWN states.
5. **100k search:** validate dual parallel-grid identities, generate placement contrasts, enforce GTS/HBM/routing constraints, run F1/F2 and publish each compatible slice's Top-5 plus named representatives.
6. **Fault Goodput:** implement five policy modes, complete checkpoint/spare/ragged state transitions, symbolic break-even output and numeric common-trace ranking only when inputs are complete.
7. **Simulation smoke:** implement a separate HCCL `CollectiveFlowProvider`; run the 16-rank two-domain 17-micro+2-E2E suite and retain Flow Smoke and F3 timing as separate gates.
8. **Final report:** explain SuperPod advantage through paired placement, cross-domain bytes, local expert hit, resource load, overlap, bubbles and failure goodput—not through peak bandwidth or a single champion.

## Acceptance gates

- Legacy GPU and no-profile paths retain their accepted behavior; profile/backend conflicts fail closed.
- Workload identity reproduces 8,414,884,746,526 accepted logical parameters and rejects GTS above 500,000,000.
- Peak HBM is at most 95% of Scenario Usable HBM for search; A2/A3 accuracy executions retain the stricter 85% safety line.
- Every strict A3 held-out case has end-to-end step-time APE≤30%; blocked and invalid executions never pass.
- F0 rejects every incomplete legality proof; F1 evaluates all legal candidates; F2 promotion and truncation follow ADR-0011 and disclose coverage.
- A5, topology, resource and fault identities never mix across ranking slices. Missing numerical fault inputs produce break-even/UNKNOWN rather than a goodput champion.
- All 19 Simulation smoke cases pass exact structural invariants for `FLOW_SMOKE_PASS`; every case must also have Analytical-vs-Simulation APE≤30% plus ordering invariants for F3.
- Public outputs contain no host identity, credential, private inventory or raw private log.

## Inputs still required during implementation

These are data/readiness inputs, not Wayfinder decisions:

- current A2/A3 driver, ABI, rank-domain and HCCL Test readiness;
- selected A5/950DT SKU/BOM and user-supplied compute, usable HBM, transfer and interconnect values where primary sources are insufficient;
- measured/fitted HCCL message curves and overlap observations in their exact domains;
- A5 failure, recovery, checkpoint and spare inputs for numerical Fault Goodput;
- framework proof before enabling exact-100k ragged execution or `RAGGED_CONTINUE`.

## Explicit non-goals

- an independent simulator or clean-room rewrite detached from Upstream SimAI;
- production-accuracy promises for A5/100k without target measurements;
- real 100k deployment, full 10T training or 100k NS-3 packet simulation;
- SimAI-Physical;
- publication of remote access details or private raw evidence;
- fee monitoring for this project.
