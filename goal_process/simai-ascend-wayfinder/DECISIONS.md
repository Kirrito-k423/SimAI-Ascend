# 决策记录

## D-001：公开仓只保存脱敏远程证据

- **时间：** 2026-08-11
- **背景证据：** `docs/adr/0003-publish-only-sanitized-calibration-evidence.md`
- **选项：** 公开原始日志 / 仅公开脱敏能力与聚合指标
- **决定：** 仅公开脱敏能力与聚合指标。
- **决定者：** 用户
- **影响：** 私有地址、账号、凭据和原始主机日志不得进入仓库或 issue。
- **回滚条件：** 不回滚；若需共享原始证据，必须使用独立受控渠道。

## D-002：取消本项目费用监控

- **时间：** 2026-08-11
- **背景证据：** Wayfinder 地图 Notes
- **选项：** 默认生成费用报告 / 不启用费用监控
- **决定：** 不启用费用监控，不更新 `RMB-Cost.md`。
- **决定者：** 用户
- **影响：** 长任务仍需有界停止，但不生成费用估算。
- **回滚条件：** 用户之后明确重新启用。

## D-003：A2 主校准、A3 留出验证、第二台 A2 仅作备份

- **时间：** 2026-08-11
- **背景证据：** `docs/research/2026-08-11-a2-a3-capability-matrix.md`；run C001 `metrics.json`
- **选项：** 任意机器混用 / 按资源空闲度与代际隔离角色
- **决定：** 8×910B2 空闲机作为首选校准；16-chip A3 空闲机作为跨代留出验证；8×910B3 忙机在负载释放和 toolkit 明确前仅作备份。
- **决定者：** 基于现场证据的工程决策
- **影响：** 先消除 A2 workload/采集问题，再在 A3 检验跨代迁移，避免首轮同时改变环境与设备代际。
- **回滚条件：** 首选机器不可用，或备份机释放资源并证明软件栈更适合目标 workload。

## D-004：共同源码契约、A2/A3 独立低层环境

- **时间：** 2026-08-11
- **背景证据：** `docs/research/2026-08-11-a2-a3-ground-truth-stack.md`；MindSpeed-LLM 26.1 官方配套与兼容表
- **选项：** 强求同一 wheel/toolkit / 固定同一源码与指标但按代际锁定兼容环境 / 两代完全不同训练栈
- **决定：** 两 lane 固定 MindSpeed-LLM 26.1、MindSpeed Core 26.1、Megatron `core_v0.12.1`、Python 3.10、PyTorch 2.7.1；A2 使用 TorchNPU 7.3.0+CANN 8.5，A3 使用 TorchNPU 26.1.0+CANN 9.1。
- **决定者：** 基于官方兼容矩阵的工程决策
- **影响：** 两代不共享 wheel、toolkit 或容器；现场全局包不进入 Ground Truth；A3 只在 A2 manifest 冻结后重放。
- **回滚条件：** A2 的 26.1 共同源码未通过 L0 时，仅 A2 回退到官方 2.3.0/CANN 8.5 商用栈，并把训练栈版本显式作为解释变量。

## D-005：官方 DeepSeek ST bootstrap 加目标语义 slice

- **时间：** 2026-08-11
- **背景证据：** MindSpeed-LLM 26.1 固定提交内的 8 NPU DeepSeek-V3 ST 脚本与 JSON 基线
- **选项：** 直接运行 2048 experts/TopK16 / 只跑微算子 / 先复现官方缩小基线再单变量推进目标语义
- **决定：** 先用官方 8 NPU、TP1/PP2/EP4、16E/TopK8、两个 active layer 的 ST shape 打通；随后推进至 4 active layers、32E/TopK16、expert width 3072、1 shared expert、MTP1、seq2048、GBS8 的 `GT-TARGET-SEMANTIC-v1`。
- **决定者：** 基于可复现性与目标语义覆盖的工程决策
- **影响：** slice 的 GTS 为 16,384，满足每 step≤500M；它不是 10T 模型，只校准结构同态的计算、路由、通信、显存和 step 契约。
- **回滚条件：** 任一 L0/L1/L2/L3 门禁失败即停止；不得同时改变软件版本和 shape。

