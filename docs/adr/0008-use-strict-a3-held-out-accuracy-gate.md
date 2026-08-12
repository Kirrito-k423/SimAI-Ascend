# Use a Strict A3 Held-out Exploration Accuracy Gate

## Status

Accepted on 2026-08-12 after user HITL review of the five decisions in “定义 A2→A3 Exploration Accuracy Gate 的留出集”.

## Context

The project prioritizes connecting the Upstream SimAI-based Ascend workflow and accepts a learning-stage end-to-end prediction error of up to 30%. A2 is the development and calibration generation; A3 is a different-generation system used to test whether the learned compute, communication, overlap and memory contracts transfer at all. If A3 performance observations are used to tune the model, the test no longer answers that question.

The fixed Ground Truth stack already defines a common MindSpeed-LLM/MindSpeed/Megatron source contract, separate A2 and A3 lower-level environments, the four-active-layer `GT-TARGET-SEMANTIC-v1` shape, and L0–L3 measurement gates. This decision specifies the exact calibration/held-out split and the evaluation rule; it does not claim that any remote run has occurred or that the gate currently passes.

## Decision

### Strict separation and freeze

1. A2 is the only performance calibration set. Before A3 performance unblinding, A3 may run L0 import, device enumeration, BF16 correctness and 16-rank HCCL group-formation checks. It may expose non-performance identity and readiness facts, but no A3 step time, kernel time, HCCL bandwidth, trace or peak-memory observation may tune the predictor.
2. Before unblinding, freeze and content-address the SimAI-Ascend revision, A2-derived parameters, A3 non-performance profile inputs, workload and placement manifests, routing seeds/artifacts, prediction and interval for every case, sampling procedure and stop rules. Viewing any A3 performance field marks the held-out set as unblinded.

### A2 calibration set

Run the official Ground Truth bootstrap first, then calibrate these three A2 end-to-end cases with the shared four-active-layer model semantics, 32 routed experts, TopK 16, expert width 3,072, MBS 1 and `TP1/PP2/EP4/DP4`:

| Case | Sequence | GBS | GA | GTS per optimizer step | Purpose |
|---|---:|---:|---:|---:|---|
| `A2-CAL-BALANCED` | 2,048 | 8 | 2 | 16,384 | balanced reference |
| `A2-CAL-COMM` | 1,024 | 16 | 4 | 16,384 | more micro-steps; startup and overlap sensitivity |
| `A2-CAL-LONG` | 4,096 | 8 | 2 | 32,768 | long-sequence compute, activation and recompute sensitivity |

A2 may additionally measure component kernels and exact HCCL message points needed by the model. It cannot reproduce the complete 16-rank `EP8/DP8` A3 end-to-end configuration.

### Strict A3 held-out set

After the freeze, replay these three A3 cases with the same model semantics, MBS 1 and `TP1/PP2/EP8/DP8`:

| Case | Sequence | GBS | GA | GTS per optimizer step | Purpose |
|---|---:|---:|---:|---:|---|
| `A3-HOLD-BALANCED` | 2,048 | 8 | 1 | 16,384 | balanced transfer |
| `A3-HOLD-COMM` | 1,024 | 16 | 2 | 16,384 | communication startup and overlap transfer |
| `A3-HOLD-LONG` | 4,096 | 8 | 1 | 32,768 | long-sequence compute, activation and recompute transfer |

All three cases are far below the 500M GTS ceiling. This gate validates the reduced Ascend Vertical Slice, not the full 10T workload or a 500M-token step.

### Sampling and error statistic

1. Exclude initialization, compilation and warmup. Take five independent bounded steady-state observations first.
2. Compute `CV = sample_standard_deviation / mean` over the observations. If CV is at most 10%, the observed representative is the five-observation median. If CV is greater than 10%, extend to ten total observations and use P90 as the representative, with P90 calculated by the fixed `linear_type7` quantile convention. Preserve and report median, min, max and CV in either branch.
3. For each case compute `APE = abs(predicted_step_time − observed_representative_step_time) / observed_representative_step_time`.
4. The Exploration Accuracy Gate passes only if every one of the three A3 cases has `APE ≤ 30%`. Report case-level APE, MAPE, maximum APE and prediction intervals; averages and intervals cannot hide a failing case.

### Execution and revalidation states

- OOM or HBM at or above the existing 85% safety line, rank loss, non-finite loss/gradient, dropped or replayed tokens, incomplete ranks, or configuration/provenance drift produces `INVALID_EXECUTION` and the gate does not pass.
- Failure to establish the pinned environment, ABI or HCCL rank domain produces `BLOCKED_ENVIRONMENT`. It is neither an accuracy pass nor an accuracy failure; the gate remains unevaluated.
- After unblinding, any predictor change turns the original A3 observations into development data. Revalidation must preregister a new routing seed/message combination and a new frozen prediction. It is reported as a second validation round, never as the original strict held-out result.

## Consequences

- The gate is deliberately small and coarse: it proves that the end-to-end calibration workflow is connected across A2 and A3 with at most 30% error on three named cases.
- A pass does not guarantee A5/Ascend 950DT or 100,000-rank accuracy, and a blocked environment cannot be relabeled as a model result.
- The communication-sensitive pair holds GTS constant while changing sequence and accumulation; the long-sequence pair doubles GTS. Together they expose different transfer failures without expanding the gate into a large benchmark campaign.
- Public artifacts contain only sanitized manifests, aggregate measurements, prediction errors and status classes; host identities and raw private logs remain excluded.

## Revisit conditions

Revisit the cases if A3 cannot expose 16 stable training ranks or the fixed slice is invalid for both generations. Do not relax strict unblinding or the per-case 30% rule merely because a result fails. A future production-accuracy program requires a larger independently held-out dataset and is outside this Wayfinder destination.
