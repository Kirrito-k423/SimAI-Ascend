# C005：完成 #19 并重算 frontier

- **开始/结束：** 2026-08-18T16:54:58+08:00 / 2026-08-18T19:52:57+08:00
- **阶段：** INTEGRATE
- **动作类型：** INTEGRATE
- **关联验收/未知量：** AC-19

## 预注册

- **本轮 micro-goal：** 只完成 #19 的 TDD、双轴 review、latest-main 集成、最终验收、评论和关闭。
- **当前假设：** 10T Target Workload 的冻结模型/GTS/显存与四资源闭包可在 Analytical CPU 真实进程内完成，不需要 NPU。
- **已有证据：** #16 Shared Run Contract、#18 完整 HCCL Analytical、ADR-0002/0004/0005/0006/0007。
- **证据等级：** E1。
- **唯一主要变量：** 增加 Model/Step/Routing/Memory 内容寻址组合与 Target AICB event binding。
- **预期观察：** 10T 参数/GTS/HBM 边界可独立复算；未绑定量 UNKNOWN；四资源与每层 event 摘要闭合；#20 projected routing 被拒绝。
- **判别规则：** 任一 AC/review/final test/main/permalink/comment 门禁失败则保持 OPEN。
- **成本与风险：** 61-source 真实链接；不占用 NPU；防止 open schema、无界输入、evidence 伪就绪、整数走私和验证/执行输入脱节。
- **停止与回滚：** 集成失败保留 Issue 分支/worktree，不关闭。

## 执行

- **分支/worktree：** `codex/issue-19` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-19`。
- **提交：** `533c5fc`、`a23f28f`、`7934e4f`、`6a3ba91`。
- **review：** Standards/Spec 四轮；依次发现并修复 bounded I/O、exact schema/evidence、tensor source-of-truth、真实 AICB event binding、number lexeme、不可变 workload snapshot、18 列 customized 传播与未知策略静默降级；最终两轴 PASS。
- **最终验证：** 等价 CMake 源集合 61-source 完整 Clang 链接；`RunContract`/`Workload` 严格编译；84/84 真实进程黑盒；18 JSON、independent oracle、digest/diff/敏感/#20/#21/#28 scope 扫描。
- **NPU：** 未使用；无锁等待。

## 结果

- **观察事实：** `origin/main@6a3ba91` 包含 #19；远端 main 与 Issue commit SHA 一致；设计/测试/fixture permalink 均经 GitHub API 验证。
- **关键保证：** 76-entry canonical tensor registry、500M GTS、七类训练显存与 95%/85% 门、四资源+AICB 五摘要、唯一 evidenceRef、bounded/exact input、不可变执行 snapshot。
- **错误签名：** 首轮 Result 缺 Target；所有 review 反例最终具有稳定 reject code；验证/执行替换反例最终执行首次验证 bytes。
- **信息增量：** #19 完成后 #20/#21 均解除全部依赖；实时 frontier 扩展为 #20/#21/#28。

## 结论

- **验收/交付更新：** AC-19 PASS；Issue #19 CLOSED；完成评论 `issuecomment-5327782010`。
- **Issue wall-clock：** 2h57m02s（16:54:58–19:52:00）。
- **Token：** 实施与 review Agent 均无平台可可靠归属的单一快照，按规则为 N/A，未估算或累计。
- **下一 micro-goal：** 主攻 #21；#20/#28 暂缓并保留在 frontier。
- **是否需决策：** D-006。
