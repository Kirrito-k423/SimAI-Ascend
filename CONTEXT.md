# SimAI-Ascend

SimAI-Ascend is the domain for modeling and calibrating Ascend large-scale training systems from measured smaller-scale evidence, then searching feasible training configurations for future target systems.

## Language

**Upstream SimAI**:
The official `aliyun/SimAI` project from which SimAI-Ascend is derived and whose simulator identity and component boundaries define the starting point.
_Avoid_: Inspiration, reference-only implementation

**SimAI-Ascend**:
The Ascend hardware adaptation developed from Upstream SimAI rather than an independent simulator or clean-room rewrite.
_Avoid_: New simulator, SimAI-like simulator

**Ascend Vertical Slice**:
The smallest end-to-end Upstream SimAI training flow that runs with an Ascend-specific workload, device profile, communication model, and an A2/A3 measurement comparison.
_Avoid_: Full Ascend port, production simulator

**Exploration Accuracy Gate**:
The learning-phase acceptance threshold under which held-out A3 end-to-end step-time predictions may differ from measurements by up to 30% after development and calibration on A2; it proves that the workflow is connected, not that A5 or 100,000-card predictions are production-accurate.
_Avoid_: Production accuracy, A5 accuracy guarantee

**A2 Calibration Set**:
The performance observations from the A2 lane that may be used to fit or tune the Ascend Vertical Slice predictor before its revision, inputs and predictions are frozen.
_Avoid_: Cross-generation truth, A3-assisted calibration

**Strict A3 Held-out Set**:
The preregistered A3 end-to-end cases whose performance observations remain unseen until the predictor, inputs, predictions and evaluation rule are frozen; after performance unblinding they cannot be reused as a strict holdout.
_Avoid_: Second calibration set, reusable validation benchmark

**Invalid Accuracy Execution**:
A held-out attempt whose training semantics or completeness is invalid, such as OOM, unsafe memory occupancy, rank loss, non-finite values, token loss/replay or provenance drift; it cannot pass the Exploration Accuracy Gate.
_Avoid_: Large prediction error, environment unavailable

**Blocked Accuracy Environment**:
A state in which the pinned runtime, ABI or rank domain cannot be established, so no valid accuracy comparison exists yet.
_Avoid_: Accuracy failure, accuracy pass

**A5 Estimated Profile**:
A user-supplied, versioned set of theoretical capabilities for this project's Ascend 950DT target, such as dtype-specific TFLOPS, memory capacity and bandwidth, H2D/D2H bandwidth, and interconnect characteristics; predictions made from it are estimates with sensitivity ranges, not calibrated A5 results.
_Avoid_: A5 calibration profile, measured A5 performance

**Sensitivity Envelope**:
The low, nominal and high internally consistent A5 scenario bundles plus single-factor sensitivity results; it describes how outputs change under assumptions and has no statistical coverage or confidence meaning.
_Avoid_: Confidence interval, prediction interval, calibrated error bar

**Scenario Usable HBM Budget**:
A per-training-rank A5 planning limit supplied directly or derived from installed capacity and an explicit reserve assumption when no allocator measurement exists.
_Avoid_: Runtime-usable HBM, measured free memory

**Robust A5 Candidate**:
A configuration that remains feasible and ranked in the Top-5 under all low, nominal and high A5 sensitivity bundles.
_Avoid_: Guaranteed winner, calibrated optimum

**Useful Throughput**:
The number of non-dropped, non-replayed training tokens completed per unit of step time; it is the primary configuration-search objective, while memory occupancy is a feasibility constraint.
_Avoid_: Allocated tokens per second, memory utilization

**Typical Configuration Set**:
The slice-local five highest-ranked feasible configurations plus named representatives for highest throughput, lowest exposed communication, highest fault-tolerant goodput, and the best Robust A5 Candidate, including their performance gaps and resource trade-offs. The goodput representative remains UNKNOWN until its fault scenario is complete.
_Avoid_: Single best configuration, champion only

**Search Fidelity Stage**:
The highest fail-closed evidence stage completed by a candidate: static validation, Analytical screening, refined Analytical evaluation, or independent Simulation audit. It is an evidence label, not a score, and a lower stage cannot impersonate a higher one.
_Avoid_: Accuracy tier, interchangeable backend result

**Analytical Ranking Slice**:
One compatible topology identity, active-rank/resource interpretation, and A5 low/nominal/high bundle within which feasible configurations may be ranked by Useful Throughput. Top-5 lists from different slices are not pooled.
_Avoid_: Global leaderboard, averaged topology scenario

**Simulation Disagreement**:
The state of a configuration family whose selected small-scale Simulation audit exceeds the separately accepted mismatch gate; its champion claim is withheld until the model is revised and revalidated, without treating the audit as a 100k measurement.
_Avoid_: Automatic Analytical back-fit, measured 100k failure

**Target 10T Workload**:
The DeepSeek-V4-Pro-derived training workload that retains its trunk, one shared expert, first three hash-routed MoE layers, MTP depth, and expert intermediate size 3,072 while using 2,048 routed experts and MoE TopK 16, yielding roughly 8.31–8.42T total parameters as the accepted 10T-scale model.
_Avoid_: Original DeepSeek-V4-Pro, dense 10T model

**Global Token Size (GTS)**:
The total number of tokens consumed across all data-parallel replicas and gradient-accumulation microbatches in one optimizer step; the configured hard upper bound is 500,000,000 tokens per step.
_Avoid_: Total training token budget, tokens per second

**Target Workload Contract**:
The content-addressed composition of a tensor-based Model Manifest, optimizer-step/GTS manifest, external Routing Artifact, and symbolic Memory Event Plan that is attached to AICB-generated execution events consumed by Upstream SimAI.
_Avoid_: Single model YAML, hardware fields embedded in the model, independent simulator

**Scoped Active Parameters**:
The logical trainable parameters touched by one token within an explicitly named execution scope—main MoE blocks, main forward including input/output, or the training graph including MTP. It is never checkpoint storage or a memory estimate.
_Avoid_: Unqualified active parameters, model memory

**Projected A2A Traffic**:
A content-addressed Analytical view of AlltoAll or AlltoAllV traffic that preserves total bytes, per-rank send and receive, domain-pair bytes, topology-resource offered load, conservation evidence and readiness without retaining endpoint flow objects.
_Avoid_: Representative flow, packet flow, measured HCCL performance

**Current-product SuperPod Topology**:
The 1,024-NPU Atlas 950 product-domain identity used as the primary 100,000-rank search basis, with its own evidence, links, shared resources and cost-model references.
_Avoid_: 8,192-NPU architecture limit, historical roadmap topology

**Architecture-limit SuperNode Topology**:
The independent 8,192-NPU architecture-scale sensitivity identity; it does not assert current product availability or inherit current-product performance parameters.
_Avoid_: Current-product SuperPod, delivered 8,192-NPU system

**Regular 98,304 Active Set**:
The divisible performance baseline with 98,304 simultaneous training ranks and 1,696 ranks reserved for spare or service capacity in a nominal 100,000-device pool.
_Avoid_: 100,000 active ranks, exact 100k training

**Exact 100,000 Ragged Set**:
The topology with exactly 100,000 active ranks and a partially populated final domain; it is executable only when every parallel and optimizer group has supported ragged semantics.
_Avoid_: Regular Cartesian grid, topology-only feasibility
