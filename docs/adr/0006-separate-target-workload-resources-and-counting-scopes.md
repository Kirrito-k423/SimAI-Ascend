# Separate Target Workload Resources and Counting Scopes

## Status

Accepted on 2026-08-12 after user HITL review of the throwaway prototype at `prototype/target-10t-workload-contract@8195c3c`.

## Context

The Target 10T Workload is derived from the fixed official `deepseek-ai/DeepSeek-V4-Pro@45040942eb0d1c4e29fa6b92a6195f110e9e7444` checkpoint. Its routed-expert weights use packed FP4 storage, while quantization scales and hash-routing tables are checkpoint auxiliaries rather than trainable model parameters. A single undifferentiated parameter count would therefore confuse logical model size, checkpoint bytes, parameters touched by a token, and eventual training memory.

Run C005 built a throwaway tensor generator and compared all 145,116 generated baseline tensors with the 64 official safetensors JSON headers. Tensor names, storage shapes and dtypes matched with no missing, extra or mismatched tensors; no tensor data was downloaded. Seven contract scenarios and an independent arithmetic check then exercised the target transform, TopK-only changes, the 500M GTS boundary, missing routing evidence and missing memory-materialization inputs.

## Decision

1. The target transform applies the global `n_routed_experts=2048` and `topk=16` configuration to all 61 main MoE blocks and the one full MTP MoE block. The resulting model has exactly 8,414,884,746,526 logical trainable parameters and is the accepted 10T-scale workload.
2. A tensor manifest is the model source of truth. Each tensor records logical shape, checkpoint storage shape and dtype, trainable role and block scope. Logical trainable parameters, checkpoint auxiliary elements and checkpoint storage bytes are separate outputs. Checkpoint value dtype is not a training or optimizer precision policy.
3. Active logical parameters are always scope-qualified: 88,950,053,982 for main blocks only; 90,803,533,923 for the main forward including embedding and head; and 92,345,423,134 for the training graph including MTP. No unqualified “active parameters” field is allowed.
4. Configured GTS is `micro_batch_sequences × sequence_tokens × data_parallel_replicas × gradient_accumulation`, with a hard upper bound of 500,000,000 tokens per optimizer step. Non-padding, dropped, replayed and useful tokens are reported independently. For the accepted model at GTS 500M, the configured routed-assignment-slot upper bound is `500M × 16 × 62 = 496B`; it is not an observed routing count or a network traffic estimate.
5. The workload contract separates content-addressed Model, Step, Routing and Memory resources. Routing counts, policy and imbalance statistics live in an external artifact. Memory remains a symbolic lifetime event plan until precision, optimizer, placement, checkpoint/recompute and runtime-profile references are all bound; missing inputs fail closed and must not produce a fabricated per-rank peak.
6. AICB remains the workload bridge and Upstream SimAI remains the simulator. Generated execution events carry the four resource content hashes rather than encoding incomplete provenance in the legacy text header. The production design will not include the prototype TUI, canned actions, fixture state machine or `PROTOTYPE_*` references.

## Consequences

- Changing TopK alone changes active parameters and routing demand, but not total trainable parameters.
- An invalid step, including GTS above 500M, does not erase an otherwise valid and inspectable model identity.
- Checkpoint storage of 4,486,847,493,752 bytes is evidence about the fixed quantized checkpoint representation, not a training-memory estimate.
- TP/PP/EP/DP placement, 2048-expert AlltoAll aggregation, A5/950DT capabilities, recompute selection, Top-5 search and the 30% accuracy gate remain separate decisions.

## Revisit conditions

Revisit exact counts if the fixed official model source, tensor layout, MTP structure or requested global expert configuration changes. The separation of model, step, routing and memory resources remains required unless production evidence proves that two resources share the same identity and lifecycle.
