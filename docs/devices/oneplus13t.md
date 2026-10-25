# OnePlus 13T (oneplus13t)

A community-maintained port for SM8750. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-Pagani_SM8750](https://github.com/IssacL04/UniXNU-Pagani_SM8750) | [IssacL04](https://github.com/IssacL04) |

## State

The stage 7 run 39 record confirms root mounting and the system interface on hardware. Touch remains unavailable. Firmware, XNU, root and UI results apply to the recorded device configuration.

## Sources

- [README: IssacL04/UniXNU-Pagani_SM8750](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/9a6b057527429159a531f37c3af5b7235806caf1/README.md)

## Dual-system storage

The layout below is recorded from the 256 GB model.

| Item | Captured value |
| --- | --- |
| Recovery kernel | `6.6.118-android15-8-o-g4d611c32bedc-enc3l-4k` |
| Disk / logical sector | `/dev/block/sda` / 4096 bytes |
| Remaining userdata | Partition 15, LBA 4020972 to 44928767 |
| UniXNU partition | Partition 16, LBA 44928768 to 61705983 |
| F2FS shrink request / expected result | 40907264 / 40907535 blocks of 4096 bytes |

Use the [dual-system partition guide](/porting/dual-system-partition) to calculate a layout from the connected device. Do not copy these LBAs to another phone. Inspect the current partition table with:

```sh
python3 scripts/dual_sys/partition.py inspect
```

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
