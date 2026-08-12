# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T15:07:44+08:00
- **状态：** 绿
- **阶段：** HITL_ACCEPTED
- **截止时间：** 未设定
- **验收进度：** 4/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约、Analytical-first Provider seam，以及 10T-scale Workload Contract 均已形成决策。
- **当前主阻塞：** AC-04 所需 workload、AlltoAll 与 Accuracy Gate 决策均已关闭；A5 输入、100k topology/placement、Top-5/goodput 与 Simulation smoke 仍有开放票据。
- **关键证据：** run C007；A2 三类校准 case；A3 三类严格留出 case；5次 median/10次 P90；三个 case 各自 APE≤30%；INVALID/BLOCKED 状态隔离。
- **已解决：** A3 性能严格留出，冻结前只做 readiness；揭盲前冻结 revision/参数/manifest/预测/规则；失败不得用平均值掩盖；揭盲后修正只能进入第二轮复验。
- **下一步：** 关闭“定义 A2→A3 Exploration Accuracy Gate 的留出集”并更新地图，再按 GitHub 原生依赖只记录下一 frontier。
- **需要决策：** 否；本票五项 Accuracy Gate 决策已全部接受。

## 交付状态

- **代码：** C006 原型只保留在 `prototype/hierarchical-a2a-projection`；主线仅接收证据账本与已接受决策，不合入原型代码。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0008、能力矩阵、Ground Truth 栈、schema 与 Accuracy Gate 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** 本票只冻结验收契约，不实现或执行远端 calibration harness。
- **实验：** 本轮不运行远端、NPU、NS-3 或真实性能测试；Accuracy Gate 尚未执行，不能声称已通过实测。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 本票关闭后，首个无阻塞、未认领子票据成为下一轮 frontier。
- **路径 B：** 若新依赖改变 frontier，只记录原生依赖证据，不在本轮解决第二票。
- **最晚决策点：** 本轮最多关闭当前 Accuracy Gate 一票。
