# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T11:44:03+08:00
- **状态：** 绿
- **阶段：** READY_FOR_HITL
- **截止时间：** 未设定
- **验收进度：** 3/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约、Analytical-first Provider seam，以及 10T-scale Workload Contract 均已形成决策。
- **当前主阻塞：** 2048-EP AlltoAll 聚合技术 probe 已完成，等待用户对 5 项 seam/边界做 HITL 评判；它仍阻塞多保真 Top-5 搜索契约。
- **关键证据：** run C006；真实 Upstream P=4 flow probe；hierarchical projection 四场景六项守恒 4/4；uniform 闭式 2048/100k 不枚举 pairs；三类缺失输入 fail-closed。
- **已解决：** Analytical cost 与 Simulation flow 分离；null 保持 legacy；profile/GPU 冲突 fail-closed；prototype 试验元素不合入 `main`。
- **下一步：** 用户评判 5 项 AlltoAll projection；接受后形成 ADR，关闭当前 HITL 票并更新地图。
- **需要决策：** 是；主 representation、复杂度诚实边界、输入 artifact、cost/readiness 和 Simulation 隔离。

## 交付状态

- **代码：** 当前 `main@7901e05`；C006 原型只进入 `prototype/hierarchical-a2a-projection`，用户评判前不合入 `main`。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0006、能力矩阵、Ground Truth 栈与 schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** C006 throwaway 原型已实现；TUI、合成矩阵、固定 capacity 和 `PROTOTYPE_*` 引用不进入 `main` production 路径。
- **实验：** 本地 Python/C++ 守恒与 Upstream P=4 probe 已完成；不运行远端、NPU、NS-3 或真实性能测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 用户全部接受，形成 AlltoAll Analytical projection ADR 并关闭当前 HITL 票。
- **路径 B：** 用户拒绝任一语义，只修改该 major variable 后重跑 P=4 probe、四场景六项守恒与规模公式。
- **最晚决策点：** 当前停在 HITL；不推进下一票，不把 prototype 合入 `main`。
