# 假设账本

| ID | 可证伪假设 | 等级 | 支持证据 | 反证/替代解释 | 下一判别动作 | 状态 |
|---|---|---|---|---|---|---|
| H-01 | 私有清单中至少一台 A2 和一台 A3 当前可通过密钥登录并完成只读盘点 | E2 | run C001：两台 A2 与一台 A3 均成功完成有界只读探测 | 仅代表采集时刻可达，不承诺持续可用 | 后续每个实验仍做单次 preflight | SUPPORTED |
| H-02 | A2/A3 至少有一种共同的 Ascend 训练或测量栈可支撑后续 Ground Truth slice | E1 | MindSpeed-LLM 26.1 官方表同时兼容 TorchNPU 7.3/26.1 与 CANN 8.5.X/9.1.X，且安装指南明确支持 A2/A3 | 具体 driver、wheel、ABI、16-rank 建域仍未现场验证 | 在两套隔离环境执行 L0；任一失败即停止 | SUPPORTED |
| H-03 | A2/A3 无需共享同一 wheel 集，只要固定共同源码/配置/指标契约和各自官方兼容环境即可形成可比较 Ground Truth | E1 | 固定 MindSpeed-LLM 26.1、MindSpeed Core 26.1、Megatron core_v0.12.1、shape manifest 与指标 schema；底层按官方兼容矩阵分 lane | 跨代算子/collective 实现差异仍可能超过 30%，必须留出验证 | A2 拟合后冻结参数，仅在 A3 重放并报告误差 | SUPPORTED |
| H-04 | 一个现有 Ascend Megatron/MindSpeed 训练栈加官方通信基准可覆盖 GEMM/MoE、AR/AG/RS/A2A 和端到端 step | E1 | 官方 DeepSeek-V3 ST 覆盖真实训练/Grouped GEMM/MoE/step；HCCL Test 官方支持 AR/AG/RS/A2A 及正确性/性能字段 | 具体产品上的工具可用性、消息曲线和端到端 trace 均未实测 | 按 L0→L1→L2→L3 门禁采集，失败不外推 | SUPPORTED |
| H-05 | 一套 provenance-first schema 可同时表达 A2/A3 实测值与无真机 A5/950DT 的厂商规格、用户输入和派生区间，而不混淆证据身份 | E1 | 固定 Upstream 源码与官方接口支持三层资源；证据类别和现场 readiness 正交；A2/A3/A5 脱敏骨架通过 YAML 解析和不变量审计 | 真实设备字段仍可能要求代际 extension；当前 schema 只承诺最小共同核心 | 在 Provider seam 与首轮 L0/L1 采集中以 validator 验证扩展点，不兼容即升级 schema 版本 | SUPPORTED |
| H-06 | HCCL raw samples、显式消息语义/拓扑/软件身份与独立 derived fit 足以替代 legacy busbw 标量，并保持 GPU 路径向后兼容 | E1 | HCCL Test 原生时间/算法带宽、C API 消息语义和 SimAI 消费点已映射；raw immutable、fit 独立、A2AV 外置 artifact、legacy adapter fail-closed | overlap 与极端非均匀路由仍需 L3/目标 workload 验证，不能由 collective curve 自动推出 | Provider seam 原型先证明 Ascend 输入显式选择且 GPU legacy 回归不变；实测后只在精确 domain 内拟合 | SUPPORTED |
