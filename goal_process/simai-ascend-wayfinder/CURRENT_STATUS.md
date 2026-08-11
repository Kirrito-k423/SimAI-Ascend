# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-11T22:16:21+08:00
- **状态：** 绿
- **阶段：** INTEGRATE
- **截止时间：** 未设定
- **验收进度：** 2/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 脱敏盘点、Ground Truth 栈，以及 Profile/RawObservation/CostModel 三层数据契约已经形成。
- **当前主阻塞：** 尚未用最小原型确认 Upstream SimAI 的 Ascend Provider seam 与既有 GPU/NCCL 路径兼容边界。
- **关键证据：** `docs/research/2026-08-11-ascend-profile-hccl-schema.md`；run C003；Hardware Profile/HCCL Schema 研究票。
- **已解决：** schema、规范单位、证据/readiness、精确 domain、A2AV artifact、外推与 fail-closed legacy adapter 决策已冻结。
- **下一步：** 进入 Provider seam prototype 票；新会话再认领，本轮不提前启动。
- **需要决策：** 无。

## 交付状态

- **代码：** 本轮不改仿真代码；schema 决策包待本轮提交并推送至 `origin/main`。
- **文档：** 研究证据库、CONTEXT、ADR、A2/A3 能力矩阵、Ground Truth 栈与 Ascend schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C003 已收敛：Profile、immutable RawObservation、DerivedCostModel、证据/不确定性和 legacy adapter 契约。
- **实现：** 不适用
- **实验：** 仅环境 probe；未运行训练或性能测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** Provider seam 原型证明最小注入点可行且 GPU 回归通过，再进入正式实现规格。
- **路径 B：** 若 seam 需要侵入核心调度，则先冻结额外接口与回归边界，不把 Ascend 特例散落到 legacy 路径。
- **最晚决策点：** Provider seam 未验证前，不开始批量实现 profile loader、HCCL model 或大规模配置搜索。
