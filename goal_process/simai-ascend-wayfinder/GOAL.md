# SimAI-Ascend Wayfinder Goal

围绕 [Wayfinder：从 Upstream SimAI 打通 Ascend 并搜索 100k/10T 典型配置](https://github.com/Kirrito-k423/SimAI-Ascend/issues/1)，逐个解决地图中的 task、research、prototype 与 grilling 票据，直到所有必要决策均有证据、地图没有未完成的必需子票据，并能无损交给 `/to-spec`。

当前 micro-goal 是“验证 Upstream SimAI Ascend Provider seam 与 GPU 兼容边界”：在独立 throwaway 分支用可交互逻辑原型和真实构建探针确认 Analytical cost model、Simulation flow provider、显式 profile 选择与 legacy GPU/NCCL 回归边界；用户完成 HITL 评判后才关闭票据。
