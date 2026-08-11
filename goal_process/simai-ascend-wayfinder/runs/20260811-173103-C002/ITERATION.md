# C002：选择 A2/A3 Ground Truth 栈与最小 DeepSeek MoE slice

- **开始/结束：** 2026-08-11T17:31:03+08:00 / 2026-08-11T19:30:59+08:00
- **阶段：** RECON → HYPOTHESIS → INTEGRATE
- **动作类型：** READ
- **关联验收/未知量：** H-02、H-03、H-04、D-06

## 预注册

- **本轮 micro-goal：** 基于一手来源选择 A2 拟合/A3 留出的共同 Ground Truth 契约，明确训练栈、两代版本策略、最小 DeepSeek MoE shape、GEMM/MoE/AR/AG/RS/A2A/端到端 step 覆盖与后续实测停止条件。
- **当前假设：** H-03、H-04。
- **已有证据：** A2/A3 脱敏能力矩阵；Upstream SimAI/AICB 固定源码；官方 Ascend、PyTorch-NPU、MindSpeed/MindSpeed-LLM 与 HCCL 资料。
- **证据等级：** 环境存在性 E2；共同栈与 slice 选择 E0。
- **唯一主要变量：** 无；纯研究决策，不切换远端版本。
- **预期观察：** 形成候选兼容矩阵、明确推荐与备选、固定 shape 和测量清单，并逐项给出一手来源。
- **判别规则：** 只有官方文档/源码能证明设备代际支持、workload 形状可表达及测量面时才推荐；若两代无共同 wheel，则固定共同源码/配置契约和两个独立锁版环境；若单栈不含四类 collective，则采用训练栈加官方通信基准的组合 harness。
- **成本与风险：** 目标≤60分钟；不运行远端、不安装、不下载私有数据；主要风险是一手支持矩阵缺失或把营销代际误映射为具体 SKU。
- **停止与回滚：** 只生成一个公开 Markdown 决策包；不臆测 A3 SKU，不以全局包快照宣称 ABI 可用；证据不足的字段显式留给后续 probe。

## 执行

- **脱敏命令：** `commands.md`
- **配置/环境差异：** 以 C001 的 A2/CANN 8.5 与 A3/CANN 9.1 beta 现场事实为边界。
- **代码差异：** 新增 `docs/research/2026-08-11-a2-a3-ground-truth-stack.md`；更新 Goal 账本，不修改训练或仿真代码。
- **日志/指标：** 固定三个共同源码 commit、两套低层版本 lane、一个官方 bootstrap shape、一个目标语义 slice，以及 L0–L3 指标/停止字段。

## 结果

- **观察事实：** MindSpeed-LLM 26.1 官方兼容表同时允许 TorchNPU 7.3/26.1 和 CANN 8.5.X/9.1.X；安装指南列出 A2/A3 训练系列。固定提交含 8 NPU DeepSeek-V3 ST 与基线。HCCL Test 官方能力面覆盖 AR/AG/RS/A2A。AICB 当前执行与 compute profiler 绑定 NCCL/CUDA/DeepGEMM，且 workload metadata/backward MoE 存在缺口。
- **错误签名：** 无。
- **推断：** 共同源码/manifest/指标 schema 加两套独立兼容环境，比强求同一 wheel 更可复现；真实训练栈加 HCCL Test 才是完整 Ground Truth harness。官方仓库性能数值缺少现场 build/拓扑，只能作 repository baseline。
- **证据等级变化：** H-02 保持 E1 但由环境候选升级为官方兼容证据；H-03、H-04 从 E0 升至 E1并标记 SUPPORTED；现场可运行性仍为 `FIELD_UNVERIFIED`。
- **信息增量：** 消除了共同训练栈、版本策略、最小 slice、AICB 职责、采集层级与停止规则六类决策迷雾；保留 driver/ABI/工具可用性给后续实测。

## 结论

- **验收/交付更新：** D-06 DELIVERED；不直接增加 Goal 的 2/6 验收计数，因为尚未运行 A2/A3 L0–L3。
- **预算变化：** 未运行远端或训练；用户已明确关闭本项目费用监控。
- **下一 micro-goal：** 下一 Wayfinder session 研究 Hardware Profile、显存与 HCCL Cost Model schema；本 session 到此停止。
- **是否需决策：** 无；未新增用户决策，后续仍按 5 个一批呈现。
