# SimAI-Ascend Wayfinder Goal

围绕 [Wayfinder：从 Upstream SimAI 打通 Ascend 并搜索 100k/10T 典型配置](https://github.com/Kirrito-k423/SimAI-Ascend/issues/1)，逐个解决地图中的 task、research、prototype 与 grilling 票据，直到所有必要决策均有证据、地图没有未完成的必需子票据，并能无损交给 `/to-spec`。

当前 micro-goal 是“闭合 10T-scale Workload Schema 与参数及激活计数”：用固定 V4-Pro checkpoint 的逐 tensor shape/dtype 为基线，在独立 throwaway 分支验证 2,048 routed experts、TopK 16、一个完整 MTP block 的逻辑参数、active 参数、GTS、路由与显存事件契约；用户完成 HITL 评判后才关闭票据。
