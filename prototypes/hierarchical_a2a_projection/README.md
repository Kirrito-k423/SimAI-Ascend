# PROTOTYPE：Hierarchical A2A Projection

这个 throwaway logic prototype 回答一个问题：Upstream SimAI 的 A2A/A2AV 是否能在进入 Analytical cost model 前流式投影为逐 rank、域对和拓扑资源负载，而不为 EP=2048 常驻构造数百万 pair flows？

它不是 HCCL 算法、测量曲线或 Simulation flow provider。所有 `PROTOTYPE_*` 引用与 `SYNTHETIC_CAPACITY_UNIT_NOT_NS` 只验证数据形状和守恒；不得解释为 950DT 性能。

## 一条命令运行

```bash
./scripts/prototype-hierarchical-a2a-projection.sh
```

批量运行四个守恒场景与三个缺失输入反例：

```bash
./scripts/prototype-hierarchical-a2a-projection.sh --scenario all
```

不联网的确定性 transcript：

```bash
python3 prototypes/hierarchical_a2a_projection/verify_conservation.py
```

使用真实 Upstream C++ flow generator 做 P=4 baseline 穿刺：

```bash
c++ -std=c++17 \
  -I astra-sim-alibabacloud \
  prototypes/hierarchical_a2a_projection/audit_upstream_flow_model.cc \
  astra-sim-alibabacloud/astra-sim/system/MockNcclGroup.cc \
  astra-sim-alibabacloud/astra-sim/system/MockNcclLog.cc \
  -o /tmp/simai-ascend-a2a-audit
/tmp/simai-ascend-a2a-audit
```

## 原型比较的四种表示

1. **Exact pair flows：** Upstream baseline；保留每个 src→dst flow，生成 P(P−1) objects。
2. **Symmetry fold：** 每个 domain-pair 一个 block-average class；仅在流量确实满足 block symmetry 时无损。
3. **Representative flow：** 单个全局加权代表流；总 bytes 可守恒，但热点、locality、resource bottleneck 通常丢失，仅作为反例。
4. **Hierarchical projection：** 对 uniform formula 使用闭式投影；对外置 A2AV counts artifact 或 routing-derived stream 做单遍归约。二者都保留 total、逐 rank 收发、域对矩阵和拓扑 resource offered load，不保留 endpoint flow objects。

## 必须成立的边界

- 小规模均匀、热点、locality、ragged 四场景对 exact baseline 的六项守恒必须全过。
- Uniform A2A 的闭式投影时间/状态为 O(P+D²+R)，不枚举 rank pair。任意 dense A2AV artifact 的流式 projection 状态仍是 O(P+D²+R)，但读取时间仍是 O(P²)；原型不声称任意矩阵存在无损次二次编码。
- Uniform A2A 可由公式直接投影；arbitrary A2AV 需要 immutable counts artifact/hash 或 routing-derived stream。100k dense matrix 若不可流式读取或没有结构性摘要，必须拒绝。
- Analytical bottleneck time 只在 topology path 和匹配的 HCCL cost model 都存在时物化；本原型的 synthetic functional 只证明输入充分性。
- Aggregate projection 永远不注入 NS-3。小规模 Simulation 仍需独立 HCCL-aware `CollectiveFlowProvider` 展开真实 endpoint flows 并单独验收。

## 若结论成立

- **保留：** `ProjectedA2ATraffic` pure contract、streaming accumulator、守恒 validator、明确 capability/fail-closed 状态。
- **删除：** TUI、四个合成矩阵、固定 capacity、ANSI、预置 action、全部 `PROTOTYPE_*` 引用。
- **后续票据：** 100k placement 决定 domain/path；A2/A3 Ground Truth 提供 HCCL cost；Simulation smoke 决定小规模 flow expansion。

## `ProjectedA2ATraffic` 候选 contract

- identity：routing/counts artifact digest、rank-mapping digest、topology digest、traffic semantics；
- conservation：total bytes、逐 rank ingress/egress、domain-pair bytes 与 row/column equality；
- resources：每个 topology/shared-resource 的 offered bytes、scope 和 path-model digest；
- readiness：routing、topology、cost-model 独立状态；任一缺失不以默认值补洞；
- output：standalone analytical time 只携带匹配的 cost-model id/domain status，永不生成 endpoint flow ids。
