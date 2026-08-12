# 交付账本

| ID | 交付物 | 目标位置 | 状态 | 证据/版本 |
|---|---|---|---|---|
| D-01 | Wayfinder 决策地图 | GitHub issue map | DELIVERED | 地图标题“从 Upstream SimAI 打通 Ascend 并搜索 100k/10T 典型配置” |
| D-02 | 一手资料证据库 | `docs/research/2026-08-11-simai-ascend-evidence.md` | DELIVERED | `main@0703bcf` |
| D-03 | A2/A3 脱敏能力矩阵 | `docs/research/2026-08-11-a2-a3-capability-matrix.md` | DELIVERED | `ccf0cdd82a9484044869ad77d40786862bcded1b`；环境盘点票已关闭 |
| D-04 | Goal 过程证据 | `goal_process/simai-ascend-wayfinder/` | WIP | C001 已形成；Goal 后续继续追加 |
| D-05 | `/to-spec` 输入 | GitHub map + linked assets | NOT_STARTED | 待地图闭合 |
| D-06 | A2/A3 Ground Truth 栈与最小 DeepSeek MoE slice 决策包 | `docs/research/2026-08-11-a2-a3-ground-truth-stack.md` | DELIVERED | run C002；Ground Truth 研究票；官方固定提交与 L0–L3 契约 |
| D-07 | Ascend Hardware Profile 与 HCCL Cost Model Schema 决策包 | `docs/research/2026-08-11-ascend-profile-hccl-schema.md` | DELIVERED | run C003；Hardware Profile/HCCL Schema 研究票；三层资源与机器校验契约 |
| D-08 | Ascend Provider seam throwaway logic prototype | `prototype/ascend-provider-seam` 分支 + Provider seam 原型票 | DELIVERED | `3f31ca1`；6/6 场景、Analytical 链接与 Simulation 核心编译通过；用户 HITL 全部接受；ADR-0005 |
| D-09 | 10T-scale Workload Schema 与逐 tensor counter throwaway prototype | `prototype/target-10t-workload-contract` 分支 + workload schema 原型票 | DELIVERED | `8195c3c`；run C005；7/7 场景；官方 145,116 tensors 全量对账；用户全部接受；ADR-0006 |
| D-10 | 2048-EP AlltoAll 聚合 seam throwaway prototype | `prototype/hierarchical-a2a-projection` 分支 + AlltoAll seam 原型票 | WIP | run C006；待四类小规模守恒、2048 表示规模、fail-closed capability 与用户 HITL |
