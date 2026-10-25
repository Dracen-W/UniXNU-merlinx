# Xiaomi Pad 7 / POCO Pad X1 (uke)

A community-maintained port for SM7675. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-uke](https://github.com/Allenwdk/UniXNU-uke) | [Allenwdk](https://github.com/Allenwdk) |
| [uefi-uke](https://github.com/Allenwdk/uefi-uke) | [Allenwdk](https://github.com/Allenwdk) |

## State

The device reaches the Apple logo, mounts the UFS root and starts `launchd`. The progress record includes `Early boot complete`, `driverkitd` startup and display-surface adoption. A subsequent reboot remains under investigation; its cause is undetermined.

GPU, physical touch and the remaining peripherals require their own device validation.

## Sources

- [Progress: Allenwdk/UniXNU-uke](https://github.com/Allenwdk/UniXNU-uke/blob/cf552816de501906082405ebb0e355d8c70f7b60/docs/PROGRESS.md)

## Build

Firmware and boot image on Linux:

```sh
cd /path/to/UniXNU
./unixnu defconfig uke_defconfig -D ../UniXNU-uke
./unixnu fetch -D ../UniXNU-uke
./unixnu build firmware --skip-preflight -D ../UniXNU-uke --uefi ../uefi-uke -j 8
./unixnu bootimg -D ../UniXNU-uke --uefi ../uefi-uke --dtb ../uke-port/data/vendor_boot.dtb --ramdisk none
```

Kernel in the repository's GitHub Actions:

```sh
gh workflow run build-uke-kernel
```

The userdata image needs macOS `hdiutil`. The current stage, artifacts and remaining work are in `docs/PROGRESS.md` in the device repository.

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
