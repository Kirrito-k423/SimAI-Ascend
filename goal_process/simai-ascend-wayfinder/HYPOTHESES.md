# 假设账本

| ID | 可证伪假设 | 等级 | 支持证据 | 反证/替代解释 | 下一判别动作 | 状态 |
|---|---|---|---|---|---|---|
| H-01 | 私有清单中至少一台 A2 和一台 A3 当前可通过密钥登录并完成只读盘点 | E2 | run C001：两台 A2 与一台 A3 均成功完成有界只读探测 | 仅代表采集时刻可达，不承诺持续可用 | 后续每个实验仍做单次 preflight | SUPPORTED |
| H-02 | A2/A3 至少有一种共同的 Ascend 训练或测量栈可支撑后续 Ground Truth slice | E1 | MindSpeed-LLM 26.1 官方表同时兼容 TorchNPU 7.3/26.1 与 CANN 8.5.X/9.1.X，且安装指南明确支持 A2/A3 | 具体 driver、wheel、ABI、16-rank 建域仍未现场验证 | 在两套隔离环境执行 L0；任一失败即停止 | SUPPORTED |
| H-03 | A2/A3 无需共享同一 wheel 集，只要固定共同源码/配置/指标契约和各自官方兼容环境即可形成可比较 Ground Truth | E1 | 固定 MindSpeed-LLM 26.1、MindSpeed Core 26.1、Megatron core_v0.12.1、shape manifest 与指标 schema；底层按官方兼容矩阵分 lane | 跨代算子/collective 实现差异仍可能超过 30%，必须留出验证 | A2 拟合后冻结参数，仅在 A3 重放并报告误差 | SUPPORTED |
| H-04 | 一个现有 Ascend Megatron/MindSpeed 训练栈加官方通信基准可覆盖 GEMM/MoE、AR/AG/RS/A2A 和端到端 step | E1 | 官方 DeepSeek-V3 ST 覆盖真实训练/Grouped GEMM/MoE/step；HCCL Test 官方支持 AR/AG/RS/A2A 及正确性/性能字段 | 具体产品上的工具可用性、消息曲线和端到端 trace 均未实测 | 按 L0→L1→L2→L3 门禁采集，失败不外推 | SUPPORTED |
