# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T14:44:40+08:00
- **状态：** 绿
- **阶段：** FRONTIER_RECORDED
- **截止时间：** 未设定
- **验收进度：** 3/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约、Analytical-first Provider seam，以及 10T-scale Workload Contract 均已形成决策。
- **当前主阻塞：** 10T-scale workload 与 2048-EP AlltoAll 已关闭；30% A2→A3 Accuracy Gate 仍未定义，因而还不能证明校准流程达到学习阶段门槛。
- **关键证据：** run C006；真实 Upstream P=4 flow probe；hierarchical projection 四场景六项守恒 4/4；uniform 闭式 2048/100k 不枚举 pairs；三类缺失输入 fail-closed。
- **已解决：** Analytical AlltoAll 使用 `ProjectedA2ATraffic`；uniform 闭式与 arbitrary A2AV 复杂度边界明确；缺 routing/topology/cost fail-closed；Simulation flow provider 独立；prototype 试验元素不合入 `main`。
- **下一步：** 下一轮处理 GitHub #13“定义 A2→A3 Exploration Accuracy Gate 的留出集”；它的 #2、#3、#8 前置均已关闭，当前无 assignee。本轮不认领、不展开第二张票。
- **需要决策：** 否；当前五项 AlltoAll 决策已全部接受。

## 交付状态

- **代码：** C006 原型只保留在 `prototype/hierarchical-a2a-projection`；主线仅接收证据账本与已接受决策，不合入原型代码。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0007、能力矩阵、Ground Truth 栈与 schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** C006 throwaway 原型已实现并完成 HITL；TUI、合成矩阵、固定 capacity 和 `PROTOTYPE_*` 引用不进入 `main` production 路径。
- **实验：** 本地 Python/C++ 守恒与 Upstream P=4 probe 已完成；不运行远端、NPU、NS-3 或真实性能测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 下一轮认领 #13，以不超过 5 个问题一批的 HITL 方式冻结 A2 校准集、A3 留出集、统计口径和 30% 判据。
- **路径 B：** 若 #13 开始前现场证据变化，先更新其输入证据，但不把 A3 留出数据泄漏进 A2 拟合。
- **最晚决策点：** 本轮已关闭且只关闭 #11；下一 frontier 已记录为 #13。
