# Xiaomi Redmi Note 9 (merlinx)

merlinx is the Xiaomi Redmi Note 9 with the MediaTek MT6768/MT6769Z SoC. The port below is an independent community project. The UniXNU organization does not maintain it.

| Repository | Maintainer | State |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | UEFI boots, custom menu and boot image work; storage init blocks UniXNU and XNU |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium port, fixed BootShim and custom GOP boot menu |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | Native eMMC driver for merlinx |

## State

| Layer | State |
| --- | --- |
| Hardware | panel, PMIC keys, p33 boot partition and bootloader behavior measured |
| UEFI firmware | `merlinxPkg` builds from source and boots from p33 |
| BootShim | self-overwrite fixed; the 320-byte source shim relocates the copy/clean block before loading the firmware volume |
| Boot menu | custom GOP menu renders on the panel; volume-down scrolls; volume-up and power both select a menu entry; the current source mapping is confirmed working |
| Boot image | 64 MiB p33 image with the fixed BootShim, 3 MiB firmware volume, trimmed DTB and Magisk ramdisk; flashes with `fastboot flash boot` |
| Storage | SdCardDxe and eMMC init time out; Internal Storage reports `EFI_NOT_FOUND`; USB boot also fails |
| UniXNU and XNU | not reached; no ADT, payload or kernel run |
| Display polish | scanout starts mid-panel; the menu is shifted down and the OEM logo can remain in the upper half |

Volume-up and power both select a menu entry because the PMIC key mask covers both the power and home/volume-up bits. Both keys work; this is the current mapping, not the final dedicated key layout.

The PMIC protocol lookup is runtime, not a `Depex` entry, so volume-down and the rest of the keypad still work if the PMIC wrapper driver is unavailable. Holding volume-up during boot is not supported: the first PMIC read is used as the baseline and returns false.

## Build

The device repository keeps `Mu_Basecore` at the upstream commit. `board.toml` lists the boot menu patch, so the shared build driver applies it to `Mu_Basecore` for the firmware build and reverses it afterwards. A fresh clone does not need a local Mu fork.

```sh
git clone --recurse-submodules https://github.com/Dracen-W/UniXNU-merlinx
git clone https://github.com/Dracen-W/uefi-merlinx
cd UniXNU-merlinx
./unixnu build firmware -D . --uefi ../uefi-merlinx --edk2 ../uefi-merlinx/Mu_Basecore
```

The firmware volume is `Build/merlinxPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`. The boot image wraps that volume with the fixed BootShim, the trimmed device tree and the ramdisk, then pads the result to 64 MiB for p33.

## Boot

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

`p33` is the GPT partition number of the Android `boot` partition, 64 MiB on this device; `fastboot flash boot` writes there.

`fastboot boot` does not work on this bootloader; it fails with `usb_read failed with status e00002ed`. Use `fastboot flash boot` and keep a stock boot image for recovery.

The panel is an FT8719 at 1080x2340. The visible UEFI scanout starts partway down the panel, so the boot menu is positioned lower than on a full-screen scanout and the stock logo can remain above it.

## Goals

1. Fix the SdCardDxe and eMMC timeout so Internal Storage enumerates.
2. Make USB storage or USB boot usable as a recovery path.
3. Clean up the panel offset, background and logo handoff.
4. Add the ADT and payload, then reach XNU entry.
5. Move from XNU entry to root mount and iOS userland.

There is no UART on this device. Early boot evidence comes from the panel and from flashing test images to p33.

## Maintenance

The maintainer owns both repositories and their release decisions. UniXNU documentation lists the port and its state; it does not take over maintenance.
