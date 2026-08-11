# C003：定义证据化 Ascend Hardware Profile 与 HCCL Cost Model Schema

- **开始/结束：** 2026-08-11T19:54:05+08:00 / 2026-08-11T22:16:21+08:00
- **阶段：** RECON → HYPOTHESIS → INTEGRATE
- **动作类型：** READ
- **关联验收/未知量：** AC-03、H-05、H-06、D-07

## 预注册

- **本轮 micro-goal：** 用一手资料冻结 Profile/Cost Model 的最小规范：字段、类型、单位、来源、证据身份、区间/统计量、消息语义、拓扑与软件 pin、校验不变量、缺失策略、派生拟合和 legacy SimAI 适配。
- **当前假设：** H-05、H-06。
- **已有证据：** Upstream SimAI 固定源码审计；A2/A3 脱敏能力矩阵；Ground Truth L0–L3 指标契约；Ascend 950 与 HCCL Test 一手资料入口。
- **证据等级：** schema 选择 E0；相关原生字段与源码消费点 E1。
- **唯一主要变量：** 无；纯研究决策，不改变远端环境或仿真代码。
- **预期观察：** 形成 normative v1alpha1 数据模型、至少一个 A2/A3 measured 示例和一个 A5 estimated 骨架、collective sample/fit 规则、legacy adapter 边界与可机器验证的不变量。
- **判别规则：** 原生数据与派生结果必须分层；每个数值必须有单位和 evidence_ref；A5 无真机值不得标为 measured；缺失关键字段必须失败或 UNKNOWN，不得静默继承 GPU/NVIDIA 默认值；旧 GPU 输入通过显式 adapter 保留。
- **成本与风险：** 目标≤75分钟；不运行远端、不安装、不压测；主要风险是营销规格与运行时语义混淆、HCCL 算法带宽误写为物理带宽、schema 过度拟合某一代硬件。
- **停止与回滚：** 只生成一个公开 Markdown 决策包与 Goal 账本；证据不足的字段保留 `FIELD_UNVERIFIED/UNKNOWN`；不填写用户尚未提供的 A5 数值。

## 执行

- **脱敏命令：** `commands.md`
- **配置/环境差异：** 不适用；本轮只读。
- **代码差异：** 不改仿真代码；新增 `docs/research/2026-08-11-ascend-profile-hccl-schema.md` 并更新 Goal 账本。
- **日志/指标：** 7 个 YAML fenced blocks 均可解析；23 个唯一外链由研究子任务逐一返回 HTTP 200；官方 950DT 白皮书固定 SHA-256；`git diff --check` 与敏感信息扫描通过。

## 结果

- **观察事实：** Upstream SimAI 的设备枚举仍是 NVIDIA-only；未知 GPU 会落到 NONE。legacy busbw 表存在 16-node 默认列、空 cell=`1`、单位 1024³ 与 GB/s 文案不一致、部分 rank/消息范围硬编码等行为。HCCL Test/C API 能提供时间、算法带宽、count 语义和 collective 类型，但这些原生字段不能替代物理链路带宽。A2AV exact traffic 在大 rank 下天然是 O(P²)。
- **错误签名：** 过程偏差为 `/research` 子任务超过预注册 75 分钟上限，主线程中断等待并要求以当时证据立即落盘；首次本地 YAML 校验因系统 Ruby 默认 `US-ASCII` 和 `Time` safe-load 白名单失败，显式指定 UTF-8 并允许标准 `Time` 类型后 7/7 通过，artifact 无需改写；父地图首次回写用 shell 双引号承载 Markdown，反引号被错误执行且换行被转义，发现后立即以单引号安全参数完整恢复，并逐项断言真实换行、upstream pin、`/to-spec`、本次决策和已移除未知项；补充账本提交首次 push 遇到单次 SSL timeout，未改变提交，立即重试成功并确认本地/远端同步。
- **推断：** 单一 busbw 标量无法同时保真表达设备事实、原始观测、拟合与跨代外推；三层资源和显式 domain 是满足可复核、30% 门槛与 100k 可扩展性的最小边界。A5 当前只能是厂商规格、用户输入或显式外推，不能伪装为现场实测。
- **证据等级变化：** H-05 E0→E1/SUPPORTED；H-06 E0→E1/SUPPORTED。
- **信息增量：** 冻结 3 个 apiVersion、6 类 evidence class、4 类 readiness、规范单位、HBM scope、AR/AG/RS/A2A/A2AV raw schema、分段 αβγ fit、A2AV artifact 摘要、validator invariants、legacy adapter 和 A2/A3/A5 脱敏骨架。

## 结论

- **验收/交付更新：** D-07 DELIVERED；AC-03 进入 IN_PROGRESS，后续还需 Provider seam 决策票。
- **预算变化：** 研究历时超过预注册 75 分钟；未运行远端或训练，未占用 NPU；用户已明确关闭本项目费用监控。
- **下一 micro-goal：** 新会话认领并执行 Provider seam prototype；本轮不提前开始。
- **是否需决策：** 无；如出现不可由证据决定的产品策略，再按 5 个一批呈现。