## D-006：AICB 只做 workload 契约，Ground Truth 来自真实 Ascend 栈

- **时间：** 2026-08-11
- **背景证据：** Upstream AICB 固定提交中的 NCCL/CUDA applyer、AIOB/DeepGEMM 限制、MoE backward 与 dump schema 缺口
- **选项：** 直接把 AICB 当 Ascend 执行器 / 只把它作为 SimAI workload bridge
- **决定：** AICB 只生成并承载 SimAI workload/消息契约；计算时间、显存、路由直方图和 HCCL 曲线必须来自 MindSpeed Ground Truth 与 HCCL Test。
- **决定者：** 基于 Upstream 源码审计的工程决策
- **影响：** 进入 SimAI 前必须补齐 schema v2、backward MoE 回归与真实路由分布；禁用 CUDA-only AIOB 路径。
- **回滚条件：** 仅当上游提供并经现场验证的 Ascend physical applyer 与完整 schema 时重新评估。

## D-007：Profile、RawObservation 与 CostModel 三层独立版本化

- **时间：** 2026-08-11
- **背景证据：** `docs/research/2026-08-11-ascend-profile-hccl-schema.md`；固定 Upstream SimAI、HCCL Test/C API、CANN Runtime 与 Ascend 官方资料
- **选项：** 单一大 YAML / Profile 加内嵌曲线 / 三层独立资源
- **决定：** 使用 `simai.ascend.profile/v1alpha1`、`simai.ascend.observation/v1alpha1`、`simai.ascend.costmodel/v1alpha1` 三种可独立版本化、哈希和复核的资源；规范单位统一为 B、B/s、FLOP/s、ns。
- **决定者：** 基于原生接口与消费边界的工程决策
- **影响：** 硬件/软件/拓扑事实、不可变原始观测与派生拟合互不覆盖；每个量值保留 evidence class，且与现场 readiness 正交。
- **回滚条件：** 若 Provider seam 证明三种资源无法独立加载，优先增加 manifest 引用层，不把 raw 与 fit 合并。

## D-008：HCCL 成本只在精确 domain 内拟合，外推与 overlap 独立建模

- **时间：** 2026-08-11
- **背景证据：** HCCL Test 原生输出、HCCL C API count 语义、Upstream SimAI collective/overlap 源码审计
- **选项：** 单一 busbw 标量 / 只保留 derived 曲线 / raw-first 精确 domain 模型
- **决定：** 保留 AR/AG/RS/A2A/A2AV 的原始时间、原生算法带宽、消息语义、rank/topology/software fingerprint 和统计量；只在完全匹配 domain 内插值，跨 rank、拓扑、软件代际或 traffic pattern 必须生成带区间的 `EXTRAPOLATED` 模型；overlap 使用独立 L3 模型。
- **决定者：** 基于可校准性和 30% Accuracy Gate 的工程决策
- **影响：** A2AV exact counts 使用外部 artifact/hash 与摘要，避免把 O(P²) 矩阵内嵌进 schema；算法带宽不得冒充物理链路带宽。
- **回滚条件：** 只有真实目标 workload 证明更低维模型在留出域持续满足误差门槛，才允许添加经验证的简化视图，原始观测仍不删除。

## D-009：Ascend 输入显式选择，legacy GPU/NCCL 由 fail-closed adapter 隔离

- **时间：** 2026-08-11
- **背景证据：** Upstream SimAI `Common.hh`、`AstraParamParse.hh`、`calbusbw.cc`、`Layer.cc` 与 legacy CSV 行为
- **选项：** 复用并扩展 legacy CSV / 静默推断设备 / 显式 Ascend profile 加隔离 adapter
- **决定：** Ascend 路径必须显式传入 `--device-profile`；与 legacy GPU 参数冲突时失败。legacy 空格、未知 rank/default column、超域和单位歧义由独立 adapter 标为 `LEGACY_ASSUMED` 或失败，不改变现有 GPU/NCCL 路径。
- **决定者：** 基于向后兼容与错误可见性的工程决策
- **影响：** 不再让未知 Ascend 设备落到 NVIDIA/NONE 默认值，也不把 legacy 1/16-node 表静默用于 100k 域。
- **回滚条件：** 不回滚 fail-closed 原则；若 upstream 提供正式 provider ABI，则 adapter 迁移到 ABI 边界。

