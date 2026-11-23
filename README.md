# UniXNU-merlinx (iOS 16.7.16 / `20H392` Branch)

UniXNU device repository for **Xiaomi Redmi Note 9 (`merlinx`, MediaTek MT6768 / Helio G85)** targeting **iPhone X (`iPhone10,6`, `d221ap`) iOS 16.7.16 (`20H392`, XNU `8796.141.3`)**.

> Looking for the iPadOS 17.7.11 (`21H461`) branch? Switch to the iPadOS branch (confirmed working with MaliMetal, though not yet fully tested).

## Current Hardware Status (iOS 16.7.16 `20H392`)

| Subsystem | Status | Notes |
| --- | --- | --- |
| 8-Core SMP + PAN | Working | `smp=1 cpus=8`, A75 1999 MHz / A55 1699 MHz, native ARMv8.1 PAN enabled in `bcopy.s` |
| Storage (`NativeEmmc`) | Working | 8-bit DDR eMMC 5.1, DMA 41.2 MB/s, OS + SystemOS + Data mounted cleanly |
| Display (`NativeLegacyDisplay`) | Working | MT6768 OVL0 scanout aligned; `backboardd` actively swapping 1080×2340 frames (iOS 16 3-arg `supportedFrameInfo` ABI fixed via build-time staging) |
| Touch (`FocalTechTouch-FT8719`) | Working | SPI0 firmware load `stage 3/3 err 0`, live HID frames captured |
| GPU Kernel (`Mali-G52`) | Working | `GPU_ID 0x74021000`, 849 MHz, `MALI M1 QUERY OK` / `M2 INFO OK` |
| GPU Userland (`MaliMetal`) | Ready for retest | `_MTLDevice` ABI size guard parameterized (`CONFIG_XNU_DW_MALIMETAL_MTLDEVICE_SIZE=0`); rebuild `mali-gpu-ios16-<HHMM-DDMMYY>.addon` via MaliMetal CI |
| Wi-Fi / Audio / Battery | Pending | Reference patches/docs retained until each bring-up phase completes |

## Unified Configuration Architecture (`CONFIG_XNU_DW_*`)

Configuration follows an Android/Linux Kconfig fragment hierarchy resolved by [`scripts/dw_config.py`](scripts/dw_config.py):

1. **SoC baseline**: `soc:configs/mt6768.config` (`UniXNU-MT6768-common`)
2. **Board general**: [`configs/merlinx_defconfig`](configs/merlinx_defconfig)
3. **OS final override**: [`configs/merlinx-iOS16.7.config`](configs/merlinx-iOS16.7.config) *(always takes final precedence)*

All APFS volume UUIDs (`CONFIG_XNU_DW_APFS_*`), OS build string (`CONFIG_XNU_DW_OS_BUILD`), and component switches (`CONFIG_XNU_DW_NATIVEEMMC_*`, `CONFIG_XNU_DW_MALIG52_*`, `CONFIG_XNU_DW_MALIMETAL_*`, `CONFIG_XNU_DW_FOCALTECH_*`, `CONFIG_XNU_DW_MT6768_*`, `CONFIG_XNU_DW_DISPLAY_*`) are centrally defined in `configs/merlinx-iOS16.7.config`. Each sub-repository provides a blank `configs/example.config` documenting its supported keys.

## Optional Debug Patches

Bring-up SMP debug patches (`zzzi-merlinx-debug-stall-smp.patch`, `zzzk-merlinx-smp-breadcrumbs.patch`) are archived in [`patches/xnu/debug-patch/`](patches/xnu/debug-patch/) and excluded from default `patches/xnu/series`. See `patches/xnu/debug-patch/README.md` to re-enable temporarily.

## Related Repositories

- Device: `Dracen-W/UniXNU-merlinx` (this repo; `porting.toml` validated against official UniXNU `porting-record.py`)
- SoC: `Dracen-W/UniXNU-MT6768-common`
- Storage: `Dracen-W/NativeEmmc`
- GPU Kernel: `Dracen-W/Mali-G52`
- GPU Userland: `Dracen-W/MaliMetal`
- Touch: `Dracen-W/FocalTechTouch-FT8719`
- Firmware: `Dracen-W/uefi-merlinx`
