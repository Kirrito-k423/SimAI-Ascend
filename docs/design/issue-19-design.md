# Issue #19：10T Target Workload、GTS 与显存契约设计

## 1. 目标、范围与依赖

本设计实现 [Issue #19](https://github.com/Kirrito-k423/SimAI-Ascend/issues/19)，父规格为 [Issue #15](https://github.com/Kirrito-k423/SimAI-Ascend/issues/15)，建立在 #16 的 Shared Run Contract、#17 的首个 Ascend Analytical vertical slice 与 #18 的完整 HCCL collective 契约之上。目标是在不建立旁路模拟器的前提下，让真实 `SimAI_analytical` 消费一个内容寻址的 V4-Pro 风格 10T-scale Target Workload，并在 `simai.result/v1` 中给出可审计的模型/GTS 身份、符号化或已物化的训练显存，以及明确的容量 gate。

本票包含：

- 冻结的 Model Manifest：`8,414,884,746,526` 个逻辑可训练参数、2,048 routed experts、TopK 16、expert width 3,072、1 shared expert；
- `sequenceTokens × microBatchSequences × dataParallelReplicas × gradientAccumulation` 的唯一 GTS 定义及 500,000,000-token 上界；
- Model、Step、Routing Artifact、Memory Event Plan 四资源的逐层 SHA-256 闭包、provenance、evidence 与 readiness；
- 参数、梯度、优化器状态、激活、通信缓冲、专家放置和重计算七类显存的独立输出；
- 未绑定 policy 的 `SYMBOLIC/UNKNOWN` 传播，以及 policy 全绑定后的容量守恒与 95%/85% gate；
- #16–#18 的 legacy GPU、typed workload decoder 和五类 collective 回归。

本票不生成 #20 的 `ProjectedA2ATraffic`，不实现 #21 的多保真 Top-5 搜索，也不实现 #28 的复合验收。它不拟合显存数值、不声称做过 A2/A3 NPU 测量、不把 checkpoint 存储格式推断成训练 precision，并且不改变 #18 的 HCCL cost provider。

## 2. 4+1 架构视图

### 2.1 逻辑视图

唯一产品 seam 仍是：

```text
simai.run/v1 -> real SimAI_analytical process -> simai.result/v1
```

`target_workload` 是 Run Manifest 内的组合身份，不是第五份可变资源。它按固定顺序组合四个资源 digest：Model 定义 canonical tensor-type registry 与模型身份；Step 绑定 Model 并定义 GTS；Routing 绑定 Model+Step 并只描述外部 routing policy；Memory 绑定 Model+Step+Routing 并描述七类对象的 lifetime、符号表达式和 policy 依赖。入口先对组合 envelope/reference shape 做 exact-key 校验，再进行四个有明确 byte limit 的单流 `max+1` 读取；因此非法 envelope 不触碰其指向的文件、FIFO 或设备。资源从叶到根逐层校验，最后要求真实 AICB header 及每层 event 都携带同一组四资源 digest 和 composite digest。任一缺失、摘要不一致、证据不完整或 schema 不闭合都会 fail closed；先通过的下层资源仍保留其 `READY` 身份，失败层与组合层标记为 `BLOCKED`。

模型逻辑参数、checkpoint auxiliary elements、checkpoint storage bytes 和 active logical parameters 都从 76 项 canonical tensor type registry 的 logical/storage shape、dtype、role、scope、kind、instances 受检查重建，不信任声明汇总。active logical parameters 分成三个明确 scope；fixed-quantized checkpoint bytes 只作为模型 provenance 输出。JSON parser 保留 number 原始词法，Target 整数消费端只接受 canonical unsigned decimal，并做 JSON-safe/range/checked arithmetic 校验；HCCL 的合法浮点字段仍走既有 `double` 路径。

Memory Event Plan 有两种合法状态：

1. precision、optimizer、placement、recomputation、runtime 全为 `UNBOUND`：七类显存保留规范表达式和 `UNKNOWN`，容量与 gate 传播 `UNKNOWN`；
2. 五个 binding 全为 `sha256:<64 hex>`：七类 `peakBytes`、capacity 与 observed execution peak 才可消费，并验证总峰值、usable HBM 和整数边界。

部分绑定从不产生半可信的峰值。`readiness.hbm=READY` 仅表示这份显式物化输入足够执行 contract gate，不表示它已被真实设备测量；证据仍是 `USER_INPUT/FIELD_UNVERIFIED`。

### 2.2 开发视图

- `analytical/RunContract.h`：在 #16–#18 的 `AnalyticalRunContract` 上保存 Target 四资源身份、GTS、模型计数、显存分项和 gate 状态。
- `analytical/RunContract.cc`：复用既有 JSON parser、SHA-256、evidence/readiness 与 Result writer；新增 exact schema、四资源 bounded loader、tensor registry 重建、checked arithmetic、composite/AICB binding validator 和显存 gate。
- `workload/WorkloadCollectiveDecoder.hh`：共享解析 legacy 12/13-column 与 target-bound 17/18-column AICB event；header 和 event 都携带五个 digest。
- `workload/Workload.{hh,cc}` 与 `Layer.{hh,cc}`：真实 runtime 用同一 decoder 消费 binding，逐层 exact 比对，并把 event binding 存入实际 `Layer`，不是 Result writer 事后合成。
- `tests/contract/fixtures/target_*.json` 与 `target_10t_workload.txt`：保存公开来源或纯合成的脱敏 Model/Step/Routing/Memory/Run/AICB fixture；无主机、IP、账号、凭据或现场日志。
- `tests/contract/test_analytical_run_contract.py`：通过临时复制和重算引用 digest 构造正负例，只观察真实进程退出码和 Result Manifest；参数、GTS、内存边界另用独立 Python 算术 oracle。
- `docs/design/issue-19-design.md`：冻结最终数据契约、对象协作、失败语义和后续边界。

本票不新增编译单元；Analytical CMake 的 glob 仍产生 56 个 AstraSim library 源和 5 个 frontend 源，共 61 个源。未提供 `--run-manifest` 时，#16 的 legacy CLI 继续权威，不触发 Target validator。

### 2.3 进程视图

```mermaid
flowchart TD
    A["启动真实 SimAI_analytical"] --> B["读取 simai.run/v1；校验 workload digest"]
    B --> C{"存在 target_workload?"}
    C -- "否" --> L["#16-#18 legacy GPU / HCCL 路径"]
    C -- "是" --> V["先 exact 校验 envelope 与四个 ref；此时无 artifact I/O"]
    V --> D["bounded 加载 Model；从 tensor registry 重建 logical/storage/aux/evidence"]
    D --> E["加载 Step；校验 Model digest 与 GTS checked product"]
    E --> F["加载 Routing；校验 Model+Step digest 与 policy"]
    F --> G["加载 Memory；校验 Model+Step+Routing digest、lifetime、binding"]
    G --> H["按固定顺序计算四资源 composite digest"]
    H --> I{"AICB header 与每层 event 的五 digest 都一致?"}
    I -- "否" --> X["写 INVALID_INPUT/UNSUPPORTED + BLOCKED Result"]
    I -- "是" --> U["Workload decoder 再消费 binding；传播到真实 Layer"]
    U -- "全部 UNBOUND" --> S["输出七类 SYMBOLIC/UNKNOWN；HBM UNKNOWN"]
    U -- "全部 BOUND" --> M["验证 component sum、base/reserve/usable 与 observed peak"]
    M --> N{"planned peak <= floor(usable*95/100)?"}
    N -- "否" --> X
    N -- "是" --> O{"observed peak 已提供?"}
    O -- "否" --> R["search PASS；execution UNKNOWN"]
    O -- "是且 < 85% base" --> P["search PASS；execution PASS"]
    O -- "是且 >= 85% base" --> Y["写 INVALID_ACCURACY_EXECUTION"]
    L --> W["写 simai.result/v1"]
    S --> W
    R --> W
    P --> W
    X --> W
    Y --> W
```

解析与资源 gate 都发生在构造/运行 `Sys` 前。有效 Target Workload 仍让真实 Upstream workload 执行；Target contract 补充 workload 身份和显存可行性，不替代 #17/#18 的 collective cost seam。

### 2.4 物理视图

实现仅依赖本地 CPU、文件系统、C++17 验证工具链和现有 Analytical 二进制；生产代码没有网络/NPU依赖。artifact path 只用于输入解析，Result 仅输出内容摘要、受控 resource id、枚举和数字，不回显本地路径或进程日志。

开发验证在 macOS arm64 上以与正式 CMake glob/exclude 等价的 61-source Clang 链接完成。没有运行真实 NPU、CANN 或远端命令，因此没有机器占用或 NPU 文件锁等待。

### 2.5 场景视图（+1）

1. 冻结模型：76 项 registry、764,124 个 tensor instances 独立重建得到 `8,414,884,746,526` logical trainable、`262,134,842,368` auxiliary elements 和 `4,486,847,493,752 B` checkpoint storage；Result 同时输出 `88,950,053,982`、`90,803,533,923`、`92,345,423,134` 三种 active scope。
2. GTS 恰好边界：`15625 × 1 × 32 × 1000 = 500,000,000`，Run 有效；routed assignment slot 上界是 `500M × 16 × 62 = 496,000,000,000`，明确不是 observed routing 或 network traffic。
3. GTS 越界或无效：500,000,001 稳定返回 `TARGET_STEP_GTS_LIMIT_EXCEEDED`；零、负数、非整数、JSON unsafe integer、乘法溢出和声明不一致分别在对应 gate 失败。
4. 未绑定显存：五个 policy 全 `UNBOUND`，七类结果为 `SYMBOLIC/UNKNOWN`，`hbm_peak_B` 与两个 gate 均为 `UNKNOWN`；checkpoint 的 4,486,847,493,752 B 仍只出现在 checkpoint storage。
5. 95% 搜索边界：base=2001 B、reserve=1000 B，scenario usable=1001 B，`floor(1001×95/100)=950 B`；planned peak 950 B 通过，951 B 返回 `HBM_SEARCH_LIMIT_EXCEEDED`。
6. 85% A2/A3 边界：base=2000 B，85% boundary=1700 B；strictly-less-than 使 1699 B 通过，1700/1701 B 返回 `INVALID_ACCURACY_EXECUTION/HBM_EXECUTION_LIMIT_REACHED`。
7. 闭包损坏：改变任一资源内容而不更新 reference，或改变下层 dependency digest、composite digest、AICB header/event 任一 hash，均在真实进程中以稳定码拒绝并保留已验证层的 readiness；任意 legacy workload 不能冒充同一 10T target。
8. evidence 损坏：四资源各自的 `evidenceRef` 必须唯一解析到一个 record，且 record class/readiness 与 spec 完全一致；缺失、歧义、非法枚举或冲突均阻断该层。
9. scope guard：Routing 顶层、spec、policy、数组或 snake_case/synonym 中的未知字段统一由 exact schema 返回 `TARGET_ROUTING_SCHEMA_INVALID`；因此 #20 字段无需易漏的三字段黑名单。

## 3. 类图与对象职责

```mermaid
classDiagram
    class AnalyticalRunContract {
      +target_workload_sha256 string
      +target_*_sha256 string
      +target_configured_gts uint64
      +target_logical_trainable_parameters uint64
      +target_memory_*_B uint64
      +target_memory_*_gate string
    }
    class TargetModelValidator {
      +ValidateTargetModel()
      +RebuildCanonicalRegistry() uint64
      +CheckedLogicalStorageAuxScopes()
    }
    class TargetStepValidator {
      +ValidateTargetStep()
      +CheckedGtsProduct() uint64
    }
    class TargetRoutingValidator {
      +ValidateTargetRouting()
    }
    class TargetMemoryValidator {
      +ValidateTargetMemoryPlan()
      +ValidateCanonicalComponents()
      +ValidateCapacityConservation()
    }
    class TargetCompositeValidator {
      +ValidateTargetComposite()
      +BuildCompositeDigest() string
      +ValidateAicbHeaderAndEvents()
    }
    class WorkloadCollectiveDecoder {
      +DecodeWorkloadHeader()
      +DecodeWorkloadLayerRecord()
      +TargetWorkloadEventBinding
    }
    class ResultManifestWriter {
      +WriteAnalyticalResultManifest()
    }
    class UpstreamRuntime {
      +Sys
      +Workload
      +Layer
    }

    AnalyticalRunContract --> TargetModelValidator
    TargetModelValidator --> TargetStepValidator : model digest
    TargetStepValidator --> TargetRoutingValidator : model + step digests
    TargetRoutingValidator --> TargetMemoryValidator : model + step + routing digests
    TargetMemoryValidator --> TargetCompositeValidator : four resource digests
    TargetCompositeValidator --> AnalyticalRunContract : verified values/readiness
    TargetCompositeValidator --> WorkloadCollectiveDecoder : header/event verification
    AnalyticalRunContract --> UpstreamRuntime : accepted run
    UpstreamRuntime --> WorkloadCollectiveDecoder : same binding consumed by Workload/Layer
    ResultManifestWriter --> AnalyticalRunContract : validated summary only
```

图中的 validator 是 `RunContract.cc` 内的职责边界，不是额外公开 ABI。JSON parser 不泄漏到 `Sys/Workload/Layer`；runtime 只知道 AICB header/event 的五-digest binding，不知道四份 JSON artifact 的内部 schema。Result writer 不重新打开 artifact、不合成 event binding，也不重新推导模型/GTS。

## 4. 输入、协作、状态与输出契约

### 4.1 Run Manifest 与四资源组合

Run Manifest 使用：

```json
{
  "schema_version": "simai.run/v1",
  "workload": {
    "path": "<AICB workload>",
    "sha256": "sha256:<workload>"
  },
  "target_workload": {
    "schema_version": "simai.target.workload/v1",
    "composition": "SHA256_NEWLINE_DELIMITED_RESOURCE_DIGESTS_V1",
    "sha256": "sha256:<composite>",
    "model": {"path": "<model>", "sha256": "sha256:<model>"},
    "step": {"path": "<step>", "sha256": "sha256:<step>"},
    "routing": {"path": "<routing>", "sha256": "sha256:<routing>"},
    "memory_event_plan": {"path": "<memory>", "sha256": "sha256:<memory>"}
  }
}
```

Composite 的 byte string 是以下四个完整 digest identifier 按固定次序用单个 LF 连接，末尾无额外 LF：

```text
sha256:<model>\nsha256:<step>\nsha256:<routing>\nsha256:<memory>
```

`SHA-256(byte string)` 必须等于 `target_workload.sha256`。Run 的旧 `workload.target_workload_sha256` 即使存在也只是非权威兼容字段；生产身份来自 AICB 文件自身。AICB header 以具名字段携带 model/step/routing/memory/composite 五个 digest，标准 event 在原 12 列后追加相同五列（17 列），`HYBRID_CUSTOMIZED` event 在原 13 列后追加五列（18 列）。RunContract 校验每个 declared event，随后真实 `Workload` 用同一 decoder 再消费并把 binding 传给 `Layer`。

组合 envelope 与四个 `{path,sha256}` reference 在任何资源 I/O 前做 exact-key/type/digest 校验。四资源使用同一个 `max+1` single-stream loader，限制分别为 Model 256 KiB、Step 64 KiB、Routing 64 KiB、Memory 128 KiB；恰好上限可读，上限加一稳定返回 `TARGET_*_ARTIFACT_TOO_LARGE`，并支持 `/dev/stdin` 这类 non-seekable 流。非法 envelope 指向不存在文件或 FIFO 时也先以 `TARGET_WORKLOAD_SCHEMA_INVALID` 拒绝，不打开路径。

各 artifact reference 验证原始 bytes 的 SHA-256 后才解析 JSON；解析后的依赖字段形成 Model → Step → Routing → Memory 的逐层闭包。Target envelope、四资源 root/metadata/spec/source/architecture/registry/entry/policy/bindings/component/capacity/evidence 都是 exact-key schema，metadata id、schema/kind/semver、类型与枚举也严格固定，未知字段 fail closed。四份 spec 都有唯一 `evidenceRef`；它必须解析到恰好一个 exact-shape record，record 的 `class/readiness` 必须是受控枚举并与 spec 完全一致。Result 的 `input_summary`、`provenance`、`evidence` 和 `readiness` 分别呈现输入身份、来源摘要、声明证据与本次消费状态，Result writer 不会把 `FIELD_UNVERIFIED` 伪造为 READY evidence。

### 4.2 Model Manifest 与独立参数口径

固定 architecture 是 hidden size 7168、61 个 main MoE layer、1 个 MTP MoE layer、前 3 个 main layer hash-routed、baseline experts 384、target experts 2048、TopK 16、expert width 3072、1 shared expert。Model 的 source of truth 是 76 项 `CANONICAL_TENSOR_TYPE_REGISTRY_V1`；每项有唯一 id、instances、logical shape/dtype、checkpoint storage shape/dtype、trainable role、block scope 与 tensor kind。生产按 shape product × instances checked 重建，而不是信任汇总字段。Registry 的 canonical line digest 冻结为 `sha256:f5984772e2fca84aeb8e36786b1273c26576a083cf17dd07a1eb0637e0d9daa2`。

独立 oracle 从 registry 得到：

```text
logical trainable elements       = 8,414,884,746,526
checkpoint auxiliary elements   =   262,134,842,368
  quantization scale elements   =   262,128,636,928
  routing table elements        =         6,205,440
checkpoint storage bytes        = 4,486,847,493,752
tensor instances                =           764,124
```

`FP4 -> PACKED_FP4_I8` 必须满足 logical:storage elements=2:1；普通 tensor 的 logical/storage shape 和 dtype 必须闭合；`CHECKPOINT_AUXILIARY` 只允许 quant scale/routing table。生产 validator 使用 checked `uint64_t` 乘加，再与声明汇总比较；测试 oracle 独立解析 fixture并用 Python 任意精度整数和独立 dtype-byte 表计算。缺项、重复 id、未知 role/scope/dtype/kind、shape 不闭合或 canonical digest 改变返回 `TARGET_MODEL_REGISTRY_INVALID`；architecture、source commit/header digest、声明汇总或 checkpoint 语义改变返回 `TARGET_MODEL_IDENTITY_MISMATCH`。这种冻结比“总数刚好相同”更严格。

三个 active scope 也从 role/scope/kind 重建：main blocks only `88,950,053,982`、main forward including global I/O `90,803,533,923`、training graph including MTP `92,345,423,134`。`checkpointStorage` 固定为 `4,486,847,493,752 B`，语义为 `FIXED_QUANTIZED_CHECKPOINT_ONLY_NOT_TRAINING_HBM`；Result 另附 `checkpoint_auxiliary_elements` 与 `used_as_training_hbm=false`。它不会进入任何 memory component 或 capacity sum。

### 4.3 Step/GTS 契约

JSON parser 为每个 number 同时保留解析值和原始 token；Target integer consumer 不从 `double` 回转整数。四个 factor 必须是大于零、JSON-safe、canonical unsigned decimal（只允许 `0` 或无前导零的十进制数字串，factor 不允许 0）且可装入范围的整数。因此 `1.00000000000000001`、`1e0`、`01`、负数/负下溢、`2^53` 及 uint64 边界走私都会稳定拒绝。计算顺序使用逐次 checked multiplication：

```text
configured GTS = sequenceTokens × microBatchSequences
               × dataParallelReplicas × gradientAccumulation
```

先验证乘法不溢出，再验证计算值等于 `configuredGlobalTokens`，最后要求 `<= 500,000,000`。`configuredRoutedAssignmentSlotsUpperBound` 必须等于 `GTS × TopK × 62`，也使用 checked multiplication。主要稳定码为：

| 条件 | exit/status | reject code |
| --- | --- | --- |
| 因子为零、负、非 canonical decimal 或 JSON unsafe | `2/INVALID_INPUT` | `TARGET_STEP_FACTOR_INVALID`（非法 JSON 本身为 `TARGET_STEP_INVALID_JSON`） |
| 逐因子乘法溢出 | `2/INVALID_INPUT` | `TARGET_STEP_GTS_OVERFLOW` |
| 计算 GTS 与声明不一致 | `2/INVALID_INPUT` | `TARGET_STEP_GTS_MISMATCH` |
| GTS > 500M | `2/INVALID_INPUT` | `TARGET_STEP_GTS_LIMIT_EXCEEDED` |
| routed slots 不一致或溢出 | `2/INVALID_INPUT` | `TARGET_STEP_ASSIGNMENT_MISMATCH` |

Step 失败不擦除已验证 Model：Result 仍输出模型参数与 `readiness.target_model=READY`，而 step/后续资源为 `BLOCKED`。

### 4.4 Routing 与 #20 边界

本票的 Routing Artifact 是 `simai.target.routing/v1alpha1` 的外部、内容寻址 policy identity。它固定 `HASH_FIRST_THREE_THEN_TOPK`、2048 experts、TopK 16、前三个 main layer hash-routed，且 `counts=SYMBOLIC_UNMATERIALIZED`。它没有 per-rank counts、domain pair bytes、topology loads、网络流量或 completion time；Result 不把 `496B` routed assignment slots 解释成以上任何量。

`projectedA2ATraffic`、`domainPairBytes`、`topologyResourceLoads` 是 #20 `ProjectedA2ATraffic` 的职责。Routing root/spec/policy 的 exact allowlist 会自动拒绝这些字段以及 nested/array/snake_case/synonym 变体，稳定返回 `TARGET_ROUTING_SCHEMA_INVALID`，而不是维护易漏的字段黑名单。这样 Memory 可以绑定 routing identity，但不能声称已获得通信投影或 topology-aware placement 结果。

### 4.5 Memory Event Plan 与 UNKNOWN

七个 component 的 category、allocate/release lifetime、expression 和所需 policy 列表都是 schema 的规范部分，不能任意改写：

| Result key | category | 关键依赖 | 未绑定输出 |
| --- | --- | --- | --- |
| `parameters` | PARAMETERS | precision, placement | logical tensors × training precision / shards |
| `gradients` | GRADIENTS | precision, placement | logical tensors × gradient precision / shards |
| `optimizer_states` | OPTIMIZER_STATES | optimizer, placement | optimizer/master tensors / shards |
| `activations` | ACTIVATIONS | precision, placement, recomputation | saved activation shape trace |
| `communication_buffers` | COMMUNICATION_BUFFERS | precision, placement, runtime | dispatch/combine/collective/runtime scratch |
| `expert_placement` | EXPERT_PLACEMENT | placement, precision | local expert weights and max local load |
| `recomputation` | RECOMPUTATION | recomputation, runtime | recomputed activations and observed workspace |

全 UNBOUND 时每项输出 `{state: "SYMBOLIC", unit: "B", value: "UNKNOWN", expression: ...}`，peak/search/execution gate 和 `results.hbm_peak_B` 传播 `UNKNOWN`。五个 policy 只有全部为 content digest 才进入物化态；每项此时必须提供 canonical、非负、安全整数 `peakBytes`，例如 `-1e-400` 不能再经 `double` 下溢冒充 0。`plannedPeakHbmB` 必须等于七项之和；`scenarioUsableHbmB` 必须等于 `baseHbmB-reserveHbmB`，并要求 base > 0、reserve < base。加法、减法和百分比均有整数溢出检查。

物化值是调用者绑定 policy 后的输入，不是由 checkpoint bytes 或逻辑参数量猜出的值；对应 evidence 仍由 artifact 声明，Result 不把 `FIELD_UNVERIFIED` 升级成 `MEASURED`。

### 4.6 95% 与 85% 容量 gate

搜索 gate 的 denominator 是 Scenario Usable HBM：

```text
scenario_usable_hbm_B = base_hbm_B - reserve_hbm_B
search_maximum_allowed_B = floor(scenario_usable_hbm_B × 95 / 100)
PASS iff planned_peak_per_rank_B <= search_maximum_allowed_B
```

恰好 boundary 可用，一字节超过返回 exit 2、`INVALID_INPUT/HBM_SEARCH_LIMIT_EXCEEDED`。Result 明确输出 denominator `SCENARIO_USABLE_HBM_B`、rounding `FLOOR_INTEGER_BYTES` 和 maximum allowed。

A2/A3 execution gate 的 denominator 是未扣 reserve 的 Base HBM，且要求严格小于 85%：

```text
execution_boundary_B = floor(base_hbm_B × 85 / 100)
execution_maximum_accepted_B = ceil(base_hbm_B × 85 / 100) - 1
PASS iff observed_execution_peak_B × 100 < base_hbm_B × 85
```

实现通过 quotient/remainder 计算阈值，避免先乘 85/100 的溢出和非整除歧义。没有 observed peak 时 execution gate 为 `UNKNOWN`；一旦提供，等于或越过 85% 都返回 exit 5、`INVALID_ACCURACY_EXECUTION/HBM_EXECUTION_LIMIT_REACHED`。这只是 A2/A3 gate contract，不把 `FIELD_UNVERIFIED` fixture 冒充真实设备观测。

### 4.7 Result Manifest 状态模型

有效 symbolic Run 的 `results.target_workload` 输出 model/step/routing/memory/composite identity；`results.memory` 输出七类符号量、binding 状态、aggregation 与 gate；`provenance`/`evidence`/`readiness` 输出四资源的可追溯闭包。

状态语义保持 #16–#18：

- `READY`：本次 contract 所需资源已完整验证；
- `FIELD_UNVERIFIED`：输入字段可消费但没有测量级证据；
- `UNKNOWN`：值未绑定或未观测，不能给出数字；
- `BLOCKED`：已选择该能力，但资源或 gate 使本次 Run 不能继续；
- `NOT_REQUIRED`：沿用既有 Result 字段时，本次路径不消费该能力。

缺失必需四资源返回 exit 3/`UNSUPPORTED`；存在但 schema/digest/依赖/evidence 错误返回 exit 2/`INVALID_INPUT`；85% execution violation 使用专门的 exit 5/`INVALID_ACCURACY_EXECUTION`。所有路径都尽力写结构化 Result，且不回显 artifact path 或异常原文。

## 5. ADR 取舍

- 遵循 [ADR 0001](../adr/0001-derive-from-upstream-simai-history.md)：Target contract 经真实 Upstream `SimAI_analytical` 和 AICB workload 运行，不另建独立计算器。
- [ADR 0002](../adr/0002-close-10t-by-adjusting-expert-width.md) 已被 [ADR 0004](../adr/0004-accept-v4-expert-width-for-10t-scale.md) supersede：10T 是 scale class，保留 V4-Pro expert width 3072，并冻结准确的约 8.415T 逻辑参数，而不是人为调宽到 `10^13`。
- 遵循 [ADR 0003](../adr/0003-publish-only-sanitized-calibration-evidence.md)：公开 fixture 只含 public metadata 或 synthetic input；不提交现场机器、IP、账号、凭据或原始日志。
- 遵循 [ADR 0006](../adr/0006-separate-target-workload-resources-and-counting-scopes.md)：Model/Step/Routing/Memory 是独立生命周期和 identity；canonical tensor registry 是 logical/storage source of truth；AICB header/event 真实绑定四资源；logical total、auxiliary、active scopes、checkpoint bytes 与训练 HBM 不混合。
- 与 [ADR 0005](../adr/0005-separate-analytical-cost-from-simulation-flow.md) 保持边界：本票只扩展 Shared Run Contract，不实现 Simulation `CollectiveFlowProvider`，也不复用 NCCL flow。
- 与 [ADR 0007](../adr/0007-use-hierarchical-projection-for-analytical-alltoall.md) 保持边界：本票的 routing 只是 policy identity；#20 才能生成 `ProjectedA2ATraffic` 的 rank/domain/resource load surface。
- 选择规范表达式而不是任意字符串：symbolic output 可审计，materializer 不能用同一 category 偷换 checkpoint 或遗漏 runtime dependency。
- 选择全 UNBOUND 或全 BOUND：避免部分 policy 产生看似精确、实则缺少 placement/runtime 假设的 per-rank HBM。

## 6. 测试映射与验证 seam

所有产品测试只启动真实 `SimAI_analytical` 子进程，断言进程退出码和解析后的 `simai.result/v1`；不实例化 validator、不 mock `Sys/Workload/Layer`、不读取私有成员。Fixture mutation 后测试会重算被修改资源和上层引用 digest，使负例能穿过文件摘要 gate，真正命中待测的 schema/依赖/容量行为。

| Issue #19 Acceptance Criteria | 真实进程与独立 oracle 证据 |
| --- | --- |
| 冻结 8,414,884,746,526 与 2048/16/3072/1 | 正例断言 Result；独立 Python 任意精度 oracle 从 76 项 logical/storage shape、dtype 与 instances 重建 logical/aux/storage/三 scope；缺/重/未知/shape 不闭合均有 registry 稳定码 |
| GTS=sequence×MBS×DP×GA，500M 可用，超过拒绝 | 500M 正例；500,000,001 稳定码；零/负/float/unsafe、raw fraction/exponent/leading token、`uint64_t` 边界/乘法溢出、声明不一致和 routed slots 不一致均有真实进程负例 |
| 四资源内容寻址组合 | 断言 Result 四 digest/composite；正确 AICB header/event、任意 legacy 替换、header 单 hash、event 缺失/篡改与三层 event 传播均走真实进程；`Workload/Layer` 使用同一 decoder |
| bounded I/O 与 exact schema | 四资源恰好 byte limit/limit+1、Routing `/dev/stdin`、非法 envelope 指向不存在路径/FIFO；metadata/spec/source/architecture/registry/entry/policy/bindings/component/capacity/evidence 未知 key/type/id mutation |
| evidence/readiness fail closed | 四资源逐一测试 missing/unresolved/ambiguous ref、非法 record readiness、spec/record class/readiness 冲突，Result 不伪造 READY evidence |
| 七类显存分别呈现 | symbolic 正例断言准确 key 集、unit、expression；物化例断言七项总和等于 peak；表达式/lifetime/依赖 mutation 被拒绝 |
| 未绑定保留 UNKNOWN；checkpoint 不是 HBM | 全 UNBOUND 正例断言七项、peak、两 gate 与 `hbm_peak_B` 均 UNKNOWN；checkpoint 只出现在 model storage 且 `used_as_training_hbm=false`；部分 binding 被拒绝 |
| 95%/85% 恰好边界与越界 | 950/951 B 和 1699/1700/1701 B 均由真实进程测试；独立 Python oracle 验证 usable denominator、floor/strict 比较和 maximum accepted |
| 不破坏 #16–#18 | 同一个 80-test suite 中保留 46 项既有真实进程回归，覆盖 legacy GPU、typed 12/13-column workload decoder、五类 collective、A2AV routing limits 与 HCCL model；Target 新增 17/18-column decoder 不改变 legacy |

正式验证还包括：RunContract 严格 `-Wall -Wextra -Wpedantic -Werror` 编译、等价 CMake glob/exclude 的 61-source 完整链接、所有 fixture JSON 可解析、逐文件 SHA-256/分层 digest/composite 的独立复算、`git diff --check`、敏感信息扫描，以及 #20/#21/#28 scope-creep 扫描。

## 7. 限制与后续依赖

- Model Manifest 冻结的是指定公开 source commit/header digest 的逻辑身份；fixture 未下载 tensor data，证据为 `USER_INPUT/FIELD_UNVERIFIED`，不宣称重新发布 checkpoint 或测量训练行为。
- Tensor Manifest 当前用 76 项 canonical tensor-type registry 表示 764,124 个 instances，而不是复制完整 checkpoint tensor header 或下载 tensor data；每项仍携带 logical/storage shape/dtype/role/scope/kind，且 canonical digest 冻结。任何 source/layout 改动必须建立新资源身份和独立回归证据。
- Memory Event Plan 提供 lifetime 与守恒 contract，不是 allocator trace 或 estimator。物化 bytes 必须由后续明确的 precision/optimizer/placement/recompute/runtime artifact 提供；本票不推断 ZeRO、TP/PP/EP sharding 或 kernel workspace。
- `CONSERVATIVE_COMPONENT_PEAK_SUM` 是七类 component peak 的保守和，不声称精确重建跨时间的 allocator overlap。需要事件级 trace 时必须版本化升级 schema。
- 85% observed execution 字段可以验证 gate 语义，但只有以后真实 A2/A3 受锁设备验证产生的 measured evidence 才能支持精度声明；本票 fixture 不具备该证据等级。
- Routing counts 仍为 symbolic；496B routed assignment slots 不是 A2A bytes。#20 必须独立消费 routing/placement/topology 并生成 `ProjectedA2ATraffic`，不得把本票 Result 当投影结果。
- 本票不枚举 candidates、不运行 Top-5 搜索、不做 multi-fidelity promotion 或复合 acceptance；这些属于 #21/#28 等后续 Issue。
- Simulation/NS-3 的 HCCL endpoint flow 仍要求 ADR 0005 定义的独立 provider；Target Workload `READY` 不代表 Simulation backend 已支持该模型。
