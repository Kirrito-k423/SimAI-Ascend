# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-11T19:30:59+08:00
- **状态：** 绿
- **阶段：** RECON
- **截止时间：** 未设定
- **验收进度：** 2/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 脱敏盘点，以及 Ground Truth 共同源码/双 lane 版本契约和最小 DeepSeek MoE slice 已形成。
- **当前主阻塞：** 尚未定义可直接承接 L1/L2/L3 实测数据的 Ascend Hardware Profile、显存与 HCCL Cost Model schema。
- **关键证据：** `docs/research/2026-08-11-a2-a3-ground-truth-stack.md`；run C002；Ground Truth 研究票。
- **已解决：** AC-02 通过；首选 A2 校准机、A3 留出验证机和 A2 备份机角色已明确。
- **下一步：** 进入“定义证据化 Ascend Hardware Profile 与 HCCL Cost Model Schema”研究票；本轮不启动下一票。
- **需要决策：** 无。

## 交付状态

- **代码：** A2/A3 交付提交 `ccf0cdd82a9484044869ad77d40786862bcded1b`，已推送至 `origin/main`。
- **文档：** 研究证据库、CONTEXT、ADR 与 A2/A3 脱敏能力矩阵已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C002 已收敛：同一 MindSpeed-LLM 26.1 源码、A2/A3 独立兼容环境、官方 DeepSeek ST bootstrap、目标语义 slice 和 HCCL Test 组合 harness。
- **实现：** 不适用
- **实验：** 仅环境 probe；未运行训练或性能测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** 无；本轮只读研究，未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** Hardware Profile/HCCL schema 能完整承接 L1/L2/L3 字段后，才允许后续 A2 L0 安装与 ABI 验证。
- **路径 B：** 若具体 driver/toolkit/build 不通过 L0，停止在环境门禁并保留 `FIELD_UNVERIFIED`，不直接运行目标 slice。
- **最晚决策点：** schema 未冻结前不开始远端实测，避免采到无法进入 SimAI 的一次性指标。
