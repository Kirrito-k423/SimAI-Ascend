# C003 脱敏研究命令与来源边界

- `gh issue view` 与 GitHub GraphQL：核对地图、frontier、原生 blocker、标签、认领和票据问题。
- 读取仓内固定 Upstream SimAI、先前研究与 Goal 账本；CodeGraph 不存在，代码定位才使用 `rg`/只读源码。
- `/research` 子代理只引用官方 Ascend/CANN/HCCL、Upstream SimAI、正式标准或原始源码；二手博客不作为技术结论依据。
- 记录固定 permalink、原生字段/单位、推导公式、证据类别、不确定性、缺失策略、校验规则与向后兼容边界。
- 公开文件不得包含私有地址、账号、hostname、认证材料、设备唯一标识、完整远端路径或原始日志。
- 本轮不执行 SSH、安装、版本切换、编译、训练或集合通信压测。
- 固定源码审计：`astra-sim-alib/src/astra-sim/system/Common.hh`、`AstraParamParse.hh`、`calbusbw.cc`、`Layer.cc`、`compute/ComputeAPI.hh` 与 `memory/AstraMemoryAPI.hh`；同时核对这些文件相对 Upstream 固定提交没有漂移。
- 官方来源核验：HCCL Test/C API、CANN Runtime `aclrtGetMemInfo`、HCCL 算法说明、Ascend 950DT 白皮书与 MindSpeed 26.1；950DT 白皮书内容 SHA-256 为 `ece3405e6a17fabdd462338fb94266558649a6407a2f28008403211387b3a927`。
- 机器可读校验：逐个抽取 Markdown 中的 YAML fenced block，以 Ruby YAML parser 加载；检查 unknown/value、evidenceRef、HBM scope、A2AV artifact 和当前无真机 A5 的 MEASURED 禁止规则。
- 收口校验：唯一外链状态、相对链接、Markdown fence、敏感字段、`git diff --check` 与 Git 工作区范围；公开 artifact 只包含脱敏示例。
