---
goal_id: simai-ascend-frontier-execution
title: 完成 SimAI-Ascend T01-T15 frontier 执行
status: READY
owner: 用户
executor: Frontier Execution Controller
created_at: 2026-08-18T11:43:16+08:00
deadline: 无固定期限；持续到终态
priority: P0
execution_skill: "$goal-execution"
process_dir: "goal_process/simai-ascend-frontier-execution"
---

# 完成 SimAI-Ascend T01-T15 frontier 执行

## 一句话目标

在 `Kirrito-k423/SimAI-Ascend` 中依赖有序地实施、验证、合入并关闭 GitHub Issues #16–#30，并以 `origin/main` 提交、黑盒验收、设计文档和 Issue 完成评论证明结果。

## 背景与价值

- **背景：** 父规格 #15 已将 SimAI-Ascend 拆分为 15 张 `ready-for-agent` 实施票。
- **业务价值：** 从 Upstream SimAI 历史出发交付可公开复现的 Ascend Vertical Slice、校准/验证、搜索、Simulation 审计和综合报告。
- **失败代价：** 绕过依赖、门禁或证据会产生不可复核的“完成”声明并污染后续票。

## 范围

### 必须完成

1. 每轮读取 #16–#30 当前状态并按正文 `## Blocked by` 计算 frontier。
2. 对每张 frontier Issue 使用独立 worktree、`codex/issue-N` 分支和全新子 Agent，按公开 seam 执行 TDD。
3. 每票交付实现、测试、脱敏示例/配置（如适用）和 `docs/design/issue-N-design.md`。
4. 每票通过 Standards 与 Spec 两轴 review，修复阻断项并重新验证。
5. 主控串行合入最新 `origin/main`、最终验收、push main、评论并关闭 Issue。
6. 全部关闭，或对所有剩余票给出真实外部阻断证据与恢复条件。

### 明确不做

- 不重新 triage、不重设计 blocking graph、不修改或关闭父 Issue #15。
- 不绕过依赖、不跳过 code-review、不把 BLOCKED/UNKNOWN 伪装为 PASS。
- 不提交远端连接信息、私有 IP、账号、凭据或原始私有日志。
- 不启用费用监控，不计算人民币或 API 费用。

## 基线与环境

- **仓库与基线提交：** `origin/main@d92814047b7e2b27722c96e5261db81e8a0a8731`（2026-08-18 C001）。
- **工作分支：** 每票 `codex/issue-N`，基于创建时最新 `origin/main`。
- **本地环境：** macOS，仓库主工作树干净；CPU 构建/测试可本地执行。
- **远端环境：** 仅在 Issue 验收需要时使用既有私有配置；共享 NPU 命令必须在目标远端通过 `flock -w 7200 /tmp/tsj-codex-running` 执行。
- **软件栈/硬件：** 由各 Issue 设计与验收确定；不得把环境阻断改写为功能结论。
- **已有成功基线：** 父规格与 ADR、四个 prototype 分支仅作决策证据，不能替代主线黑盒验收。

## 事实、假设与未知量

### 已知事实

- F-01：2026-08-18 C001 时 #16–#30 全部 OPEN；证据：`runs/20260818-C001/ITERATION.md`。
- F-02：#16 的 `Blocked by` 为 None，是当前唯一 frontier；证据同上。
- F-03：最高级产品验收 seam 是 Shared Run Contract；证据：父 Issue #15 与 `CONTEXT.md`。
- F-04：共享 main worktree 无改动，且没有既有 Issue worktree；证据：C001 基线命令。

### 待验证假设

- H-01：当前本地/CI 环境足以完成 #16 的真实 `SimAI_analytical` 黑盒构建与测试；等级 E1（已有上游构建入口与 prototype 证据）；反证为可复现的环境/工具链阻断。

### 关键未知量

- U-01：各后续票是否需要 NPU 实测及目标 lane 可用性；仅在该票进入 frontier 后按验收最低成本验证。
- U-02：平台是否暴露可归属到每个子 Agent 的 token_count；若无则按合同记录 N/A，禁止估算。

## 验收标准

| ID | 可观察的完成条件 | 必需证据 | 验收人 |
|---|---|---|---|
| AC-16..30 | 对应 Issue 已合入 `origin/main`、最终验收通过、设计文档/交付件 permalink 有效、完成评论成功且 Issue CLOSED | main SHA、测试记录、GitHub permalink、Issue 状态 | 主控/用户 |
| AC-FINAL | #16–#30 全部 CLOSED；或所有剩余票均有真实外部阻断证据及恢复条件 | 最终状态快照与汇总报告 | 用户 |

