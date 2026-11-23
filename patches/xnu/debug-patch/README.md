# Optional XNU Debug Patches (`patches/xnu/debug-patch/`)

These patches were used during early MT6768 SMP bring-up and stall diagnosis.
They are **disabled by default** (not listed in `patches/xnu/series`) to keep production/bring-up kernels clean.

| Patch | Purpose |
| --- | --- |
| `zzzi-merlinx-debug-stall-smp.patch` | Extra SMP stall instrumentation and CPU wake diagnostics |
| `zzzk-merlinx-smp-breadcrumbs.patch` | Secondary-core bring-up breadcrumb ring (`[smp-crumb]`) |

## How to Enable Temporarily

Add `debug-patch/<patch-name>.patch` to the end of `patches/xnu/series`, rebuild the kernel, and remove the line once diagnosis is complete.
