# NativeEmmc

Native eMMC storage driver for the Redmi Note 9 (merlinx, MediaTek MT6768). It publishes the device userdata storage to XNU and is used for the root, OS and SystemOS path.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | Native eMMC block driver for merlinx; GPT userdata discovery and the OS/SystemOS block path |

## State

| Layer | State |
| --- | --- |
| Host and controller | the driver replaces the timed-out UEFI storage path and publishes the eMMC userdata block device to XNU |
| Partitions | GPT discovery exposes the Android userdata partition and the APFS OS and SystemOS volumes |
| Root path | XNU mounts the SystemOS cache carrier, runs initproc and reaches launchd, xpcproxy and SpringBoard |
| Data | a packed Data seed is mounted read-write; watchdogd is disabled for these bring-up images |
| Persistence | the manifest backend and persistence path are still stubbed; Data and manifest state are not production ready |
| Remaining work | replace the bring-up stubs, verify writes across reboot and attach the real boot-manifest backend |

The storage path is board-specific. Keep the eMMC controller, PHY, clock, reset and power inputs in the device repository and keep generic block logic in the driver.

## Integration

`board.toml`, `components.toml` and `configs/merlinx_defconfig` select `NativeEmmc`. The device tree supplies the controller and pinctrl resources. The owner patches mount the SystemOS cache carrier and the packed Data seed.

## Verification

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

On the panel, `ROOT`, `VOL`, `CACHE`, `EXEC`, `LAUNCHD` and `SPAWN` show the storage and userland checkpoints. A writable Data seed does not by itself prove persistence across reboot.

## Maintenance

The maintainer owns the repository and its release decisions. UniXNU documentation records the driver and its state; it does not take over maintenance.
