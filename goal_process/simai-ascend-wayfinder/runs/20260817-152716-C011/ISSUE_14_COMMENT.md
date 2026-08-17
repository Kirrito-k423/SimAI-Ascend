用户已一次性接受 Simulation smoke 的五项决策：16 ranks/2×8 synthetic domains；五collective×三消息点加两种额外A2AV共17 micro；reduced A3 shape 的 topology-aware/striped 两个E2E；独立 HCCL flow provider 的19-case结构门；逐case Analytical-vs-Simulation APE≤30%、消息单调和 placement ordering 的F3门。

`FLOW_SMOKE_PASS` 与 `F3_SIMULATION_AUDITED` 分离；时间失败保留结构支持但标 `SIMULATION_DISAGREEMENT`。throwaway state model 固定在 `prototype/simulation-smoke-contract@b9c3297`，不进入main且未运行NS-3。证据：ADR-0013、run C011 与最终 `/to-spec` 输入。
