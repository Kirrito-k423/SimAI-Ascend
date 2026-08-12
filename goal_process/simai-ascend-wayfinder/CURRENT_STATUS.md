# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T10:54:34+08:00
- **状态：** 绿
- **阶段：** READY_FOR_HITL
- **截止时间：** 未设定
- **验收进度：** 3/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约，以及 Analytical-first Provider seam 均已形成决策。
- **当前主阻塞：** 10T-scale workload contract 的技术 probe 已完成，等待用户对 5 项语义做 HITL 评判；本票不闭合 AlltoAll、Accuracy Gate、并行 placement 或 A5 估算。
- **关键证据：** run C005；固定官方 64 shard、145,116 tensor 名称/dtype/storage shape 全量对账 0 mismatch；目标逐 tensor 与独立算术均为 8,414,884,746,526 logical params；7/7 场景通过。
- **已解决：** Analytical cost 与 Simulation flow 分离；null 保持 legacy；profile/GPU 冲突 fail-closed；prototype 试验元素不合入 `main`。
- **下一步：** 用户评判 5 项 workload contract；接受后将 ADR/账本结论合入 `main`，原型继续留在 throwaway 分支。
- **需要决策：** 是；全局 E/K 作用域、参数与 active 口径、GTS/token accounting、四层 schema、显存 fail-closed。

## 交付状态

- **代码：** 当前 `main@8c2e727`；C005 原型只进入 `prototype/target-10t-workload-contract`，用户评判前不合入 `main`。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0005、能力矩阵、Ground Truth 栈与 schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** C005 原型已实现；禁止把 TUI、fixture action 或 `PROTOTYPE_*` 引用直接当 production manifest。
- **实验：** 本地 CPU 计数与 validation 已完成；只读取官方 JSON header，未下载约 865GB 权重，不运行训练或远端测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 用户全部接受，形成 workload contract ADR 并关闭当前 HITL 票。
- **路径 B：** 用户拒绝任一语义，只修改该项 major variable 后重跑 7 场景与官方 baseline gate。
- **最晚决策点：** 当前停在 HITL，不推进下一张票，不把 prototype 合入 `main`。