## D-010：Analytical cost 与 Simulation flow 使用正交 Provider seam

- **时间：** 2026-08-12
- **背景证据：** run C004；`prototype/ascend-provider-seam@3f31ca1`；`docs/adr/0005-separate-analytical-cost-from-simulation-flow.md`
- **选项：** 统一复用 MockNCCL / 在 workload 内散布 Ascend 分支 / Analytical cost 与 Simulation flow 分离
- **决定：** 首版采用 Analytical-first：入口显式解析 Profile/HCCL model，经 `Sys` 非 owning 注入 `CollectiveCostModel`，`Layer::compute_time()` 只做一次 dispatch；null 保持旧 GPU 路径。Simulation 使用独立 `CollectiveFlowProvider` capability，未实现前对 Ascend 明确 unsupported。
- **决定者：** 用户完成 prototype HITL 后全部接受
- **影响：** `--device-profile` 与 legacy `--gpu_type` 冲突时 fail-closed；workload schema 不引入硬件字段；TUI、fake `424242 ns`、stub profile 和全部 `Prototype` 类型不进入 `main`。
- **回滚条件：** Upstream 提供正式 provider ABI 时可迁移装配位置，但 cost/flow 职责分离、显式选择与 fail-closed 语义保持不变。

## D-011：10T Workload 使用四层资源与带 scope 的计数口径

- **时间：** 2026-08-12
- **背景证据：** run C005；`prototype/target-10t-workload-contract@8195c3c`；`docs/adr/0006-separate-target-workload-resources-and-counting-scopes.md`
- **选项：** 单一模型参数/YAML / 逐 tensor 模型加硬件假设 / content-addressed Model、Step、Routing、Memory 四层资源
- **决定：** 全局 E=2048/K=16 作用于 61 main+1 MTP MoE block，目标为 8,414,884,746,526 logical trainable params；active 参数必须带三种明确 scope；GTS=`micro batch×sequence×DP×GA` 且每 step≤500M；routing 外置，memory 在 precision/optimizer/placement/recompute/runtime 未绑定时保持 symbolic 和 UNKNOWN。
- **决定者：** 用户完成 prototype HITL 后全部接受 5 项决策
- **影响：** 4,486,847,493,752 B 量化 checkpoint storage 不能冒充训练显存；500M GTS 下 496B assignment slots 是 62 个 routed blocks 的配置上界，不是实测路由或网络流量；AICB/SimAI 只通过内容哈希连接四层资源。
- **回滚条件：** 固定官方模型源、tensor layout、MTP 结构或全局 expert 配置改变时重算精确数字；资源职责分离除非被 production 生命周期证据否定，否则不合并。

## D-012：Analytical AlltoAll 使用分层投影而非常驻 pair flows

- **时间：** 2026-08-12
- **背景证据：** run C006；`prototype/hierarchical-a2a-projection@691346f`；`docs/adr/0007-use-hierarchical-projection-for-analytical-alltoall.md`
- **选项：** 显式 pair flows / 对称折叠 / 代表流 / 保留守恒面的分层投影
- **决定：** Analytical 主表示采用 content-addressed `ProjectedA2ATraffic`，保留总量、逐 rank 收发、域对矩阵、拓扑资源负载和守恒证据；uniform 闭式为 O(P+D²+R)，arbitrary dense A2AV 只承诺同阶常驻状态而读取仍为 O(P²)；routing、topology 或匹配 cost model 缺失时返回不同 UNKNOWN；Simulation 使用独立 HCCL flow provider。
- **决定者：** 用户完成 prototype HITL 后全部接受 5 项决策
- **影响：** EP=2048 和 100k Analytical 搜索不再预先生成 O(P²) endpoint flow objects；A2AV 需要带哈希的 immutable counts artifact 或 routing stream；合成 capacity 不得产出 ns 或性能结论；aggregate 不进入 NS-3。
- **回滚条件：** 实测 HCCL 算法证明当前守恒面缺少必要充分统计时扩展 projection；不回滚 Analytical 与 Simulation 语义隔离。
