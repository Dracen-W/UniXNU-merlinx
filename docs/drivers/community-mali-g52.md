# Mali-G52

Community XNU GPU driver for the ARM Mali-G52 in the Xiaomi Redmi Note 9 (merlinx, MediaTek MT6768). The driver is maintained in the device project, not by the UniXNU organization.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [Dracen-W/Mali-G52](https://github.com/Dracen-W/Mali-G52) | [Dracen-W](https://github.com/Dracen-W) | Bifrost v7 kernel service, read-only probe, M2 power and clock bring-up, MMU, job slots and the MaliMetal wire ABI |

## State

| Layer | State |
| --- | --- |
| IOKit matching | the `NativeMaliGpuService` personality matches `mediatek,mali` on the generated ADT |
| GPU window | `0x13040000`, size `0x4000`; fixed physical mapping is validated by GICD_PIDR2 `0x3b` |
| Platform probe | SPM, INFACFG, topckgen, mfgcfg, pwrap and MT6358 reads run without configuration writes |
| PMIC path | MT6358 PSEQ_ID reads `0x0800` through pwrap WACS2 |
| M2 | the voltage, clock, four-domain and clock-gate path is compiled in; execution stops at the first VSRAM pwrap read |
| M3 and later | GPU reset, MMU enable, job submission, fences and MaliMetal are not yet exercised on hardware |
| Userland | no GPU device is published; display still fails in `IOMFB_FDR_loader` |

The kernel status lines used during bring-up are `MALI MATCH`, `MALI START`, `MALI RC`, `MALI M2P`, `MALI PMIC`, `MALI GIC`, `M2V` through `M2G`, and the voltage sub-steps `V1` through `V4` and `VOK`.

## Sources

| Source | Purpose |
| --- | --- |
| `NativeMaliGpuService.cpp` | IOKit service, ADT matching and diagnostic stages |
| `NativeMaliBringup.cpp` | M1 through M4 ordering and the staged M2 sequence |
| `NativeMaliPower.cpp` | SPM, topckgen, mfgcfg and M2 power sequence |
| `NativeMaliMt6358.cpp` | VGPU and VSRAM voltage selection |
| `NativeMaliPwrap.cpp` | PMIC WACS2 transport |
| `NativeMaliGpuCore.cpp` | GPU identity, feature reads, reset and core power |
| `NativeMaliService.cpp` | device lifetime, address spaces and reset orchestration |

The current source revision is pinned by `MaliG52` in `components.toml`.

## Integration

`board.toml` selects `MaliG52`. `scripts/build-mali-g52.py --m2-writes` builds the archive, `patches/xnu/native-mali-personality.patch` adds the IOKit personality, and `scripts/make-merlinx-adt.py` publishes the `mediatek,mali` node. The kernel links the archive with `-force_load`.

The build uses board constants for the GPU, SPM, INFRACFG, topckgen, mfgcfg and pwrap windows. The generated ADT does not describe those addresses.

## Verification

Build the kernel and payload, then flash the boot image to p33:

```sh
./unixnu build kernel
./unixnu build payload
fastboot flash boot out/merlinx-uefi-emmc-mali-vsteps.img
```

Read the GOP status lines. The expected stopping point is currently `V1 read VSRAM`. A GPU result requires M2 to complete and `GPU_ID` to read `0x70021000`.

## Maintenance

The maintainer owns the driver repository and its release decisions. Device integration and hardware evidence stay in [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx).
