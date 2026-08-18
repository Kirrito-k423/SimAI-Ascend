# C006：#21 本地合同集成与 BLOCKED_ENV 固证

- **开始/结束：** 2026-08-18T19:57:18+08:00 / 2026-08-18T22:36:46+08:00
- **阶段：** EXECUTE
- **动作类型：** INTEGRATE + BLOCKED_ENV
- **关联验收/未知量：** AC-21

## 预注册

- **本轮 micro-goal：** 建立 A2 Ground Truth/校准合同并在真实 A2 上执行三冻结场景；若环境失败则以诚实 BLOCKED_ENV 和最低修复固证。
- **当前假设：** 至少一条脱敏 A2 环境可形成冻结 Python/TorchNPU/CANN/HCCL/source identity，并完成 8-rank L0 与三场景。
- **已有证据：** #18 HCCL Analytical、#19 Target contract、父 #15 与 ADR-0008。
- **证据等级：** 远端环境 E0；本地 contract E1。
- **唯一主要变量：** 从 synthetic contract 进入真实 A2 5/10 次 ground truth 与 calibrated model。
- **预期观察：** 三场景 step/HBM/HCCL raw 可采，invalid 不拟合，model 可由 Analytical 重读；否则返回可操作 BLOCKED_ENV。
- **判别规则：** 无真实 A2 samples/model 则 Issue 不得 PASS/CLOSE；本地代码需双轴 PASS 才可集成。
- **成本与风险：** 所有远端 NPU/驱动命令整段持 flock；禁止泄露连接信息和伪造 Ground Truth。
- **停止与回滚：** 环境阻断时保留脱敏 evidence、恢复条件和独立 worktree。

## 执行

- **分支/worktree：** `codex/issue-21` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-21`。
- **提交：** `7b593f4`、`237555c`、`393cadf`、`1fbfa8f`。
- **远端诊断：** 一条 A2 记录可达并枚举 8 device；另一条认证失败。三次可达诊断整段持 `/tmp/tsj-codex-running` flock；长 import 中止后锁探针立即重获。
- **review：** Standards/Spec 多轮；修复 verified dead gate、world8/EP4 semantic closure、state/schema、exact evidence、typed overflow/Type-7 uint64、Profile first-record 洗白和 C++11 compatibility；最终本地两轴 PASS。
- **最终验证：** 正式 C++11 62-source 完整链接；strict A2/RunContract；108/108 真实进程；JSON/digest/membership/determinism/diff/敏感/#22/#23 scope 扫描。
- **NPU：** 仅只读诊断；未运行训练/采样；未生成 Ground Truth。

## 结果

- **本地观察：** `origin/main@1fbfa8f` 包含安全的 GroundTruth contract、BLOCKED_ENV 与 synthetic unverified 路径；本地双轴 PASS。
- **外部观察：** A2 目标 runtime/source/ABI 无法闭合；缺真实三场景、5/10 样本、MEASURED raw、real DerivedCostModel 与独立 A2 复验。
- **关键保证：** synthetic 永不 eligible；verified 路径只有完整 MEASURED/FV/hardware evidence 与合法 world8/EP4 subgroup 才可 eligible；BLOCKED/invalid 无拟合。
- **信息增量：** #21 的阻断不影响 #20/#28 frontier，但继续阻塞 #22→#23。

## 结论

- **验收/交付更新：** AC-21=`BLOCKED_ENV`；Issue #21 保持 OPEN；阻断评论 `issuecomment-5329707796`。
- **Wall-clock：** 2h39m28s（19:57:18–22:36:46）。
- **Token：** 实施与 review Agent 均无可靠单一快照，按规则 N/A，未估算或累计。
- **下一 micro-goal：** 主攻 #20；#28 保留 frontier；#21 等待最低解除条件。
- **是否需决策：** D-007。
