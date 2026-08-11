# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T00:20:00+08:00
- **状态：** 绿
- **阶段：** REVIEW
- **截止时间：** 未设定
- **验收进度：** 2/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 脱敏盘点、Ground Truth 栈，以及 Profile/RawObservation/CostModel 三层数据契约已经形成。
- **当前主阻塞：** 技术探针已通过，只等待用户 HITL 评判；评判前不关闭 Provider seam 票据，也不把 throwaway code 合入 `main`。
- **关键证据：** run C004 的 6/6 resolver 场景、Analytical 完整链接和 Simulation `libapplications-obj` 编译均通过；完整 NS3 仍有 prototype 之前已存在的 macOS arm64 链接基线问题。
- **已解决：** schema、规范单位、证据/readiness、精确 domain、A2AV artifact、外推与 fail-closed legacy adapter 决策已冻结。
- **下一步：** 推送 throwaway 分支，用户运行一命令 TUI 并评判 Analytical-first、参数冲突和保留/删除清单。
- **需要决策：** 3 项，合并为一个不超过 5 项的 HITL 批次。

## 交付状态

- **代码：** `main@e55d982` 只含预注册；可运行原型只位于 `prototype/ascend-provider-seam`，不会直接合入 `main`。
- **文档：** 研究证据库、CONTEXT、ADR、A2/A3 能力矩阵、Ground Truth 栈与 Ascend schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C004 已定位 CLI、`Sys`、`Layer::compute_time()` 与 `MockNcclGroup` 四个候选 seam。
- **实现：** C004 原型已实现并通过本地验证；TUI、fake cost 和 `Prototype` 类型必须在正式实现前删除。
- **实验：** 本地 CPU 场景与编译探针已完成；未运行训练或远端测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 用户接受 3 项边界后，把 E2 seam 决策写入 `main` 并关闭本票；prototype 分支保留为证据，不合并。
- **路径 B：** 用户要求修改时，只在 throwaway 分支调整 resolver/dispatch，再交付下一轮 HITL。
- **最晚决策点：** 当前已到 HITL 决策点；没有用户评判就停止扩展本票。
