"""PROTOTYPE ONLY: pure Target 10T Workload contract and tensor counter.

Question: can one explicit contract distinguish logical parameters, active
parameters, checkpoint storage, GTS, routing provenance and symbolic memory
events while exactly reproducing the fixed official V4-Pro checkpoint header?
This is not a production schema, training manifest or calibrated memory model.
"""

from __future__ import annotations

import hashlib
import json
import math
from dataclasses import asdict, dataclass, replace
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


GTS_LIMIT = 500_000_000
STORAGE_BYTES = {
    "BF16": 2,
    "F32": 4,
    "F8_E4M3": 1,
    "F8_E8M0": 1,
    "PACKED_FP4_I8": 1,
    "I64": 8,
}


def _product(values: Sequence[int]) -> int:
    result = 1
    for value in values:
        result *= value
    return result


@dataclass(frozen=True)
class TensorSpec:
    name: str
    logical_shape: Tuple[int, ...]
    storage_shape: Tuple[int, ...]
    storage_dtype: str
    logical_dtype: str
    kind: str
    trainable: bool
    block_scope: str

    @property
    def logical_elements(self) -> int:
        return _product(self.logical_shape)

    @property
    def storage_elements(self) -> int:
        return _product(self.storage_shape)

    @property
    def storage_bytes(self) -> int:
        return self.storage_elements * STORAGE_BYTES[self.storage_dtype]


@dataclass(frozen=True)
class Architecture:
    vocab_size: int
    hidden_size: int
    num_hidden_layers: int
    num_mtp_layers: int
    num_hash_layers: int
    num_attention_heads: int
    head_dim: int
    q_lora_rank: int
    o_groups: int
    o_lora_rank: int
    moe_intermediate_size: int
    n_routed_experts: int
    n_shared_experts: int
    topk: int
    hc_mult: int
    index_n_heads: int
    index_head_dim: int
    compress_ratios: Tuple[int, ...]

    @staticmethod
    def from_dict(value: Dict[str, object]) -> "Architecture":
        copied = dict(value)
        copied["compress_ratios"] = tuple(copied["compress_ratios"])
        return Architecture(**copied)


@dataclass(frozen=True)
class StepConfig:
    micro_batch_sequences: int
    sequence_tokens: int
    data_parallel_replicas: int
    gradient_accumulation: int
    nonpadding_tokens: Optional[int] = None
    dropped_tokens: int = 0
    replayed_tokens: int = 0

    @property
    def configured_gts(self) -> int:
        return (
            self.micro_batch_sequences
            * self.sequence_tokens
            * self.data_parallel_replicas
            * self.gradient_accumulation
        )

    @property
    def useful_tokens(self) -> Optional[int]:
        if self.nonpadding_tokens is None:
            return None
        return self.nonpadding_tokens - self.dropped_tokens - self.replayed_tokens


@dataclass(frozen=True)
class ContractState:
    architecture: Architecture
    step: StepConfig
    routing_artifact_ref: Optional[str] = None
    precision_policy_ref: Optional[str] = None
    optimizer_policy_ref: Optional[str] = None
    placement_ref: Optional[str] = None
    checkpoint_policy_ref: Optional[str] = None
    runtime_profile_ref: Optional[str] = None


def _tensor(
    name: str,
    shape: Sequence[int],
    dtype: str,
    kind: str,
    trainable: bool,
    scope: str,
    logical_shape: Optional[Sequence[int]] = None,
) -> TensorSpec:
    storage_shape = tuple(shape)
    return TensorSpec(
        name=name,
        logical_shape=tuple(logical_shape or shape),
        storage_shape=storage_shape,
        storage_dtype=dtype,
        logical_dtype="FP4" if dtype == "PACKED_FP4_I8" else dtype,
        kind=kind,
        trainable=trainable,
        block_scope=scope,
    )


