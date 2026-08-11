# A2/A3 Ground Truth 训练栈与最小 DeepSeek MoE Slice

> 研究日期：2026-08-11（Asia/Shanghai）  
> 对应决策票：[选择 A2/A3 Ground Truth 训练栈与最小 DeepSeek MoE Slice](https://github.com/Kirrito-k423/SimAI-Ascend/issues/3)  
> 方法：只读审计官方文档、官方仓库固定提交、发布说明、测试脚本与 CI 基线；本研究没有连接机器、安装软件、编译、训练或运行通信测试。

## 决策摘要

**主推荐：用同一套固定源码与测量契约、两套独立锁版环境，禁止复用现场全局 Python 包。**

- 共同源码固定为 [MindSpeed-LLM `v26.1.0`（`2b7130ca`）](https://github.com/Ascend/MindSpeed-LLM/tree/2b7130ca7bea7083a91ed66812eec95067d057a2)、[MindSpeed Core `v26.1.0_core_r0.12.1`（`81570f17`）](https://github.com/Ascend/MindSpeed/tree/81570f17ee091e783fa68428c04fa536da122dc1) 和 [Megatron-LM `core_v0.12.1`（`a845aa7e`）](https://github.com/NVIDIA/Megatron-LM/tree/a845aa7e12b3a117e24c2352b9e3e60bad2e3a17)，共同使用 Python 3.10 与 PyTorch 2.7.1。
- `A2-calibration-1` lane 锁为 TorchNPU 产品版本 7.3.0 + CANN 8.5.0；`A3-validation-1` lane 锁为 TorchNPU 26.1.0 + CANN 9.1.0。MindSpeed-LLM 26.1.0 的官方兼容表分别把 TorchNPU 7.3.0/26.1.0 和 CANN 8.5.X/9.1.X 标为兼容，因此可共享源码、shape manifest 和指标口径，但**不共享 wheel、toolkit 或容器**。[官方 26.1.0 配套与兼容表](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/release_notes_llm.md#L40-L138)
- 训练执行器是 MindSpeed-LLM + MindSpeed Core + Megatron；匹配 CANN 版本的 HCCL Test 是通信微基准。固定在 Upstream SimAI 中的 AICB 只做 **SimAI workload/消息契约生成器**，不是 Ascend 真实训练执行器，也不使用其 `--aiob_enable` 采集 Ascend 计算时间。
- A2 先拟合，A3 仅在 manifest、采集字段与停止规则冻结后作为跨代留出；A3 的公开硬件身份仍只写 `9382 / Ascend910 V1、16 chip`，不映射为更具体营销 SKU。

**回退：若 26.1.0 源码在 A2 的 TorchNPU 7.3.0/CANN 8.5 lane 未通过 L0，则仅把 A2 lane 回退到商业发布栈。**

- A2：MindSpeed-LLM [`v2.3.0`（`1bdd9700`）](https://github.com/Ascend/MindSpeed-LLM/tree/1bdd97003882083da7c951fceedd71f96116be2f) + [MindSpeed Core `2.3.0_core_r0.12.1`（固定 branch head `674226a`）](https://github.com/Ascend/MindSpeed/tree/674226a15a8214cc0c78f986b0b89fbe6b879ba1) + Megatron `core_v0.12.1` + Python 3.10 + PyTorch 2.7.1 + TorchNPU 7.3.0 + CANN 8.5.0；这是官方明确列出的 A2 商用配套。[2.3.0 版本表](https://github.com/Ascend/MindSpeed-LLM/blob/62d4640b977443db69121fcabf71ea40c98de063/README.md#L51-L64)
- A3：仍用 26.1.0 lane。两边继续共用 Megatron 固定版本、`GT-TARGET-SEMANTIC-v1` manifest 与指标 schema；报告中把训练栈版本作为解释变量，不能把跨栈误差归因于硬件。

上述都是**待现场验证的选择**，不是已运行结论。本票不证明探索精度门槛已经达到；最终门槛仍是 A2 拟合后 A3 steady-state step time 相对误差不超过 30%。

## 证据分级与现场边界

本文使用四种证据标签：

| 标签 | 含义 |
|---|---|
| `OFFICIAL_SUPPORT` | 厂商发布说明、安装指南或支持表明确列出 |
| `SOURCE_EXPRESSIBLE` | 固定提交中的参数、代码或示例可以表达，但不代表该组合运行过 |
| `REPOSITORY_BASELINE` | 官方仓库附带脚本与数值/CI 基线，但公开基线没有充分标注现场硬件与软件 build |
| `FIELD_UNVERIFIED` | 本研究未运行；现场全局包、驱动、ABI、算子、通信与性能均未验证 |

现场边界只继承公开的[脱敏能力矩阵](./2026-08-11-a2-a3-capability-matrix.md)：

| lane | 公开现场事实 | 与候选契约的差异 |
|---|---|---|
| A2 主校准 | 8×910B2、64 GiB/片、driver 25.3.rc1、CANN 8.5.0；全局 Python 3.13.11 / torch 2.11.0 / torch_npu 2.10.0 | CANN 与 A2 lane 相同；Python、torch、torch_npu 均不采用全局包；driver 与新隔离环境的兼容性未知 |
| A3 留出 | 16 chip、64 GiB/chip、driver 26.0.rc1、CANN 9.1.0-beta.1；板级只确认 `9382 / Ascend910 V1`；全局 Python 3.10.12 / torch 2.13.0 / torch_npu 2.7.1 开发包 | 目标是稳定 CANN 9.1.0 + TorchNPU 26.1.0；现场 beta toolkit 和开发 wheel 不能冒充稳定配套；营销 SKU 未知 |

## 为什么选这条路线

### 候选比较

| 候选 | 一手证据 | 能否做真实 Ground Truth | 决策 |
|---|---|---|---|
| MindSpeed-LLM + MindSpeed Core + Megatron Mcore | 26.1.0 是 2026 年 7 月正式版；版本表固定 Python/PyTorch/TorchNPU/CANN/Megatron，安装指南明确支持 Atlas A2/A3 训练系列；仓内有 DeepSeek-V3 8 NPU ST 脚本和 loss/time/memory 基线。[发布说明](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/release_notes_llm.md#L5-L52)；[硬件支持与源码安装](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/pytorch/training/install_guide.md#L5-L35) | 是；覆盖真实前反向、优化器、MoE/Grouped GEMM、HCCL 和 step | **主推荐** |
| AICB / AIOB | Upstream SimAI 明确把 AICB 定位为计算/通信 pattern 与 workload 生成组件，而 SimAI 执行仿真。[SimAI 组件边界](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/README_CN.md#L99-L134) | 当前 pin 的 physical applyer 和 AIOB 绑定 CUDA/NCCL/DeepGEMM，不能开箱即用地执行 Ascend 训练 | 只保留为 workload 契约来源 |
| MindSpeed-LLM 的 MindSpore 后端 | 固定 26.1.0 文档列出 DeepSeek-V3、MoE EP/分布式优化器；官方 8 NPU 示例可表达 4 层、64 experts、TopK8 与 AlltoAll-seq。[MindSpore 后端](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/mindspore/readme.md)；[8 NPU DeepSeek 示例](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/examples/mindspore/deepseek3/pretrain_deepseek3_1b_4k_8p_ms.sh) | 源码可表达，但现场未发现 MindSpore；会同时引入第二框架、第二算子栈和不同权重语义，难以归因 | 不做首选；仅在 PyTorch/TorchNPU 适配长期失败时用于隔离问题 |
| 原生 NVIDIA Megatron-LM | `core_v0.12.1` 提供 MoE/并行语义，是 MindSpeed 官方指定基础版本。[固定源码](https://github.com/NVIDIA/Megatron-LM/tree/a845aa7e12b3a117e24c2352b9e3e60bad2e3a17) | 原生执行路径不是 Ascend 适配层 | 作为共同语义底座，不单独执行 |

### 26.1.0 契约的版本锚点

| 组件 | 共同/分 lane 锁 | 官方支持 | 源码可表达 | 现场已验证 |
|---|---|---|---|---|
| Python | 两 lane 均 3.10 | 26.1 配套表明确 3.10 | 是 | 否；A2 全局为 3.13，必须隔离 |
| PyTorch | 两 lane 均 2.7.1 | 26.1 配套表明确 2.7.1 | 是 | 否 |
| MindSpeed-LLM | 两 lane 均 `2b7130ca` | 正式版 26.1.0 | DeepSeek/MoE/MTP/并行参数均可表达 | 否 |
| MindSpeed Core | 两 lane 均 `81570f17` | 官方指定 `26.1.0_core_r0.12.1` | Grouped GEMM、dispatcher、通信优化可表达 | 否 |
| Megatron-LM | 两 lane 均 `a845aa7e` | 官方指定 `core_v0.12.1` | TP/PP/DP/EP/CP 与 MoE 可表达 | 否 |
| TorchNPU | A2：7.3.0；A3：26.1.0 | MindSpeed-LLM 26.1 对二者均标 `Y`；A2 的 CANN 8.5/PyTorch 2.7.1 对应扩展 2.7.1.post2 与 branch `v2.7.1-7.3.0`。[TorchNPU 配套表](https://github.com/Ascend/pytorch/blob/90441fa551dc016f9f3c6eedb427ae6cfa83ed0c/README.md#L148-L168)；A3 安装示例 wheel 为 `torch_npu-2.7.1post8`。[26.1 安装指南](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/pytorch/training/install_guide.md#L160-L173) | 是 | 否 |
| CANN/HCCL | A2：8.5.0；A3：9.1.0 | MindSpeed-LLM 26.1 对 8.5.X/9.1.X 均标 `Y` | 是 | A2 toolkit 版本观测相同但未运行；A3 仅 beta.1，不等于稳定 9.1.0 |
| Driver/Firmware | 各 lane 记录现场精确 build，不由 manifest 猜测 | 安装指南要求按硬件选择配套驱动，但固定页未证明 25.3.rc1/26.0.rc1 与目标 lane 的组合 | 不适用 | 否；L0 blocker |
| Triton-Ascend | GT 首轮禁用非必要 Triton 特性 | 26.1 配套表写 3.2.2，但同版安装示例写 3.2.1，官方材料不一致。[配套表](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/release_notes_llm.md#L40-L52)；[安装示例](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/docs/zh/pytorch/training/install_guide.md#L175-L187) | 不作为首轮依赖 | 否 |
| HCCL Test | 各 lane 使用与 CANN toolkit 同源的源码/二进制并记录 commit/build | 官方文档提供工具、算子和指标，但现场 `PATH` 均未发现二进制 | AR/AG/RS/A2A 可表达 | 否 |

“兼容表为 `Y`”只说明官方发布关系，不证明现场驱动、OS、CPU 架构、wheel 或算子已通过。禁止原地升级系统栈；后续只能在隔离环境中做 L0，并保留可回滚的环境清单。

## 最小 Ground Truth 契约

### 先复现官方仓库基线，再进入目标语义 slice

26.1.0 仓内的 [DeepSeek-V3 ST 脚本](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/tests/st/shell_scripts/deepseek_v3_mcore_tp1_pp2_ep4.sh) 是最强起点：单机 8 NPU、TP1/PP2/EP4、`num-layers=4` 但 `noop-layers=2,3`（实际两个 active layers）、16 experts、TopK8、expert width 2048、一个 shared expert、MTP1、Grouped GEMM、`alltoall_seq`、BF16、seq4096、MBS1/GBS8、局部重计算。对应 [JSON 基线](https://github.com/Ascend/MindSpeed-LLM/blob/2b7130ca7bea7083a91ed66812eec95067d057a2/tests/st/baseline_results/deepseek_v3_mcore_tp1_pp2_ep4.json) 记录了 loss、稳定阶段约 2.81–2.83 秒 step 和最高 22,651 MiB（约 22.1 GiB）的 allocated memory。

公开基线没有标明具体 NPU 型号、driver/CANN build、拓扑，也引用内部数据、tokenizer 和 checkpoint，故只把它记为 `REPOSITORY_BASELINE`：后续先用等价公开/合成资产验证代码与 shape，不把仓内数值当作 A2/A3 阈值。

若 A2 触发版本回退，先复现 2.3.0 的官方 [8 NPU DeepSeek MoE pipeline case](https://github.com/Ascend/MindSpeed-LLM/blob/1bdd97003882083da7c951fceedd71f96116be2f/tests/pipeline/deepseek/deepseek2_tp1_pp1_mcore_moe.sh)；其 [基线](https://github.com/Ascend/MindSpeed-LLM/blob/1bdd97003882083da7c951fceedd71f96116be2f/tests/pipeline/baseline/deepseek2_tp1_pp1_mcore_moe.json) 记录约 4.35–4.57 秒稳定 step 与约 29.3 GiB 峰值，但同样不能直接当现场阈值。

### `GT-TARGET-SEMANTIC-v1`

在官方 26.1 ST shape 通过后，按单变量顺序取消 noop、改专家数、改 TopK、改 expert width、缩短序列并扩大 A3 EP；每一步都保留 manifest 和 stop reason。最终统一契约如下：

| 字段 | A2 主校准 | A3 留出 | 理由 |
|---|---:|---:|---|
| world size | 8 | 16 | 使用当时可用的全部 chip；A3 的 16 rank 可用性先由 L0 证明 |
| TP / PP / EP / CP | 1 / 2 / 4 / 1 | 1 / 2 / 8 / 1 | 保留 pipeline 与 expert parallel；不同 EP 用每 token/每 expert 归一化比较 |
| DP / gradient accumulation | 4 / 2 | 8 / 1 | `world/(TP×PP)`；两边保持相同 GBS 与 GTS |
| active transformer layers | 4：第一层 dense，随后 3 层 routed MoE | 同 A2 | `first-k-dense-replace=1`、`moe-layer-freq=1`；取消官方 ST 的 noop 后保留三个真实 MoE 层 |
| hidden / dense FFN / heads | 7168 / 18432 / 128 | 同 A2 | 保留 DeepSeek-V3/V4 类 trunk 的主 GEMM 形状 |
| MLA | q-lora 1536、kv-lora 512、qk-pos 64、qk 128、v-head 128 | 同 A2 | 保留官方 ST 的 MLA 结构与维度 |
| routed / shared experts | 32 / 1 | 32 / 1 | 32 是 `E > TopK16` 的最小实用稀疏选择；shared expert 数保持目标语义 |
| MoE TopK / expert width | 16 / 3072 | 16 / 3072 | 精确保留目标 TopK16 与 expert intermediate size 3072 |
| dispatcher / router | `alltoall_seq`；Grouped GEMM；确定性、可记录的平衡路由种子 | 同 A2 | 真实触发 EP A2A；记录而非假定 token 分布 |
| sequence / MBS / GBS | 2048 / 1 / 8 | 2048 / 1 / 8 | 每步 GTS=`2048×8=16,384`，远低于 500M；降低首次显存风险 |
| dtype | BF16，router FP32 | 同 A2 | HCCL 与训练脚本均有一手支持；首轮不启用 FP8/Triton |
| optimizer | Adam，β1=0.9、β2=0.999、weight decay 0.01，distributed optimizer，初始 loss scale 65536 | 同 A2 | 继承官方 ST 口径，覆盖 optimizer state 与 DP 通信 |
| recompute | `full + uniform + 1 layer` | 同 A2 | 部分重计算；后续显存/性能搜索可把它作为离散变量 |
| MTP | 1 layer，loss scale 0.3 | 同 A2 | 保留目标 MTP 深度语义 |
| data/checkpoint | 固定 seed 的合成 token，vocab 129280；随机初始化；不加载私有 checkpoint | 同一生成规则 | 消除数据与 checkpoint 隐私/可得性；只要求 finite loss 和稳定 step，不比对官方 loss |

显存可行性是**有根据的推断，不是实测**：官方 26.1 ST 在更长 seq4096 下报告最高约 22.1 GiB allocated；目标 slice 虽增加 active MoE 层、TopK 和 width，但用 seq2048、PP2 和仅 32 experts。A2 每 EP rank 8 experts、A3 每 EP rank 4 experts，64 GiB HBM 有测试空间；仍必须以 85% HBM 水位作为停止线，不能用该推断跳过 L1。

保留的目标语义：DeepSeek MLA trunk 主形状、一个 shared expert、三个 routed MoE layers、TopK16、expert width3072、MTP1、BF16、Grouped GEMM 和 A2A dispatcher。缩小的部分：总层数 4 而非完整 trunk、32 而非 2048 routed experts、EP4/8 而非目标级 EP、seq2048、GTS16,384、CP1；它不是 10T 模型，只是为 10T/2048E/TopK16 SimAI contract 提供可测的结构同态切片。

### AICB/SimAI 适配 seam

Upstream SimAI 固定的 AICB commit 是 [`23eec3c4`](https://github.com/aliyun/aicb/tree/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f)。它能读取 EP、expert 数和 TopK，并生成 AG/RS/A2A 等 MoE 消息，因此可作为 workload schema 的来源：[参数读取](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/SimAI_training_workload_generator.py#L89-L101)；[MoE 消息合成](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/SimAI_training_workload_generator.py#L433-L486)。在送入 SimAI 前必须补齐以下 P0 seam：

1. `WorkloadSchemaV2` 头部必须保存 `config_id`、完整模型 shape、experts、TopK、dtype、router/dispatcher、TP/PP/DP/EP/CP、GTS、重计算、软件 pin、rank mapping、路由直方图与消息语义；当前 dump header 只写少量并行字段，遗漏 experts/TopK/dtype/路由。[当前 dump](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/SimAI_training_workload_generator.py#L841-L868)
2. 固定 pin 的 `DeepSeekMoE.moe_mlp_backward()` 调用了 `permutation()`/`unpermutation()` 却没有把返回 workload `extend` 进结果，因此凡消费该方法的路径会丢 backward MoE communication；SimAI generator 的另一条 MoE 分支会自行合成 backward A2A，二者必须用回归测试对齐，不能笼统假定已经正确。[缺失的 `extend`](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/mocked_model/training/MockedDeepSeek.py#L354-L366)
3. 不使用 AICB physical applyer 采 Ground Truth：入口默认 `nccl`，applyer 直接调用 `torch.cuda.device_count/set_device/synchronize`。[默认 backend](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/aicb.py#L29-L33)；[CUDA device 路径](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_applyer.py#L35-L60)
4. 不在 Ascend 上启用 `--aiob_enable`：当前 `AiobDeepSeek.py` 明确依赖 `torch.cuda` 与只支持 SM90/SM100 的 DeepGEMM，且 DeepEP 仍是 TODO。[AIOB 限制](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/mocked_model/training/AiobDeepSeek.py#L1-L43)；[DeepGEMM import](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/mocked_model/training/AiobDeepSeek.py#L154-L168)
5. AICB 当前注释承认 token-to-expert 均匀分配是假设；Ground Truth 必须导出实际 route histogram、每 expert token P50/P90/max 与 dropped token，再决定 SimAI 使用均匀、直方图或偏斜场景。[均匀分配假设](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/mocked_model/training/MockedDeepSeek.py#L321-L350)

## 分级测量与停止规则

所有层级都记录 `config_id`、代码 commit、环境锁、driver/firmware/CANN/HCCL build、chip count、rank mapping、dtype、随机种子、开始/结束时间和 sanitized error class。任何层失败都停止后续层，不在一次 probe 中一边改版本一边改 shape。

### L0：import / device / ABI

| 项目 | 约定 |
|---|---|
| 动作 | 在隔离环境中导入 torch/TorchNPU/MindSpeed/Megatron；打印版本；枚举预期 rank；单卡执行小型 FP32/BF16 `mm` 与同步；先单进程，再 2 rank，最后全机 |
| 指标 | import/device-init 时间、可见 device/rank 数、算子返回码、finite/checksum、峰值 HBM、错误分类 |
| PASS | 两 lane 各自版本与 manifest 完全一致；A2 看到 8 rank、A3 看到预期 16 rank；BF16 mm finite；无 undefined symbol/ABI/device-init/HCCL 建域错误 |
| STOP | 版本漂移、driver/toolkit/so 冲突、A3 只能建立 8 而非 16 rank、任一进程挂起或错误；禁止原地升级，回到环境选择 |
| 送入 SimAI | `software_profile_id`、device count、dtype 可用性、初始化固定开销；不把初始化时间混入 steady-state compute |

### L1：GEMM / MoE kernel

| 项目 | 约定 |
|---|---|
| 动作 | 先跑官方 ST 的 dense/MLA/Grouped GEMM shape，再跑目标 slice；重点采主路径 7168→18432（回退基线 5120→12288）的 dense GEMM，以及 expert width 3072、`M={1024,2048,4096,8192}` 的 gate/up 与 down expert GEMM。统一 L3 每 optimizer step 的平均路由量为 `GTS×TopK/experts=8192` token assignments/expert，但单次 kernel 的 M 还受 EP、microbatch 与实际路由影响，必须记录真实分布，不能从均匀假设反推 |
| 指标 | kernel wall/device time P50/P90、TFLOPS、kernel/compile 首次开销、HBM allocated/reserved/peak、每 expert token histogram、dropped token、输出 finite/checksum |
| PASS | 官方 bootstrap shape 先通过；目标 shape 不 OOM、输出 finite、每个 routed expert 有可解释 token 数，HBM 峰值低于 85% |
| STOP | OOM、HBM≥85%、非 finite、路由严重偏斜且无法由 seed/config 解释、算子 fallback 或编译不收敛 |
| 送入 SimAI | `op_id,dtype,M,K,N,batch/expert_count,time_p50,time_p90,achieved_tflops,hbm_bytes,route_histogram` |

### L2：HCCL AR / AG / RS / A2A 扫描

CANN 8.5 和 9.1-beta.3 官方文档均把 HCCL Test 定位为基于 HCCL 单算子 API 的正确性与性能工具；参数页明确列出 `all_reduce_test`、`all_gather_test`、`reduce_scatter_test`、`alltoall_test`/`alltoallv_test`，支持 `-b/-e` 消息范围、`-i/-f` 步长、`-w` 预热、`-n` 迭代和 `-c` 正确性校验。[CANN 8.5 工具与产品边界](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/devaids/hccltool/HCCLpertest_16_0001.html)；[8.5 参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/devaids/hccltool/HCCLpertest_16_0005.html)；[9.1-beta.3 参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/devaids/hccltool/HCCLpertest_16_0005.html)。

| 项目 | 约定 |
|---|---|
| 动作 | 分别对 AR/AG/RS/A2A 做 BF16 扫描；覆盖 1 KiB–1 GiB 的 4× 递增点，并额外插入 L3 trace 中的精确消息尺寸；A2 rank size 2/4/8，A3 rank size 2/4/8/16；默认 warmup 10、迭代 20、正确性开启 |
| 指标 | 工具原生 `data_size`、`aveg_time/avg_time (us)`、`alg_bandwidth (GB/s)`、correctness，外加 rank size、算法/环境变量和 P50/P90；算法带宽不能写成物理链路带宽 |
| PASS | 每个目标 collective/rank/size 正确性通过，得到单调可解释的延迟—带宽曲线；精确 workload 消息点无缺口 |
| STOP | correctness failed、HCCL timeout/hang、rank 建域失败、工具不支持现场产品或目标 dtype；该点记 `UNSUPPORTED/UNKNOWN`，不得用理论峰值补齐 |
| 送入 SimAI | `collective,rank_count,bytes_per_rank,dtype,algorithm,time_us,alg_bw_gbps,topology_scope,software_profile_id` |

HCCL Test 是否能在两台**具体现场设备**上运行仍未知：盘点只证明 `PATH` 中未见二进制；8.5 文档的 A2 产品限制也不足以把脱敏 910B2 自动映射成被列出的营销产品。后续可定位 toolkit 内同源源码或受控构建，但本票不执行。官方还把 A2/A3 通用 collective 上限写为 32K rank、A2A/A2AV 为 8K rank；这些工具结果不能声称直接验证 100K rank。

### L3：端到端 steady-state step

| 项目 | 约定 |
|---|---|
| 动作 | `GT-BOOTSTRAP` 先过，再按单变量推进至 `GT-TARGET-SEMANTIC-v1`；固定 seed 和合成 token；A2 先测并冻结 manifest，A3 不参与调参，只做留出重放 |
| 指标 | optimizer step wall time、有效 tokens/s、loss/grad finite、每类 collective 的 count/bytes/exposed time、kernel breakdown、HBM peak/occupancy、route histogram、recompute time |
| 稳态采样 | 丢弃初始化、编译和至少前三个 warmup step；先取 5 个 measured steps。若 step time 变异系数超过 10%，扩展为 10 个独立有界测量并报告 P90，同时保留 median/min/max |
| PASS | 至少 5 个连续 optimizer steps；loss/grad finite；无 dropped/replayed token；HBM<85%；所有 rank 完成；采集字段齐全 |
| STOP | OOM/HBM≥85%、非 finite、HCCL hang/timeout、rank loss、数据重放/丢弃、配置漂移；只公开脱敏错误分类与聚合指标 |
| 送入 SimAI | 完整 schema v2 manifest、per-layer compute、collective trace/曲线、overlap/exposed time、memory lifetime、step-time distribution 与 provenance |

L3 的主要比较口径是同一 GTS 的 optimizer step time 与 useful tokens/s；A2/A3 rank 数、EP 与 GA 不同，故同时报告每 token、每 active layer、每 expert 的归一化值。任何只在 A2 拟合的参数在 A3 运行前冻结，避免把留出集变成第二个训练集。

## 研究不能证明的量与后续 probe

1. 现场 driver 25.3.rc1/26.0.rc1 是否分别兼容 A2/A3 目标 lane；官方版本表没有证明这些具体 build 组合。
2. A3 的 CANN 9.1.0-beta.1 能否与稳定 9.1.0 lane 等价；不能，除非官方矩阵或 L0/L1 实证给出证据。
3. A3 的 16 chip 能否作为 16 个训练 rank 被 torchrun/HCCL 稳定枚举；盘点只证明设备观测。
4. 官方 DeepSeek ST 数值基线来自哪一种硬件、driver/CANN build 与拓扑；公开文件未说明。
5. `GT-TARGET-SEMANTIC-v1` 的真实峰值显存、step time、collective trace、路由偏斜和 A2/A3 跨代差异。
6. HCCL Test 在具体 910B2 与脱敏 A3 板级设备上的支持性，以及对应源码/二进制的不可变 build。
7. MindSpeed-LLM 26.1 配套表与安装示例中 Triton-Ascend 3.2.2/3.2.1 的差异；GT 首轮不依赖它。
8. 合成 token 数据、公开 tokenizer 和随机初始化与官方内部 checkpoint/data 的数值差异；本契约只比较 steady-state 系统性能，不比较训练收敛质量。
9. AICB backward MoE、元数据 schema 与实际 route histogram 补齐前，不能把生成的 workload 当成 Ground Truth 等价物。
10. 用户允许的 30% A3 step-time 误差门槛是否满足；必须在后续真实 L0–L3 与 SimAI-Ascend 校准完成后判断。

## 最终选择

采用 **MindSpeed-LLM/MindSpeed Core/Megatron 26.1.0 共同源码 + A2(7.3.0/8.5) 与 A3(26.1.0/9.1) 独立软件 lane + 匹配版本 HCCL Test + `GT-TARGET-SEMANTIC-v1`**。这条契约最大化复用厂商正式发布、跨 CANN/TorchNPU 兼容表、DeepSeek ST 和共同 Megatron 语义，又不会虚构一套现场不存在的共同 wheel。

AICB 继续承担 Upstream SimAI workload 契约桥接；真实时间、显存、路由和 HCCL 曲线只能来自 MindSpeed Ground Truth。下一步应是受控的 A2 L0，而不是直接跑目标 slice；A2 manifest 冻结后才进入 A3 留出。
