# C007：#20 Projected A2A 集成与 frontier 重算

- **开始/结束：** 2026-08-18T22:42:06+08:00 / 2026-08-19T00:06:48+08:00
- **阶段：** EXECUTE
- **动作类型：** IMPLEMENT + REVIEW + INTEGRATE
- **关联验收/未知量：** AC-20

## 预注册

- **本轮 micro-goal：** 在真实 Analytical provider 路径建立 Projected A2A，闭合小规模枚举、守恒、dense 输入和 100k 有界状态。
- **当前假设：** uniform 可用 `O(P + D² + R)` 统计态表达，dense 路由可由外部不可变 artifact 显式支付读取成本。
- **已有证据：** #18 HCCL Analytical、#19 Target contract、父 #15 与 ADR-0005/0006/0007。
- **证据等级：** E1，本地真实进程可验证。
- **唯一主要变量：** 投影策略从 uniform 扩展到 locality/hotspot，同时不创建 O(P²) resident flows。
- **预期观察：** 小规模与独立枚举一致；100k 在 60s/受控 RSS/Result 下完成；所有守恒与 readiness 闭合。
- **判别规则：** provider 必须真实消费；失败运行不得发布 READY 定量结果；双轴 review 均 PASS 后才可合入。
- **成本与风险：** CPU-only；严禁提前实现 #24 topology/placement、#28 flows 或 #29 smoke。
- **停止与回滚：** 任一 P1/P2 finding 返回同一实施 Agent 做 RED→GREEN；保留独立 worktree。

## 执行

- **分支/worktree：** `codex/issue-20` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-20`。
- **提交：** `3b41708`（实现）、`cd47c6a`（review 修复）。
- **实现：** typed projector 进入 `Layer -> HcclCostModel::Estimate`；Result 输出 global/rank/domain/matrix/resource/provenance/readiness；uniform 不物化 flows，dense artifact 有界加载并逐 cell 绑定。
- **review：** 首轮 Standards/Spec 独立发现同一 P1：第二个 request 失败后仍泄漏第一次 READY summary；真实进程 RED→GREEN 后双轴最终 PASS。
- **最终验证：** 正式 C++11 63-source 完整链接、strict 三模块、114/114 真实进程、独立 100k、24 JSON、digest/determinism/diff/敏感/scope。
- **NPU：** 不适用；未连接远端。

## 结果

- **主控观察：** `origin/main@cd47c6a4c3667751e2275b833972eabc1380ba43`；114/114 PASS；100k 单项 0.354s，外层 RSS 83,886,080 B，state units 110,002。
- **正确性：** uniform/locality/hotspot 独立枚举与六项守恒闭合；多请求失败定量投影为 UNKNOWN；#19 schema guard 与 legacy 回归保留。
- **资源性：** uniform represented pairs 9,999,900,000、materialized flows 0；Result 7,892,501 B。
- **信息增量：** #20 CLOSED 解锁 #24；实时 frontier 为 #24/#28，#21 仍 BLOCKED_ENV。

## 结论

- **验收/交付更新：** AC-20=`PASS`；完成评论 `issuecomment-5330829501`；Issue #20 CLOSED。
- **Wall-clock：** 1h24m42s（22:42:06–00:06:48，含实施、双轴 review、修复、主控集成与 frontier 重算）。
- **Token：** 实施与 review Agent 均无可靠单一快照，按规则 N/A，未估算或累计。
- **下一 micro-goal：** 主攻 #24；#28 保留 frontier；#21 等待最低解除条件。
- **是否需决策：** D-008。
