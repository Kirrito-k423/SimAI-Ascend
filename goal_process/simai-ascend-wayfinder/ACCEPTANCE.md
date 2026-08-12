# 验收账本

| ID | 完成条件 | 状态 | 证据 | 最近更新 |
|---|---|---|---|---|
| AC-01 | Upstream SimAI 研究基线保留完整历史、固定子模块并已推送 | PASS | `0703bcf7c673a0a48a2a04d88a79199d6c076e83` | 2026-08-11 |
| AC-02 | 至少一台 A2 与一台 A3 形成脱敏硬件/软件/工具能力记录 | PASS | `docs/research/2026-08-11-a2-a3-capability-matrix.md`；run C001 `metrics.json` | 2026-08-11 |
| AC-03 | Hardware Profile、HCCL Cost Model 与 Provider seam 决策关闭 | PASS | Profile/HCCL schema 决策包；run C004；ADR-0005；prototype `3f31ca1`；用户 HITL 全部接受 | 2026-08-12 |
| AC-04 | 10T-scale workload、AlltoAll 与 30% Accuracy Gate 决策关闭 | IN_PROGRESS | 10T Workload Contract 已由 C005/ADR-0006 关闭；AlltoAll 已由 C006/ADR-0007 关闭；Accuracy Gate 待后续票据 | 2026-08-12 |
| AC-05 | 100k 拓扑、A5 估算、Top-5 搜索与 goodput 决策关闭 | NOT_STARTED | Wayfinder 子票据 | 2026-08-11 |
| AC-06 | 地图无必需开放子票据，Decisions-so-far 完整且可交给 `/to-spec` | NOT_STARTED | GitHub map | 2026-08-11 |
