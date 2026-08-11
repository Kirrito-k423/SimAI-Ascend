# Derive SimAI-Ascend from Upstream SimAI history

SimAI-Ascend will preserve the full `aliyun/SimAI` Git history and begin from the pinned upstream `master` commit `f5efb5a93ea9be7db25a8843f9f7ff54044f6062`, rather than reimplementing the simulator independently. `origin` will remain the SimAI-Ascend repository, `upstream` will identify `aliyun/SimAI`, upstream upgrades will be explicit reviewable changes, and reproducible experiments will use the submodule commits recorded by the chosen upstream baseline instead of advancing them with `git submodule update --remote`.
