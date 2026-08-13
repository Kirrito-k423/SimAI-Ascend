# C010：定义多保真配置搜索与 Top-5 输出契约

- **开始/结束：** 2026-08-13T17:00:00+08:00 / 2026-08-13T17:29:10+08:00
- **阶段：** RECON → GRILLING → HITL_ACCEPTED
- **动作类型：** DECISION
- **关联验收/未知量：** AC-05、H-18、H-20、H-21、D-14

## 输入证据

- ADR-0007 已将 2048-EP Analytical traffic 与 Simulation flow provider 分离；ADR-0009 已固定非概率 A5 low/nominal/high 与 Robust A5 Candidate；ADR-0010 已固定 topology/resource slices、folded placement、GTS 与 HBM 门禁。
- 当前仍无 A5 真机、故障率/恢复曲线或可声称为 100k 实测的数据；goodput 参数由 #10 决定，Simulation smoke 规模与误差门由 #14 决定。
- 用户要求输出冠军之外的 Top-5，并以 Useful Throughput 为主目标；公开结果必须完整可追溯且脱敏。

## HITL 决策

1. 使用 `F0 static validation → F1 all-candidate Analytical → F2 promoted refined Analytical → F3 independent Simulation audit`，非法候选永不排名。
2. 每个切片晋级 Useful Throughput Top-20 加通信/HBM/recompute Pareto 边界、拐点与多样性代表，F2 上限 100；截断时披露覆盖率且不称全局最优。
3. 按 Useful Throughput 排名，以暴露通信、HBM 余量、重计算和配置 digest 确定性破同分；不完整时间只进 traffic-only；low/nominal/high 均在 Top-5 才称 Robust。
4. topology × active/resource × A5 bundle 独立输出 Top-5；另标最高吞吐、最低通信、最高 fault goodput 与最佳 Robust，goodput 未闭合前为 UNKNOWN。
5. 小规模 Simulation 只审计，超过 #14 的独立门限即标 `SIMULATION_DISAGREEMENT` 并暂停配置族冠军结论；所有输入、拒绝、评分与排名均内容寻址。

## 结果

- 用户全部接受五项决策；形成 ADR-0011 与三个领域术语，并更新 Typical Configuration Set。
- 本轮没有运行远程机器、NPU、NS-3、真实性能测试或具体 100k 配置搜索。
- 本轮只关闭 Top-5 搜索票；下一 frontier 记录为 #10 故障 goodput，不提前解决第二张票。
