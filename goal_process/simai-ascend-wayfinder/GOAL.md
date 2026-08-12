# SimAI-Ascend Wayfinder Goal

围绕 [Wayfinder：从 Upstream SimAI 打通 Ascend 并搜索 100k/10T 典型配置](https://github.com/Kirrito-k423/SimAI-Ascend/issues/1)，逐个解决地图中的 task、research、prototype 与 grilling 票据，直到所有必要决策均有证据、地图没有未完成的必需子票据，并能无损交给 `/to-spec`。

“验证 Upstream SimAI Ascend Provider seam 与 GPU 兼容边界”已通过 throwaway prototype 和用户 HITL 完成：首版 Analytical-first，cost/flow provider 分离，显式 profile 与 legacy 参数冲突 fail-closed，试验实现不合入 `main`。下一 micro-goal 由 GitHub frontier 在新 Wayfinder 会话中选择。
