# C005：闭合 10T-scale Workload Schema 与参数及激活计数

- **开始/结束：** 2026-08-12T10:33:15+08:00 / 2026-08-12T10:54:34+08:00
- **阶段：** RECON → HYPOTHESIS → PROBE → HITL_ACCEPTED
- **动作类型：** PROBE
- **关联验收/未知量：** AC-04、H-10、H-11、H-12、D-09

## 预注册

- **本轮 micro-goal：** 在独立 throwaway 分支构建一个一命令、可交互的 Workload Contract 原型，用固定官方逐 tensor manifest 验证 Target 10T Workload 的模型身份、参数/active 口径、GTS hard gate、routing provenance 与 symbolic memory events 是否能同时闭合。
- **当前假设：** H-10、H-11、H-12。
- **已有证据：** 官方 V4-Pro commit `45040942eb0d1c4e29fa6b92a6195f110e9e7444` 的 config、inference model、index 与 64 个 safetensors header；Upstream AICB/SimAI workload 消费边界；ADR-0004。
- **证据等级：** E1；官方 header 已给真实 tensor shape/dtype，但尚无仓内可运行 target counter/schema 原型。
- **唯一主要变量：** 是否以逐 tensor logical manifest 为模型真值，并把 checkpoint storage、step/routing/memory execution resources 分离为正交视图。
- **预期观察：** 官方 baseline 精确复现 145,116 tensors、约 1.59884T logical params；目标在 61 主干+1 MTP block 全部扩展到 E=2048/K=16 后落在已接受 8.31–8.42T；GTS 超 500M 明确失败；缺 routing artifact 或 memory materialization inputs 明确 incomplete；TopK 只改变 active/routing，不改变 total params。
- **判别规则：** 若逐 tensor generator 与官方 baseline aggregate 完全一致，目标计数满足 ADR-0004，四类资源能独立变化且 validation fail-closed，则支持 H-10～H-12；若只能靠 1.6T/49B residual、总 bytes/param 或隐式默认补洞，则拒绝 schema 并停止扩写。
- **成本与风险：** 本地 CPU≤60分钟；官方文件仅 Range 读取 header，不下载权重；不使用远端/NPU；主要风险是 packed FP4 语义、量化 scale、MTP 权重归属或训练 dtype 被误合并。
- **停止与回滚：** prototype 仅提交到 `prototype/target-10t-workload-contract`，不合入 `main`；用户未评判前不关闭 HITL 票据；任何无法从固定 source 解释的 residual 都标 UNKNOWN 而非调数字。

## 执行

- **脱敏命令：** `commands.md`
- **配置/环境差异：** 本地 Python 标准库；无远端/NPU；只用 HTTP Range 读取固定官方 checkpoint JSON header，并对忽略 Range 或异常 header size 的响应立即失败。
- **代码差异：** throwaway 分支新增 `prototypes/target_10t_workload_contract/` 和一命令启动脚本；未修改 Upstream AICB 子模块或 production SimAI 路径。
- **日志/指标：** `metrics.json`；7 个 scripted scenarios；64 shard、145,116 tensor 逐项 shape/dtype 对账。

## 结果

- **观察事实：** 官方 checkpoint header 共 145,116 tensors、864,704,792,696 storage bytes；generator 对名称、dtype、storage shape 做全量对账，missing/extra/mismatch 均为 0。expert FP4 权重以 packed I8 shape 存储，每个 storage element 表示 2 个 logical parameters，另有独立量化 scale 与 hash routing table。
- **错误签名：** 无。
- **推断：** logical parameters、带 scope 的 active logical parameters、checkpoint auxiliary/storage、训练显存和 token accounting 必须独立计数；模型错误与 step 错误也必须正交，否则 501M GTS 会错误抹去模型身份。
- **证据等级变化：** H-10～H-12 由 E1 升至 E2；技术 probe 支持，用户已全部接受 5 项语义。
- **信息增量：** baseline 精确为 1,598,837,347,742 logical params；61 主干和 1 MTP block 使用全局 E=2048/K=16 后，target 精确为 8,414,884,746,526 logical params；独立算术与逐 tensor 枚举一致。active 分别为主干 blocks 88,950,053,982、含 IO 的 main forward 90,803,533,923、含 MTP 的 training graph 92,345,423,134。

## 结论

- **验收/交付更新：** D-09 DELIVERED；ADR-0006；AC-04 仍 IN_PROGRESS（本票不包含 AlltoAll 与 Accuracy Gate）。
- **预算变化：** 用户已明确关闭费用监控；本轮不使用远端算力。
- **下一 micro-goal：** 按地图依赖选择下一张未认领前沿票据；不把 TUI、fixture action 和 `PROTOTYPE_*` 引用合入 production 设计。
- **是否需决策：** 否；本票 5 项已全部接受。
