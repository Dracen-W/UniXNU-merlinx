# OnePlus Ace 5 Pro (hummer)

A community-maintained port for SM8750. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-hummer](https://github.com/2426120557/UniXNU-hummer) | [2426120557](https://github.com/2426120557) |

## State

UEFI display and keys work on hardware. XNU starts six CPUs through PSCI, and the 160 MiB payload builds. The current device run waits for the root device. The root UUID adjustment requires another device run; Data, GPU and touch integration remain pending.

## Sources

- [README: 2426120557/UniXNU-hummer](https://github.com/2426120557/UniXNU-hummer/blob/a24338025de2c8e1629f090ea4c3a51f980388a0/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
