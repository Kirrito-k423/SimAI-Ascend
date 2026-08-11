# 假设账本

| ID | 可证伪假设 | 等级 | 支持证据 | 反证/替代解释 | 下一判别动作 | 状态 |
|---|---|---|---|---|---|---|
| H-01 | 私有清单中至少一台 A2 和一台 A3 当前可通过密钥登录并完成只读盘点 | E2 | run C001：两台 A2 与一台 A3 均成功完成有界只读探测 | 仅代表采集时刻可达，不承诺持续可用 | 后续每个实验仍做单次 preflight | SUPPORTED |
| H-02 | A2/A3 至少有一种共同的 Ascend 训练或测量栈可支撑后续 Ground Truth slice | E1 | A2 主机与 A3 均见 PyTorch/`torch_npu` 家族和 `hccn_tool` | CANN、驱动与 Python 包版本跨度明显；尚未验证 import、ABI 或 workload | 用独立环境验证设备枚举、单算子和最小 DeepSeek MoE slice | OPEN |
