# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-17T15:27:16+08:00
- **状态：** 完成
- **阶段：** READY_FOR_TO_SPEC
- **截止时间：** 未设定
- **验收进度：** 6/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream SimAI 基线、A2/A3 Ground Truth、Profile/HCCL schema、Analytical Provider seam、10T workload、2048-EP traffic、30% Accuracy Gate、A5 sensitivity、100k placement、Top-5、Fault Goodput 与 Simulation smoke 全部形成接受决策。
- **最后证据：** run C011；ADR-0012/0013；prototype `simulation-smoke-contract@b9c3297`；用户一次性接受剩余10项。
- **地图状态：** 没有剩余必要子票据或未具体化迷雾；关闭两张末票后即可关闭地图。
- **下一步：** 以 `TO-SPEC.md`、CONTEXT 和 ADR-0001～0013 为输入运行 `/to-spec`，把决策转成实现规格、阶段和测试。
- **需要决策：** 否。A5 数值、现场 ABI、HCCL 曲线、failure observations 与 exact-100k ragged support 是实现输入或实证门禁，不是未决产品选择。

## 交付状态

- **代码：** Wayfinder 不合入生产实现；四个 throwaway prototype 分支仅保留一手决策证据。
- **文档：** `CONTEXT.md`、ADR-0001～0013、三份研究证据、Goal 账本与 `TO-SPEC.md` 完整。
- **复现：** 公开仓只含脱敏版本、shape、聚合指标、状态与内容哈希，不含远程身份、认证信息或私有原始日志。
- **成本：** 用户明确关闭费用监控；未更新 `RMB-Cost.md`。

## 实施期已知门禁

- A2/A3 必须先通过 L0 环境与共同栈，再冻结预测并执行严格 A3 留出；三 case 逐项 APE≤30%。
- A5 无真机，只能消费用户/厂商输入和 EXTRAPOLATED sensitivity；缺字段逐消费点 UNKNOWN。
- exact 100,000 ragged 只有训练框架证明 group/shard/optimizer/recovery 语义后才进入 executable lane。
- Fault Goodput 在缺 MTBF/恢复/checkpoint 数字时只输出符号面和 break-even，不产生最高 goodput 代表。
- Simulation 先过 `FLOW_SMOKE_PASS`，再过逐 case≤30%的 F3；任何 disagreement 暂停相关冠军结论。
