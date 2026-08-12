# Separate Ascend Analytical cost from Simulation flow

## Status

Accepted on 2026-08-12 after user HITL review of the throwaway prototype at `prototype/ascend-provider-seam@3f31ca1`.

## Context

Upstream SimAI computes Analytical collective duration in `Layer::compute_time()` through the legacy `GPUType/cal_busbw` path, while Simulation constructs communication flows through NVIDIA-oriented `MockNcclGroup` behavior. Treating these two responsibilities as one generic provider would either leak Ascend branches through the workload path or silently reuse NCCL assumptions for HCCL.

Run C004 tested a minimal optional injection in the real Upstream compilation units and a six-scenario provider resolver. The modified Analytical target linked successfully, the Simulation object target containing `Sys` and `Layer` compiled successfully, and a null provider preserved all existing constructor call sites. The complete NS-3 link remains blocked by an upstream macOS arm64 linker error that was reproduced before the prototype.

## Decision

1. The first Ascend vertical slice is Analytical-first. A `CollectiveCostModel` receives the existing workload semantics—collective operation, group, rank count, bytes, TP size and EP size—and returns an estimated duration in nanoseconds.
2. The selected `AcceleratorProfile` and derived HCCL cost model are resolved explicitly at the backend entry and injected into `Sys`; a null injection delegates to the unchanged legacy GPU path.
3. Simulation uses a separate `CollectiveFlowProvider` capability. Ascend+Simulation is explicitly unsupported until an HCCL-aware flow provider exists; it must never fall back to `MockNcclGroup`.
4. `--device-profile` and the legacy `--gpu_type` selector are mutually exclusive. A conflict fails closed instead of guessing the accelerator.
5. The production implementation will preserve these seams but will not merge the prototype TUI, the fake `424242 ns` estimate, `Prototype`-suffixed types, interactive actions, or stub profiles.

## Consequences

- Existing GPU workloads and CLI behavior retain their original path when no Ascend profile is supplied.
- The workload schema remains hardware-neutral; evidence, topology and calibrated costs stay in the versioned Profile/RawObservation/DerivedCostModel resources.
- The Analytical slice can be implemented and calibrated before the Simulation flow model without misrepresenting Simulation support.
- A later Simulation smoke test must supply and validate an HCCL-specific `CollectiveFlowProvider` independently of the Analytical cost model.

## Revisit conditions

Revisit the assembly location if Upstream SimAI publishes a stable accelerator-provider ABI. The separation between duration estimation and flow generation, explicit profile selection, and fail-closed conflict behavior remain required unless new evidence disproves their semantic boundary.
