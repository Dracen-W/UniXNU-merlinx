# macOS on sheng

A community-maintained port for SM8550. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [macOS-ForSheng](https://github.com/Jelly10086/macOS-ForSheng) | [Jelly10086](https://github.com/Jelly10086) |

## State

The macOS 26.5 QEMU path runs the original arm64e `launchd`, `sh` and `bash`, uses a temporary Data volume and reaches Aqua initialization. WindowServer HID and display integration remain pending.

On sheng hardware, the recorded result is a UEFI EL1 probe. The QEMU userland results have their own configuration and evidence.

## Sources

- [README: Jelly10086/macOS-ForSheng](https://github.com/Jelly10086/macOS-ForSheng/blob/daec3714f8b4e12f5cd129efa79be894511353e5/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
