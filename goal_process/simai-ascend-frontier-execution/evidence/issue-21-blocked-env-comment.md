## BLOCKED_ENV 进展汇报（Issue 保持 OPEN）

### 当前结论
- Issue #21 尚未完成，状态为 `BLOCKED_ENV`，不满足关闭条件。
- 本地 Ground Truth contract、统计、异常分类、校准模型重读与诚实阻断路径已集成 `origin/main`：
  https://github.com/Kirrito-k423/SimAI-Ascend/commit/1fbfa8f59c1009a09a318d9d2a23e08755bd4cf2
- 未运行训练、未采样、未生成或冒充真实 A2 Ground Truth。

### 已集成的本地能力
- 冻结 balanced、communication-heavy、long-sequence 三场景 schema 与 E32/TopK16/width3072/MBS1/TP1/PP2/EP4/DP4 contract。
- 5 次采样、CV>10% 扩至 10 次、Type-7 P90；`uint64` 高端与 85% HBM 比较无溢出。
- OOM、HBM≥85%、rank loss、non-finite、token loss/replay、provenance drift 七类 `INVALID_ACCURACY_EXECUTION` 稳定分类且不进入拟合。
- `VALID`、`BLOCKED_ENV` 与七类 invalid 使用 exact discriminated schema；malformed 输入保持 `INVALID_INPUT`。
- Run/Result/Profile/workload/topology/EP subgroup/raw/model 内容寻址与逐字段 evidenceRef 闭包。
- `MEASURED/FIELD_VERIFIED` verified contract 可达；synthetic `USER_INPUT/FIELD_UNVERIFIED` 永远 `calibration_eligible=false`。
- DerivedCostModel 可由真实 `SimAI_analytical` 进程重读；synthetic known point 输出 `61943ns`，不代表真实 A2 校准结果。

### 外部阻断证据
- 三次连续脱敏诊断：`20:01:06`、`20:02:34`、`20:11:12`（Asia/Shanghai）。
- 可达环境枚举到 8 个 NPU device，整段诊断命令均通过 `/tmp/tsj-codex-running` flock 持锁，未观察到排队；中止长时间 import 后已验证锁释放。
- 目标冻结 lane 未同时成立：缺隔离 Python 3.10、PyTorch 2.7.1、匹配 TorchNPU 7.3.0；MindSpeed-LLM 目标 checkout 不存在；现有源码身份不完整或不干净。
- CANN 8.5/HCCL runtime/ABI 无法闭合，目标 HCCL 动态库身份不可核验；因此不能诚实执行 L0、BF16、8-rank domain formation 或三场景采样。
- 另一路环境记录为认证失败，已作为独立 retrospective attempt 记录；评论与仓库证据均不含主机、账号、地址、凭据或原始命令。
- 脱敏证据：
  https://github.com/Kirrito-k423/SimAI-Ascend/blob/1fbfa8f59c1009a09a318d9d2a23e08755bd4cf2/docs/evidence/issue-21-a2-blocked-env.json
- Probe contract：
  https://github.com/Kirrito-k423/SimAI-Ascend/blob/1fbfa8f59c1009a09a318d9d2a23e08755bd4cf2/docs/evidence/issue-21-a2-probe-contract.json

### 最低解除条件
1. 在隔离目录提供 Python 3.10 + PyTorch 2.7.1 + TorchNPU 7.3.0 的可导入、版本固定环境。
2. 提供 MindSpeed-LLM、MindSpeed、Megatron 三个冻结且 clean 的源码 checkout。
3. 提供与 CANN 8.5 匹配的 HCCL runtime/ABI，并记录脱敏 digest。
4. 通过带整段 flock 的 L0 import、BF16、8-rank topology/domain formation 验证。
5. 随后真实执行三场景 5/10 次采样，产出 step、peak HBM、HCCL RawObservation 与 DerivedCostModel，再由独立 A2 复验。

### 本地验证
- 正式 C++11、CMake glob/exclude 等价 62-source 完整链接：PASS。
- 主控二进制 SHA-256：`8c56740f1a8cb73ead72a579d3353d70150949f8d155018b7ec64e8a752ed712`。
- `A2GroundTruth` / `RunContract` strict 编译：PASS。
- 真实 `SimAI_analytical` contract 回归：108/108 PASS；#16–#19 回归保留。
- JSON/digest/membership/determinism、`git diff --check`、敏感信息与 #22/#23 scope 扫描：PASS。
- Standards review：本地代码 PASS，无 findings。
- Spec/Issue review：本地 contract PASS；整体 Issue 为 `BLOCKED_ENV`。

### 时间与 Token
- 开始：`2026-08-18T19:57:18+08:00`
- 本地集成/阻断确认：`2026-08-18T22:34:53+08:00`
- Wall-clock：`2h37m35s`
- Token：N/A；平台未暴露可可靠归属的单一快照，未估算或累计。
- NPU 锁：仅执行带整段 flock 的只读诊断；未运行训练或采样。
