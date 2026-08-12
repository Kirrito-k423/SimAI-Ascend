# 验收账本

| ID | 完成条件 | 状态 | 证据 | 最近更新 |
|---|---|---|---|---|
| AC-01 | Upstream SimAI 研究基线保留完整历史、固定子模块并已推送 | PASS | `0703bcf7c673a0a48a2a04d88a79199d6c076e83` | 2026-08-11 |
| AC-02 | 至少一台 A2 与一台 A3 形成脱敏硬件/软件/工具能力记录 | PASS | `docs/research/2026-08-11-a2-a3-capability-matrix.md`；run C001 `metrics.json` | 2026-08-11 |
| AC-03 | Hardware Profile、HCCL Cost Model 与 Provider seam 决策关闭 | PASS | Profile/HCCL schema 决策包；run C004；ADR-0005；prototype `3f31ca1`；用户 HITL 全部接受 | 2026-08-12 |
| AC-04 | 10T-scale workload、AlltoAll 与 30% Accuracy Gate 决策关闭 | PASS | C005/ADR-0006；C006/ADR-0007；C007/ADR-0008；Accuracy Gate 三类严格 A3 留出与逐 case≤30% 规则由用户接受 | 2026-08-12 |
| AC-05 | 100k 拓扑、A5 估算、Top-5 搜索与 goodput 决策关闭 | IN_PROGRESS | A5 估算已由 C008/ADR-0009 关闭；100k topology/placement、Top-5 搜索与 goodput 待后续票据 | 2026-08-12 |
| AC-06 | 地图无必需开放子票据，Decisions-so-far 完整且可交给 `/to-spec` | NOT_STARTED | GitHub map | 2026-08-11 |
