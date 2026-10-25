# Xiaomi Pad 5 Pro 5G (enuma)

A community-maintained port for SM8250-AC. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-enuma](https://github.com/wh1stl3/UniXNU-enuma) | [wh1stl3](https://github.com/wh1stl3) |
| [UniXNU-enuma](https://github.com/Tlrenhb/UniXNU-enuma) | [Tlrenhb](https://github.com/Tlrenhb) |
| [uefi-enuma](https://github.com/Tlrenhb/uefi-enuma) | [Tlrenhb](https://github.com/Tlrenhb) |

## State

The wh1stl3 port assembles an experimental 29-component prelink, structured ADT and 96 MiB FAT payload. Host-side loader checks pass. Storage transport, root integration and device boot validation remain pending.

The Tlrenhb repository supplies a separate board configuration with measured inputs still to complete.

## Sources

- [README: wh1stl3/UniXNU-enuma](https://github.com/wh1stl3/UniXNU-enuma/blob/adfb63f9bc10532d0fe6603ea806d0fdda6fae9e/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
