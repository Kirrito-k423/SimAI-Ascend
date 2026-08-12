# C008：定义 A5 Estimated Profile 输入与敏感性协议

- **开始/结束：** 2026-08-12T16:18:00+08:00 / 2026-08-12T16:35:04+08:00
- **阶段：** RECON → GRILLING → HITL_ACCEPTED
- **动作类型：** DECISION
- **关联验收/未知量：** AC-05、H-17、H-18、D-12

## 输入证据

- 当前无 A5/950DT 真机；Profile schema 禁止 `hardwareAvailable=false` 的目标出现 MEASURED，允许 USER_INPUT/VENDOR_SPEC/EXTRAPOLATED 与 FIELD_UNVERIFIED。
- 950DT 官方白皮书给出多个 compute/memory SKU、物理峰值和 UB/UBoE/PCIe 端口复用；这些不是用户最终 BOM 或 workload HCCL 曲线。
- 用户只能提供 TFLOPS、内存、H2D 和互联等理论数据，要求以此估算而非实测校准。

## HITL 决策

1. A5 必须选择 SKU 或分离 SKU scenarios；固定 training-rank scope、dtype peak、HBM/带宽、scenario usable budget、使用到的 link/shared-resource 与每项 provenance。
2. H2D/D2H、功耗/温度按消费点条件必填；只有物理 link 时只能输出 traffic/load，不能输出 HCCL/step time。
3. A2/A3 只迁移按 op/dtype/shape、memory pattern、topology/traffic、phase 分域的无量纲效率与显式修正，全部 EXTRAPOLATED+FIELD_UNVERIFIED。
4. 同域有效样本≥5时 low/nominal/high 分别为 type7 P10/P50/P90；不足5个需用户给三点或 UNKNOWN；Sensitivity Envelope 不是 CI。
5. 缺字段逐消费点 fail-closed；三个情景分别 Top-5，仅三者均可行且均为 Top-5 的配置标 Robust A5 Candidate。

## 结果

- 用户全部接受五项决策；形成 ADR-0009 与三个领域术语。
- 本轮没有 A5 数值、远端/NPU 运行或性能输出；只关闭估算与敏感性协议。
- 下一步按 GitHub 原生依赖选择 frontier；本轮不解决第二张票。
