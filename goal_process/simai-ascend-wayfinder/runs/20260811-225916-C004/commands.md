# C004 脱敏命令与边界

- 只读定位：`rg` 审计 `AstraParamParse`、Analytical/NS3 入口、`Sys` 装配、`Layer::compute_time()`、`MockNcclGroup` 与 CMake glob。
- baseline：使用仓内既有构建入口分别验证 Analytical 与 NS3/Simulation；记录命令、退出码、耗时和首个稳定错误，不保存机器唯一信息。
- prototype：只在 `prototype/ascend-provider-seam` 分支新增 clearly-marked throwaway TUI/pure resolver，并以最小实际注入检查接口可编译性。
- 场景：legacy Analytical、legacy Simulation、Ascend Analytical、Ascend Simulation、profile+legacy GPU 冲突；每次 action 后完整打印 state 与 resolution。
- 回归：不新增 production tests；只运行现有构建和公开示例/原型 scripted transcript。构建产物不提交。
- 公开安全：不得包含私有主机、账号、凭据、绝对远端路径、原始日志或真实设备唯一标识。

## 实际验证命令

- 一命令交互原型：`./scripts/prototype-ascend-provider-seam.sh`
- 批量场景：`./scripts/prototype-ascend-provider-seam.sh --scenario all`
- Analytical：在任务级临时 CMake 环境中执行 `cmake --build astra-sim-alibabacloud/build/simai_analytical/build -j4`。
- Simulation 核心：把 canonical `astra-sim/` 同步到 ignored NS3 build copy 后，执行 `cmake --build <ignored-ns3-cache> --target libapplications-obj -j4`。
- 静态检查：`git diff --check`、prototype shell syntax、公开敏感信息扫描。

完整 NS3 最终链接不作为本 prototype 的通过条件：prototype 之前已在本机稳定触发 Upstream macOS arm64 未定义符号；本轮要求包含改动文件的 `libapplications-obj` 重新编译成功，并显式保留该基线限制。
