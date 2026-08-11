# C002 脱敏研究命令与来源边界

- 读取仓内固定 commit 的 Upstream SimAI 与 AICB 源码、README、参数解析和 DeepSeek workload 生成逻辑。
- 只引用官方 Ascend/PyTorch-NPU/MindSpeed/MindSpeed-LLM/HCCL 文档、仓库、release、requirements、示例或 CI；技术结论不以二手博客为依据。
- 记录固定 commit/permalink、版本约束、支持范围、可表达 shape 与测量能力；区分“文档支持”“源码存在”“现场可运行”三种证据。
- 公开文件不得包含私有地址、账号、hostname、认证材料、设备唯一标识、完整远端路径或原始日志。
- 本轮不执行 SSH、安装、版本切换、编译、训练或集合通信压测。

## 实际只读检查

- `gh issue view 3 --repo Kirrito-k423/SimAI-Ascend --json ...`：核对研究票标题、标签、认领与开放状态。
- `gh issue view 1 --repo Kirrito-k423/SimAI-Ascend --json ...`：核对地图目的地、已有决策和未决项。
- `curl` 读取 MindSpeed-LLM `2b7130ca...` 的固定 raw release notes 与 install guide：复核 26.1 配套表、跨 TorchNPU/CANN 兼容表、A2/A3 硬件支持、源码安装和镜像边界。
- `/research` 子代理只写单一决策包，主线程检查固定 permalink、证据标签、FIELD_UNVERIFIED 边界和公开脱敏要求。
- `git diff --check`、唯一 URL 状态检查、秘密与私有标识扫描：作为提交前门禁。