## 交付物

| ID | 交付物 | 目标位置 | 完成条件 |
|---|---|---|---|
| D-ISSUES | 15 张 Issue 的代码、测试、配置/示例 | `origin/main` | 各票 AC 全部有证据 |
| D-DESIGN | 每票最终实现设计文档 | `docs/design/issue-N-design.md` | 含 4+1、流程图、类图、运行流程、契约与验收映射 |
| D-REPORTS | 完成评论与最终索引 | GitHub Issues / Goal 过程目录 | 链接有效且不含敏感信息 |
| D-PROCESS | 执行账本与脱敏证据 | `goal_process/simai-ascend-frontier-execution/` | 可从状态、运行记录和 SHA 复核 |

## 里程碑

| ID | 里程碑 | 退出条件 | 目标时间 |
|---|---|---|---|
| M1 | 基线与首个 frontier | 规则、规格、Issue、ADR 已读；#16 worktree/Agent 已启动 | C001 |
| M2 | 依赖有序实施 | 每个关闭动作后重算 frontier，所有可执行票完成 | 条件化 |
| M3 | 集成与终态 | 全部关闭或所有剩余票真实阻断 | 条件化 |

## 预算

- **总墙钟时间：** 未设硬上限；持续至 Goal 终态。
- **代理执行时间：** 受平台 Goal 生命周期约束；每轮必须增加信息或交付。
- **算力：** CPU 按风险相称使用；NPU 仅按票验收需要并持有远端 flock。
- **费用上限：** 不适用；用户明确禁止费用监控和人民币/API 费用计算。
- **高成本实验上限：** 每次需 E2 证据；无明确收益不得执行。
- **无新证据的同签名重跑上限：** 1。
- **无新依据的版本候选上限：** 2。
- **单次 NPU 锁等待：** 7200 秒；超时返回 `BLOCKED_NPU_BUSY`。
- **费用报告：** 不适用，按本 Goal 明确禁令覆盖 `$goal-execution` 默认条款。

## 权限边界

### 已授权

- 只读调查；创建 Issue worktree/分支；修改、测试、提交和 push `codex/issue-N`。
- 主控串行合入并 push `origin/main`；向 #16–#30 发布完成评论并关闭已满足门槛的票。
- 读取既有私有远程配置并执行脱敏验证；远端优先在 `/home/t00906153` 工作。

### 需审批

- 目标范围、验收、资源或权限的实质变化；外部依赖导致需要新资源或新凭据。

### 禁止

- 修改/关闭 #15；删除/reset/覆盖未知改动；无锁占用 NPU；泄露秘密；绕过 review/依赖/最终验收。

## 执行与证据规则

- 使用 `$goal-execution`；每轮一个 micro-goal，证据落盘且脱敏。
- 每票 red→green，一次一个公开可观察行为；refactor 放在 review 阶段。
- 每票两轴 code-review 使用两个相互独立的审查子 Agent。
- 任何完成声明必须能定位到测试、SHA、permalink 和 Issue 状态。

## 汇报

- **节奏：** 每个 frontier 变化、每票完成/阻断、Goal 终态；持续工具执行期间不超过 60 秒无更新。
- **对象：** 用户。
- **必答问题：** 完成了什么、当前 frontier/阻塞、证据、时间与 token、下一 micro-goal、是否需要决策。

## 停止与升级

- **DONE：** #16–#30 全部关闭且最终汇总完整。
- **BLOCKED：** 剩余开放票 frontier 为空，或所需外部环境/硬件/权限在有证据的恢复条件前不可用。
- **ESCALATE：** 同错误签名无新证据重复、NPU 锁超时、目标/验收需变更、出现不可安全解决的未知改动。
- **ROLLBACK：** 单票集成失败时保留分支/worktree，不关闭 Issue；使用可审计 revert，不丢弃其他票修改。
- **不得自行缩小：** 范围或验收变化必须由用户确认。

## 条件化 ETA

- **路径 A：** 若各票 CPU/依赖验证直接通过，按依赖 frontier 连续推进；E1。
- **路径 B：** 若出现 NPU、ABI、远端或严格 A3/A5 输入阻断，时间取决于恢复条件；E0。
- **最晚决策点：** 首次确认需要 Goal 外资源、权限或验收变更时立即升级。

## 待确认

- 无。用户已明确“现在开始”并授权合同中的执行流。

## 确认记录

- 2026-08-18T11:43:16+08:00：用户以 Goal objective 明确确认范围、终态、权限和执行规则，状态为 READY。
