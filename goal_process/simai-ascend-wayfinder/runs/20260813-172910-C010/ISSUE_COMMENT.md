已按用户 2026-08-13 的五项 HITL 确认关闭本决策：

1. F0 静态合法性 → F1 全量 Analytical → F2 晋级精算 → F3 独立 Simulation 审计；非法候选不排名。
2. 每切片晋级 Useful Throughput Top-20 加通信/HBM/recompute Pareto、拐点与多样性代表，F2 上限 100；截断披露覆盖率且不称全局最优。
3. Useful Throughput 为主排名；暴露通信、HBM 余量、重计算和 digest 确定性破同分；不完整时间只进 traffic-only；三档都在 Top-5 才称 Robust。
4. topology × active/resource × A5 bundle 独立输出 Top-5；最高 fault goodput 在 #10 输入闭合前保持 UNKNOWN。
5. 小规模 Simulation 只审计；超过 #14 门限标 `SIMULATION_DISAGREEMENT` 并暂停配置族冠军结论，不冒充 100k 实测或静默回填。

仓内证据：ADR-0011、CONTEXT 领域词汇、run C010 与 Goal 账本。下一 frontier 是 #10；本轮不解决第二张票。
