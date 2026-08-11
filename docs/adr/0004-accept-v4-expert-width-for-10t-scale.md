# Accept the V4-Pro expert width for the 10T-scale workload

The project treats “10T” as a scale class rather than an exact parameter target, so the approximately 8.31–8.42T workload obtained by setting 2,048 routed experts and MoE TopK 16 already qualifies. The target therefore keeps the V4-Pro expert intermediate size of 3,072 instead of perturbing the model solely to hit `10^13`, superseding ADR-0002 while retaining an exact parameter counter for reporting total and active parameters.
