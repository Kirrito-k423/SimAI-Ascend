# Ascend Hardware Profile 与 HCCL Cost Model 最小契约

> 状态：开发前研究决策；不是 A2/A3 校准结果，也不是 A5 性能结论
> 截止：2026-08-11（Asia/Shanghai）
> Upstream 基线：[`aliyun/SimAI@f5efb5a`](https://github.com/aliyun/SimAI/tree/f5efb5a93ea9be7db25a8843f9f7ff54044f6062)
> 证据边界：仅使用固定源码、Ascend/CANN/HCCL/MindSpeed 官方材料和正式规范；本项目当前 A5/950DT 无真机，在 `hardwareAvailable=false` 时永不标为 `MEASURED`

## 1. 结论

【工程决定】采用三个可独立版本化、可哈希的资源，而不是把规格、样本和拟合系数塞进一张 `busbw.yaml`：

1. `simai.ascend.profile/v1alpha1`：设备身份、软件 pin、容量、计算能力、传输能力和分层拓扑的**事实/假设快照**。
2. `simai.ascend.observation/v1alpha1`：不可变的 GEMM、内存传输、HCCL 和端到端**原始观测**；本票详细定义 HCCL 样本。
3. `simai.ascend.costmodel/v1alpha1`：从一组带哈希的 raw observation 得到的**拟合、插值或外推模型**。

这样分层的必要性是：同一份 raw 可以重拟合不同模型；换 CANN/HCCL、rank mapping 或拓扑不会污染旧样本；A5 可表达为 `USER_INPUT/VENDOR_SPEC + FIELD_UNVERIFIED`，而无需伪造“实测曲线”；模型适用域和误差也不会被误认为硬件事实。

| 候选 | 优点 | 致命问题 | 结论 |
|---|---|---|---|
| 沿用单一 `busbw.yaml` | 改动小 | 无单位、版本、拓扑、消息域、误差和 provenance；不能表达 A2AV 偏斜 | 拒绝作为 Ascend 真值 |
| Profile 内嵌 raw 与拟合 | 文件少 | 原始证据会随重拟合而变化，A5 假设容易冒充测量 | 拒绝 |
| Profile + immutable raw + derived model | 来源、测量与推导身份清楚，可独立迁移和缓存 | 需要 validator 和 registry | **采用** |

## 2. 一手证据约束

### 2.1 Upstream SimAI 的真实消费边界

【源码事实】固定 pin 的 [`GPUType`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/Common.hh#L14-L45) 只有 NVIDIA 型号；[`AstraParamParse.hh`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraParamParse.hh#L99-L170) 对未知型号落到 `NONE`。[`calbusbw`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/calbusbw.cc) 硬编码 NVLink、ConnectX、BlueField、NVLS 和 PCIe 常量；[`Layer::compute_time`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/workload/Layer.cc#L940-L1050) 再按 collective/rank 套用 bus-bandwidth 公式。

【源码事实】旧路径存在不能迁移到 HCCL 的隐含行为：

- `GBps` 实际定义为 `1/(1024³)`，而常量和输出名仍写 `GB/s`，存在 GB/s 与 GiB/s 语义混用。
- 未知节点数在 `getValue()` 中默认取 16-node 列；CSV 空格会被 `readCSV()` 填成 `1`；超出消息范围抛异常。
- `<1 MiB` 只为 2/4/8/16/32/64/128 ranks 硬编码时延，其他 rank 可返回 0。
- EP A2A 的特定跨机分支固定取经验列，DP/DP_EP ratio 恒为 1。
- [`example/busbw.yaml`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/example/busbw.yaml) 与 [`Tutorial`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/docs/Tutorial.md#L90-L115) 暴露 `-busbw`，但固定 pin 的参数解析源码没有对应分支；它是 legacy/documented surface，不能当成已证明可消费的稳定接口。
- overlap 是 report 阶段按 DP/TP/EP scalar 扣减 exposed time，不是 standalone collective 的固有属性。

【源码事实】[`ComputeAPI`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraComputeAPI.hh#L14-L26) 只有 `M/K/N + delay`，缺 dtype、accumulate dtype、layout、transpose、batch 和 op role；[`AstraMemoryAPI`](https://github.com/aliyun/SimAI/blob/f5efb5a93ea9be7db25a8843f9f7ff54044f6062/astra-sim-alibabacloud/astra-sim/system/AstraMemoryAPI.hh#L14-L22) 只有按 size 的读写延迟，没有容量、allocator 可用量、峰值或 lifetime。Profile 是未来 Provider/MemoryModel 的输入，不代表 Upstream 已经消费这些字段。

### 2.2 Ascend/HCCL 原生口径

【官方事实】HCCL Test 输出的 `data_size` 是单 NPU 参与通信的数据量（Bytes），`aveg_time` 是微秒，`alg_bandwidth` 是 GB/s；算法带宽按数据量/平均时间计算，并非物理链路带宽。[官方 HCCL 构建与结果说明](https://gitcode.com/cann/hccl/blob/406b6502dc3eaaeb36f835237738269119aba450/docs/en/build/build.md)

固定官方 [`oam-tools@dc30e7ed`](https://gitcode.com/cann/oam-tools/tree/dc30e7ed6053723117eca8814728dd691c7c884c/src/hccl_test) 进一步证明各算子的 buffer/bandwidth basis 不相同：AR 是本 rank buffer；AG 的带宽 basis 是聚合后的 `send × rank_size`；RS 是 `recv × rank_size`；A2A/A2AV 是本 rank 总 send buffer。[HCCL Test README](https://gitcode.com/cann/oam-tools/blob/dc30e7ed6053723117eca8814728dd691c7c884c/src/hccl_test/README_en.md)；[AG 源码](https://gitcode.com/cann/oam-tools/blob/dc30e7ed6053723117eca8814728dd691c7c884c/src/hccl_test/opbase_test/hccl_allgather_rootinfo_test.cc)；[RS 源码](https://gitcode.com/cann/oam-tools/blob/dc30e7ed6053723117eca8814728dd691c7c884c/src/hccl_test/opbase_test/hccl_reducescatter_rootinfo_test.cc)；[A2AV 源码](https://gitcode.com/cann/oam-tools/blob/dc30e7ed6053723117eca8814728dd691c7c884c/src/hccl_test/opbase_test/hccl_alltoallv_rootinfo_test.cc)。因此 native 字段必须原样保留，不能只保存一个含糊的 `bandwidth`。

【官方事实】HCCL 使用分层 collective 和 α–β–γ 分析口径，且算法受 server/supernode 层、产品与 `HCCL_ALGO` 影响。[CANN 8.5 算法概览](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000115.html)；[CANN 8.5 `HCCL_ALGO`](https://www.hiascend.com/document/detail/en/canncommercial/850/commlib/hcclug/hcclug_000075.html)。950PR/DT 的 beta 文档只证明接口/算法入口，不证明稳定版性能。[950PR/DT `HCCL_ALGO`](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta3/API/hcclug/docs/zh/user_guide/hccl_env/HCCL_ALGO.md)

【官方事实】`aclrtGetMemInfo` 返回 runtime 可见的 free/total bytes，且 total 排除系统保留内存；这与物理安装容量不是同一个量。[CANN 8.5 API](https://www.hiascend.com/document/detail/en/canncommercial/850/API/appdevgapi/aclcppdevg_03_0107.html)

【官方事实】950DT 白皮书同时存在容量/SKU 档位、UB/UBoE/PCIe 端口复用和双向物理峰值；这些值不能相加，也不能直接改名为 HCCL alg bandwidth。[昇腾 950 NPU 架构白皮书](https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf)

## 3. 公共 envelope 与 evidence model

三个资源均使用：

```yaml
apiVersion: simai.ascend.<profile|observation|costmodel>/v1alpha1
kind: <AscendHardwareProfile|HcclRawSample|HcclCostModel>
schemaSemver: 0.1.0
metadata:
  id: globally-unique-sanitized-id
  createdAt: 2026-08-11T00:00:00+08:00
  contentSha256: null
spec: {}
extensions: {}
```

时间为 [RFC 3339](https://www.rfc-editor.org/rfc/rfc3339)；对象哈希应在移除 `metadata.contentSha256` 后的 canonical JSON 上计算（推荐 [RFC 8785 JCS](https://www.rfc-editor.org/rfc/rfc8785)），再把结果写回，避免自引用。版本遵循 [Semantic Versioning 2.0.0](https://semver.org/spec/v2.0.0.html)。

### 3.1 Evidence class 与 readiness 正交

| `evidence.class` | 含义 | 可用于校准 |
|---|---|---|
| `MEASURED` | 在明确环境/方法下直接观测 | 仅在 readiness、correctness 与 domain 同时通过时 |
| `VENDOR_SPEC` | 厂商一手规格/文档 | 只能作 ceiling/constraint，不冒充实测 |
| `USER_INPUT` | 用户提供的目标 SKU/TFLOPS/带宽等 | 用于情景和敏感性 |
| `DERIVED` | 从已引用输入按确定公式得到 | 必须列依赖与公式 |
| `EXTRAPOLATED` | 超出观测 shape/rank/topology/software 域 | 必须给区间，不能称 calibrated |
| `LEGACY_ASSUMED` | 从旧 SimAI 常量/CSV/模糊单位迁移 | 只保兼容/对照，不进入 Ascend 真值拟合 |

| `readiness` | 含义 |
|---|---|
| `FIELD_VERIFIED` | 在精确硬件、软件和条件上完成要求的 probe/correctness gate |
| `SOURCE_VERIFIED` | 一手来源和引用已核验，但未在目标现场运行 |
| `FIELD_UNVERIFIED` | 已观测或已给定，但对应 ABI、workload、rank 或性能 gate 未完成 |
| `NOT_APPLICABLE` | 对该字段明确不适用 |

所以 `MEASURED + FIELD_UNVERIFIED`（例如盘点已读到容量，但目标 CANN lane/L0 未通过）合法；`VENDOR_SPEC + SOURCE_VERIFIED` 也合法。来源身份绝不自动提升 readiness。

### 3.2 数值对象 `Quantity`

每个物理/性能数值必须是以下对象或引用等价 evidence record：

```yaml
status: KNOWN              # KNOWN | UNKNOWN | NOT_APPLICABLE
value: 123                 # UNKNOWN 时必须为 null
unit: B                    # count|B|B/s|FLOP/s|ns|Hz|W|C|ratio
evidenceRef: evidence-id
readiness: FIELD_VERIFIED
uncertainty:
  kind: empirical_interval # none|empirical_interval|vendor_range|scenario_range|unknown
  lower: 120
  upper: 126
  confidence: 0.95
  sampleCount: 20
```

对应 `evidence[]` 记录必须有：`class`、`source.uri`、不可变 `source.ref`（commit/doc version/SHA-256）、`method.name/version`、`asOf`、`conditions`、`sanitization`。`DERIVED/EXTRAPOLATED` 还必须有 `inputRefs[]`、`formula/modelRef`。`status=KNOWN` 时 `evidenceRef` 必填；`status=UNKNOWN` 时 `value=null`、`uncertainty.kind=unknown`、`unknownReason` 必填，`evidenceRef` 可省略，若提供则仍必须可解析，不能用虚构的“待执行 gate”充当来源。规范单位只允许 **bytes、decimal B/s、FLOP/s、ns** 等表中 SI/显式单位；原生 `us`、`GB/s` 仅可放在 `native` 区，并同时提供无歧义 normalized 值。

## 4. `simai.ascend.profile/v1alpha1`

### 4.1 Normative 字段

`R`=必填；`C`=条件必填；`O`=可选。必填字段允许显式 `UNKNOWN` 的，会在“缺失行为”列说明。

| 字段 | 类型/必填 | 语义与允许 evidence | 缺失行为 |
|---|---|---|---|
| `metadata.id` | string/R | 脱敏 profile id | FAIL |
| `spec.identity.vendor` | enum/R | `HUAWEI_ASCEND` | FAIL |
| `identity.generation` | enum/R | `A2/A3/A5/UNKNOWN`；来源可 MEASURED/VENDOR_SPEC/USER_INPUT | FAIL |
| `identity.observedProductLabel` | string/O | 工具原样标签；不得推导营销 SKU | allowed |
| `identity.sku`、`chipRevision` | status+string/R | 可 UNKNOWN；每个已知值有 evidence | UNKNOWN，禁止猜测 |
| `identity.physicalChipCount`、`managementDeviceCount` | Quantity/R | `count`，MEASURED 或 USER_INPUT | 语法可 UNKNOWN；需要 rank mapping 时 FAIL |
| `rankGranularity.trainingRankUnit` | enum/R | `CHIP/MANAGEMENT_DEVICE/PACKAGE/UNKNOWN` | UNKNOWN 时分布式模型不可用 |
| `rankGranularity.trainingRanksPerUnit` | Quantity/R | 不能假设 A3 一 chip 一 rank | UNKNOWN 时 L2/L3 FAIL |
| `rankGranularity.rankMappingRef` | ArtifactRef/C | rank order、host/board/cabinet 映射的脱敏哈希 | rank>1 的模型 FAIL |
| `software.pins[]` | object/C | `component,version,build,commit,packageSha256,evidenceRef,readiness`；至少 driver/firmware/CANN/HCCL；训练另含 torch/torch_npu/MindSpeed/Megatron | MEASURED 样本缺精确 pin：不得入拟合 |
| `compute.capabilities[]` | object/O | `opId,dtype,accumulateDtype,layout,transposeA/B,batch,M/K/N domain,peakFLOP/s,observationRefs` | 请求该 op 时 UNKNOWN |
| `memory.hbm.installedCapacity` | scoped Quantity/R | 物理容量，MEASURED/VENDOR_SPEC/USER_INPUT；`scope` 必须为 `PER_PHYSICAL_CHIP/PER_PACKAGE/AGGREGATE_PROFILE` 之一 | 可 UNKNOWN；不能用于 feasibility |
| `memory.hbm.runtimeTotal` | scoped Quantity/R | `aclrtGetMemInfo` 可见总量，绑定软件/条件；通常为 `PER_RUNTIME_DEVICE` | 可 UNKNOWN；allocator feasibility FAIL |
| `memory.hbm.usableBudget` | scoped Quantity/R | allocator 可用上限，通常为 `PER_TRAINING_RANK`；`DERIVED`，引用 runtime total、guard 和实测峰值 | UNKNOWN 时所有“用满显存”配置 FAIL |
| `memory.hbm.guard` | Quantity/C | 保留/碎片/OOM guard | usable 已知时必填 |
| `memory.hbm.bandwidth[]` | object/O | direction、pattern、dtype、B/s、并发与 observation/spec ref | memory cost 请求时 UNKNOWN |
| `transfers[]` | object/O | `H2D/D2H/D2D`、host memory `PAGEABLE/PINNED`、path、size domain、concurrency、B/s/curve ref | offload/checkpoint 涉及时缺失=FAIL，否则 allowed |
| `topology.levels[]` | object/R | `CHIP/BOARD/HOST/CABINET/SUPERPOD/CLUSTER`、parent、members/ragged artifact | 通信 profile 至少到测量 scope；缺失=FAIL |
| `topology.links[]` | object/C | endpoints、link kind、方向、payload/line-rate、B/s、latency ns、oversubscription、sharedResourceGroup | 使用该 scope 时 FAIL |
| `topology.sharedResources[]` | object/C | UB/UBoE/PCIe 端口复用、共享 capacity、`EXCLUSIVE/CAPACITY_SHARED` 约束 | 有复用端口却缺失=FAIL |
| `clock.core/memory` | Quantity/O | Hz，绑定 power/thermal 条件 | 性能样本若受锁频控制则 C，否则 allowed |
| `power.limit/draw` | Quantity/O | W，draw 只能 MEASURED | 能耗目标或降频模型需要时 FAIL |
| `evidence[]` | Evidence/R | 所有 Quantity/事实的可解析 evidenceRef | dangling ref=FAIL |

【工程决定】link bandwidth 还必须携带 `bandwidthSemantics`：`ONE_WAY_PER_DIRECTION`、`AGGREGATE_BIDIRECTIONAL`、`PAYLOAD_EFFECTIVE` 或 `LINE_RATE`。相同 `sharedResourceGroup` 的 UB/UBoE/PCIe 能力不可相加；求和器遇到共享组必须按约束求解或返回 UNKNOWN。

## 5. HCCL raw sample schema

### 5.1 必填结构

```yaml
apiVersion: simai.ascend.observation/v1alpha1
kind: HcclRawSample
schemaSemver: 0.1.0
metadata: {id: sample-id, createdAt: null, contentSha256: null}
spec:
  profileRef: profile-id
  profileDigest: null
  softwareFingerprint: null
  harness: {name: hccl_test, version: null, commit: null, binarySha256: null}
  collective: ALL_REDUCE # ALL_GATHER|REDUCE_SCATTER|ALL_TO_ALL|ALL_TO_ALL_V|ALL_TO_ALL_VC
  group:
    rankCount: null
    scope: HOST # BOARD|HOST|CABINET|SUPERPOD|INTER_SUPERPOD|CUSTOM
    rankMappingDigest: null
    topologyDigest: null
  payload:
    bytesPerRank:
      semantics: API_INPUT_BYTES
      uniformValueBytes: null
      valuesArtifact: null
      summary: {min: null, p50: null, p90: null, max: null, cv: null}
    outputBytesPerRank: null
    dtype: BF16
    reduction: SUM # NONE for non-reduction
    traffic:
      semantics: UNIFORM_EQUAL_SPLIT
      countsUnit: ELEMENTS
      representation: GENERATOR # SPARSE_COO|DENSE_ARTIFACT|ROUTE_HISTOGRAM|SUMMARY_ONLY
      artifact: {uri: null, sha256: null, shape: null}
      summary: {zeroFraction: null, rowCv: null, columnCv: null, hotspotRatio: null}
  execution:
    requestedAlgorithm: null
    observedAlgorithm: null
    protocol: null
    acceleratorMode: null
    timingScope: DEVICE_ONLY # HOST_E2E
    environmentAllowlist: {}
    warmupIterations: 10
    measuredIterations: 20
    independentRepetitions: 1
  raw:
    iterationTimesNs: []
    stdoutArtifact: {uri: null, sha256: null}
    native:
      dataSizeBytes: null
      avegTimeUs: null
      algBandwidthGBps: null
      bandwidthBasisBytes: null
  normalized:
    averageTimeNs: null
    algBandwidthBps: null
    p50TimeNs: null
    p90TimeNs: null
    coefficientOfVariation: null
  correctness: {status: NOT_RUN, checkLevel: null, failedRanks: null}
  eligibility: {fit: false, reasons: []}
  evidence: []
```

### 5.2 `bytesPerRank` 的唯一语义

【工程决定】`API_INPUT_BYTES` 指“每个 rank 传给 collective API 的逻辑 input buffer 总字节，完成 dtype count 对齐后、HCCL 算法展开前”：

| op | canonical `bytesPerRank` | output |
|---|---|---|
| AR | `count × typeSize` | 同 input |
| AG | `sendCount × typeSize`（本 rank contribution） | `input × P` |
| RS | `recvCount × typeSize × P`（完整 reduction input） | `input / P` |
| A2A | `sendCountPerPeer × P × sendTypeSize` | 本 rank total recv |
| A2AV | `Σ_j sendCounts_i[j] × sendTypeSize`；通常是每 rank 不同的向量 | `Σ_j recvCounts_i[j] × recvTypeSize` |

HCCL Test 的 native `data_size`/bandwidth basis 按工具和 op 原样保存，**不得**用 native `data_size` 覆盖 canonical `bytesPerRank`。`algBandwidthBps = native algBandwidthGBps × 10^9`，但该字段仍是算法带宽，不是 UB/HCCS/PCIe 物理带宽。

### 5.3 A2AV/偏斜

AllToAllV 的 counts/displacements 以**元素**为单位；全局通信可描述为 `P×P` counts，但 schema 不强迫内嵌 O(P²) JSON。允许：

- 小规模 uniform 用 `GENERATOR`；
- 精确重放用带 SHA-256 的 dense 或 sparse COO artifact；
- 训练 route trace 用脱敏 histogram/artifact；
- 只有摘要时用 `SUMMARY_ONLY`，但不能训练 pairwise/skew-aware model。

至少保存每 rank row/column bytes 的 min/P50/P90/max/CV、zero fraction、`max/mean` hotspot ratio。全局 send/recv 总字节不守恒、dtype size 不匹配或 artifact hash 不匹配均 FAIL。

### 5.4 重复、统计和正确性

native `aveg_time` 必须保留；若 wrapper 能取逐次时间，则保存 warmup 后的 `iterationTimesNs`，P50/P90 用固定 `linear_type7`，CV=`sample_std/mean`。样本变异超过 10% 时按项目既定规则扩展到 10 次独立测量并报告 P90。`NULL`、overflow skip、未校验或任一 rank failed 的记录可归档，但 `eligibility.fit=false`。

## 6. Derived HCCL cost model

```yaml
apiVersion: simai.ascend.costmodel/v1alpha1
kind: HcclCostModel
schemaSemver: 0.1.0
metadata: {id: model-id, createdAt: null, contentSha256: null}
spec:
  profileDigest: null
  softwareFingerprint: null
  collective: ALL_REDUCE
  dtype: BF16
  reduction: SUM
  timingScope: DEVICE_ONLY
  groupDomain:
    rankCounts: []
    scopes: []
    topologyDigests: []
    rankMappingDigests: []
  trafficDomain: {semantics: UNIFORM_EQUAL_SPLIT, skewRange: null}
  messageDomainBytes: {min: null, max: null}
  algorithmDomain: {requested: null, observed: null, protocol: null, acceleratorMode: null}
  inputSamples: [] # {id, sha256}
  fit:
    family: PIECEWISE_MONOTONE # ALPHA_BETA|ALPHA_BETA_GAMMA|LOOKUP
    formula: null
    segments: []
    interpolation: LOG_BYTES_LINEAR_TIME
  validation:
    split: null
    mape: null
    p90Ape: null
    bootstrap95: null
  evidenceClass: DERIVED
  extrapolation: {allowed: false, policy: FAIL, uncertainty: null}
```

【工程决定】raw 永不被 fit 回写。每个 segment 显式给消息、rank、scope、拓扑、软件、算法和 traffic domain；`α/β/γ` 只在可辨识时使用，否则用单调分段曲线/lookup。插值只能发生在**完全相同的** collective、dtype/reduction、timing scope、rank count、topology/rank mapping family、software fingerprint、algorithm/protocol 和 traffic class 内。

跨 rank、拓扑层、CANN/HCCL、A2→A3、A3→A5 或 uniform→skew 都是外推：必须产生新的 `EXTRAPOLATED` model，列出变换、区间和 sensitivity；禁止 registry 静默选邻近模型。

训练 overlap 单独建模：`standalone HCCL time` 来自本资源；`exposed fraction/critical-path delay` 由 L3 trace、phase、stream、bucket、compute concurrency 拟合。不得把 Upstream 的 scalar overlap ratio 混进 raw 或改写 α/β。

## 7. Validator invariants

| invariant | 结果 |
|---|---|
| 未知 major/apiVersion、缺 id、dangling evidence/artifact ref、非法单位 | FAIL |
| 已知数值缺 source/ref/method/as_of/conditions/readiness | FAIL |
| 当前项目中 `hardwareAvailable=false` 或 `status=BLOCKED_MISSING_USER_INPUT` 的 A5/950DT Profile 出现 `MEASURED` | FAIL；未来只有绑定真实目标硬件 fingerprint、方法与 raw artifact 的 A5 样本才能标 MEASURED |
| 同一 scope（或有已验证 scope 映射）下 `usableBudget > runtimeTotal` 或 `runtimeTotal > installedCapacity` | FAIL；scope 不可比时返回 UNKNOWN |
| 同一 shared resource 的 UB/UBoE/PCIe 物理峰值被相加 | FAIL |
| measured HCCL 样本缺 driver/firmware/CANN/HCCL/harness pin | 可归档，fit FAIL |
| rank>1 缺 rank mapping/topology digest | 可归档，fit FAIL |
| correctness 非 PASS、native `NULL` 或 failed rank>0 | 可归档，fit FAIL |
| 有精确 generator/counts artifact 的 A2AV row/column 总量不守恒或 artifact hash 错 | FAIL；`SUMMARY_ONLY` 无法证明守恒且 fit=false |
| native 单位仅写 `GB/s/us` 而无 normalized B/s/ns | FAIL |
| `alg_bandwidth` 被标为 physical link bandwidth | FAIL |
| model 请求超出 message/rank/topology/software/traffic domain | UNKNOWN；严格仿真 FAIL |
| workload 不使用 H2D/D2H，而 transfer 缺失 | allowed；一旦 offload/checkpoint 依赖即 FAIL |
| clock/power 缺失 | 默认 allowed；能耗或限频研究为 UNKNOWN/FAIL |
| legacy 未声明 `legacyUnitSemantics` | FAIL；CSV 空 cell 导入为 UNKNOWN，未知 rank/default column 或对 UNKNOWN 点求值时 FAIL，绝不填 `1` 或落到 16-node |
| 未识别 extension | 保留并 WARN；核心未知字段不丢弃、不执行 |

## 8. 三个脱敏 YAML 骨架

以下是 schema 用法，不是新增测量；数字只复述已公开的[脱敏能力矩阵](./2026-08-11-a2-a3-capability-matrix.md)。

### 8.1 A2 measured inventory（性能仍 UNKNOWN）

```yaml
apiVersion: simai.ascend.profile/v1alpha1
kind: AscendHardwareProfile
schemaSemver: 0.1.0
metadata: {id: a2-calibration-sanitized, createdAt: "2026-08-11T00:00:00+08:00", contentSha256: null}
spec:
  status: INCOMPLETE
  identity:
    vendor: HUAWEI_ASCEND
    generation: A2
    observedProductLabel: 910B2
    sku: {status: UNKNOWN, value: null}
    chipRevision: {status: UNKNOWN, value: null}
    physicalChipCount: {status: KNOWN, value: 8, unit: count, evidenceRef: inv-a2, readiness: FIELD_VERIFIED}
    managementDeviceCount: {status: KNOWN, value: 8, unit: count, evidenceRef: inv-a2, readiness: FIELD_VERIFIED}
  rankGranularity: {trainingRankUnit: CHIP, trainingRanksPerUnit: {status: UNKNOWN, value: null, unit: count, readiness: FIELD_UNVERIFIED, unknownReason: L0 rank enumeration not run, uncertainty: {kind: unknown}}}
  software:
    pins:
      - {component: CANN, version: 8.5.0, evidenceRef: inv-a2, readiness: FIELD_UNVERIFIED}
  compute: {capabilities: []}
  memory:
    hbm:
      installedCapacity: {status: KNOWN, value: 68719476736, unit: B, scope: PER_PHYSICAL_CHIP, evidenceRef: inv-a2, readiness: FIELD_VERIFIED}
      runtimeTotal: {status: UNKNOWN, value: null, unit: B, scope: PER_RUNTIME_DEVICE, readiness: FIELD_UNVERIFIED, unknownReason: aclrtGetMemInfo not run, uncertainty: {kind: unknown}}
      usableBudget: {status: UNKNOWN, value: null, unit: B, scope: PER_TRAINING_RANK, readiness: FIELD_UNVERIFIED, unknownReason: allocator probe not run, uncertainty: {kind: unknown}}
  transfers: []
  topology: {levels: [], links: [], sharedResources: []}
  evidence:
    - id: inv-a2
      class: MEASURED
      source: {uri: ./2026-08-11-a2-a3-capability-matrix.md, ref: ccf0cdd82a9484044869ad77d40786862bcded1b}
      method: {name: sanitized-read-only-inventory, version: 1}
      asOf: 2026-08-11T00:00:00+08:00
      conditions: {loadState: idle-snapshot}
      sanitization: aggregate-only
```

### 8.2 A3 measured but field-unverified

```yaml
apiVersion: simai.ascend.profile/v1alpha1
kind: AscendHardwareProfile
schemaSemver: 0.1.0
metadata: {id: a3-validation-sanitized, createdAt: "2026-08-11T00:00:00+08:00", contentSha256: null}
spec:
  status: INCOMPLETE
  identity:
    vendor: HUAWEI_ASCEND
    generation: A3
    observedProductLabel: 9382-Ascend910-V1
    sku: {status: UNKNOWN, value: null}
    chipRevision: {status: UNKNOWN, value: null}
    managementDeviceCount: {status: KNOWN, value: 8, unit: count, evidenceRef: inv-a3, readiness: FIELD_UNVERIFIED}
    physicalChipCount: {status: KNOWN, value: 16, unit: count, evidenceRef: inv-a3, readiness: FIELD_UNVERIFIED}
  rankGranularity: {trainingRankUnit: UNKNOWN, trainingRanksPerUnit: {status: UNKNOWN, value: null, unit: count, readiness: FIELD_UNVERIFIED, unknownReason: 16-chip training rank mapping not verified, uncertainty: {kind: unknown}}}
  software:
    pins:
      - {component: CANN, version: 9.1.0-beta.1, evidenceRef: inv-a3, readiness: FIELD_UNVERIFIED}
  memory:
    hbm:
      installedCapacity: {status: KNOWN, value: 68719476736, unit: B, scope: PER_PHYSICAL_CHIP, evidenceRef: inv-a3, readiness: FIELD_UNVERIFIED}
      runtimeTotal: {status: UNKNOWN, value: null, unit: B, scope: PER_RUNTIME_DEVICE, readiness: FIELD_UNVERIFIED, unknownReason: aclrtGetMemInfo not run, uncertainty: {kind: unknown}}
      usableBudget: {status: UNKNOWN, value: null, unit: B, scope: PER_TRAINING_RANK, readiness: FIELD_UNVERIFIED, unknownReason: allocator probe not run, uncertainty: {kind: unknown}}
  topology: {levels: [], links: [], sharedResources: []}
  evidence:
    - id: inv-a3
      class: MEASURED
      source: {uri: ./2026-08-11-a2-a3-capability-matrix.md, ref: ccf0cdd82a9484044869ad77d40786862bcded1b}
      method: {name: sanitized-read-only-inventory, version: 1}
      asOf: 2026-08-11T00:00:00+08:00
      conditions: {targetStableLaneMatched: false}
      sanitization: aggregate-only
```

### 8.3 A5 Estimated Profile（无数字，不伪造测量）

```yaml
apiVersion: simai.ascend.profile/v1alpha1
kind: AscendHardwareProfile
schemaSemver: 0.1.0
metadata: {id: a5-950dt-user-estimate-template, createdAt: "2026-08-11T00:00:00+08:00", contentSha256: null}
spec:
  status: BLOCKED_MISSING_USER_INPUT
  hardwareAvailable: false
  identity:
    vendor: HUAWEI_ASCEND
    generation: A5
    observedProductLabel: null
    sku: {status: UNKNOWN, value: null}
    chipRevision: {status: UNKNOWN, value: null}
    physicalChipCount: {status: UNKNOWN, value: null, unit: count, readiness: FIELD_UNVERIFIED, unknownReason: target deployment input missing, uncertainty: {kind: unknown}}
    managementDeviceCount: {status: UNKNOWN, value: null, unit: count, readiness: FIELD_UNVERIFIED, unknownReason: target deployment input missing, uncertainty: {kind: unknown}}
    targetFamily: {value: Ascend-950DT, evidenceRef: user-target, readiness: FIELD_UNVERIFIED}
  rankGranularity: {trainingRankUnit: UNKNOWN, trainingRanksPerUnit: {status: UNKNOWN, value: null, unit: count, readiness: FIELD_UNVERIFIED, unknownReason: no A5 runtime, uncertainty: {kind: unknown}}}
  compute:
    capabilities:
      - {opId: GEMM, dtype: null, peakFLOPsPerS: {status: UNKNOWN, value: null, unit: FLOP/s, readiness: FIELD_UNVERIFIED, unknownReason: user value not provided, uncertainty: {kind: unknown}}}
  memory:
    hbm:
      installedCapacity: {status: UNKNOWN, value: null, unit: B, scope: PER_PHYSICAL_CHIP, readiness: FIELD_UNVERIFIED, unknownReason: target SKU input missing, uncertainty: {kind: unknown}}
      runtimeTotal: {status: UNKNOWN, value: null, unit: B, scope: PER_RUNTIME_DEVICE, readiness: FIELD_UNVERIFIED, unknownReason: no A5 runtime, uncertainty: {kind: unknown}}
      usableBudget:
        status: UNKNOWN
        value: null
        unit: B
        scope: PER_TRAINING_RANK
        evidenceRef: derived-usable
        readiness: FIELD_UNVERIFIED
        unknownReason: runtimeTotal and guard are unknown
        uncertainty: {kind: unknown}
  transfers:
    - {direction: H2D, bandwidth: {status: UNKNOWN, value: null, unit: B/s, readiness: FIELD_UNVERIFIED, unknownReason: user value not provided, uncertainty: {kind: unknown}}}
    - {direction: D2H, bandwidth: {status: UNKNOWN, value: null, unit: B/s, readiness: FIELD_UNVERIFIED, unknownReason: user value not provided, uncertainty: {kind: unknown}}}
  topology:
    levels: []
    links: []
    sharedResources:
      - {id: ub-u_boe-pcie-port-pool, constraint: UNKNOWN, evidenceRef: vendor-whitepaper, readiness: SOURCE_VERIFIED}
  evidence:
    - id: user-target
      class: USER_INPUT
      source: {uri: "input-manifest://a5-target", ref: pending}
      method: {name: explicit-user-supplied-capability, version: 1}
      asOf: 2026-08-11T00:00:00+08:00
      conditions: {hardwareAvailable: false}
      sanitization: no-host-data
    - id: vendor-whitepaper
      class: VENDOR_SPEC
      source: {uri: "https://public-download.obs.cn-east-2.myhuaweicloud.com/ascend/%E6%98%87%E8%85%BE950%20NPU%E6%9E%B6%E6%9E%84%E7%99%BD%E7%9A%AE%E4%B9%A6.pdf", ref: "sha256:ece3405e6a17fabdd462338fb94266558649a6407a2f28008403211387b3a927"}
      method: {name: primary-source-schema-extraction, version: 1}
      asOf: 2026-08-11T00:00:00+08:00
      conditions: {claimScope: product-family, hardwareAvailable: false}
      sanitization: public-source
    - id: derived-usable
      class: DERIVED
      source: {uri: "formula://runtime-total-minus-guard", ref: v1}
      method: {name: subtraction, version: 1}
      inputRefs: [user-target]
      formula: usableBudget=runtimeTotal-guard
      asOf: 2026-08-11T00:00:00+08:00
      conditions: {dependenciesKnown: false}
      sanitization: no-host-data
```

## 9. Upstream adapter 与兼容规则

【工程决定】新增 profile/provider seam，不把 Ascend 塞进 NVIDIA `GPUType` 默认分支：

1. `--device-profile <file>` 选择 `ASCEND_PROFILED` provider；legacy `-g_type/-nv/-nic` 继续选择原 GPU/NCCL 路径。两类参数冲突即 FAIL。
2. `calbusbw`、NVLink/NIC ratio CSV 和小消息硬编码只属于 `NVIDIA_LEGACY`；Ascend 路径绝不调用，未知设备绝不回落到 H100/A100 常量。
3. `HcclCollectiveProvider::estimate()` 输入 op、canonical bytes-per-rank、dtype/reduction、rank/group/scope、topology/rank mapping、software/algorithm/traffic fingerprint，输出 `time_ns + model_id + domain_status`。未覆盖返回 UNKNOWN，不能返回 0。
4. AR/AG/RS/A2A legacy scalar 映射时，SimAI `data_size` 分别按 AR input、AG gathered output、RS input、A2A total input 解释；A2AV 必须走 count artifact/provider，不能压成一个平均 scalar 后声称保真。
5. `Layer::compute_time` 的 measured/provider latency 是最终 standalone collective time；不再先转 busbw 再套 NVIDIA ring factor。若需要报告 algbw，只按明确 op basis 派生并标 `DERIVED`。
6. Compute observation 至少以 `opId,dtype,accumulateDtype,layout,transposeA/B,batch,M,K,N` 为 key；MemoryModel 分开 installed/runtime-total/usable/peak/lifetime。旧 API 仅由 adapter 接受降级输入，不能反向宣称字段已被 Upstream 支持。
7. Legacy importer 必填 `legacyUnitSemantics=DECIMAL_GBPS|BINARY_GIBPS|UNKNOWN`、源码 pin 和原字段；`UNKNOWN` 只可生成 `LEGACY_ASSUMED`、不可进入 Ascend fit。CSV 空 cell 导入为显式 UNKNOWN；只有查询该空点、未知 rank 默认列、超域或字段冲突时 FAIL，不能复刻旧代码的填 `1`/落 16-node 行为。

Schema 兼容：`schemaSemver` patch 只修验证器；minor 只加可选字段/enum capability，consumer 必须保留未知 `extensions`；删除/改语义只在 major。字段至少跨一个 minor 标 deprecated；迁移必须输出 source/target digest 和 loss report。已知资源 load→save 必须 round-trip 保留未知字段、native 字段和 artifact ref；GPU/NCCL golden tests 必须字节级或指标容差内不回归。

## 10. 下游消费矩阵

| 后续票 | 本票输入 |
|---|---|
| A2 Ground Truth L1 | compute key、HBM runtime/usable、clock/power conditions；另建 immutable kernel samples |
| A2/A3 L2 | 完整 HCCL raw schema、correctness、software/topology fingerprint、A2AV traffic artifact |
| A2/A3 L3 | profile id、standalone model id、overlap/route/memory lifetime 的独立 trace |
| Provider seam | profile registry、strict domain lookup、legacy 隔离和 UNKNOWN 传播 |
| A5 敏感性 | USER_INPUT TFLOP/s、HBM/usable、H2D/D2H、每层 link/shared-resource、取值区间；全部 FIELD_UNVERIFIED |
| 100k 拓扑 | level/link/shared resource、rank mapping、ragged group artifact；跨 SuperPoD model 必须显式 EXTRAPOLATED |

## 11. 决策、风险和最低成本 probe

### 已决

- Profile、raw observation、derived model 三层；raw append-only/immutable。
- Evidence class 与 readiness 正交；当前无真机的 A5 Profile 永不 `MEASURED`，未来只有目标硬件 fingerprint、方法和 raw artifact 均可复核时才能升级为实测证据。
- 全部规范时间/容量/速率使用 ns/B/B/s/FLOP/s；native HCCL 字段原样并存。
- HCCL model 以 latency curve 为主，algbw 只作 native/derived 指标；overlap 独立。
- A2AV exact counts 外置哈希，摘要用于搜索；不得静默跨 rank/topology/software/traffic 外推。

### `FIELD_UNVERIFIED/UNKNOWN`

1. A2/A3 目标软件 lane 的 L0、training rank granularity、实际 rank mapping。
2. 两端 HCCL Test 的匹配版本 build、算法实际选择、P50/P90/CV 和正确性曲线。
3. A2/A3 的 runtime total/allocator usable HBM、H2D/D2H、GEMM/Grouped GEMM 曲线。
4. A5/950DT 精确 SKU、TFLOP/s、可用 HBM、H2D/D2H、实际端口配置、HCCL/CCU 曲线与跨 SuperPoD 拓扑。
5. 100k 下 rank/ragged group、contention、外推误差和 overlap。

### 最低成本 probe 顺序

1. A2/A3 各自 L0：只读版本 pin、`aclrtGetMemInfo`、rank unit/mapping；失败即停。
2. A2 L1：固定 BF16 GEMM/Grouped GEMM shape，记录完整 key、time ns、HBM peak。
3. A2 L2：AR/AG/RS/A2A/A2AV，rank 2/4/8、warmup 10、measure 20、correctness on；变异>10% 才扩为 10 次独立测量。
4. 冻结 schema/model 后在 A3 重放，不参与 A2 拟合；再做 L3 overlap。
5. 用户提供 A5 TFLOP/s、HBM/usable、H2D/D2H 和 link/shared-resource 范围后，只做 `USER_INPUT/EXTRAPOLATED` 敏感性，直到有 A5 真机再升级 readiness。

本规范不证明 A3 30% Exploration Accuracy Gate、A5 性能或 100k 结论；它只确保这些后续结论能追溯到正确单位、版本、适用域与证据身份。
