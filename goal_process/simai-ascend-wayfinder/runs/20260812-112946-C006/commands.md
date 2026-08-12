# C006 脱敏命令与边界

- 只读源码审计：定位 Upstream AlltoAll workload parsing、`MockNcclGroup` flow generation、`Sys` collective assembly 和 `NcclTreeFlowModel` consumption。
- prototype：只在 `prototype/hierarchical-a2a-projection` 分支新增 pure projection logic 与 TUI。
- 场景：均匀、单 rank 热点、强域内局部性、ragged domains；另做 EP=2048 和 100k 表示规模公式检查。
- 守恒：total、per-rank egress/ingress、intra/inter domain、per-resource offered load、固定 analytical functional 的 bottleneck time。
- 边界：合成 capacity 只用于逻辑等价，不是硬件性能；缺 routing/topology/cost model 时输出 UNKNOWN；不把 aggregate projection 送入 NS-3。
- 公开安全：不得包含私有主机、账号、凭据、绝对远端路径、原始日志或设备唯一标识。
