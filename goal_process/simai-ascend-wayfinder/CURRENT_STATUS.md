# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-11T22:59:16+08:00
- **状态：** 绿
- **阶段：** PROBE
- **截止时间：** 未设定
- **验收进度：** 2/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 脱敏盘点、Ground Truth 栈，以及 Profile/RawObservation/CostModel 三层数据契约已经形成。
- **当前主阻塞：** Provider seam 尚未通过可交互原型、Analytical/Simulation 构建和 legacy GPU 行为探针，也尚未得到用户 HITL 评判。
- **关键证据：** `Layer::compute_time()` 直接绑定 `GPUType/cal_busbw`；Simulation 另由 `MockNcclGroup` 生成流；run C004。
- **已解决：** schema、规范单位、证据/readiness、精确 domain、A2AV artifact、外推与 fail-closed legacy adapter 决策已冻结。
- **下一步：** 在 throwaway 分支实现 `CollectiveCostModel`/`CollectiveFlowProvider` 选择状态机和最小真实注入探针，形成一命令 TUI。
- **需要决策：** 无。

## 交付状态

- **代码：** 当前 `main@8f216a9b7767c900d0d1cfb657c415b7ebb5e72c`；原型将只进入独立 throwaway 分支。
- **文档：** 研究证据库、CONTEXT、ADR、A2/A3 能力矩阵、Ground Truth 栈与 Ascend schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C004 已定位 CLI、`Sys`、`Layer::compute_time()` 与 `MockNcclGroup` 四个候选 seam。
- **实现：** C004 原型待实现；禁止把 TUI 或 stub 合入 `main`。
- **实验：** 仅计划本地 CPU 构建与交互探针；不运行训练或远端测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 若单一 cost-model dispatch 加独立 flow-provider gate 可编译并保持 legacy 行为，等待用户确认后把 seam 写入决策包，证据目标 E2。
- **路径 B：** 若实际构建证明 `Layer/Sys` 注入仍侵入核心调度，停止扩写原型，回到接口边界设计，证据等级 E1。
- **最晚决策点：** 本地构建或原型累计 45 分钟即停止；同一稳定错误无新增证据只重试一次。
