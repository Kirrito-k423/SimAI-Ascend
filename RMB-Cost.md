# RMB Cost

- Generated at: `2026-08-11T07:12:09Z`
- Confidence: `estimate`
- USD/CNY: `7.2`
- Task: SimAI-Ascend 一手资料证据库研究（2026-08-11T06:35:48Z 至 2026-08-11T07:12:09Z；root 增量 + 3 个后台 agent session）

## Token Usage

| Item | Tokens | M tokens |
|---|---:|---:|
| Input total | 30,092,645 | 30.092645 |
| Cached input | 28,854,528 | 28.854528 |
| Uncached input | 1,238,117 | 1.238117 |
| Output | 115,552 | 0.115552 |
| Reasoning output, included in output when provider reports it that way | 31,812 | 0.031812 |

## Price Assumptions

| Model | Input USD/M | Cached input USD/M | Output USD/M | Note |
|---|---:|---:|---:|---|
| gpt-5.5 | 5 | 0.5 | 30 | Estimate：聚合 root 研究窗口与三个后台 agent 的 token_count；本轮未核验最新 GPT API 价格。 |
| deepseek-v4-pro | 0.435 | 0.003625 | 0.87 | Counterfactual estimate：相同 token 量按默认 DeepSeek API 单价估算；本任务未实际调用 DeepSeek。 |

## Cost Breakdown

| Model | Component | Cache status | Tokens | M tokens | USD/M | USD | RMB |
|---|---|---|---:|---:|---:|---:|---:|
| gpt-5.5 | Input (cache miss) | not cached | 1,238,117 | 1.238117 | 5 | 6.19 | 44.57 |
| gpt-5.5 | Input (cache hit) | cached | 28,854,528 | 28.854528 | 0.5 | 14.43 | 103.88 |
| gpt-5.5 | Output | not applicable | 115,552 | 0.115552 | 30 | 3.47 | 24.96 |
| deepseek-v4-pro | Input (cache miss) | not cached | 1,238,117 | 1.238117 | 0.435 | 0.5386 | 3.88 |
| deepseek-v4-pro | Input (cache hit) | cached | 28,854,528 | 28.854528 | 0.003625 | 0.1046 | 0.7531 |
| deepseek-v4-pro | Output | not applicable | 115,552 | 0.115552 | 0.87 | 0.1005 | 0.7238 |

## Cost Summary

| Model | Input cache miss RMB | Input cache hit RMB | Output RMB | Total RMB | Total USD |
|---|---:|---:|---:|---:|---:|
| gpt-5.5 | 44.57 | 103.88 | 24.96 | 173.41 | 24.08 |
| deepseek-v4-pro | 3.88 | 0.7531 | 0.7238 | 5.35 | 0.7437 |

## Formula

`uncached_input = input_total - cached_input`

`uncached_input_cost = uncached_input_M * input_usd_per_M`

`cached_input_cost = cached_input_M * cached_input_usd_per_M`

`output_cost = output_M * output_usd_per_M`

`total_rmb = (uncached_input_cost + cached_input_cost + output_cost) * USD_CNY`

## Notes

- Re-run with current official API prices and FX before using this for reimbursement or budget approval.
- Codex goal `tokensUsed` can differ from raw session input/output because it is an effective meter, while session logs also expose cache hits and repeated context reads.
