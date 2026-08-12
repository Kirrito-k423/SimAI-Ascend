# C006：验证 2048-EP AlltoAll 聚合模型的可扩展 seam

- **开始/结束：** 2026-08-12T11:29:46+08:00 / 2026-08-12T11:44:03+08:00
- **阶段：** RECON → HYPOTHESIS → PROBE → READY_FOR_HITL
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
- **配置/环境差异：** 本地 Python 3 标准库与系统 C++17；无远端/NPU/NS-3；合成 capacity 只用于逻辑等价。
- **代码差异：** throwaway 分支新增 pure projection logic、TUI、确定性 transcript 和真实 Upstream P=4 C++ probe；未修改 Upstream 或 production 文件。
- **日志/指标：** `metrics.json`；四场景六项守恒；三个缺失输入反例；P=4 C++ probe；EP=2048 与 ragged 100k 公式投影。

## 结果

- **观察事实：** Upstream P=4 probe 实际生成 12 个 unique directed flows、24 个 duplicated rank-view entries、36 个 unique `prev` integer references；每 flow 为 4096/4=1024 B，总 network bytes=12,288。Hierarchical projection 在均匀、热点、locality、ragged 四场景的 total/rank/domain/resource/bottleneck 六项均与 exact baseline 相同。
- **错误签名：** 对称折叠在 hotspot/ragged 失败；全局代表流除 uniform 外失败；这是预期反证而非实现错误。编译 Upstream 文件暴露三条既有 warning，本票不修改。
- **推断：** 当前生成阶段 flow objects O(P²)、`prev` refs O(P³)；主 Analytical seam 必须在 materialization 前做 projection。Uniform A2A 可闭式 O(P+D²+R)；arbitrary dense A2AV 只能承诺流式状态 O(P+D²+R)，读取时间仍 O(P²)。
- **证据等级变化：** H-13/H-14 由 E1 升至 E2，技术 probe 支持，等待用户 HITL 后才标 SUPPORTED。
- **信息增量：** P=2048 为 4,192,256 directed flows、8,581,548,032 个 `prev` refs，对应 projection 8,200 summary cells；100k/98 ragged domains 的 uniform 闭式 projection 为 419,208 cells，不枚举 9,999,900,000 pairs。以上是 record/cell count，不是内存或性能实测。

## 结论

- **验收/交付更新：** D-10 READY_FOR_HITL；AC-04 仍 IN_PROGRESS。
- **预算变化：** 用户已关闭费用监控；本轮不使用远端算力。
- **下一 micro-goal：** 用户评判 5 项 seam 决策；接受后仅把 ADR/账本结论合入 `main`，不合入 TUI/合成矩阵/capacity。
- **是否需决策：** 是；5 项，见本轮 HITL 请求。
