# C003：完成 #17 并重算 frontier

- **开始/结束：** 2026-08-18T12:55:58+08:00 / 2026-08-18T14:21:27+08:00
- **阶段：** INTEGRATE
- **动作类型：** INTEGRATE
- **关联验收/未知量：** AC-17

## 预注册

- **本轮 micro-goal：** 只完成 #17 的 TDD、双轴 review、latest-main 集成、最终验收、评论和关闭。
- **当前假设：** 首个 Ascend HCCL AllReduce Analytical slice 可在 CPU 环境完成，不需要 NPU。
- **已有证据：** #16 Shared Run Contract；父 #15；ADR-0001/0003/0005。
- **证据等级：** E1。
- **唯一主要变量：** HCCL collective cost provider 通过公开 seam 注入 Analytical。
- **预期观察：** 有效三层资源输出 timing/traffic/provenance；缺资源与域外输入 fail closed；legacy 保持兼容。
- **判别规则：** 任一 AC/review/final test/main/permalink/comment 门禁失败则保持 OPEN。
- **成本与风险：** 真实全量链接；不占用 NPU；严格阻断 evidence/readiness 伪就绪。
- **停止与回滚：** 集成失败保留 Issue 分支/worktree，不关闭。

## 执行

- **分支/worktree：** `codex/issue-17` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-17`。
- **提交：** `c1dfa5a`、`8e6afe8`。
- **review：** Standards 与 Spec 初审发现数值域、结构、逐字段证据门和 raw/model 一致性问题；修复后原审查 Agent 复审均 PASS。
- **最终验证：** 等价 CMake 源集合 61-source 完整 Clang 链接；严格新增模块编译；18/18 真实进程黑盒；JSON/digest/diff/敏感/#18 scope 扫描。
- **NPU：** 未使用；无锁等待。

## 结果

- **观察事实：** `origin/main@8e6afe8` 包含 #17；该 SHA 是远端 main 祖先；设计/测试/fixture permalink 均经 GitHub API 验证。
- **review 修复：** `duration < 2^63`、非法单位/公式/evidence conflict、共享 artifact loader 与 typed validators、consumed Profile evidence gate、raw timing/bandwidth 一致性。
- **错误签名：** 首轮 RED 为 profile-only Manifest 在 #16 基线 exit2；最终 18 个真实进程用例全部 PASS。
- **信息增量：** 首个 HCCL cost provider 已真实注入 Analytical，#18/#28 依赖解除。

## 结论

- **验收/交付更新：** AC-17 PASS；Issue #17 CLOSED；完成评论 `issuecomment-5324382311`。
- **Issue wall-clock：** 1h21m15s（12:55:58–14:17:13）。
- **Token：** 实施 Agent 无可靠可归属单一 JSONL，按规则为 N/A；两 review Agent 已知最新单快照子合计 input 7,206,432 / cached 6,841,088 / uncached 365,344 / output 26,388，不冒充 Issue 总量。
- **下一 micro-goal：** 主攻 #18；#19/#28 暂缓并保留在 frontier。
- **是否需决策：** 无。
