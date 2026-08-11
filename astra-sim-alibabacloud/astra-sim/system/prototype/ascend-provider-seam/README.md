# PROTOTYPE：Ascend Provider Seam

这个 throwaway prototype 回答一个问题：能否在不改变无 profile 的 legacy GPU/NCCL 路径和 workload schema 的前提下，用显式 `AcceleratorProfile` 选择 Ascend，并把 Analytical 通信耗时与 Simulation 流模型拆成两个正交 capability？

它不是正式 Ascend 实现，不读取真实 Profile，不包含校准 HCCL 数值。TUI 中的 `424242 ns` 明确标为 `PROTOTYPE_FAKE_NOT_PERFORMANCE_DATA`，只证明接口 dispatch。

## 一条命令运行

```bash
./scripts/prototype-ascend-provider-seam.sh
```

脚本用系统 C++ 编译器在临时目录构建并启动 TUI，不写持久状态。每次动作都会重绘完整 state 与 resolution。

批量观察六个预置场景：

```bash
./scripts/prototype-ascend-provider-seam.sh --scenario all
```

## 预期评判点

1. 无 profile：Analytical 继续走 `cal_busbw`，Simulation 继续走 `MockNccl`，resolver 不重新解释 legacy GPU 参数。
2. Ascend+Analytical：选择 `HCCL_COST_MODEL`，`Layer::compute_time()` 仅增加一个可选 dispatch。
3. Ascend+Simulation：没有独立 `CollectiveFlowProvider` 时明确 `UNSUPPORTED_BACKEND`，绝不复用 MockNCCL。
4. `--device-profile` 与 legacy `--gpu_type` 同时出现时 fail-closed。
5. workload 仍只表达 op/group/ranks/bytes；硬件、证据和 cost model 继续由独立 schema 资源承载。

## 若结论成立，保留与删除

- **保留为设计契约：** `CollectiveCostRequest`/`CollectiveCostModel`、入口 resolver、`Sys` 非 owning 注入、`Layer` 单一 dispatch、独立 `CollectiveFlowProvider` capability gate。
- **删除：** TUI、fake `424242 ns` model、`Prototype` 后缀类型、交互 action、任何 stub profile。
- **不在本原型解决：** 正式 YAML loader、HCCL 插值/外推、Ascend Simulation flow、NS3 macOS 链接问题。
