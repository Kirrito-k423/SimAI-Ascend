# PROTOTYPE：Target 10T Workload Contract

这个 throwaway logic prototype 回答一个问题：能否从固定官方 V4-Pro checkpoint 的真实 tensor shape/dtype 出发，生成 2,048 routed experts、TopK 16 的 Target 10T Workload，同时不混淆逻辑参数、active 参数、量化 checkpoint storage、GTS、路由证据与训练显存事件？

它不是 production schema，不下载权重，不包含 A5/950DT 性能或显存预测。`PROTOTYPE_*_NOT_MEASURED` 只证明引用和状态机，不能作为校准数据。

它服务于本仓“从 [Upstream SimAI README_CN](https://github.com/aliyun/SimAI/blob/master/README_CN.md) 开始做 Ascend 适配”的主路径：模型/step/routing/memory 内容哈希最终附着到 AICB 生成、SimAI 消费的执行 workload；本原型不另造训练模拟器，也不把 Ascend 硬件字段塞进模型身份。

## 一条命令运行

```bash
./scripts/prototype-target-10t-workload-contract.sh
```

批量观察所有预置场景：

```bash
./scripts/prototype-target-10t-workload-contract.sh --scenario all
```

运行不联网的确定性不变量检查：

```bash
python3 prototypes/target_10t_workload_contract/verify_invariants.py
```

输出完整 prototype JSON：

```bash
./scripts/prototype-target-10t-workload-contract.sh --scenario target-contract-complete --json
```

若本机能访问固定 Hugging Face source，可逐 tensor 对照全部 64 个官方 header：

```bash
python3 prototypes/target_10t_workload_contract/audit_official_headers.py
```

该命令只通过 HTTP Range 读取 JSON header，不下载 tensor data。

## 硬门禁与评判点

1. 官方 384E/TopK6 baseline 必须精确复现 145,116 tensors、1,598,837,347,742 logical trainable params 和 864,704,792,696 checkpoint storage bytes。
2. Target 使用同一个全局 MoE 配置扩展 61 个主干 block 和 1 个完整 MTP block，输出 8.414884746526T logical params；不再保留“是否扩 MTP”的隐式变体。
3. `active_params_per_token` 必须带 scope；主干 block、含 embedding/head 的 main forward、含 MTP 的训练图不能压成一个无定义数字。
4. GTS 硬门禁作用于 `micro_batch_sequences × sequence_tokens × DP × GA`；padding/drop/replay/useful tokens 独立报告。
5. Tensor manifest、Step manifest、外置 Routing artifact 与 symbolic Memory event plan 分层引用；缺 routing 或 precision/optimizer/placement/checkpoint/runtime 输入时 fail-closed，不输出假 `peak_bytes_per_rank`。

逐 module/dtype 统计里的 dtype 是固定 checkpoint 的 value format，不是未来训练或 optimizer dtype；后二者必须由外置 precision/optimizer policy 决定。

## Schema-stage capacity pre-check

这是 `model-scale-estimate` 的预运行槽位，但本票没有权力替“100k 拓扑与并行放置”票选择 TP/PP/EP/DP，也没有 A5 实测可用 HBM 或正式 optimizer/precision policy。因此只报告已经闭合的全局下界，并对 per-rank fit fail-closed。

### Assumptions

| Item | Value | Source | Confidence |
|---|---:|---|---|
| Logical trainable params | 8,414,884,746,526 | fixed official headers + target transform | E2 prototype |
| Quantized checkpoint storage | 4,486,847,493,752 B | packed FP4/FP8 shapes + scales + routing tables | E2 prototype |
| Training weight/gradient/optimizer dtype | UNKNOWN | requires `precision_policy` and `optimizer_policy` | not materialized |
| Per-rank sharding/replication | UNKNOWN | requires `placement` | not materialized |
| Target usable HBM and guard | UNKNOWN | requires measured `runtime_profile` | not materialized |
| Activation/recompute | symbolic events only | requires shape trace and `checkpoint_policy` | not materialized |

### Fit matrix

| device profile | tp | pp | ep | dp | cp/sp | micro batch | sequence | estimated peak/rank | margin | verdict |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 950DT 96GB vendor-capacity scenario | unbound | unbound | unbound | unbound | unbound | unbound | unbound | UNKNOWN | UNKNOWN | BLOCKED_BY_INPUT |
| 950DT 144GB vendor-capacity scenario | unbound | unbound | unbound | unbound | unbound | unbound | unbound | UNKNOWN | UNKNOWN | BLOCKED_BY_INPUT |

- **Sequence limit：** config 的 1,048,576 只是架构上限，不是训练 fit 证据；在 activation trace、CP/SP、micro batch 和 checkpoint policy 缺失时保持 UNKNOWN。
- **Throughput expectation：** UNKNOWN；本原型没有 kernel/HCCL timing，禁止从 checkpoint bytes 推 tokens/s 或 step time。
- **Recommendation：** 先绑定五个 materialization refs，再从已冻结的 `GT-TARGET-SEMANTIC-v1` 小 slice 启动；任何 full target launch 建议都属于后续 placement/search 票。
- **Calibration slot：** `actual_peak_bytes_per_rank=UNKNOWN`、`actual_tokens_s=UNKNOWN`、`status=NOT_RUN`、`estimator_update=PENDING_A2_A3_GROUND_TRUTH`。

## 若结论成立，保留与删除

- **保留为设计契约：** 逐 tensor logical/storage shape、active scope、GTS hard gate、routing artifact/hash、symbolic memory lifetime events、AICB/SimAI adapter content hash。
- **删除：** TUI、ANSI 输出、预置 action、`PROTOTYPE_*` 引用和 fixture 状态机。
- **不在本票解决：** 正式 optimizer/Muon 状态、并行 placement、A5 显存值、2048-EP AlltoAll 聚合、100k 配置搜索与性能预测。
