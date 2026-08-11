# C004：验证 Ascend Provider seam 与 GPU 兼容边界

- **开始/结束：** 2026-08-11T22:59:16+08:00 / 等待用户 HITL
- **阶段：** RECON → HYPOTHESIS → PROBE → VERIFY → REVIEW
- **动作类型：** PROBE
- **关联验收/未知量：** AC-03、H-07、H-08、H-09、D-08

## 预注册

- **本轮 micro-goal：** 在独立 throwaway 分支构建一个一命令、可交互的 Provider selection 原型，并用实际 Upstream 构建边界验证：新增 Ascend Analytical cost model seam 时，旧 GPU workload、Analytical 与 Simulation 构建是否能保持不变；明确最终保留和删除的试验元素。
- **当前假设：** H-07、H-08、H-09。
- **已有证据：** 固定 Upstream `f5efb5a93ea9be7db25a8843f9f7ff54044f6062`；schema 决策包；`AstraParamParse.hh`、`AnalyticalAstra.cc`、`Sys.cc`、`Layer.cc`、`MockNcclGroup.cc` 源码。
- **证据等级：** E1；有集中调用点和两类后端源码，尚无可运行注入原型。
- **唯一主要变量：** 是否在通信时间计算前加入独立 `CollectiveCostModel` dispatch，并把 Simulation flow capability 作为正交 gate。
- **预期观察：** 无 profile 的 legacy NVIDIA 在 Analytical/Simulation 均沿原路径；Ascend profile+Analytical 选择 HCCL cost model；Ascend profile+Simulation 返回明确 unsupported；profile 与 legacy GPU 参数冲突失败；原型展示完整 state/resolution，且两个既有 backend 仍可构建。
- **判别规则：** 若上述五项成立且核心改动集中在入口装配、`Sys` 依赖与 `Layer` 单一 dispatch，则支持 H-07～H-09；若需要修改 workload CSV、散布 Ascend 分支或让 Simulation 复用 MockNCCL，则拒绝该 seam 并停止扩写。
- **成本与风险：** 本地 CPU≤45分钟；每个 backend 一次构建加一次有新证据的重试；不使用远端/NPU；主要风险是 Upstream 构建环境自身失败、prototype 误成 production patch、build script 生成大量未跟踪文件。
- **停止与回滚：** prototype 仅提交到 `prototype/ascend-provider-seam`，不合入 `main`；构建目录保持 ignored，出现 tracked 污染立即停止并恢复到干净分支；用户未评判前不关闭 HITL 票据。

## 执行

- **脱敏命令：** `commands.md`
- **配置/环境差异：** 本机缺少系统 CMake，使用任务级临时 Python/CMake 工具路径完成构建；NS3 必须使用仓内兼容的 Python 3.12，系统 Python 3.14 会在 configure 阶段触发 `argparse` 不兼容。
- **代码差异：** throwaway 分支新增纯 resolver、一命令 TUI 和 fake cost model；`Sys` 增加默认 null 的非 owning 注入点；`Layer::compute_time()` 增加单一可选 dispatch。没有修改 workload schema、legacy parser、`cal_busbw` 或 `MockNcclGroup`。
- **日志/指标：** `metrics.json`；6/6 预置状态场景符合判别规则；改动后的 Analytical 完整链接成功，Simulation `libapplications-obj` 编译成功。

## 结果

- **观察事实：** 无 profile 的两种 backend 均解析到 legacy provider；Ascend+Analytical 解析到独立 `HCCL_COST_MODEL`；Ascend+Simulation 在缺少 flow capability 时返回 `UNSUPPORTED_BACKEND`；profile 与 legacy GPU 参数冲突时返回 `CONFLICT`；future flow capability 可独立开启。真实 Upstream 编译单元接受 `Sys`/`Layer` 最小注入，且所有旧构造调用点继续依赖默认 null 参数。
- **错误签名：** 根构建包装器会尝试写 `/etc/astra-sim` 且系统缺少 CMake；改用内部构建入口后消除。完整 NS3 在 macOS arm64 最终链接阶段出现既有 `Ipv4Header`/`UdpHeader`/`MtpInterface` 未定义符号；该签名在 prototype 之前已稳定复现，改动涉及的 `libapplications-obj` 可单独成功编译，因此记为 Upstream/平台基线问题而非本 seam 回归。
- **推断：** `CollectiveCostModel` 可作为 Analytical 的最小硬件相关边界；Simulation 需要独立 `CollectiveFlowProvider`，首版不得借用 NVIDIA `MockNccl`。显式 profile resolver 应位于 backend 入口装配层，并对旧/新参数并用 fail-closed。
- **证据等级变化：** H-07～H-09 从 E1 提升为 E2（可运行状态探针 + 两 backend 核心编译证据）；设计决策仍等待用户 HITL，不视为最终冻结。
- **信息增量：** 证明了拆分 Analytical `CollectiveCostModel` 与 Simulation `CollectiveFlowProvider` 不只是源码推断，而能以最小注入通过两个既有 backend 的核心编译边界。

## 结论

- **验收/交付更新：** D-08 READY_FOR_HITL；AC-03 仍 IN_PROGRESS，用户评判前不关闭票据。
- **预算变化：** 用户已明确关闭费用监控；本轮不使用远端算力。
- **下一 micro-goal：** 用户运行一命令原型并评判 3 项边界；接受后只把设计决策写回 `main`，不合并 TUI、fake model 或 `Prototype` 类型。
- **是否需决策：** 是；Analytical-first、参数冲突策略、保留/删除清单共 3 项。
