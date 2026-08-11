# SimAI-Ascend：10 万张 Ascend 950DT、约 10T MoE 训练仿真证据库

> 截止日期：2026-08-11（Asia/Shanghai）
> 文档性质：开发前研究证据库，不是已校准的性能结论
> 目标：基于 `aliyun/SimAI` 构建 Ascend 版本，研究 100,000 张 Ascend 950DT、DeepSeek-V4-Pro 派生 MoE（2048 routed experts、TopK=16）、“GTS ≤ 500M”的训练配置；回答“显存用满时怎样更快、如何发挥超节点优势”需要哪些可验证证据。

## 1. 摘要

### 1.1 目前能下的结论

1. **SimAI 是合适的基线，但不能直接把 GPU/NCCL 名称替换为 NPU/HCCL 就宣称完成移植。** 截至仓库提交 [`f5efb5a`](https://github.com/aliyun/SimAI/tree/f5efb5a93ea9be7db25a8843f9f7ff54044f6062)，SimAI 已将工作负载、计算、集合通信和网络分层；训练工作负载包含 TP/PP/DP/EP、AllReduce/AllGather/ReduceScatter/AllToAll 以及 checkpoint 标记。与此同时，其解析器、总线带宽公式、集合通信实现和拓扑生成器仍带有 GPU/NCCL/NVLink/NIC 语义与经验常数，需建立显式的 Ascend/HCCL/UnifiedBus 适配层。
2. **“DeepSeek V4 Pro”已有公开一手配置，但用户目标不是原版 V4-Pro。** DeepSeek 官方报告给出 V4-Pro：61 层、hidden size 7168、每个专家 intermediate size 3072、384 个 routed experts、1 个 shared expert、每 token 激活 6 个 routed experts、1.6T 总参数、49B 激活参数；官方模型配置与报告一致（[报告 v1](https://arxiv.org/html/2606.19348v1)、[官方配置固定提交](https://huggingface.co/deepseek-ai/DeepSeek-V4-Pro/blob/45040942eb0d1c4e29fa6b92a6195f110e9e7444/config.json)）。2048 experts、TopK=16 是**用户定义的派生工作负载**。
3. **只改专家数和 TopK 不会得到精确 10T，但已满足本项目的 10T 量级口径。** 按 V4-Pro 的 61 个主干 block、`h=7168`、专家宽度 `d_e=3072` 和 SwiGLU 三矩阵估算，仅把 `E:384→2048`、`K:6→16`，总参数约从 1.6T 变为 **8.31T**，激活参数约从 49B 变为 **89.3B**；若独立 MTP block 也包含并同步扩展一组 MoE 权重，则相应约为 **8.42T/90.0B**。这是基于官方四舍五入总量的推导，须用最终模型代码逐参数计数；用户已确认保持 `d_e=3072` 并接受该范围。
4. **950DT 已有架构白皮书，且必须按 SKU 建模。** 官方白皮书公开 950DT 的 36/32/28 Cube Core 档位、144GB 或 96GB 片上内存、最高 4TB/s、UB 2.0 物理峰值 2016GB/s 双向、PCIe/UBoE 与 UB 端口复用、CCU collective offload 等信息（[昇腾 950 NPU 架构白皮书](https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf)）。不能把满规格芯片峰值默认为当前 Atlas 950 的实际 SKU。
5. **10 万卡必然跨超节点，但“一个超节点多大”有三种不同证据口径。** 2026 当前 Atlas 950 商品页最大 1,024 NPU；白皮书给的是 8,192 卡架构上限；2025 演讲给的是 8,192 卡历史路线图。按当前 1,024 卡商品单元，100,000 active NPU 需要 98 个 SuperPoD（97 个满配 + 1 个 672 卡残缺单元；或采购 98 个满配单元、留 352 个 spare）。因此必须把三种场景分开，不能把 8,192 卡规划当作当前商品配置。
6. **“用满显存”不是性能目标。** 正确约束是 `M_peak ≤ M_usable - M_guard`，目标是最大化有效 token/s 或最小化 step time，同时满足 OOM 风险、路由长尾和故障恢复约束。把剩余显存用于增大 micro-batch、减少重计算、增加通信 bucket 或预取，只能通过实测曲线决定；显存占用率本身不等于 MFU 或吞吐。
7. **当前不能给出可信的“最快并行度”。** 需要先获得 950DT 可用显存、算子曲线、HCCL 分层 collective 曲线、跨超节点拓扑、功耗降频、路由分布和框架 trace。现阶段只能定义候选搜索空间和淘汰规则。
8. **SimAI 的既有准确率不能外推到本目标。** NSDI'25 论文在 A100/H100、最大 1,024 GPU 的验证中报告端到端误差小于 4%、平均 98.1% 对齐；100,000 张 NPU、MoE TopK=16、按当前商品口径跨 98 个超节点，远超已公开验证域（[USENIX 论文](https://www.usenix.org/system/files/nsdi25-wang-xizheng-simai.pdf)，第 9–10 页）。

### 1.2 证据状态标记

- **【已证实事实】** 可由一手公开来源或固定提交源码直接核验。
- **【由证据推导】** 从已证实量经过明确公式得到；不能替代实测。
- **【用户给定假设】** 本项目输入，不代表厂商或模型作者公开规格。
- **【待确认】** 缺少公开一手证据或语义不唯一；不得进入定量结论。

## 2. 口径与关键结论

### 2.1 目标工作负载的身份

| 字段 | 状态 | 当前口径 |
|---|---|---|
| 设备数 | 【用户给定假设】 | 100,000 张 Ascend 950DT |
| 基础架构 | 【已证实事实】 | DeepSeek-V4-Pro 公开架构，见第 6 节 |
| routed experts | 【用户给定假设】 | 2,048（原版为 384） |
| TopK | 【用户给定假设】 | 16（原版 `num_experts_per_tok=6`） |
| “10T 模型” | 【用户已确认】 | 指 10T 量级；接受仅改 E/K 后推导的约 8.31–8.42T，expert intermediate size 保持 3,072 |
| shared experts | 【待确认】 | 是否保持 1；是否每 token 总是激活 |
| 前 3 个 Hash-routing MoE 层 | 【待确认】 | 是否保留；Hash 路由在 K=16 下的定义 |
| 训练精度 | 【待确认】 | BF16/FP8/FP4 的 forward、backward、梯度、主权重和通信各自 dtype |
| 训练序列长度 | 【待确认】 | V4-Pro 原训练从 4K 逐步延长至 16K、64K、1M；本项目取哪一阶段 |
| optimizer | 【待确认】 | 是否复现 V4 的 Muon + AdamW 混合优化器及混合 ZeRO |
| GTS ≤ 500M | 【用户已确认】 | 每个 optimizer step 的全局 token 数，上限为 500,000,000 tokens/step；见下节 |

### 2.2 “GTS ≤ 500M”的已确认口径

【用户已确认，2026-08-11】GTS 指**每个 optimizer step 的全局 token 数**，硬上限为 `500,000,000 tokens/step`。DeepSeek-V4 报告使用“batch size (in tokens)”而没有使用 “GTS” 缩写；V4-Pro 报告中的最大值为 94.4M tokens（[报告 §4.2.2](https://arxiv.org/html/2606.19348v1#S4.SS2.SSS2)）。本项目的 500M 是独立的用户约束，不应冒充官方 V4-Pro 配置。

基本计数公式为：

\[
GTS_{step}=\sum_{r\in DP\ replicas}\sum_{a=1}^{GA}\sum_{m=1}^{MBS} tokens(r,a,m)\le 500{,}000{,}000
\]

若所有序列等长，简化为：

\[
GTS_{step}=DP\times GA\times MBS\times S
\]

TP/PP/CP/EP 是同一数据副本的模型并行维度，不应再次乘入。若采用 MoE Parallel Folding，也必须按数据副本去重。`GTS` 不是整个训练任务的 token budget，也不是 token/s 吞吐率。

配置 schema 应固定写入 `gts_semantics=global_tokens_per_optimizer_step`、`gts_value≤500000000`、`gts_unit=tokens/step` 并拒绝隐式默认。仍需在实现票据中关闭一个次级口径：`tokens(r,a,m)` 统计有效非 padding tokens，还是统计实际调度并参与计算的 token slots；MoE drop、padding 与 replay 必须分别记账，不能悄悄改变 GTS。

### 2.3 “最快”的目标函数

建议至少输出两个目标，不把“显存占满”当目标：

\[
Throughput_{useful}=\frac{T_{step}-T_{dropped}-T_{replayed}}{t_{step}}
\]

\[
MFU=\frac{FLOPs_{model/token}\times Throughput_{useful}}{N_{active}\times PeakFLOPs_{selected\ dtype}}
\]

其中 `FLOPs_model/token` 必须按 V4 的 CSA/HCA、mHC、MTP、K=16 与 forward/backward 精确计算；不能套用 dense Transformer 的 `6P` 经验式。还应单列：step time、tokens/s、每卡有效 FLOP/s、通信暴露比例、重计算比例、路由 P50/P95/P99 负载、能耗/token、故障后 goodput。

## 3. 来源与证据矩阵

| 主题 | 一手来源（访问 2026-08-11） | 版本/固定点 | 能证明 | 不能证明 |
|---|---|---|---|---|
| SimAI 代码 | [`aliyun/SimAI`](https://github.com/aliyun/SimAI/tree/f5efb5a93ea9be7db25a8843f9f7ff54044f6062) | `f5efb5a93ea9be7db25a8843f9f7ff54044f6062`, 2026-04-24 | 现有模块、输入格式、算法和构建链 | Ascend 950DT 已适配或已校准 |
| SimAI 方法/验证域 | [NSDI'25 论文](https://www.usenix.org/system/files/nsdi25-wang-xizheng-simai.pdf) | NSDI 2025, pp. 541–558 | 方法、A100/H100 128–1024 GPU 校准结果、已知局限 | 100k NPU 误差或 950DT 性能 |
| DeepSeek-V4-Pro 架构 | [技术报告](https://arxiv.org/html/2606.19348v1) | arXiv:2606.19348v1, 2026-04-26 | 结构、训练设置、EP overlap、Muon/ZeRO、重计算 | 用户派生 10T 模型的精确参数/性能 |
| DeepSeek-V4-Pro 配置 | [官方 HF 配置](https://huggingface.co/deepseek-ai/DeepSeek-V4-Pro/blob/45040942eb0d1c4e29fa6b92a6195f110e9e7444/config.json) | commit `45040942...` | 关键 config 字段 | 训练时所有内部实现、950DT kernel |
| 950DT 芯片架构 | [昇腾 950 NPU 架构白皮书](https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf) | copyright 2026；PDF 元数据 2026-06-04；正文无版本号 | SKU 档位、算力、片上内存、UB/PCIe/UBoE、CCU、8K/128K 架构上限 | 当前商品 BOM、10 万卡物理拓扑、实测 HCCL 曲线 |
| Atlas 950 当前商品 | [昇腾产品页](https://www.hiascend.com/hardware/cluster) | 页面无发布日期，访问 2026-08-11 | 64/1024 NPU、96GB/NPU、1.72PB/s 聚合、16+4 柜、1/2 EFLOPS | 8192 卡已商品化或 100k 组网 |
| Atlas 950 路线图 | [华为主题演讲](https://www.huawei.com/en/news/2025/9/hc-xu-keynote-speech) | 2025-09-18 | 950DT、8192 NPU、160 柜、整机 FP8/FP4/内存/互联指标、Q4 2026 计划 | 2026 当前商品 BOM、每链路曲线 |
| Atlas 950 真机 | [华为 WAIC 公告](https://www.huawei.com/cn/news/2026/7/atlas-950-superpod) | 2026-07-17 | 1024 卡展示配置、全局内存、整机算力、RTT 宣称 | 8192 卡配置与 100k 集群实测 |
| Atlas 950 全球发布 | [华为 MWC 2026](https://www.huawei.com/en/news/2026/3/mwc-superpod-ai) | 2026-03-02 | 每柜 64 NPU、最大 8192 NPU、UnifiedBus | 单卡训练规格和链路细节 |
| HCCL 分层算法 | [CANN 8.5 HCCL 算法概览](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000115.html) | CANN Commercial 8.5.0 | α–β–γ 模型、2/3 层 collective、AllToAll 层级 | 950DT 的实测 α/β/γ |
| HCCL 算法配置 | [`HCCL_ALGO`](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000075.html) | CANN Commercial 8.5.0 | 可配置 server/supernode 层算法及产品约束 | Atlas 950 最优算法 |
| 950DT HCCL 软件状态 | [CANN 9.1.0-beta.3 HCCL](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/api_comm_impl.md) | 9.1.0-beta.3，页面无发布日期 | 文档明确 950PR/DT RootInfo/EID 建域要求 | 稳定版支持、性能或最优算法 |
| Ascend profiling | [`msprof` 文档](https://www.hiascend.com/document/detail/en/canncommercial/850/devaids/profiling/atlasprofiling_16_0055.html) | CANN Commercial 8.5.0 | task/HCCL/AI Core/内存等 trace 输出 | 未运行时的性能数值 |
| 通用并行策略 | [Megatron-Core 并行指南](https://docs.nvidia.com/megatron-core/developer-guide/latest/user-guide/parallelism-guide.html) | rolling latest, 访问 2026-08-11 | TP/PP/CP/EP/FSDP 语义 | Ascend 上的性能收益 |
| MoE 并行/调度 | [Megatron-Core MoE 指南](https://docs.nvidia.com/megatron-core/developer-guide/nightly/user-guide/features/moe.html) | rolling nightly, 访问 2026-08-11 | dispatcher、capacity、drop、folding、overlap 的字段集合 | Ascend 实现和最优参数 |
| 状态显存参考 | [Megatron-Core Distributed Optimizer](https://docs.nvidia.com/megatron-core/developer-guide/latest/user-guide/features/dist_optimizer.html) | rolling latest, 访问 2026-08-11 | 特定 dtype/Adam 假设下的 bytes/param | Muon/Ascend 的直接字节数 |

## 4. SimAI 基线审计

### 4.1 构建与运行链

【已证实事实】顶层仓由以下组件组成（[README_CN 固定提交](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/README_CN.md)）：

- AICB：生成/采集训练与推理 workload；顶层锁定子模块提交 [`23eec3c`](https://github.com/aliyun/aicb/tree/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f)。
- SimCCL：把 collective 展开为 P2P；顶层锁定 [`403610a`](https://github.com/aliyun/SimCCL/tree/403610a0f91659e628428afa8d489cb046ef9503)。
- `astra-sim-alibabacloud`：顶层内嵌执行引擎、analytical/NS-3/physical backend。
- `ns-3-alibabacloud`：packet-level 网络；顶层锁定 [`7e3cb5b`](https://github.com/aliyun/ns-3-alibabacloud/tree/7e3cb5b88c99abcb582c5abc3919484a4805111b)。

官方构建入口（[`scripts/build.sh`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/scripts/build.sh)）为：

```bash
git submodule update --init --recursive
./scripts/build.sh -c analytical
./scripts/build.sh -c ns3
```

复现实验应停在顶层 pin，并运行 `git submodule update --init --recursive`；README 另有 `git submodule update --remote` 建议（[README L163–177](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/README.md#L163-L177)），该命令会把子模块推进到远端分支而脱离上述固定提交，不应进入校准流水线。

官方示例运行链：

```bash
./bin/SimAI_analytical \
  -w example/workload_analytical.txt -g 9216 -g_p_s 8 \
  -r test- -busbw example/busbw.yaml

python3 astra-sim-alibabacloud/inputs/topo/gen_Topo_Template.py \
  -topo Spectrum-X -g 128 -gt A100 -bw 100Gbps -nvbw 2400Gbps

AS_SEND_LAT=3 AS_NVLS_ENABLE=1 ./bin/SimAI_simulator \
  -t 16 -w example/microAllReduce.txt \
  -n ./Spectrum-X_128g_8gps_100Gbps_A100 \
  -c astra-sim-alibabacloud/inputs/config/SimAI.conf
```

### 4.2 已有工作负载接口

【已证实事实】[`Workload::initialize_workload`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/workload/Workload.cc#L1134) 读取首行的：

- `model_parallel_NPU_group`（实际作为 TP）
- `ep`
- `pp`
- `vpp`
- `ga`
- `all_gpus`
- `pp_comm`
- checkpoint layer 与 forward-in-backward initiation

每层记录 forward、input-gradient、weight-gradient 的 compute time、collective 类型、group 类型和 message size。可解析 `ALLREDUCE`、`ALLTOALL`、`ALLGATHER`、`REDUCESCATTER`，以及 `EP`、`DP_EP` 后缀。[`Layer::compute_time`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/workload/Layer.cc#L940) 根据 TP/EP/DP/DP_EP、设备/服务器、bus bandwidth 与经验 ratio 估时。

【已证实事实】集合通信算法源码已有 Ring、Double Binary Tree、Halving-Doubling 和 AllToAll（[`collective/`](https://github.com/aliyun/SimAI/tree/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/collective)）。拓扑生成器目前显式生成 AlibabaHPN、Spectrum-X、DCN+ 等 GPU/NVSwitch/NIC 网络（[`gen_Topo_Template.py`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/inputs/topo/gen_Topo_Template.py)）。

【已证实事实】现有可复用 seam 与阻塞点更具体如下：

- 网络后端接口 [`AstraNetworkAPI`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraNetworkAPI.hh#L77-L151) 提供 `sim_schedule/send/recv`；计算/内存也有 [`ComputeAPI`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraComputeAPI.hh#L14-L26) 与 [`AstraMemoryAPI`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraMemoryAPI.hh#L14-L22)，但后者只有读写延迟，没有训练显存容量或生命周期账本。
- `GPUType` 只列 A100/A800/H100/H800/H20（[`Common.hh`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/Common.hh#L14-L45)），CLI 只解析这些型号（[`AstraParamParse.hh`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraParamParse.hh#L138-L147)），[`calbusbw.h`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/calbusbw.h#L4-L47) 硬编码 NVLink/CX/BF/NVLS/PCIe 与 NVIDIA CSV。
- Analytical 路径的 `sim_send/sim_recv` 直接返回（[`AnalyticalNetwork.cc`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/network_frontend/analytical/AnalyticalNetwork.cc#L33-L70)），通信时间由 `Layer::compute_time` 事后估算；它不是 UB/HCCL 通信事件模型。
- `MockNcclGroup` 假设 `TP×DP×PP=world`、`EP×DP_EP=DP`，带 NVIDIA/NVSwitch 建组语义，且 `Sys` 把 `PP_size` 写成 1（[`MockNcclGroup.cc`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/MockNcclGroup.cc#L26-L158)、[`Sys.cc`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/Sys.cc#L1355-L1397)）；固定的 SimCCL 子模块本身只有 README，且声明完整版本尚未开放（[SimCCL README](https://github.com/aliyun/SimCCL/blob/403610a0f91659e628428afa8d489cb046ef9503/README.md#L1-L6)）。
- AICB 能读取 expert 数、TopK 并合成 MoE 通信量（[`SimAI_training_workload_generator.py`](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/SimAI_training_workload_generator.py#L433-L486)），但 dump header 不保存这些语义字段（[同文件](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/SimAI_training_workload_generator.py#L841-L868)），所以需要 schema v2 而非靠文件名恢复 provenance。
- 固定 AICB 的 DeepSeek compute profiler 直接依赖 `torch.cuda`/DeepGEMM，且标明 DeepGEMM 只支持 SM90/SM100、DeepEP 仍为 TODO（[`AiobDeepSeek.py`](https://github.com/aliyun/aicb/blob/23eec3c48ca2d2d93dd888a4c7b22ab4421e782f/workload_generator/mocked_model/training/AiobDeepSeek.py#L1-L43)）；Ascend 必须导入 `torch_npu`/CANN 真机 kernel 数据，不能复用该 CUDA 路径。

### 4.3 必须新建/改造的 Ascend seam

以下是【由源码推导】的最小模块边界，最终文件名可在设计阶段调整：

1. **`AscendWorkloadProvider`：** 将真实 PyTorch/MindSpore + CANN trace 或符号模型转成 SimAI workload。不要把 950DT 计算时间硬编码进 AICB 的 GPU profiler。
2. **`HcclCollectiveModel`：** 用 HCCL collective、层级算法、dtype、message segmentation、stream/core 占用替代 NCCL 专用展开和校准表；保留统一 `CollectiveOp` 接口。
3. **`UnifiedBusTopologyProvider`：** 表达 NPU→板/柜→SuperPoD→SuperCluster 多层拓扑、统一内存语义和故障域，避免继续使用 `gpu_per_server + nvlink_bw + NIC` 的扁平假设。
4. **`AscendComputeModel`：** 对 CSA/HCA、mHC、router/top-k、Grouped Expert GEMM、dispatch/combine、Muon update 建立 shape/dtype→时间曲线，并记录版本与置信区间。
5. **`MemoryModel`：** 训练显存必须独立于 1.6 版本新增的推理 KV-cache 模型；输出 peak timeline 而不是只有静态参数量。
6. **`ParallelPlacement`：** 显式产生 attention 与 MoE 两套 process groups，支持 TP/PP/CP/DP 与 ETP/EP/EDP folding、超节点亲和性和跨域流量预算。
7. **`CalibrationRegistry`：** 以 `device SKU + firmware/driver + CANN/HCCL + framework commit + power mode + topology + message/shape/dtype` 为 key，禁止不同硬件表静默复用。

### 4.4 基线限制

- 【已证实事实】SimAI 论文的 EP 模型假定 gating token 分布均衡（[USENIX 论文](https://www.usenix.org/system/files/nsdi25-wang-xizheng-simai.pdf)，第 8 页）。对 2,048 experts、TopK=16，这会抹去决定 P99 step time、buffer 峰值与丢 token 的长尾，必须改为可注入真实 histogram/trace。
- 【已证实事实】`Layer::compute_time` 对小于 1 MiB 的消息存在按 rank 数写死的延迟分支，最大列到 128 ranks；不能用于 100k/HCCL 定量外推（[源码](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/workload/Layer.cc#L956)）。
- 【已证实事实】AllToAll 以每个 `i→j` 显式生成流（[`genAlltoAllFlowModels`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/MockNcclGroup.cc#L342-L390)）；`EP=2048` 的单组就是 `2048×2047=4,192,256` 条有向流，尚未计层、micro-batch、正反向，必须改为分层聚合/稀疏通信矩阵。
- 【已证实事实】Analytical 的事件循环使用 FIFO queue 并逐 tick 前进（[`AnaSim.cc`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/network_frontend/analytical/AnaSim.cc#L20-L52)）；NS-3 路径则为每个 GPU/NVSwitch endpoint 建 `ASTRASimNetwork` 与 `Sys` 对象（[`AstraSimNetwork.cc`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/network_frontend/ns3/AstraSimNetwork.cc#L260-L335)）。两者在 100k 前都需要事件跳跃、对称折叠与代表流。
- 【已证实事实】拓扑生成器只接受 GPU/NVSwitch/NVLink 等参数，且拒绝超过单 Pod 容量（[`gen_Topo_Template.py`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/inputs/topo/gen_Topo_Template.py#L9-L24)）；其现有模板上限不是 100k Ascend 分层拓扑的一手支持证据。
- 【已证实事实】NSDI 论文公开验证到 1,024 GPU；论文称多线程/无锁优化支持“超过 1,000 GPU”，没有公开 100k 精度或运行时结果（[论文 §3.5、§4](https://www.usenix.org/system/files/nsdi25-wang-xizheng-simai.pdf)）。
- 【由证据推导】100k 全量 packet-level NS-3 不应成为日常搜索内环；先 analytical/flow-level 筛选，再对关键 collective/拥塞场景 packet-level 抽样，最后回填代理模型。

## 5. Ascend 950DT 与超节点拓扑

### 5.1 950DT 芯片与 UB 2.0：公开架构事实

【已证实事实】《昇腾 950 NPU 架构白皮书》给出的 950DT 规格如下；这些是产品族/SKU 档位，不代表用户最终采购 BOM（[官方 PDF](https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf)，copyright 2026，PDF 元数据 2026-06-04，正文无版本号）：

| 项目 | 白皮书公开内容 |
|---|---|
| 封装 | 2 AI Die + 2 IO Die + 4 memory modules，Chiplet UMA |
| AI 子系统 | 满芯片 36 个；每个 1 Cube + 2 Vector；DT SKU 为 36/32/28 Cube、72/64/56 Vector |
| Cube+Vector MXFP4 峰值 | 2007/1784/1561 TFLOPS |
| HiF8/MXFP8/FP8 峰值 | 1034/919/804 TFLOPS |
| BF16/FP16 峰值 | 547/486/425 TFLOPS |
| 片上内存 | 最高 96GB 或 144GB；最高 4TB/s，须按 SKU 选取 |
| L2 | 128MB；512B cache line，4×128B sector |
| UB 2.0 | 72 lanes = 18 个 x4 端口；每 lane 最高 112Gbps；合计 2016GB/s 双向物理峰值 |
| UBoE / PCIe | 2×400Gbps UBoE 复用 2 个 UB 端口；PCIe 5.0×16 为 128GB/s 双向并复用 4 个 UB 端口，峰值不可相加 |
| CCU | 硬件支持 Broadcast/ReduceScatter/AllGather/AllReduce/All2All/All2AllV |
| UB 拓扑与规模 | Clos、FullMesh+Clos、nD-Mesh；SuperNode 架构最大 8192 卡，cluster 架构超过 128K |

【由证据推导】`2016GB/s` 是 72 lane 双向物理峰值，不是某个 HCCL communicator 的可用单向 bus bandwidth；端口复用、协议、拓扑、CCU、并发与跨机柜光模块都会改变有效曲线，必须用微基准拟合。

### 5.2 “当前商品、架构上限、历史路线图”三种口径

| 证据身份 | 已证实事实 | 可怎样使用 |
|---|---|---|
| **CURRENT_PRODUCT** | 截至 2026-08-11 的[昇腾商品页](https://www.hiascend.com/hardware/cluster)公开 64/1024 NPU 配置；1024 配置为 16 计算柜 + 4 UB 互联柜、最高 256 Kunpeng 950 CPU、`1024×96GB` 片上内存、最高 4TB/s/NPU、1.72PB/s 系统互联；其“单柜总互联带宽”写为 `64×1.68TB/s（双向）` | 当前可采购/可校准基线；仍要 BOM 和真机核验可用内存 |
| **CURRENT_DEMO** | 2026-07-17 [WAIC 真机公告](https://www.huawei.com/cn/news/2026/7/atlas-950-superpod)展示 1024 卡、1 EFLOPS FP8、2 EFLOPS FP4、256TB 全局统一编址、3μs RTT | 证明 1024 配置存在；`3μs` 缺消息大小、端点、负载和统计口径，不能直接设为 HCCL `α` |
| **ARCHITECTURE_LIMIT** | 白皮书给出 UB SuperNode 最大 8192 卡、cluster 超过 128K | 用作架构情景上限；不证明 8192 卡当前已交付或已校准 |
| **PUBLIC_ANNOUNCEMENT** | 2026-03-02 [MWC 公告](https://www.huawei.com/en/news/2026/3/mwc-superpod-ai)称每柜 64 NPU、最大 8192 NPU | 佐证公开产品方向；不替代商品 BOM |
| **HISTORICAL_ROADMAP** | 2025-09-18 [主题演讲](https://www.huawei.com/en/news/2025/9/hc-xu-keynote-speech)描述 8192 NPU、128 计算柜 + 32 通信柜、8/16 EFLOPS FP8/FP4、1152TB、16 或 16.3PB/s、计划 2026 Q4 | 仅作路线图情景；同页 4.91M token/s 无模型/batch/序列/精度定义，不能做训练基准 |

【待确认】当前 1024 商品配置、8192 架构上限与 8192 历史路线图的机柜/链路/BOM 不得混成同一拓扑。`256TB` 全局统一编址空间也不等于 1024 rank 各自可无代价使用的本地 HBM；“用满显存”仍须以目标 SKU 的 allocator/`npu-smi` 峰值为准。

【已证实事实】商品页的 `1.68TB/s（双向）/NPU` 配置值低于芯片白皮书 `2.016TB/s（双向）` 72-lane 物理上限；差额与端口复用再次说明不能把芯片 ceiling 当作当前整机有效带宽，更不能把任一值当 HCCL `algbw`。

【已证实事实】CANN 9.1.0-beta.3 文档已明确提到 950PR/DT 的 RootInfo/EID 建域要求（[通信域文档](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/api_comm_impl.md)），并在 `HCCL_ALGO` 中对 950PR/DT 明确列出 NHR，而非让用户无条件套用 Pairwise/NB/AHC（[算法配置](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/hccl_env/HCCL_ALGO.md)）。这证明 beta 文档层面的 DT 入口，不证明稳定版支持或任何性能值；CANN 8.5 文档只用于通用层级公式。

### 5.3 100k 分层拓扑符号模型

设：

- `N=100000`：NPU 总数；
- `N_sp,current≤1024`：截至报告日商品页公开的单 SuperPoD 配置；精确 100,000 active ranks 需 `P_current=ceil(100000/1024)=98`，即 97 个满配域 + 1 个 672-rank 残缺域；若采购 98 个满配域，则容量 100,352、留 352 个 spare；
- `N_sp,arch≤8192`：白皮书架构情景上限；对应 `P_arch=13`，不能称为当前商品拓扑；
- 层级 `l∈{device, board, cabinet, superpod, cluster}`；
- 每层 `α_l` 为启动/协议时延，`β_l=1/B_l` 为每字节时间，`γ_l` 为 reduction 每字节成本；
- `c_l` 为 oversubscription/contention，`q_l(t)` 为队列状态；
- `A(op,group,placement)` 为 collective 算法与 rank 映射。

HCCL 官方文档本身采用 `D=α+nβ+nγ` 并说明可使用 intra-rank/inter-rank/inter-supernode 的两层或三层 collective（[CANN 8.5](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000115.html)）。因此 SimAI-Ascend 至少应使用：

\[
t_{collective}=\sum_l steps_l(A)\cdot(\alpha_l+n_l\beta_l+n_l\gamma_l)\cdot c_l + t_{queue}+t_{sync}
\]

所有 `α/β/γ/c` 都必须由对应层级微基准标定；公开的整机“16.3 PB/s”不能替代它们。

【由证据推导】规模还有一个不能忽略的整除问题：`100000` 既不能被 `1024`、`8192` 整除，也不能被 `2048` 整除。`98,304 = 96×1,024 = 12×8,192 = 48×2,048` 是同时对当前商品域、架构上限域和 expert group 整齐的候选 active set，剩余 1,696 张可作为 spare/服务池；但这不再是 100,000 张同时训练。若要求精确 100,000 active ranks，就必须允许残缺 SuperPoD、非 2,048 的 EP group/专家副本，或不规则 communicator。至少比较：`98,304 active + 1,696 spare`、`100,000 ragged`、`98×1024 capacity with 352 spare` 三种情景。

### 5.4 故障域

至少建模：NPU、板/节点、计算柜、通信柜、SuperPoD、跨 SuperPoD 网络、软件进程/作业。输出不只 steady-state step time，还要输出：

- MTBF/故障注入下的有效 goodput；
- checkpoint 周期与保存/恢复耗时；
- rank 重建、通信域缩容/重映射；
- 当前商品口径 98 个 SuperPoD（架构上限情景 13 个）的相关故障与维护域。

【待确认】公开材料未给出 950DT 各故障域概率、重配置时间和跨 SuperPoD 链路细节，先使用符号分布与敏感性范围，不能编造点估计。

## 6. 模型工作负载

### 6.1 官方 V4-Pro 基线

【已证实事实】DeepSeek-V4-Pro 的关键字段如下（[技术报告 §4.2.1](https://arxiv.org/html/2606.19348v1#S4.SS2.SSS1)、[官方 config](https://huggingface.co/deepseek-ai/DeepSeek-V4-Pro/blob/45040942eb0d1c4e29fa6b92a6195f110e9e7444/config.json)）：

| 字段 | 官方 V4-Pro |
|---|---:|
| layers | 61 |
| hidden size | 7168 |
| attention heads | 128 |
| head dim | 512 |
| CSA attention top-k | 1024（注意：不是 MoE TopK） |
| routed experts/layer | 384 |
| shared experts/layer | 1 |
| expert intermediate size | 3072 |
| routed experts activated/token | 6 |
| hash-routed MoE layers | 前 3 个 |
| MTP depth | 1 |
| mHC expansion | 4 |
| total params | 1.6T（报告四舍五入） |
| activated params/token | 49B（报告四舍五入） |
| max global batch in training | 94.4M tokens |
| train tokens | 33T |
| sequence curriculum | 4K→16K→64K→1M |

注意 `index_topk=1024` 是 CSA 稀疏注意力选择的 KV entries，`num_experts_per_tok=6` 才是 MoE TopK；SimAI 配置必须用不同字段名，避免混淆。

### 6.2 派生 2048 experts / TopK=16 的参数审计

一个无 bias 的 SwiGLU expert 有三块主矩阵：

\[
P_{one\ expert}\approx 3hd_e=3\times7168\times3072=66{,}060{,}288
\]

令 `L_e` 为实际拥有独立 routed-expert 权重且随配置扩展的 block 数。61 个主干 block 全部是 MoE；若 MTP 的独立 block 也拥有并同步扩展该组权重，则 `L_e=62`，否则为 61：

\[
P'_{total}\approx1.6T+(2048-384)\times L_e\times66{,}060{,}288
\approx8.305\text{--}8.415T
\]

\[
P'_{active}\approx49B+(16-6)\times L_e\times66{,}060{,}288
\approx89.30\text{--}89.96B
\]

以上是【由证据推导】，误差来自官方 1.6T/49B 的四舍五入、bias/scale/router/mHC/MTP 等细节。结论是：

- 增大 `E` 增加存储参数与 optimizer state，但不直接增加每 token FLOPs；
- 增大 `K` 不增加存储参数，却近似线性增加 token-expert assignments、专家 FLOPs 和 dispatch/combine 负载；
- 只做这两个改动无法精确达到 10T；【用户已确认，2026-08-11】本项目只要求 10T 量级，因此接受约 8.31–8.42T，并保持 expert intermediate size 3,072。

必须实现一个逐 tensor 参数计数器，并分别报告：`total_params`、`active_params/token`、`trainable_params`、`optimizer_state_elements`、`parameters_by_module_and_dtype`。

### 6.3 MoE 路由与通信方程

设本 step 有 `T` 个有效 token，`K=16`，hidden `h`，dispatch/combine element bytes 分别为 `b_d,b_c`：

\[
assignments=T\times K
\]

忽略 metadata、padding、locality 和反向传播时，前向 activation payload 的逻辑下界近似：

\[
V_{EP,fwd}\approx T\times K\times h\times(b_d+b_c)
\]

实际网络字节还取决于本地专家命中率、复制、分块、metadata、路由不均衡、capacity padding/drop、AllToAll 算法和反向传播。DeepSeek 报告以每 token-expert pair 的 `6hd` FLOPs 和 `3h` bytes（FP8 dispatch + BF16 combine）讨论 V4-Pro EP overlap（[§3.1](https://arxiv.org/html/2606.19348v1#S3.SS1)）；该比值是其实现口径，不能直接替代 Ascend 训练前后向实测。

每层每专家负载：

\[
L_{l,e}=\sum_{t=1}^{T}\mathbf{1}[e\in route_l(t)]
\]

至少保存 `mean/P50/P95/P99/max`、变异系数、Gini、超 capacity 数、drop 数、padding 数；step time 更接近最慢专家/最慢通信 wave，而非平均负载。

### 6.4 仍需补齐的模型字段

- 8.31–8.42T 目标的精确逐 tensor 参数计数与参数文件；
- shared experts 个数/宽度、前 3 层 Hash routing 的 K 与分布；
- capacity factor、padding、drop/overflow policy；
- router score、normalization、bias 更新、sequence-wise balance loss；
- training dtype matrix（expert/attention/router/grad/optimizer/comm）；
- CSA/HCA 各层排列、压缩率与 1M 上下文 curriculum；
- mHC 与 MTP 是否保持；
- Muon/AdamW 参数分组和状态精度；
- token packing、有效长度分布、micro-batch 与 GA；
- determinism 是否是硬约束（会改变 kernel/buffer/性能）。

## 7. 训练显存模型

### 7.1 峰值方程

对 rank `i`：

\[
M^{peak}_i=M_{param}+M_{grad}+M_{master}+M_{optim}+M_{act}^{saved}
+M_{recompute\ workspace}+M_{comm}+M_{temp}+M_{runtime}+M_{fragment}+M_{reserve}
\]

约束：

\[
\max_t M_i(t)\le M^{usable}_{i}-M^{guard}_{i}
\]

其中 `M_usable` 必须由目标 950DT SKU 在目标 CANN/driver 下实测，不使用 SuperPoD 全局内存除法代替。

对参数集合 `p` 的通用状态项：

\[
M_{state,i}=\sum_p\left(
\frac{s_w(p)}{R_w(p)}+
\frac{s_g(p)}{R_g(p)}+
\frac{s_m(p)}{R_m(p)}+
\sum_j\frac{s_{opt,j}(p)}{R_{opt,j}(p)}
\right)+M_{padding}
\]

`R_*` 由参数实际落在哪个 PP stage、TP/ETP/EP shard、DP/EDP/ZeRO shard 决定；不能用一个全局除数处理 dense 与 expert 参数。

Megatron-Core 在其特定实现中给出 BF16 param + FP32 grad 的理论值：非分布式 optimizer 为 18 bytes/param，distributed optimizer 为 `6+12/d` bytes/param（`d` 为 DP size）（[官方文档](https://docs.nvidia.com/megatron-core/developer-guide/latest/user-guide/features/dist_optimizer.html)）。这只作为单元测试参考；V4 的 Muon/AdamW 混合状态、BF16 Newton-Schulz、expert 独立优化和混合 ZeRO 必须另算（[DeepSeek-V4 §3.4.1](https://arxiv.org/html/2606.19348v1#S3.SS4.SSS1)）。

### 7.2 激活与重计算

建立 shape 驱动、事件时间线模型：

\[
M_{act}^{saved}=f(B_\mu,S,h,L_{stage},TP,CP,EP,\text{CSA/HCA},mHC,MTP,policy,dtype)
\]

任何 `κ × Bμ × S × h × L` 简式中的 `κ` 都是待校准经验常数。尤其：

- mHC 扩大 residual stream；DeepSeek 报告明确它增加 activation 与 PP communication；
- CSA/HCA 有压缩 KV、indexer、select-and-pad 等不同中间张量；
- MoE 需要 router logits、indices、permutation、capacity padding、dispatch/combine buffer；
- PP schedule 同时驻留的 micro-batch 数决定峰值；
- deterministic backward 可能增加独立 accumulation buffer。

候选重计算策略按粒度搜索：无重计算、selective tensor、attention、MoE activation、整层。DeepSeek-V4 采用 tensor-level checkpointing，并对 mHC 选择性重算 hidden states/normalized inputs、避免重算昂贵算子（[§3.4.2–3.4.4](https://arxiv.org/html/2606.19348v1#S3.SS4)）。这支持“部分重计算”方向，但不提供 Ascend 上的时间/字节收益，需实测 Pareto 曲线。

### 7.3 显存用满时怎样更快：可检验顺序

在保留经故障/碎片实测得到的 guard 后，按以下优先级做 A/B：

1. 增大 `Bμ`，直到专家 GEMM/attention tile 达到饱和或路由 buffer/长尾恶化；
2. 用额外显存保存重计算代价最高的 tensor，减少 selective recompute；
3. 增大/双缓冲 dispatch-combine 与 DP bucket，以增加 overlap，但限制 concurrent streams/AI Core 占用；
4. 降低 PP stage 不均衡与 bubble（调整 layer placement、VPP、micro-batch 数）；
5. 对长序列用 CP 替代盲目增大 TP 或全层重算；
6. 只有在带宽、DMA、host memory 实测支持时才考虑 activation/optimizer offload。

每项都比较 `Δtokens/s / ΔGiB`、暴露通信、重计算 FLOPs、P99 memory 和 OOM 率。若吞吐不升，留空显存通常优于为“占满”增加无效缓存。

## 8. 并行、通信与超节点放置

### 8.1 两套并行恒等式

传统耦合方式的典型约束：

\[
N=TP\times CP\times EP\times DP\times PP
\]

MoE Parallel Folding 则有两套覆盖同一 world 的分组：

\[
N=TP\times CP\times DP\times PP
\]

\[
N=ETP\times EP\times EDP\times PP
\]

Megatron-Core 官方说明 Folding 将 attention 的 `TP×CP×DP×PP` 与 MoE 的 `ETP×EP×EDP×PP` 解耦，并建议 fine-grained MoE 的 ETP 从 1 起步（[MoE 指南](https://docs.nvidia.com/megatron-core/developer-guide/nightly/user-guide/features/moe.html)）。这是通用设计参考，Ascend 上的组合仍须验证。

### 8.2 超节点亲和性原则（待验证假设）

1. **优先把高频、细粒度、延迟敏感的 TP/ETP 和 EP dispatch wave 放进 SuperPoD。**
2. **CP 与 EP 做 folding 候选，**使长上下文 attention 通信与 MoE AllToAll 在不同时段复用高速域，而不是机械相乘扩大 world size。
3. **PP 边界对齐故障域/超节点边界，**但需同时最小化 activation P2P 并平衡 CSA/HCA/MoE/mHC 层耗时。
4. **DP/EDP 优先承担跨 SuperPoD 的大 bucket 通信，**因为其消息更容易摊薄启动时延并与 backward overlap；这不是绝对规则，取决于 HCCL 实测。
5. **2048 experts 的放置以 locality-aware routing/replication 为搜索维度。** 若 EP 跨当前商品口径的 98 个 SuperPoD，TopK=16 可能制造全域 AllToAll；候选包括超节点内 expert groups、热点 expert 副本和两级 dispatch，但会改变 EDP、状态显存与一致性通信。
6. **共享专家计算与 dispatch overlap、expert wave pipeline、grouped GEMM、router/permute fusion**应成为独立开关。DeepSeek-V4 报告给出 wave-based dispatch/compute/combine pipeline，并强调完全隐藏由 `C/B ≤ V_comp/V_comm` 决定（[§3.1](https://arxiv.org/html/2606.19348v1#S3.SS1)）。

TopK 从 6 增至 16 时，理想情况下每 token-expert pair 的计算/通信强度不变（`K` 在分子分母同时放大），因此不能仅凭总通信字节增加就断言 overlap 变差；真实差异来自 expert GEMM shape、fan-out/local-hit、wave 数、buffer 峰值、拥塞和最慢专家长尾，必须进入事件模型。

### 8.3 HCCL 与 SimAI 映射

HCCL 8.5 通用文档确认：

- AllReduce/AllGather/ReduceScatter/AllToAll 等采用层级 orchestration；
- 可通过 `HCCL_ALGO` 指定 server 或 supernode 层算法；
- AHC 等算法面向 communicator 内多层、可能非对称的 NPU 分布；
- 某些算法/模式存在产品与跨超节点适用约束。

来源：[算法概览](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000115.html)、[`HCCL_ALGO`](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000075.html)。对 950DT 则以 CANN 9.1.0-beta.3 的 [950PR/DT `HCCL_ALGO`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/hccl_env/HCCL_ALGO.md) 为版本证据；它公开的是 NHR 配置约束，不是性能曲线。

因此配置不能只写 `alltoall, size, ranks`；至少增加：`hierarchy`、`rank_order`、`algorithm_per_level`、`dtype`、`stream/core mode`、`chunk`、`inplace`、`deterministic`、`concurrency`、`topology_version`。

## 9. 性能模型与“超节点优势”实验

### 9.1 Step time 分解

\[
t_{step}=t_{fwd}+t_{bwd}+t_{optim}+t_{bubble}+t_{exposed\ comm}+t_{recompute}+t_{input}+t_{imbalance}+t_{fault}
\]

同时保存不重叠的 raw compute/raw communication 与 overlap timeline，避免重复相加。每个 MoE 层拆为：router→permute→dispatch→expert GEMM1→activation→expert GEMM2→combine→shared expert；前反向分别建模。

### 9.2 超节点优势必须由对照实验体现

同一 workload、同一 NPU 数、同一 kernel 曲线下做：

- `flat cluster` vs `SuperPoD-aware placement`；
- 随机 rank order vs topology-aware rank order；
- 全域 EP vs SuperPoD-local EP + EDP；
- 无 overlap vs shared-expert overlap vs wave overlap；
- 同构 collective vs 分层 HCCL algorithm；
- 当前商品口径 97 满 + 1 个 672-rank 残缺 SuperPoD、98,304-active+spares；另把 12 满 + 1 残缺作为 8192 架构上限情景，二者不混算；
- 正常、单链路/单柜故障、降频/拥塞场景。

报告 gain 的同时给出其来源：减少跨域 bytes、降低启动次数、提高 local expert hit、改善 overlap、减少 bubble 或提升 GEMM shape。不能只报告总 step speedup。

### 9.3 不可直接迁移的公开性能数

DeepSeek-V4 报告的 fine-grained EP 在 NVIDIA GPU 与 Huawei Ascend NPU 上验证过，报告通用推理 `1.50–1.73×`、低时延场景最高 `1.96×`（[§3.1](https://arxiv.org/html/2606.19348v1#S3.SS1)）。这是**原版模型、报告所用平台与推理 workload**的结果，不是 950DT、10T、训练、TopK=16 或 100k 卡的预测；本项目只能把其 wave overlap 机制作为候选方案。

## 10. 校准与验证方案

### 10.1 校准阶梯

1. **单 NPU：** GEMM/grouped GEMM、CSA/HCA、mHC、router/top-k、permute、expert forward/backward、Muon/AdamW update；shape×dtype×batch sweep；采集 warmup 后分布、功耗/频率、HBM 峰值。
2. **单板/单柜：** P2P、AllReduce、AllGather、ReduceScatter、AllToAll(V/VC)，按 1 B–多 GiB 对数 message sweep、并发 collective、rank order、in-place/out-of-place、deterministic。
3. **单 SuperPoD：** 分层 collective、EP token-distribution trace、不同 cabinet/rank mapping、拥塞、wave overlap、统一内存访问边界；先以当前 1,024 卡商品配置闭环，8192 只作为独立架构情景。
4. **多 SuperPoD：** 当前商品路径按 2→4→8→16→32→64→98，测跨域 α/β、oversubscription、collective algorithm、98 个域的非整齐规模与故障恢复；8192 架构情景另做 2→4→8→13。
5. **端到端小模型/缩放模型：** 固定模型语义，逐级增加卡数；对每一级冻结上一层参数后再拟合新增层。
6. **100k 外推：** 仅在多 SuperPoD 有真值并覆盖关键拓扑跃迁后；当前商品口径应尽量接近 98 个域。若做不到，输出区间/敏感性，不输出单点“真实性能”。

### 10.2 数据采集

- `msprof` 可导出 CANN/runtime/task、HCCL communication statistics、AI Core、L2/内存等信息（[CANN 8.5 profiling](https://www.hiascend.com/document/detail/en/canncommercial/850/devaids/profiling/atlasprofiling_16_0055.html)）。
- 官方 `hccn_tool`/HCCL test 方法支持 `-b/-e` 消息区间、`-i/-f` 步进、`-w` warmup、`-n` repeat、`-c` 正确性，并输出 `avg_time(us)` 与 `alg_bandwidth(GB/s)`（[工具说明](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/devaids/hccltool/HCCLpertest_16_0001.html)、[参数](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/devaids/hccltool/HCCLpertest_16_0005.html)、[结果](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/devaids/hccltool/HCCLpertest_16_0006.html)）。文档列出的 AllToAll/AllToAllV 最大测试规模为 8K ranks、一般 collective 为 32K，均不能直接覆盖 100K；且该工具页没有给出 950DT 实测数据，故只采纳方法，不宣称 DT 已被该工具矩阵覆盖。
- 自建 harness 还需保存 message、dtype、ranks、rank order、算法、P50/P95/P99 latency、algbw/busbw、错误/重试；以 full-rank HCCL profiling 核对关键路径（[HCCL profiling](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/commlib/hcclug/hcclug_000014.html)）。
- 框架 trace 保存 operator dependency 与 overlap；路由 trace 保存每层 token→expert histogram，隐私场景可只存聚合统计。
- 每条校准记录固定：硬件 SKU/BOM、固件、driver、CANN/HCCL、框架/模型 commit、环境变量、功耗模式、拓扑、温度、时间戳。

### 10.3 误差与不确定性

至少报告：

- kernel/collective/step 的 `MAPE`、median APE、P95 APE；
- 时间线分量误差和关键路径识别准确率；
- bootstrap 95% CI；
- 未见 shape/message/topology 的留出集误差；
- 对 `α/β/γ`、路由 CV、可用显存、power throttle、故障率、overlap efficiency 的 Sobol/局部敏感性；
- 100k 预测区间随校准层级扩大/收缩的变化。

验收建议：阈值不在研究阶段拍脑袋，先以业务决策容忍度定义。例如若两个方案差距小于模型 95% CI，则结论应为“不可区分”，不是选择数值更大的一个。

### 10.4 仿真能与不能回答什么

**校准后能回答：** 在明确 workload、版本和拓扑下，各并行/放置/重计算候选的相对 step time、显存峰值、流量、暴露通信、bubble 与故障敏感性。

**不能单独回答：** 未发布 SKU 的真实可用显存、编译器/kernel 未来优化、训练稳定性/收敛、真实路由分布、故障概率、功耗限频、跨 100k 的运维 goodput；这些必须来自真机或运营数据。

## 11. 最小可执行研究计划

### 阶段 0：关闭口径（无性能数字）

- 决定 10T/序列长度/精度/optimizer/路由字段，并固化已确认的 GTS schema；
- 取得 950DT SKU/BOM、可用 HBM、目标 SuperPoD 配置与跨域拓扑；
- 固定 SimAI、CANN、HCCL、训练框架 commit。

### 阶段 1：让模型“可计数”

- 实现 V4-Pro 派生 config schema 与 exact parameter/FLOP/communication/memory counter；
- 用官方 384/6 配置回归 1.6T/49B 数量级；
- 对 2048/16、`d_e=3072` 输出精确 total/active params 与逐模块分解。

### 阶段 2：最小 Ascend vertical slice

- 一个单层 MoE forward/backward trace；
- 一个 HCCL AllToAll + grouped expert GEMM；
- 一个 2 层拓扑（柜内/柜间）；
- SimAI 预测与真机在预先定义误差内一致。

### 阶段 3：单 SuperPoD

- 增加 PP/DP/CP、Muon/AdamW、selective recompute 与显存 timeline；
- 扫描 EP/ETP/EDP folding 和 rank placement；
- 校准均衡与真实路由 trace。

### 阶段 4：多 SuperPoD 与 100k

- 建立跨域模型、当前 98-domain 与架构情景 13-domain 的非整齐 placement、拥塞/故障；
- analytical 大规模搜索→flow/packet 抽样复核→真机校准；
- 只对 CI 能分离的方案给出“更快”结论。

## 12. 风险与开放问题

| 风险/开放问题 | 当前状态 | 影响 | 关闭证据 |
|---|---|---|---|
| 10T 量级参数尚未由代码精确计数 | 中 | 参数/显存/算力存在四舍五入偏差 | exact parameter counter + 已确认的 8.31–8.42T 配置 |
| GTS token 计数边界未实现 | 中 | padding/drop/replay 可能导致 DP/GA/step 统计偏差 | 固化 schema，并用有效 token 与调度 token 对照测试 |
| 950DT 可用显存未知 | 高 | 无法做“显存用满” | 目标 SKU `npu-smi`/allocator 实测 |
| 整机指标口径与单卡不可分解 | 高 | roofline 错误 | SKU datasheet + kernel benchmark |
| 跨 SuperPoD 拓扑/带宽未知 | 高 | 100k 关键路径不可判定 | 网络图、HCCL microbench |
| SimAI EP 均衡假设 | 高 | P99、buffer、drop 被低估 | 真实/合成路由 histogram |
| V4 混合 optimizer 状态 | 高 | 显存与通信错误 | 参数分组、dtype、ZeRO 代码/trace |
| 100k 超出公开验证域 | 高 | 误差无法界定 | 当前商品路径尽量逼近 98 SuperPoD 的分级校准与 CI |
| 1024 商品/8192 架构/8192 路线图混淆 | 高 | 卡数、机柜、链路、故障域全错 | 三套独立 topology manifest，禁止跨身份复用参数 |
| SimAI 显式 AllToAll 为 `O(EP²)` | 高 | 2048 EP 与 100k 事件爆炸 | 聚合通信矩阵/代表流与小规模守恒回归 |
| power/thermal throttling | 中高 | overlap 同时压满资源时降频 | 长稳态功耗/频率 trace |
| HCCL 版本与算法选择 | 中高 | collective 曲线漂移 | 固定版本、算法与回归库 |
| fault/recovery 未建模 | 中高 | 理论吞吐≠goodput | 故障与 checkpoint 数据 |

## 13. 可直接转成 Wayfinder 的决策票据

1. **D-001：10T 量级模型参数闭合。** 已决策：保持 expert intermediate size 3,072，接受约 8.31–8.42T。验收：参数计数器给出精确 total/active params 与逐模块分解。
2. **D-002：实现已确认的 GTS 口径。** 已决策：每个 optimizer step 的全局 token 数，`≤500M tokens/step`。验收：schema、公式、padding/drop/replay 计数和上限行为均有测试。
3. **D-003：950DT SKU 和可用内存口径。** 验收：BOM + 真机 allocator/npu-smi 证据。
4. **D-004：训练精度与 optimizer state matrix。** 验收：每参数组 dtype、bytes、shard/replica 规则闭合。
5. **D-005：V4 派生路由语义。** 验收：K=16、shared/hash expert、capacity/drop/balance 全定义。
6. **D-006：Ascend workload provider 的输入真值。** 验收：单层 trace 可重复转成 SimAI workload。
7. **D-007：HCCL collective 抽象与算法映射。** 验收：AllToAll/AR/AG/RS 在两层拓扑通过微基准回归。
8. **D-008：UnifiedBus/SuperPoD 拓扑 schema。** 验收：1024/8192/100k、残缺 domain 和故障域可表示。
9. **D-009：attention 与 MoE parallel folding。** 验收：两套 group 恒等式、rank map 与流量守恒测试通过。
10. **D-010：显存 peak timeline。** 验收：单卡实测峰值与各分量误差可接受，OOM 边界可复现。
11. **D-011：selective recompute 搜索空间。** 验收：GiB↔ms Pareto 曲线覆盖目标 shapes。
12. **D-012：EP 不均衡与 buffer 模型。** 验收：可重放真实 histogram，预测 P95/P99 与 drop/padding。
13. **D-013：超节点内/间 placement 策略。** 验收：跨域 bytes、collective time、step time 可归因。
14. **D-014：100k 多保真仿真策略。** 验收：analytical、flow、packet 三层抽样误差和运行成本明确。
15. **D-015：校准验收与不确定性政策。** 验收：留出集、CI、敏感性和“不可区分”规则固化。
16. **D-016：故障与训练 goodput。** 验收：checkpoint/restart/rank remap 对 100k goodput 可模拟。
17. **D-017：100k 是名义容量还是同时 active。** 验收：98,304-active+1,696 spare、100,000 ragged、98×1,024 capacity+352 spare 都有拓扑、communicator 与利用率结论。
18. **D-018：拓扑证据身份。** 验收：`CURRENT_PRODUCT=1024`、`ARCHITECTURE_LIMIT=8192`、`HISTORICAL_ROADMAP=8192` 分库存储，任何性能报告显示所用身份。

## 14. 参考文献

1. Alibaba Cloud, *SimAI* source, commit `f5efb5a93ea9be7db25a8843f9f7ff54044f6062`, 2026-04-24: <https://github.com/aliyun/SimAI/tree/f5efb5a93ea9be7db25a8843f9f7ff54044f6062>（访问 2026-08-11）。
2. X. Wang et al., *SimAI: Unifying Architecture Design and Performance Tuning for Large-Scale Large Language Model Training with Scalability and Precision*, NSDI 2025: <https://www.usenix.org/system/files/nsdi25-wang-xizheng-simai.pdf>（访问 2026-08-11）。
3. DeepSeek-AI, *DeepSeek-V4: Towards Highly Efficient Million-Token Context Intelligence*, arXiv:2606.19348v1, 2026-04-26: <https://arxiv.org/html/2606.19348v1>（访问 2026-08-11）。
4. DeepSeek-AI, *DeepSeek-V4-Pro config.json*, commit `45040942eb0d1c4e29fa6b92a6195f110e9e7444`: <https://huggingface.co/deepseek-ai/DeepSeek-V4-Pro/blob/45040942eb0d1c4e29fa6b92a6195f110e9e7444/config.json>（访问 2026-08-11）。
5. Huawei, *Groundbreaking SuperPoD Interconnect: Leading a New Paradigm for AI Infrastructure*, 2025-09-18: <https://www.huawei.com/en/news/2025/9/hc-xu-keynote-speech>（访问 2026-08-11）。
6. Huawei, *Huawei Unveiled the Latest SuperPoD, Making an AI Infrastructure New Option to the World*, 2026-03-02: <https://www.huawei.com/en/news/2026/3/mwc-superpod-ai>（访问 2026-08-11）。
7. Huawei, *昇腾950超节点真机亮相2026世界人工智能大会*, 2026-07-17: <https://www.huawei.com/cn/news/2026/7/atlas-950-superpod>（访问 2026-08-11）。
8. Huawei Ascend, *Algorithm Overview—HCCL*, CANN Commercial 8.5.0: <https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000115.html>（访问 2026-08-11）。
9. Huawei Ascend, *HCCL_ALGO*, CANN Commercial 8.5.0: <https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000075.html>（访问 2026-08-11）。
10. Huawei Ascend, *Profiling with Environment Variables*, CANN Commercial 8.5.0: <https://www.hiascend.com/document/detail/en/canncommercial/850/devaids/profiling/atlasprofiling_16_0055.html>（访问 2026-08-11）。
11. NVIDIA, *Megatron-Core Parallelism Strategies Guide*: <https://docs.nvidia.com/megatron-core/developer-guide/latest/user-guide/parallelism-guide.html>（rolling latest，访问 2026-08-11）。
12. NVIDIA, *Megatron-Core Mixture of Experts*: <https://docs.nvidia.com/megatron-core/developer-guide/nightly/user-guide/features/moe.html>（rolling nightly，访问 2026-08-11）。
13. NVIDIA, *Megatron-Core Distributed Optimizer*: <https://docs.nvidia.com/megatron-core/developer-guide/latest/user-guide/features/dist_optimizer.html>（rolling latest，访问 2026-08-11）。
14. Huawei Ascend, *昇腾 950 NPU 架构白皮书*, copyright 2026，PDF metadata 2026-06-04，正文无版本号: <https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf>（访问 2026-08-11）。
15. Huawei Ascend, *Atlas 950 SuperPoD 液冷超节点—产品特性与技术规格*, 页面无发布日期: <https://www.hiascend.com/hardware/cluster>（访问 2026-08-11）。
16. Huawei Ascend, *通信域初始化—HCCL*, CANN Community Edition 9.1.0-beta.3: <https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/api_comm_impl.md>（访问 2026-08-11）。
17. Huawei Ascend, *HCCL_ALGO*, CANN Community Edition 9.1.0-beta.3: <https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/hccl_env/HCCL_ALGO.md>（访问 2026-08-11）。
18. Huawei Ascend, *HCCL 集合通信性能测试工具/参数/结果*, CANN Community Edition 9.1.0-beta.3: <https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/devaids/hccltool/HCCLpertest_16_0001.html>（访问 2026-08-11）。
19. Huawei Ascend, *HCCL Profiling*, CANN Community Edition 9.0.0-beta.2: <https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900beta2/commlib/hcclug/hcclug_000014.html>（访问 2026-08-11）。

## 15. 研究完整性声明

- 本文没有把 Ascend 910、Atlas A2/A3 或 NVIDIA GPU 的数值冒充 Ascend 950DT；通用文档只用于接口/公式参考。
- 本文引用的 Atlas 950 性能均保留整机/展示配置口径，没有作为已校准的单卡输入。
- 本文把当前 1024 卡商品配置、8192 卡架构上限、8192 卡历史路线图分为独立证据身份；100k 当前商品路径按 98 个 SuperPoD，而不是 13 个。
- 本文没有给出 100k 卡的单点 token/s、MFU 或训练时长，因为公开一手证据不足以支持这些数值。
- 8.31–8.42T/89.3–90.0B 是透明公式推导，不是厂商发布规格；MTP 权重归属和最终数字必须由最终代码精确计数。
- 链接自检：54 个唯一 URL 中，华为/昇腾、USENIX、arXiv、Megatron 与 GitHub 固定链接已获得可达响应；Hugging Face 固定提交的 `config.json` 在本轮网络环境中未能重新拉取，字段已由 DeepSeek 官方技术报告交叉验证，但该固定链接仍标记为待网络复核。
- 任何未来性能结论都必须同时携带 workload schema、硬件/软件版本、校准集、误差与置信区间。
