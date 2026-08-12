# C006：验证 2048-EP AlltoAll 聚合模型的可扩展 seam

- **开始/结束：** 2026-08-12T11:29:46+08:00 / 进行中
- **阶段：** RECON → HYPOTHESIS → PROBE
- **动作类型：** PROBE
- **关联验收/未知量：** AC-04、H-13、H-14、D-10

## 预注册

- **本轮 micro-goal：** 在独立 throwaway 分支构建一命令逻辑原型，判断哪种 A2A/A2AV 表示能在小规模对显式 pair-flow baseline 保持关键守恒量，同时避免 EP=2048 与 100k 候选搜索中的 O(P²) 常驻 flow objects。
- **当前假设：** H-13、H-14。
- **已有证据：** Upstream `MockNcclGroup::genAlltoallFlowModels`、`Sys::generate_collective_phase`、`NcclTreeFlowModel`；ADR-0005、ADR-0006；run C003 Profile/HCCL schema。
- **证据等级：** E1；真实源码边界已定位，但尚无同场景表示/守恒原型。
- **唯一主要变量：** A2A traffic 的中间表示：exact pair flows、symmetry fold、weighted representative flows、hierarchical resource projection。
- **预期观察：** exact baseline 为 P(P−1) directed flows；symmetry 只在均匀场景压缩且可能丢逐-rank热点；representative weighting 可守恒总 bytes 但不能普遍守恒资源瓶颈；hierarchical projection 以 O(P+D²+R) 状态保留逐-rank收发、域对矩阵和资源负载，在固定小规模 analytical functional 下与 exact baseline 等价。
- **判别规则：** 均匀、热点偏斜、强域内局部、ragged 域四场景必须逐项匹配 total bytes、rank egress/ingress、intra/inter-domain bytes、resource load vector 和 bottleneck time；任一缺失即不得作为主 Analytical 表示。真实时间、无匹配 cost model 或 Simulation queueing 不得由守恒等价性推出。
- **成本与风险：** 本地 CPU≤60分钟；不使用远端/NPU/NS-3；主要风险是把配置字节、实际 A2AV counts、网络传输 bytes 和并发完成时间混成一个口径。
- **停止与回滚：** 原型仅进入 `prototype/hierarchical-a2a-projection`；不改 Upstream/production 源码；用户未评判前不关闭 HITL 票；若 projection 只能守恒 total bytes，则拒绝并保持 E1。

## 执行

- **脱敏命令：** `commands.md`
- **配置/环境差异：** 待记录。
- **代码差异：** 待原型分支提交。
- **日志/指标：** 待记录。

## 结果

- **观察事实：** Upstream 对 P ranks 生成 P(P−1) 个 flow objects、每条 `chunksize=data_size/P`，且每条复制包含其余 P−1 ranks 的 `prev` vector；rank view 随后只保留 src/dst 涉及自己的 flows。
- **错误签名：** 暂无运行错误；发现表示复杂度风险。
- **推断：** 当前生成阶段 flow objects 为 O(P²)，`prev` integer references 最坏 O(P³)；必须在 flow materialization 前引入 Analytical projection seam，而不是事后压缩每-rank view。
- **证据等级变化：** H-13/H-14 保持 E1，待可运行对照。
- **信息增量：** P=2048 时为 4,192,256 directed flows；按当前实现每 flow 复制 P−1 个 ranks，仅 `prev` integer references 即 8,581,548,032 个，不把语言对象字节估算写成实测内存。

## 结论

- **验收/交付更新：** D-10 WIP；AC-04 仍 IN_PROGRESS。
- **预算变化：** 用户已关闭费用监控；本轮不使用远端算力。
- **下一 micro-goal：** 实现四表示、四场景与一组 2048-scale 公式投影。
- **是否需决策：** 原型完成后最多 5 项 HITL。
