# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T10:16:16+08:00
- **状态：** 绿
- **阶段：** RECON
- **截止时间：** 未设定
- **验收进度：** 3/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约，以及 Analytical-first Provider seam 均已形成决策。
- **当前主阻塞：** 无；本轮只收口 Provider seam，下一 frontier 需在新 Wayfinder 会话认领。
- **关键证据：** run C004 的 6/6 resolver 场景、Analytical 完整链接、Simulation 核心目标编译、prototype `3f31ca1` 与用户 HITL 全部接受。
- **已解决：** Analytical cost 与 Simulation flow 分离；null 保持 legacy；profile/GPU 冲突 fail-closed；prototype 试验元素不合入 `main`。
- **下一步：** GitHub 原生子票顺序中的首个未认领 frontier 是“闭合 10T-scale Workload Schema 与参数及激活计数”；另有“A5 Estimated Profile 输入与敏感性协议”和“Simulation smoke test 最小拓扑与验收”已解锁。
- **需要决策：** 无。

## 交付状态

- **代码：** `main` 只固化 ADR 与账本；throwaway prototype 固定于 `prototype/ascend-provider-seam@3f31ca1`，不合并。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0005、能力矩阵、Ground Truth 栈与 schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C004 已闭合 CLI、`Sys`、`Layer::compute_time()` 与 `MockNcclGroup` 的职责边界。
- **实现：** C004 throwaway 原型已交付并经用户评判；禁止把 TUI 或 stub 合入 `main`。
- **实验：** 本地 CPU 场景和编译探针已完成；未运行训练或远端测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 按子票顺序先处理 10T-scale Workload Schema，可解锁 AlltoAll、Accuracy Gate 与拓扑搜索，当前证据 E1。
- **路径 B：** 若该票已被并发认领，则处理 A5 Estimated Profile 输入协议或 Simulation smoke，当前证据 E1。
- **最晚决策点：** 新会话按原生依赖和 issue 顺序认领第一个未认领 frontier，不在本轮解决第二票。