def _linear(
    prefix: str,
    out_features: int,
    in_features: int,
    dtype: str,
    kind: str,
    scope: str,
) -> Iterable[TensorSpec]:
    if dtype == "FP4":
        if in_features % 2 or in_features % 32:
            raise ValueError("FP4 packed dimensions must be divisible by 32")
        yield _tensor(
            prefix + ".weight",
            (out_features, in_features // 2),
            "PACKED_FP4_I8",
            kind,
            True,
            scope,
            logical_shape=(out_features, in_features),
        )
        yield _tensor(
            prefix + ".scale",
            (out_features, in_features // 32),
            "F8_E8M0",
            "quant_scale",
            False,
            scope,
        )
    elif dtype == "FP8":
        yield _tensor(
            prefix + ".weight",
            (out_features, in_features),
            "F8_E4M3",
            kind,
            True,
            scope,
        )
        yield _tensor(
            prefix + ".scale",
            (math.ceil(out_features / 128), math.ceil(in_features / 128)),
            "F8_E8M0",
            "quant_scale",
            False,
            scope,
        )
    elif dtype in ("BF16", "F32"):
        yield _tensor(
            prefix + ".weight",
            (out_features, in_features),
            dtype,
            kind,
            True,
            scope,
        )
    else:
        raise ValueError("unsupported linear dtype: " + dtype)


def _norm(name: str, size: int, scope: str) -> TensorSpec:
    return _tensor(name + ".weight", (size,), "BF16", "norm", True, scope)


def _expert(prefix: str, architecture: Architecture, routed: bool, scope: str) -> Iterable[TensorSpec]:
    dtype = "FP4" if routed else "FP8"
    kind = "routed_expert" if routed else "shared_expert"
    h = architecture.hidden_size
    d = architecture.moe_intermediate_size
    yield from _linear(prefix + ".w1", d, h, dtype, kind, scope)
    yield from _linear(prefix + ".w2", h, d, dtype, kind, scope)
    yield from _linear(prefix + ".w3", d, h, dtype, kind, scope)


def _compressor(
    prefix: str,
    architecture: Architecture,
    ratio: int,
    head_dim: int,
    scope: str,
) -> Iterable[TensorSpec]:
    coefficient = 2 if ratio == 4 else 1
    width = coefficient * head_dim
    yield _tensor(prefix + ".ape", (ratio, width), "F32", "attention", True, scope)
    yield from _linear(prefix + ".wkv", width, architecture.hidden_size, "BF16", "attention", scope)
    yield from _linear(prefix + ".wgate", width, architecture.hidden_size, "BF16", "attention", scope)
    yield _norm(prefix + ".norm", head_dim, scope)


def _attention(prefix: str, architecture: Architecture, ratio: int, scope: str) -> Iterable[TensorSpec]:
    h = architecture.hidden_size
    heads = architecture.num_attention_heads
    head_dim = architecture.head_dim
    yield _tensor(prefix + ".attn_sink", (heads,), "F32", "attention", True, scope)
    yield from _linear(prefix + ".wq_a", architecture.q_lora_rank, h, "FP8", "attention", scope)
    yield _norm(prefix + ".q_norm", architecture.q_lora_rank, scope)
    yield from _linear(
        prefix + ".wq_b",
        heads * head_dim,
        architecture.q_lora_rank,
        "FP8",
        "attention",
        scope,
    )
    yield from _linear(prefix + ".wkv", head_dim, h, "FP8", "attention", scope)
    yield _norm(prefix + ".kv_norm", head_dim, scope)
    yield from _linear(
        prefix + ".wo_a",
        architecture.o_groups * architecture.o_lora_rank,
        heads * head_dim // architecture.o_groups,
        "FP8",
        "attention",
        scope,
    )
    yield from _linear(
        prefix + ".wo_b",
        h,
        architecture.o_groups * architecture.o_lora_rank,
        "FP8",
        "attention",
        scope,
    )
    if ratio:
        yield from _compressor(prefix + ".compressor", architecture, ratio, head_dim, scope)
        if ratio == 4:
            yield from _linear(
                prefix + ".indexer.wq_b",
                architecture.index_n_heads * architecture.index_head_dim,
                architecture.q_lora_rank,
                "FP8",
                "attention_indexer",
                scope,
            )
            yield from _linear(
                prefix + ".indexer.weights_proj",
                architecture.index_n_heads,
                h,
                "BF16",
                "attention_indexer",
                scope,
            )
            yield from _compressor(
                prefix + ".indexer.compressor",
                architecture,
                ratio,
                architecture.index_head_dim,
                scope,
            )


def _hyperconnection(prefix: str, architecture: Architecture, scope: str) -> Iterable[TensorSpec]:
    mix_hc = (2 + architecture.hc_mult) * architecture.hc_mult
    hc_dim = architecture.hc_mult * architecture.hidden_size
    for phase in ("attn", "ffn"):
        yield _tensor(prefix + ".hc_" + phase + "_fn", (mix_hc, hc_dim), "F32", "hyperconnection", True, scope)
        yield _tensor(prefix + ".hc_" + phase + "_base", (mix_hc,), "F32", "hyperconnection", True, scope)
        yield _tensor(prefix + ".hc_" + phase + "_scale", (3,), "F32", "hyperconnection", True, scope)


def _block(
    prefix: str,
    layer_id: int,
    ratio: int,
    architecture: Architecture,
    scope: str,
) -> Iterable[TensorSpec]:
    yield from _attention(prefix + ".attn", architecture, ratio, scope)
    yield _norm(prefix + ".attn_norm", architecture.hidden_size, scope)
    yield _norm(prefix + ".ffn_norm", architecture.hidden_size, scope)
    yield from _hyperconnection(prefix, architecture, scope)
    yield _tensor(
        prefix + ".ffn.gate.weight",
        (architecture.n_routed_experts, architecture.hidden_size),
        "BF16",
        "router",
        True,
        scope,
    )
    if layer_id < architecture.num_hash_layers:
        yield _tensor(
            prefix + ".ffn.gate.tid2eid",
            (architecture.vocab_size, architecture.topk),
            "I64",
            "routing_table",
            False,
            scope,
        )
    else:
        yield _tensor(
            prefix + ".ffn.gate.bias",
            (architecture.n_routed_experts,),
            "F32",
            "router",
            True,
            scope,
        )
    for expert_id in range(architecture.n_routed_experts):
        yield from _expert(
            prefix + ".ffn.experts." + str(expert_id),
            architecture,
            True,
            scope,
        )
    for shared_id in range(architecture.n_shared_experts):
        shared_prefix = prefix + ".ffn.shared_experts"
        if architecture.n_shared_experts > 1:
            shared_prefix += "." + str(shared_id)
        yield from _expert(shared_prefix, architecture, False, scope)


def generate_tensor_manifest(architecture: Architecture) -> Iterable[TensorSpec]:
    if len(architecture.compress_ratios) != architecture.num_hidden_layers + architecture.num_mtp_layers:
        raise ValueError("compress_ratios must cover all main and MTP blocks")
    yield _tensor(
        "embed.weight",
        (architecture.vocab_size, architecture.hidden_size),
        "BF16",
        "embedding",
        True,
        "global",
    )
    for layer_id in range(architecture.num_hidden_layers):
        yield from _block(
            "layers." + str(layer_id),
            layer_id,
            architecture.compress_ratios[layer_id],
            architecture,
            "main_block",
        )
    yield _norm("norm", architecture.hidden_size, "global")
    yield _tensor(
        "head.weight",
        (architecture.vocab_size, architecture.hidden_size),
        "BF16",
        "head",
        True,
        "global",
    )
    hc_dim = architecture.hc_mult * architecture.hidden_size
    yield _tensor("hc_head_fn", (architecture.hc_mult, hc_dim), "F32", "hyperconnection", True, "global")
    yield _tensor("hc_head_base", (architecture.hc_mult,), "F32", "hyperconnection", True, "global")
    yield _tensor("hc_head_scale", (1,), "F32", "hyperconnection", True, "global")
    for mtp_id in range(architecture.num_mtp_layers):
        block_id = architecture.num_hidden_layers + mtp_id
        prefix = "mtp." + str(mtp_id)
        yield from _block(
            prefix,
            block_id,
            architecture.compress_ratios[block_id],
            architecture,
            "mtp_block",
        )
        yield from _linear(prefix + ".e_proj", architecture.hidden_size, architecture.hidden_size, "FP8", "mtp_projection", "mtp_block")
        yield from _linear(prefix + ".h_proj", architecture.hidden_size, architecture.hidden_size, "FP8", "mtp_projection", "mtp_block")
        yield _norm(prefix + ".enorm", architecture.hidden_size, "mtp_block")
        yield _norm(prefix + ".hnorm", architecture.hidden_size, "mtp_block")
        yield _norm(prefix + ".norm", architecture.hidden_size, "mtp_block")
        yield _tensor(prefix + ".hc_head_fn", (architecture.hc_mult, hc_dim), "F32", "hyperconnection", True, "mtp_block")
        yield _tensor(prefix + ".hc_head_base", (architecture.hc_mult,), "F32", "hyperconnection", True, "mtp_block")
        yield _tensor(prefix + ".hc_head_scale", (1,), "F32", "hyperconnection", True, "mtp_block")


def _active_params(tensors: Sequence[TensorSpec], topk: int, scopes: Tuple[str, ...]) -> int:
    total = 0
    routed_by_block: Dict[str, Dict[str, int]] = {}
    for tensor in tensors:
        if tensor.block_scope not in scopes or not tensor.trainable:
            continue
        if tensor.kind == "routed_expert":
            block = tensor.name.split(".ffn.experts.", 1)[0]
            expert = tensor.name.split(".ffn.experts.", 1)[1].split(".", 1)[0]
            routed_by_block.setdefault(block, {}).setdefault(expert, 0)
            routed_by_block[block][expert] += tensor.logical_elements
        else:
            total += tensor.logical_elements
    for experts in routed_by_block.values():
        sizes = set(experts.values())
        if len(sizes) != 1 or len(experts) < topk:
            raise ValueError("routed expert tensors are not uniform or topk exceeds experts")
        total += topk * next(iter(sizes))
    return total


def summarize_tensors(tensors: Sequence[TensorSpec], topk: int) -> Dict[str, object]:
    trainable_by_kind: Dict[str, int] = {}
    trainable_by_dtype: Dict[str, int] = {}
    by_module_and_dtype: Dict[str, Dict[str, int]] = {}
    auxiliary_by_kind_and_dtype: Dict[str, Dict[str, int]] = {}
    digest = hashlib.sha256()
    for tensor in tensors:
        if tensor.trainable:
            trainable_by_kind[tensor.kind] = (
                trainable_by_kind.get(tensor.kind, 0) + tensor.logical_elements
            )
            trainable_by_dtype[tensor.logical_dtype] = (
                trainable_by_dtype.get(tensor.logical_dtype, 0)
                + tensor.logical_elements
            )
            module = tensor.block_scope + "/" + tensor.kind
            module_dtypes = by_module_and_dtype.setdefault(module, {})
            module_dtypes[tensor.logical_dtype] = (
                module_dtypes.get(tensor.logical_dtype, 0) + tensor.logical_elements
            )
        else:
            auxiliary_dtypes = auxiliary_by_kind_and_dtype.setdefault(tensor.kind, {})
            auxiliary_dtypes[tensor.storage_dtype] = (
                auxiliary_dtypes.get(tensor.storage_dtype, 0)
                + tensor.storage_elements
            )
        canonical = {
            "name": tensor.name,
            "logical_shape": tensor.logical_shape,
            "storage_shape": tensor.storage_shape,
            "storage_dtype": tensor.storage_dtype,
            "logical_dtype": tensor.logical_dtype,
            "kind": tensor.kind,
            "trainable": tensor.trainable,
            "block_scope": tensor.block_scope,
        }
        digest.update(
            json.dumps(canonical, sort_keys=True, separators=(",", ":")).encode("utf-8")
        )
        digest.update(b"\n")
    return {
        "tensor_count": len(tensors),
        "tensor_manifest_sha256": digest.hexdigest(),
        "logical_trainable_params": sum(t.logical_elements for t in tensors if t.trainable),
        "quant_scale_elements": sum(t.storage_elements for t in tensors if t.kind == "quant_scale"),
        "routing_table_elements": sum(t.storage_elements for t in tensors if t.kind == "routing_table"),
        "checkpoint_storage_bytes": sum(t.storage_bytes for t in tensors),
        "active_params_per_token": {
            "main_blocks_only": _active_params(tensors, topk, ("main_block",)),
            "main_forward_including_io": _active_params(tensors, topk, ("global", "main_block")),
            "training_graph_including_mtp": _active_params(tensors, topk, ("global", "main_block", "mtp_block")),
        },
        "trainable_logical_params_by_kind": dict(sorted(trainable_by_kind.items())),
        # This is the checkpoint value format, not the future training or
        # optimizer precision policy. Training materialization stays external.
        "logical_trainable_params_by_checkpoint_value_dtype": dict(
            sorted(trainable_by_dtype.items())
        ),
        "logical_trainable_params_by_module_and_checkpoint_value_dtype": {
            module: dict(sorted(dtypes.items()))
            for module, dtypes in sorted(by_module_and_dtype.items())
        },
        "checkpoint_aux_elements_by_kind_and_storage_dtype": {
            kind: dict(sorted(dtypes.items()))
            for kind, dtypes in sorted(auxiliary_by_kind_and_dtype.items())
        },
    }


def memory_event_templates() -> List[Dict[str, object]]:
    return [
        {"component": "PARAM", "allocate_at": "JOB_INIT", "release_at": "JOB_END", "expression": "tensor_manifest × precision_policy ÷ placement_shards", "requires": ["precision_policy", "placement"]},
        {"component": "GRADIENT", "allocate_at": "BACKWARD_START_OR_PREALLOC", "release_at": "OPTIMIZER_STEP_END", "expression": "trainable_tensors × gradient_dtype ÷ gradient_shards", "requires": ["precision_policy", "placement"]},
        {"component": "MASTER_WEIGHT", "allocate_at": "OPTIMIZER_INIT", "release_at": "JOB_END", "expression": "optimizer groups requiring master copy", "requires": ["optimizer_policy", "placement"]},
        {"component": "OPTIMIZER_STATE", "allocate_at": "OPTIMIZER_INIT", "release_at": "JOB_END", "expression": "optimizer state tensors ÷ optimizer_shards", "requires": ["optimizer_policy", "placement"]},
        {"component": "SAVED_ACTIVATION", "allocate_at": "FORWARD_TENSOR_PRODUCED", "release_at": "BACKWARD_TENSOR_CONSUMED", "expression": "shape trace minus recompute set", "requires": ["checkpoint_policy", "placement"]},
        {"component": "ROUTER_LOGITS", "allocate_at": "ROUTER_START", "release_at": "TOPK_COMPLETE", "expression": "local_tokens × routed_experts × router_dtype", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "ROUTE_INDEX", "allocate_at": "TOPK_COMPLETE", "release_at": "COMBINE_COMPLETE", "expression": "local_tokens × topk × index_dtype", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "PERMUTED_TOKEN", "allocate_at": "PERMUTE_START", "release_at": "EXPERT_INPUT_CONSUMED", "expression": "padded assignments × hidden × activation_dtype", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "DISPATCH_BUFFER", "allocate_at": "DISPATCH_START", "release_at": "DISPATCH_COMPLETE", "expression": "routing matrix × dispatch dtype/metadata", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "EXPERT_INTERMEDIATE", "allocate_at": "EXPERT_GEMM1", "release_at": "EXPERT_GEMM2", "expression": "max local expert load × moe_intermediate × activation_dtype", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "COMBINE_BUFFER", "allocate_at": "COMBINE_START", "release_at": "COMBINE_COMPLETE", "expression": "routing matrix × combine dtype/metadata", "requires": ["routing_artifact", "precision_policy", "placement"]},
        {"component": "COLLECTIVE_SCRATCH", "allocate_at": "COLLECTIVE_START", "release_at": "COLLECTIVE_END", "expression": "HCCL runtime algorithm scratch", "requires": ["runtime_profile", "placement"]},
        {"component": "RECOMPUTE_WORKSPACE", "allocate_at": "BACKWARD_RECOMPUTE_START", "release_at": "BACKWARD_RECOMPUTE_END", "expression": "checkpoint policy × observed kernel workspace", "requires": ["checkpoint_policy", "runtime_profile"]},
        {"component": "RUNTIME_RESERVE", "allocate_at": "RUNTIME_INIT", "release_at": "RUNTIME_END", "expression": "measured runtime reserve", "requires": ["runtime_profile"]},
        {"component": "FRAGMENTATION_GUARD", "allocate_at": "CAPACITY_CHECK", "release_at": "CAPACITY_CHECK", "expression": "measured allocator guard, never inferred from free memory", "requires": ["runtime_profile"]},
    ]


def resolve_contract(state: ContractState, baseline_expected: Dict[str, int]) -> Dict[str, object]:
    model_errors: List[str] = []
    step_errors: List[str] = []
    incomplete: List[str] = []
    architecture = state.architecture
    if architecture.n_routed_experts <= 0 or architecture.topk <= 0:
        model_errors.append("EXPERTS_AND_TOPK_MUST_BE_POSITIVE")
    if architecture.n_routed_experts < architecture.topk:
        model_errors.append("TOPK_EXCEEDS_ROUTED_EXPERTS")
    if architecture.num_hash_layers > architecture.num_hidden_layers:
        model_errors.append("HASH_LAYERS_EXCEED_MAIN_LAYERS")
    if architecture.n_shared_experts != 1:
        model_errors.append("TARGET_REQUIRES_ONE_SHARED_EXPERT")
    if any(
        value <= 0
        for value in (
            state.step.micro_batch_sequences,
            state.step.sequence_tokens,
            state.step.data_parallel_replicas,
            state.step.gradient_accumulation,
        )
    ):
        step_errors.append("STEP_FACTORS_MUST_BE_POSITIVE")
    if state.step.configured_gts > GTS_LIMIT:
        step_errors.append("GTS_EXCEEDS_500M")
    if state.step.nonpadding_tokens is not None and state.step.nonpadding_tokens > state.step.configured_gts:
        step_errors.append("NONPADDING_EXCEEDS_CONFIGURED_GTS")
    if state.step.useful_tokens is not None and state.step.useful_tokens < 0:
        step_errors.append("NEGATIVE_USEFUL_TOKENS")

    # Model and step resources are orthogonal: an invalid GTS must not erase a
    # valid, content-addressed model manifest from diagnostics.
    tensors = list(generate_tensor_manifest(architecture)) if not model_errors else []
    summary = summarize_tensors(tensors, architecture.topk) if tensors else {}
    if state.routing_artifact_ref is None:
        incomplete.append("ROUTING_ARTIFACT_REQUIRED_FOR_LOAD_AND_BUFFER_MODEL")
    memory_refs = (
        state.precision_policy_ref,
        state.optimizer_policy_ref,
        state.placement_ref,
        state.checkpoint_policy_ref,
        state.runtime_profile_ref,
    )
    if any(value is None for value in memory_refs):
        incomplete.append("MEMORY_MATERIALIZATION_INPUTS_INCOMPLETE")

    baseline_gate = None
    if architecture.n_routed_experts == 384 and architecture.topk == 6 and summary:
        actual = {
            "tensor_count": summary["tensor_count"],
            "logical_trainable_params": summary["logical_trainable_params"],
            "quant_scale_elements": summary["quant_scale_elements"],
            "routing_table_elements": summary["routing_table_elements"],
            "storage_bytes": summary["checkpoint_storage_bytes"],
        }
        mismatches = {
            key: {"expected": baseline_expected[key], "actual": actual[key]}
            for key in actual
            if actual[key] != baseline_expected[key]
        }
        baseline_gate = {"status": "PASS" if not mismatches else "FAIL", "mismatches": mismatches}
        if mismatches:
            model_errors.append("OFFICIAL_BASELINE_REPRODUCTION_FAILED")

    identity_payload = {
        "apiVersion": "simai.ascend.model/v1alpha1-prototype",
        "architecture": asdict(architecture),
        "source": "deepseek-ai/DeepSeek-V4-Pro@45040942",
    }
    model_identity = hashlib.sha256(
        json.dumps(identity_payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    ).hexdigest()
    prototype_refs = any(
        isinstance(value, str) and value.startswith("PROTOTYPE_")
        for value in (
            state.routing_artifact_ref,
            state.precision_policy_ref,
            state.optimizer_policy_ref,
            state.placement_ref,
            state.checkpoint_policy_ref,
            state.runtime_profile_ref,
        )
    )
    errors = model_errors + step_errors
    if errors:
        status = "INVALID"
    elif incomplete:
        status = "STRUCTURALLY_VALID_INCOMPLETE"
    elif prototype_refs:
        status = "CONTRACT_COMPLETE_WITH_PROTOTYPE_REFS_NOT_ANALYTICAL_DATA"
    else:
        status = "READY_FOR_ANALYTICAL_INPUT"
    moe_blocks = architecture.num_hidden_layers + architecture.num_mtp_layers
    assignments = state.step.configured_gts * architecture.topk * moe_blocks
    return {
        "status": status,
        "errors": errors,
        "incomplete": incomplete,
        "model_identity_sha256": model_identity,
        "model": {
            "apiVersion": "simai.ascend.model/v1alpha1-prototype",
            "kind": "ModelManifest",
            "architecture": asdict(architecture),
            "tensor_summary": summary,
            "baseline_reproduction_gate": baseline_gate,
        },
        "step": {
            "apiVersion": "simai.ascend.workload/v1alpha1-prototype",
            "kind": "StepManifest",
            "configured_gts": state.step.configured_gts,
            "gts_limit": GTS_LIMIT,
            "nonpadding_tokens": state.step.nonpadding_tokens,
            "dropped_tokens": state.step.dropped_tokens,
            "replayed_tokens": state.step.replayed_tokens,
            "useful_tokens": state.step.useful_tokens,
            "configured_routed_assignment_slots_upper_bound": assignments,
            "configured_assignment_scope": "61_MAIN_MOE_BLOCKS_PLUS_1_MTP_MOE_BLOCK",
            "configured_routed_layers_in_scope": moe_blocks,
            "formula": "micro_batch_sequences × sequence_tokens × data_parallel_replicas × gradient_accumulation",
        },
        "routing": {
            "apiVersion": "simai.ascend.routing/v1alpha1-prototype",
            "kind": "RoutingArtifactRef",
            "ref": state.routing_artifact_ref,
            "required_statistics": ["mean", "p50", "p95", "p99", "max", "cv", "gini", "overflow", "drop", "padding"],
            "required_policy_fields": ["capacity_factor", "overflow_policy", "drop_policy", "padding_policy", "locality_policy", "determinism"],
            "inline_counts": False,
            "hash_layers": list(range(architecture.num_hash_layers)),
        },
        "memory": {
            "apiVersion": "simai.ascend.memory/v1alpha1-prototype",
            "kind": "SymbolicMemoryEventPlan",
            "materialization_refs": {
                "precision_policy": state.precision_policy_ref,
                "optimizer_policy": state.optimizer_policy_ref,
                "placement": state.placement_ref,
                "checkpoint_policy": state.checkpoint_policy_ref,
                "runtime_profile": state.runtime_profile_ref,
            },
            "events": memory_event_templates(),
            "optimizer_state_elements": None,
            "peak_bytes_per_rank": None,
            "reason": "PROTOTYPE_SYMBOLIC_NOT_A_MEMORY_ESTIMATE",
        },
        "adapter": {
            "target": "Upstream SimAI/AICB workload",
            "rule": "generated execution events carry model/step/routing/memory content hashes; v1 text header is not provenance",
        },
    }


def target_architecture(baseline: Architecture, experts: int = 2048, topk: int = 16) -> Architecture:
    return replace(baseline, n_routed_experts=experts, topk=topk)
