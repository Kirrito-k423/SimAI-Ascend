# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T16:38:16+08:00
- **状态：** 绿
- **阶段：** FRONTIER_RECORDED
- **截止时间：** 未设定
- **验收进度：** 4/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约、Analytical-first Provider seam，以及 10T-scale Workload Contract 均已形成决策。
- **当前主阻塞：** A5 输入与敏感性协议已关闭；100k topology/placement、Top-5/goodput 与 Simulation smoke 仍有开放票据。
- **关键证据：** run C008；SKU 分离、必填/条件字段、domain-keyed efficiency、low/nominal/high Sensitivity Envelope、局部 fail-closed 与 Robust A5 Candidate。
- **已解决：** 无真机 A5 只允许 USER_INPUT/VENDOR_SPEC/EXTRAPOLATED；物理峰值不冒充 HCCL 时间；可用 HBM 可作情景预算但不冒充 allocator 实测；三情景排名不称置信区间。
- **下一步：** 下一轮处理“定义 100k SuperPoD 拓扑与并行放置搜索空间”；其 Profile 与 workload 前置均已关闭，当前无 assignee。本轮不认领、不展开第二张票。
- **需要决策：** 否；本票五项 A5 决策已全部接受。

## 交付状态

- **代码：** C006 原型只保留在 `prototype/hierarchical-a2a-projection`；主线仅接收证据账本与已接受决策，不合入原型代码。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0009、能力矩阵、Ground Truth 栈、schema、Accuracy Gate 与 A5 敏感性决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** 本票只冻结 A5 输入与敏感性契约，不实现最终 Profile validator 或搜索器。
- **实验：** 本轮不运行远端、NPU、NS-3 或真实性能测试，也没有收到用户最终 A5 数值；不能生成 A5 性能结论。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 下一轮以最多 5 项一批冻结 1024 当前商品/8192 架构情景、98,304 active+spares/100,000 ragged、并行 folding、rank placement 与跨域 AlltoAll 边界。
- **路径 B：** 不把 8192 架构上限或历史路线图与当前 1024 商品 BOM 混为一个拓扑。
- **最晚决策点：** 本轮已关闭且只关闭 A5 Profile 一票；下一 frontier 已记录为“定义 100k SuperPoD 拓扑与并行放置搜索空间”。
