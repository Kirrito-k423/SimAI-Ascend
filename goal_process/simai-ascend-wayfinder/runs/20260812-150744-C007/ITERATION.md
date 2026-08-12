# C007：定义 A2→A3 Exploration Accuracy Gate 的留出集

- **开始/结束：** 2026-08-12T14:48:00+08:00 / 2026-08-12T15:07:44+08:00
- **阶段：** RECON → GRILLING → HITL_ACCEPTED
- **动作类型：** DECISION
- **关联验收/未知量：** AC-04、H-15、H-16、D-11

## 输入证据

- A2 主校准为 8×910B2，A3 留出为 16-chip；两 lane 固定共同源码/shape/指标契约并使用各自兼容底层环境。
- `GT-TARGET-SEMANTIC-v1` 固定四个 active layers、32 routed experts、TopK16、expert width3072、MBS1、部分重计算；A2 为 TP1/PP2/EP4/DP4，A3 为 TP1/PP2/EP8/DP8。
- 既有统计规则为 CV≤10% 时 5 次 median，CV>10% 时扩到 10 次并报告 type7 P90；Exploration Accuracy Gate 上限为 30%。

## HITL 决策

1. A3 性能严格留出；冻结前只允许 L0 readiness/correctness，不允许性能数据调参。
2. A3 留出为 balanced(seq2048/GBS8/GA1)、communication(seq1024/GBS16/GA2)、long(seq4096/GBS8/GA1) 三类 case。
3. A2 校准为对应的 balanced(seq2048/GBS8/GA2)、communication(seq1024/GBS16/GA4)、long(seq4096/GBS8/GA2) 三类 case，另先过官方 bootstrap。
4. 逐 case 使用冻结 prediction 与 steady-state representative 计算 APE；CV≤10% 用 5 次 median，CV>10% 用总计 10 次 type7 P90。
5. 三个 A3 case 均 APE≤30% 才通过；INVALID_EXECUTION、BLOCKED_ENVIRONMENT 和揭盲后复验使用不同状态/身份。

## 结果

- 用户全部接受五项决策；形成 ADR-0008 与四个领域术语。
- 本轮没有连接远端、安装环境、运行 NPU 或产生 A2/A3 性能数据；这里只关闭验收契约，不宣称 Gate 已执行或通过。
- 下一步按 GitHub 原生依赖选择 frontier；本轮不解决第二张票。
