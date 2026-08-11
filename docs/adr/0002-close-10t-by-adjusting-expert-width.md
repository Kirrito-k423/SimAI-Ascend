---
status: superseded by ADR-0004
---

# Close the 10T model by adjusting expert width

The target workload will retain the DeepSeek-V4-Pro layer count, hidden size, attention structure, one shared expert, first three hash-routed MoE layers, and MTP depth while changing the routed-expert count to 2,048 and the MoE TopK to 16. Because those two changes produce only about 8.31–8.42T parameters, an exact parameter counter will solve and hardware-tile-align the expert intermediate size to approach 10T instead of changing the trunk depth or hidden size.
