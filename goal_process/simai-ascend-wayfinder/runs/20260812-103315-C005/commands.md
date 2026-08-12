# C005 脱敏命令与边界

- 只读定位：审计固定官方 config、inference model、safetensors index/header，以及 AICB/SimAI workload header/consumer。
- 官方 tensor header：只对 64 个固定 checkpoint shard 使用 HTTP Range 读取 8-byte header length 和 JSON header，不下载 tensor data；公开记录固定 commit、source digest 与 aggregate，不记录本机临时路径。
- prototype：只在 `prototype/target-10t-workload-contract` 分支新增 clearly-marked throwaway pure counter/schema 与 TUI。
- 场景：官方 384/6 baseline、目标 2048/16、只改 TopK、GTS=500M 边界、GTS 超限、routing/memory inputs 缺失。
- 回归：不新增 production tests；运行 prototype scripted transcript、独立算术复核、JSON/schema invariants 与公开敏感信息扫描。
- 公开安全：不得包含私有主机、账号、凭据、绝对远端路径、原始日志或真实设备唯一标识。
