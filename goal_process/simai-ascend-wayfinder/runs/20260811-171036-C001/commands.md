# 脱敏命令模板

```bash
ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 \
  -o ConnectionAttempts=1 -o IdentitiesOnly=yes \
  -i <PRIVATE_KEY> <USER>@<PRIVATE_HOST> '<READ_ONLY_PROBE>'
```

`<READ_ONLY_PROBE>` 只包含身份、系统、磁盘、NPU、驱动/CANN/HCCL、Python/框架、编译器、工具与候选 workload 路径查询；不包含安装、删除、进程终止、服务启动或配置修改。

## 探测类别

- 系统：架构、发行版、主存、指定工作区和文件系统容量。
- NPU：`npu-smi` 数量、型号、HBM、健康与占用聚合状态；唯一硬件标识不发布。
- 软件：激活 CANN basename、驱动/工具版本、Python 与过滤后的全局包快照。
- 工具：`hccn_tool`、`msprof`、HCCL Test、`torchrun`、`mpirun` 是否在 `PATH`。
- workload：限定搜索深度内只记录候选仓 basename，不公开完整远端路径。

本机缺少 GNU `timeout`，初次本地包装命令在 SSH 前以 exit 127 停止。依据错误签名只修正一次：删除不可用的本地包装，保留 SSH 连接超时及远端 20 秒命令上限。三台目标随后均返回成功。

公开前删除或归类地址、账号、密钥路径、hostname、PCI Bus ID、进程 ID、设备唯一标识和完整远端路径；原始远端输出不入仓。

公开工件的首次 `rg` 扫描把两个目标路径误作为单个 argv，因路径不存在而失败；改为显式传递两个 argv 后，敏感模式、IPv4、行尾空白扫描和 `jq empty` 均通过。该修正没有触发远端重跑。
