# C004：验证 Ascend Provider seam 与 GPU 兼容边界

- **开始/结束：** 2026-08-11T22:59:16+08:00 / 进行中
- **阶段：** RECON → HYPOTHESIS → PROBE
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
- **配置/环境差异：** 待记录。
- **代码差异：** 待原型分支提交。
- **日志/指标：** 待记录。

## 结果

- **观察事实：** 待原型。
- **错误签名：** 无。
- **推断：** 待原型。
- **证据等级变化：** 待原型。
- **信息增量：** 已将原先单一 “provider” 问题拆成 Analytical `CollectiveCostModel` 与 Simulation `CollectiveFlowProvider` 两项正交 capability。

## 结论

- **验收/交付更新：** D-08 WIP；AC-03 仍 IN_PROGRESS。
- **预算变化：** 用户已明确关闭费用监控；本轮不使用远端算力。
- **下一 micro-goal：** 建立 baseline 构建证据并实现一命令 logic prototype。
- **是否需决策：** 原型完成后以不超过 5 项交给用户 HITL 评判。
