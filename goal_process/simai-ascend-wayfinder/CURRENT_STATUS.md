# 当前状态

- **Goal：** simai-ascend-wayfinder
- **更新时间：** 2026-08-12T17:21:03+08:00
- **状态：** 绿
- **阶段：** HITL_ACCEPTED
- **截止时间：** 未设定
- **验收进度：** 4/6

## 一分钟摘要

- **目标：** 关闭 Wayfinder 地图全部必要决策并形成 `/to-spec` 输入。
- **已完成：** Upstream 基线、A2/A3 环境与 Ground Truth 栈、三层 Profile/HCCL 契约、Analytical-first Provider seam，以及 10T-scale Workload Contract 均已形成决策。
- **当前主阻塞：** 100k topology/placement 已关闭；Top-5/goodput 与 Simulation smoke 仍有开放票据。
- **关键证据：** run C009；1,024/8,192 独立拓扑身份；98,304/100,000/100,352 三口径；attention/MoE folding；placement 对照；GTS/HBM fail-closed。
- **已解决：** 98,304 是规则 performance baseline 但非 100k active；exact 100k topology 不自动等于可执行 grid；超节点优势必须由相同 workload/rank count 的成对 placement 与可归因资源指标证明。
- **下一步：** 关闭“定义 100k SuperPoD 拓扑与并行放置搜索空间”并更新地图，再按 GitHub 原生依赖只记录下一 frontier。
- **需要决策：** 否；本票五项 topology/placement 决策已全部接受。

## 交付状态

- **代码：** C006 原型只保留在 `prototype/hierarchical-a2a-projection`；主线仅接收证据账本与已接受决策，不合入原型代码。
- **文档：** 研究证据库、CONTEXT、ADR-0001～0010、能力矩阵、Ground Truth 栈、schema、Accuracy Gate、A5 敏感性与 100k placement 决策包已形成。
- **复现：** 只发布脱敏探测命令模板，不发布地址和认证信息。
- **日志与报告：** `goal_process/simai-ascend-wayfinder/`。

## 时间与预算

- **环境：** 3 台可达机器完成只读盘点：A2×2、A3×1。
- **调研：** C005 已通过官方 Range header 审计区分 logical parameters、FP4 packed storage、quant scales 与 hash routing table。
- **实现：** 本票只冻结 topology/placement 搜索契约，不实现 100k rank-map generator 或搜索器。
- **实验：** 本轮不运行远端、NPU、NS-3 或真实性能测试，也不生成具体冠军配置。
- **文档与交付：** 1 份公开矩阵、1 份结构化指标、Goal 账本更新。
- **资源等待：** `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求按现有证据立即落盘；未占用远端算力。
- **剩余预算：** 未设置 token、时间、算力或重试预算；后续实验继续执行有界停止规则。
- **费用报告：** 用户明确要求本项目不启用费用监控；不更新 `RMB-Cost.md`。

## 条件化 ETA

- **路径 A：** 本票关闭后，首个无阻塞、未认领子票据成为下一轮 frontier。
- **路径 B：** 若新依赖改变 frontier，只记录原生依赖证据，不在本轮解决第二票。
- **最晚决策点：** 本轮最多关闭当前 100k placement 一票。
