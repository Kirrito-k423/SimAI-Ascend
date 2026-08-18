# C008：#24 topology/placement 集成与 frontier 重算

- **开始/结束：** 2026-08-19T00:12:26+08:00 / 2026-08-19T02:23:28+08:00
- **阶段：** EXECUTE
- **动作类型：** IMPLEMENT + REVIEW + INTEGRATE
- **关联验收/未知量：** AC-24

## 预注册

- **本轮 micro-goal：** 为 1024/8192 topology identity 和 98,304/100,000/100,352 资源场景建立可审计并行网格、候选放置与成对流量比较。
- **当前假设：** #19 target binding 与 #20 typed Projected A2A 可在不物化 rank map/endpoint flows 的前提下支持 100k placement summary。
- **已有证据：** #19 Target contract、#20 ProjectedA2A、父 #15 与 ADR-0009/0010/0011/0013。
- **证据等级：** E1，本地真实进程与独立 oracle 可验证。
- **唯一主要变量：** flat/random 与 topology-aware rank mapping 及 global/local EP candidate。
- **预期观察：** 两 topology 与三资源语义独立；完整 folded grid 与 evidence 闭合；100k 在受控状态/Result/RSS 内完成。
- **判别规则：** subject-bound evidence、完整 communication groups、同基准成对比较、fail-closed 状态均需双轴 PASS。
- **成本与风险：** CPU-only；禁止进入 #25 search、#28 flow 或 #29 smoke。
- **停止与回滚：** findings 回到同一实施 Agent RED→GREEN；双轴均 PASS 后才合入。

## 执行

- **分支/worktree：** `codex/issue-24` / `/Users/Zhuanz/work/github/SimAI-Ascend-worktrees/issue-24`。
- **提交：** `536853f`（实现）、`85f1bd6`（review 修复）。
- **实现：** 新增 typed `TopologyPlacement`；RunContract 提供内容寻址、专用 evidence、#19/#20 binding 和 Result seam；mixed-radix grids、PRP V2 与 candidate/pair summaries。
- **review：** 首轮两轴去重 6 项：evidence主体、folded grid、early readiness、random退化、reference exact schema、path TOCTOU；全部真实进程 RED→GREEN，最终两轴 PASS。
- **最终验证：** 64-source C++11 link、strict 2/2、130/130 真实进程、独立 rank/domain/group oracle、100k、JSON/digest/determinism/diff/敏感/scope。
- **NPU：** 不适用；未连接远端。

## 结果

- **主控观察：** `origin/main@85f1bd681c48a215480e9c7cb7b2402b48c43a5a`；130/130 PASS；100k exact/product 2.117s，外层 RSS 45,154,304 B。
- **正确性：** identity/evidence、三资源语义、十候选、完整 folded group membership、pair deltas 与独立 oracle 闭合。
- **资源性：** 10 candidates、96,040 matrix cells、单 projector 最大 2,048 rank states；rank maps/endpoint flows resident=0；Result 约 755 KiB。
- **信息增量：** #24 CLOSED；#25 仅余 #23，#29 仅余 #22/#28；当前唯一可执行 CPU frontier 为 #28。

## 结论

- **验收/交付更新：** AC-24=`PASS`；完成评论 `issuecomment-5332316044`；Issue #24 CLOSED。
- **Wall-clock：** 2h11m02s（00:12:26–02:23:28）。
- **Token：** 实施与 review Agent 均无可靠单一快照，按规则 N/A，未估算或累计。
- **下一 micro-goal：** 主攻 #28；#21 等待最低解除条件。
- **是否需决策：** D-009。
