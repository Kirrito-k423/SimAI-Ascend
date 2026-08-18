# Codex Session Profile Fallback — Issue #21 Rereview

- `fix_started`: `2026-08-18T21:03:54+0800`
- `fix_finished`: `2026-08-18T21:42:41+0800`
- `wall_seconds`: `2327`（38 分 47 秒）
- `token_snapshot`: `N/A`（当前执行环境未提供可验证的 token 用量快照）
- `profiled_at`: `2026-08-18T21:30:02+0800`
- `profile_tool`: `profile-codex-session` 仍不在当前可用 skill/工具清单；本文为真实 fallback，未伪造工具输出。
- `scope`: 双轴 review 的本地 findings；未连接远端或执行 NPU 操作。

## 耗时原因

1. verified dead gate 需要在兼容既有 v0.1 fixtures 的同时，引入严格、版本化的 v0.2 RawObservation/DerivedCostModel schema。
2. 原 4-rank TP worked example 与冻结 world8/TP1/PP2/EP4/DP4 不闭合；修复需要让 Profile、workload、Run/Result、raw/model subgroup 与 membership digest 同时一致。
3. exact schema、discriminated state 和 typed numeric findings 必须分别通过真实进程或 typed 编译 seam 建立 RED，再保持既有回归。

## 当前结论

- strict A2/RunContract 编译和 62-source 链接已通过。
- unverified synthetic 链保持 `calibration_eligible=false`；完整 verified 合同链真实进程正例已得到 `true`。
- 105 项真实进程回归、独立 digest/membership oracle、JSON/determinism/sensitive/scope 检查均通过。
- 远端环境结论未变化：仍为 `BLOCKED_ENV`，本轮没有重新探测、训练或采样。

## Final residual fix audit

- `fix_started`: `2026-08-18T21:57:05+0800`
- `fix_finished`: `2026-08-18T22:15:27+0800`
- `wall_seconds`: `1102`（18 分 22 秒，未超过 20 分钟，无需新增 profile fallback）
- `token_snapshot`: `N/A`（当前执行环境未提供可验证的 token 用量快照）
- `scope`: Profile consumed-evidence exact gate 与 Type-7 `uint64_t` 插值两个最终残余；未连接远端或执行 NPU 操作。
- `verification`: strict 双模块、62-source 链接、108 项全 suite、独立 digest/membership/Profile oracle、敏感与范围扫描均通过。
