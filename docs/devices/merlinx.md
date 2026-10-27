# Xiaomi Redmi Note 9 (merlinx)

merlinx is the Xiaomi Redmi Note 9 with the MediaTek MT6768/MT6769Z SoC. The port below is an independent community project. The UniXNU organization does not maintain it.

| Repository | Maintainer | State |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | XNU mounts SystemOS, initproc runs, and launchd exec succeeds; Data volume and userland services are pending |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium port, fixed BootShim and custom GOP boot menu |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | Native eMMC block driver used by the userdata and SystemOS path |

## State

| Layer | State |
| --- | --- |
| Hardware | panel, PMIC keys, p33 boot partition and bootloader behavior measured |
| UEFI firmware | `merlinxPkg` builds from source and boots from p33 |
| BootShim | self-overwrite fixed; the 320-byte source shim relocates the copy/clean block before loading the firmware volume |
| Boot menu | custom GOP menu renders on the panel; volume-down scrolls; volume-up and power both select a menu entry; the current source mapping is confirmed working |
| Boot image | 64 MiB p33 image with the fixed BootShim, 3 MiB firmware volume, trimmed DTB and ramdisk; flashes with `fastboot flash boot` |
| Storage | NativeEmmc enumerates the userdata partition; the OS and SystemOS volumes appear; the owner path mounts the SystemOS cache carrier |
| UniXNU and XNU | XNU reaches the root and the SystemOS cache; the 4 KiB kernel uses `CONFIG_XNU_NATIVE_CS_HASH_CACHE` for the original 16 KiB CodeDirectory; initproc and launchd exec complete |
| Kernel display | the kernel draws `ROOT`, `VOL`, `CACHE`, `EXEC` and `LAUNCHD` status lines through the GOP framebuffer |
| Data | not verified; the repository does not include a Data seed, so writable `/private/var` and the launchd service graph are pending |
| Userland | launchd exec is confirmed; userland services, display ownership and rendering are not verified |
| Display polish | scanout starts mid-panel; the menu is shifted down and the OEM logo can remain in the upper half |

Volume-up and power both select a menu entry because the PMIC key mask covers both the power and home/volume-up bits. Both keys work; this is the current mapping, not the final dedicated key layout.

The PMIC protocol lookup is runtime, not a `Depex` entry, so volume-down and the rest of the keypad still work if the PMIC wrapper driver is unavailable. Holding volume-up during boot is not supported: the first PMIC read is used as the baseline and returns false.

## Build

The device repository keeps `Mu_Basecore` at the upstream commit. `board.toml` lists the boot menu patch, so the shared build driver applies it to `Mu_Basecore` for the firmware build and reverses it afterwards. A fresh clone does not need a local Mu fork.

```sh
git clone --recurse-submodules https://github.com/Dracen-W/UniXNU-merlinx
git clone https://github.com/Dracen-W/uefi-merlinx
cd UniXNU-merlinx
./unixnu defconfig merlinx_defconfig -D .
./unixnu build kernel
./unixnu build payload
./unixnu build bootimg --uefi ../uefi-merlinx --dtb <merlinx.dtb>
```

The payload step consumes the board trust cache, platform identity and other inputs described by the device repository. These inputs are not distributed. The Data seed is not part of the current image.

The firmware volume is `Build/merlinxPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`. The boot image wraps that volume with the fixed BootShim, the trimmed device tree and the ramdisk, then pads the result to 64 MiB for p33.

## Data seed

The repository does not include a Data seed. The generation helpers are derived from the UniXNU-fajita Data seed generators and are licensed under BSD-2-Clause-Patent:

- [make-data-seed-linux.py](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-linux.py)
- [make-data-seed-macos.py](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-macos.py)
- [data-seed.json](https://github.com/johnpanos/UniXNU-fajita/blob/main/data-seed.json)

`data-seed.json` describes a case-sensitive, unencrypted APFS Data volume with no role and only empty directories and preference files. It contains no keys, user records, or Apple files.

On Linux, the generator formats the image with `mkapfs` and `apfsck` and edits the fresh APFS catalog. It needs a packer from the UniXNU tree:

```sh
python3 scripts/make-data-seed-linux.py \
  --packer drivers/DarwinBoot/Test/packed_ramdisk.py \
  --fetch-apfsprogs \
  --spec data-seed.json \
  --out .cache/data-seed
```

On macOS, the generator uses `diskutil` and `hdiutil`:

```sh
python3 scripts/make-data-seed-macos.py \
  --packer drivers/DarwinBoot/Test/packed_ramdisk.py \
  --spec data-seed.json \
  --out .cache/data-seed
```

The current bring-up image uses a temporary NativeEmmc manifest stub and a volatile RamBlock path. It does not provide persistent Data or production boot-manifest verification.

The generators write `data-linux.dbr` or `data-macos.dbr` and a JSON report with the APFS volume UUID. Use that UUID for `CONFIG_XNU_OWNER_DATA_VOLUME_UUID`, and pass the packed seed to the payload step with `--data-ramdisk`. The raw image, packed seed and report are local inputs and are not committed.

## Boot

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

`p33` is the GPT partition number of the Android `boot` partition, 64 MiB on this device; `fastboot flash boot` writes there.

`fastboot boot` does not work on this bootloader; it fails with `usb_read failed with status e00002ed`. Use `fastboot flash boot` and keep a stock boot image for recovery.

The panel is an FT8719 at 1080x2340. The visible UEFI scanout starts partway down the panel, so the boot menu is positioned lower than on a full-screen scanout and the stock logo can remain above it.

## Goals

1. Mount a Data seed and prove that `/private/var` is writable.
2. Confirm the first launchd services and identify the first missing service or resource.
3. Add a userland display path after the early service graph is stable. Userland rendering is not implemented.
4. Clean up the panel offset, background and logo handoff.
5. Add a persistent log or recovery path without relying on UART.

There is no UART on this device. Early boot evidence comes from the panel and from flashing test images to p33.

## Maintenance

The maintainer owns the three repositories and their release decisions. UniXNU documentation lists the port and its state; it does not take over maintenance.
