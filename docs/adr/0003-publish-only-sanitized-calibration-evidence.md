# Publish only sanitized calibration evidence

The public SimAI-Ascend repository may contain A2/A3 generation labels, software versions, workload shapes, aggregate measurements, and errors, but it must not contain machine addresses, accounts, credentials, private inventory, or raw host logs. Sensitive source material remains under the local AutoResearch workspace, trading some raw-data reproducibility for a boundary that allows the simulator, manifests, and derived calibration evidence to be developed safely in public.
