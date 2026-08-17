# C011：一次性关闭 Fault Goodput 与 Simulation smoke 决策

- **开始/结束：** 2026-08-14 / 2026-08-17T15:27:16+08:00
- **阶段：** RECON → PROTOTYPE → GRILLING → HITL_ACCEPTED → TO_SPEC_READY
- **动作类型：** DECISION
- **关联验收/未知量：** AC-05、AC-06、H-22、H-23、D-05、D-15、D-16

## 输入证据

- 地图只剩“定义故障 goodput 敏感性场景”和“决定 Simulation smoke test 的最小拓扑与验收”两张票；用户明确要求一次性确定，覆盖默认逐票节奏。
- 无 A5 MTBF、恢复或 checkpoint 实测，三类100k资源口径也不能把 inactive capacity 自动当 hot spare。
- ADR-0005/0007 已分离 Analytical cost 与 HCCL flow；ADR-0011 要求 Simulation disagreement 暂停冠军，但仍需独立定义具体 smoke gate。
- throwaway `prototype/simulation-smoke-contract@b9c3297` 证明 `FLOW_SMOKE_PASS`、`F3_SIMULATION_AUDITED` 与 `SIMULATION_DISAGREEMENT` 必须是不同状态；prototype 未运行 NS-3 且不进入 main。

## HITL 决策

1. Fault Goodput 使用 committed useful tokens/总墙钟时间，并在 Analytical slice 上增加独立 Fault Scenario 维度。
2. 故障按拓扑域显式输入 rate/trace、相关性和 detection→warmup；缺字段 UNKNOWN，不默认独立。
3. 固定 no-failure、rollback、hot-spare、ragged、correlated-loss 五策略；无数字只输出 response surface/break-even。
4. checkpoint 是完整训练状态；spare 是有耗尽/补充的状态；ragged 需要框架 recovery 语义证明。
5. common traces 先5次，CV>10%扩10次并按不利P10 goodput；跨完整 fault scenarios 的 Top-5 交集才标 Fault-Robust。
6. smoke 使用16 ranks、2×8 logical domains 的 synthetic topology，Linux CPU单线程，suite≤60分钟。
7. 五collective×4KiB/1MiB/64MiB加两种额外A2AV，共17 micro。
8. reduced A3 shape 以 topology-aware/striped 两个 rank maps 运行2个 E2E cases。
9. 独立HCCL flow provider、19 cases结构/守恒/digest全通过才是 Flow Smoke；任何 NVIDIA fallback 失败。
10. 19 cases逐项 Analytical-vs-Simulation APE≤30%，并通过消息单调和 placement ordering，才升级F3；否则保留结构支持并标 disagreement。

## 结果

- 用户一次性接受全部10项；形成 ADR-0012/0013、领域词汇、最终 Goal 账本与 `TO-SPEC.md`。
- 本轮未运行远程机器、NPU、HCCL或NS-3；没有生成 A5/100k 性能或 goodput冠军。
- 两张末票关闭后地图没有剩余必要决策，可以关闭并进入 `/to-spec`。
