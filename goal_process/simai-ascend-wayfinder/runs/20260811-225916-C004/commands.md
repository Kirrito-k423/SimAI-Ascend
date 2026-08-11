# C004 脱敏命令与边界

- 只读定位：`rg` 审计 `AstraParamParse`、Analytical/NS3 入口、`Sys` 装配、`Layer::compute_time()`、`MockNcclGroup` 与 CMake glob。
- baseline：使用仓内既有构建入口分别验证 Analytical 与 NS3/Simulation；记录命令、退出码、耗时和首个稳定错误，不保存机器唯一信息。
- prototype：只在 `prototype/ascend-provider-seam` 分支新增 clearly-marked throwaway TUI/pure resolver，并以最小实际注入检查接口可编译性。
- 场景：legacy Analytical、legacy Simulation、Ascend Analytical、Ascend Simulation、profile+legacy GPU 冲突；每次 action 后完整打印 state 与 resolution。
- 回归：不新增 production tests；只运行现有构建和公开示例/原型 scripted transcript。构建产物不提交。
- 公开安全：不得包含私有主机、账号、凭据、绝对远端路径、原始日志或真实设备唯一标识。
