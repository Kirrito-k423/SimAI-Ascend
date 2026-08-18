# C004：完成 #18 并重算 frontier

- **开始/结束：** 2026-08-18T14:25:52+08:00 / 2026-08-18T16:50:13+08:00
- **阶段：** INTEGRATE
- **动作类型：** INTEGRATE
- **关联验收/未知量：** AC-18

## 预注册

- **本轮 micro-goal：** 只完成 #18 的 TDD、双轴 review、latest-main 集成、最终验收、评论和关闭。
- **当前假设：** 完整 HCCL Analytical collective 可在 #17 cost seam 上以 CPU 真实进程完成，不需要 NPU。
- **已有证据：** #16 Shared Run Contract、#17 HCCL AllReduce provider、ADR-0005/0007。
- **证据等级：** E1。
- **唯一主要变量：** 从单一 AllReduce 扩展到五类 collective 与分段模型/routing/adapter contract。
- **预期观察：** 五类 identity/payload/timing/traffic 正确，边界单调；缺 routing/topology/cost 可区分且 fail closed；legacy 保持兼容。
- **判别规则：** 任一 AC/review/final test/main/permalink/comment 门禁失败则保持 OPEN。
- **成本与风险：** 61-source 真实链接；不占用 NPU；防止 GPU fallback、解析漂移、资源无上界和 UNKNOWN 伪就绪。
- **停止与回滚：** 集成失败保留 Issue 分支/worktree，不关闭。

## 执行

- **分支/worktree：** `codex/issue-18` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-18`。
- **提交：** `d1b1ce5`、`c9bd2fb`、`bb7c904`、`c0ac23a`、`6e371d4`。
- **review：** Standards/Spec 四轮；依次发现并修复离散单调、metadata 豁免、多请求闭包、legacy A2AV、routing 上界、非 seekable 绕过与 12/13 列 parser 漂移；最终两轴 PASS。
- **最终验证：** 等价 CMake 源集合 61-source 完整 Clang 链接；核心/Workload 严格编译；46/46 真实进程黑盒；13 JSON/digest/diff/敏感/#19/#28 scope 扫描。
- **NPU：** 未使用；无锁等待。

## 结果

- **观察事实：** `origin/main@6e371d4` 包含 #18；该 SHA 是远端 main 祖先；设计/测试/fixture permalink 均经 GitHub API 验证。
- **关键保证：** 五类 HCCL cost、分段整数单调、显式 busbw adapter、可区分 readiness、单请求结果闭包、同流 1MiB routing 硬上界、共享 12/13 列 typed decoder。
- **错误签名：** 首轮 AllGather/RS/A2A/A2AV 基线 exit2；所有 review 反例最终具稳定 reject code 且定量结果 UNKNOWN。
- **信息增量：** #18 完整 HCCL Analytical collective 能力已建立，与 #19 完成后将解锁 #20/#21。

## 结论

- **验收/交付更新：** AC-18 PASS；Issue #18 CLOSED；完成评论 `issuecomment-5325842617`。
- **Issue wall-clock：** 2h22m22s（14:25:52–16:48:14）。
- **Token：** 实施与 review Agent 均无平台可可靠归属的单一快照，按规则为 N/A，未估算或累计。
- **下一 micro-goal：** 主攻 #19；#28 暂缓并保留在 frontier。
- **是否需决策：** 无。
