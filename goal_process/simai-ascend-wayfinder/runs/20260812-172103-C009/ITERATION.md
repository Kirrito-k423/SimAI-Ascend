# C009：定义 100k SuperPoD 拓扑与并行放置搜索空间

- **开始/结束：** 2026-08-12T16:52:00+08:00 / 2026-08-12T17:21:03+08:00
- **阶段：** RECON → GRILLING → HITL_ACCEPTED
- **动作类型：** DECISION
- **关联验收/未知量：** AC-05、H-19、H-20、D-13

## 输入证据

- 当前商品公开最大单域为 1,024 NPU；白皮书 8,192 是架构上限；历史路线图不是当前产品 BOM。
- 100,000 不能被 1,024、8,192 或 2,048 整除；98,304 同时可被三者整除。
- ADR-0007 已冻结跨域 Projected A2A Traffic；ADR-0009 已冻结无真机 A5 Sensitivity Envelope 与缺失字段规则。

## HITL 决策

1. 1,024 current-product 为主场景，8,192 architecture-limit 为独立敏感性场景，历史路线图不入当前排名。
2. 同时比较 98,304 active+1,696 spare、100,000 active ragged、100,352 product capacity with 352 spare 三类口径。
3. 使用 attention/MoE 双 folding 恒等式；98,304 搜索规则 powers-of-two/EP 因数；exact 100,000 无规则可行解时诚实失败，只有框架支持时开放 ragged lane。
4. 每个 grid 生成 flat/topology-aware、global/local EP 等成对 placements，并保存 rank/group 与跨域守恒证据。
5. MBS、GA、recompute 在 GTS≤500M、95% Scenario Usable HBM Budget 与 Useful Throughput 目标下搜索，所有合法性和成本依赖 fail-closed。

## 结果

- 用户全部接受五项决策；形成 ADR-0010 与四个领域术语。
- 本轮没有生成实际 100k 配置、A5 时间或真实性能；只关闭 topology 与 placement 搜索边界。
- 下一步按 GitHub 原生依赖选择 frontier；本轮不解决第二张票。
