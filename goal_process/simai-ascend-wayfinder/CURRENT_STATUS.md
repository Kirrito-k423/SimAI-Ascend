# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T10:33:15+08:00
- **状态：** 绿
- **阶段：** RECON
- **截止时间：** 未设定
- **验收进度：** 3/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约，以及 Analytical-first Provider seam 均已形成决策。
- **当前主阻塞：** 10T-scale workload 仍缺可运行的逐 tensor counter、GTS hard gate、routing provenance 和显存事件 schema，也尚未得到用户 HITL 评判。
- **关键证据：** 固定官方 V4-Pro config/model/index 与 64 个 safetensors header；run C005；官方 baseline checkpoint 含 145,116 tensors、逻辑参数 1,598,837,347,742。
- **已解决：** Analytical cost 与 Simulation flow 分离；null 保持 legacy；profile/GPU 冲突 fail-closed；prototype 试验元素不合入 `main`。
- **下一步：** 在 throwaway 分支构建一命令 workload contract TUI，逐项展示逻辑参数/checkpoint storage、active 参数、GTS、routing 与显存事件的状态和 validation。
- **需要决策：** 无。

## 交付状态

- **代码：** 当前 `main@9ff8ff6`；C005 原型只进入新 throwaway 分支，用户评判前不合入 `main`。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0005、能力矩阵、Ground Truth 栈与 schema 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** C005 原型待实现；禁止把 TUI 或 derived fixture 直接当 production manifest。
- **实验：** 仅计划本地 CPU 计数与 validation；不下载约 865GB 权重，不运行训练或远端测试。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 若 compact tensor generator 精确复现官方 baseline 并产生目标 manifest，形成 E2 决策原型后等待用户 HITL。
- **路径 B：** 若无法复现 145,116 tensor baseline，停止目标外推，回到官方 tensor template 审计，保持 E1。
- **最晚决策点：** 本地原型或审计累计 60 分钟即停止；同一计数不一致无新增证据只重试一次。
