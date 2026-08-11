# A2/A3 脱敏能力矩阵

> 采集时间：2026-08-11（Asia/Shanghai）
>
> 范围：三台当时可通过密钥登录的私有 Ascend 机器
>
> 方法：单次、有界、只读 SSH 探测；不安装软件、不启动服务、不修改远端环境

## 结论

- `A2-calibration-1` 作为首选校准机：8×910B2、每片 64 GiB HBM，卡均空闲，CANN 8.5.0、`msprof` 与 MindSpeed/Megatron 候选源码可见。
- `A3-validation-1` 作为跨代留出验证机：8 个逻辑 NPU、每个 2 chip（共 16 chip），每片 64 GiB HBM，卡均空闲，CANN 9.1.0-beta.1。板级只读查询只证明 `9382 / Ascend910 V1`；公开信息不足以推导更精确的营销 SKU。
- `A2-calibration-2` 作为备份机：8×910B3、每片 64 GiB HBM，但快照时 6/8 卡被推理负载占用，且未发现激活的 CANN toolkit。
- 三台机器的 `PATH` 中都未发现 HCCL Test；后续要么定位训练环境内已有二进制，要么在独立任务中受控构建。
- A2 主校准机与 A3 验证机都具备 PyTorch/`torch_npu` 家族的全局包，但版本跨度明显。包清单不证明可导入、ABI 兼容或 workload 可运行；共同 Ground Truth 栈仍须下一票用最小 DeepSeek MoE slice 验证。

## 硬件与系统

| 脱敏别名 | 预定角色 | CPU 架构 / OS | NPU 现场观测 | HBM | 采集时负载 | 主存 | 数据盘可用 |
|---|---|---|---|---|---|---|---|
| `A2-calibration-1` | 首选校准 | aarch64 / openEuler 22.03 | 8×910B2 | 64 GiB/片 | 8/8 空闲 | 约 1.96 TiB | 约 984 GiB |
| `A3-validation-1` | 跨代留出验证 | x86_64 / Ubuntu 22.04 | 8 个逻辑 NPU × 2 chip；板级查询为 `9382 / Ascend910 V1` | 64 GiB/chip | 16/16 chip 空闲 | 约 1.11 TiB | 约 452 GiB |
| `A2-calibration-2` | 备份 | aarch64 / Ubuntu 22.04 | 8×910B3 | 64 GiB/片 | 6/8 被推理负载占用，2/8 空闲 | 约 1.96 TiB | 约 36.8 GiB |

“A2/A3”是本项目用于区分校准代际的脱敏标签，不替代厂商 SKU。设备数量与型号来自 `npu-smi` 现场观测；A3 的 product 查询不受当前工具支持，因此不做超出证据的 SKU 映射。

## 软件、编译器与测量工具

| 脱敏别名 | 驱动 / `npu-smi` | 激活 CANN | Python / 编译器 | 全局 Python 包快照 | 测量与启动工具 | 候选训练源码 |
|---|---|---|---|---|---|---|
| `A2-calibration-1` | 25.3.rc1 | 8.5.0 | Python 3.13.11；GCC/G++ 10.3.1；CMake 3.22.0 | `torch 2.11.0`、`torch_npu 2.10.0`、`megatron-core 0.16.0`、`transformers 5.2.0`、`numpy 2.5.0` | `hccn_tool`、`msprof`、`torchrun` 可见；HCCL Test、`mpirun` 未见 | MindSpeed、Megatron-LM、Megatron-Bridge 及两个脱敏诊断工作区 basename 可见 |
| `A3-validation-1` | 26.0.rc1 | 9.1.0-beta.1 | Python 3.10.12；GCC/G++ 11.4.0；CMake 3.22.1 | `torch 2.13.0`、`torch_npu 2.7.1.post6.dev20260722`、`deepspeed 0.19.4`、`megatron-energon 7.4.0`、`transformers 5.9.0`、`numpy 2.2.6` | `hccn_tool`、`torchrun` 可见；`msprof`、HCCL Test、`mpirun` 未见 | 在限定搜索深度内未见目标仓 basename |
| `A2-calibration-2` | 26.0.rc1 | 未发现激活 toolkit | Python 3.10.12；GCC/G++ 11.4.0；CMake 3.22.1 | 过滤清单仅见 `numpy 2.2.6` | `hccn_tool` 可见；`msprof`、HCCL Test、`torchrun`、`mpirun` 未见 | 在限定搜索深度内未见目标仓 basename |

这里的“未见”仅表示本轮 `PATH` 或限定深度只读搜索没有找到，不等价于机器上绝对不存在。全局包版本来自包管理器快照，本轮没有执行 import、动态链接、分布式启动或训练验证。

## 跨代兼容性判断

| 能力面 | A2 主校准 | A3 留出验证 | 当前判断 | 下一判别动作 |
|---|---|---|---|---|
| 可用计算资源 | 8×910B2，全部空闲 | 16 chip，全部空闲 | 可进入最小 workload 探索 | 先单卡，再机内多卡 |
| CANN / 驱动 | CANN 8.5.0 / 25.3.rc1 | CANN 9.1.0-beta.1 / 26.0.rc1 | 形成有价值的跨代边界，但不是同构环境 | 固定两个独立可复现环境，不强求同一 wheel 集 |
| PyTorch/Ascend | 两类包均可见 | 两类包均可见 | 仅支持“框架家族相交”；版本与 ABI 可运行性未知 | 验证 import、设备枚举、单算子与最小 MoE slice |
| HCCL 测量 | `hccn_tool` 可见，HCCL Test 未见 | `hccn_tool` 可见，HCCL Test 未见 | 尚不能给出实测集合通信曲线 | 定位或构建 HCCL Test，再采集带宽/时延 |
| Profiling | `msprof` 可见 | `msprof` 未见 | A2 可先形成细粒度校准证据；A3 暂以运行时指标为主 | 明确 A3 profiler 来源或采用共同的聚合指标 |
| SimAI 适配精度 | 尚未运行 | 尚未运行 | 不能从盘点推断 | 后续以用户允许的 30% 误差门槛逐级校准 |

## 对后续工作的约束

1. Ground Truth 栈应先在 `A2-calibration-1` 验证，`A3-validation-1` 只在 workload 和采集口径稳定后做留出复现，避免把跨代差异混入首轮调试。
2. `A2-calibration-2` 在释放负载且补齐可运行 toolkit 前不进入主实验矩阵；其剩余磁盘也不足以承载大规模构建缓存。
3. 第一组通信校准必须显式记录 HCCL/CANN/驱动和拓扑，不能用理论端口带宽代替实测有效带宽。
4. 公开证据只保存脱敏别名、版本、形状、聚合指标与错误分类；地址、账号、认证材料、hostname、设备唯一标识和原始远端日志不入仓。
5. 本矩阵只证明“有可用 A2 与 A3 环境”，不证明 100k 拓扑、10T workload 或 A5 性能。这些结论必须由后续 Wayfinder 票据分别关闭。

## 复现与证据边界

本轮采用 `BatchMode`、8 秒连接超时、单次连接尝试和远端有界只读脚本。公开命令模板及结构化指标见：

- `goal_process/simai-ascend-wayfinder/runs/20260811-171036-C001/commands.md`
- `goal_process/simai-ascend-wayfinder/runs/20260811-171036-C001/metrics.json`

第三个私有 A3 端点在既有清单中已标记为 TCP/22 不可达，本轮依照停止规则没有重复探测。该状态不影响“至少一台 A2 与一台 A3 已形成能力记录”的当前验收，但仍限制可做的 A3 多机复现。
