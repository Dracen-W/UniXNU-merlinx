# Nothing Phone (3a) (asteroids)

A community-maintained port for SM7635. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-asteroids](https://github.com/rootstockos/UniXNU-asteroids) | [rootstockos](https://github.com/rootstockos) |

| [uefi-asteroids](https://github.com/rootstockos/uefi-asteroids) | [rootstockos](https://github.com/rootstockos) |

## State

XNU runs on eight CPUs and mounts the UFS userdata root. `launchd`, SystemOS and SpringBoard start, and the text console works. SpringBoard restarts while the display client opens without presenting a frame.

Adreno 810 and GMU integration are in development. Display presentation and GPU execution require device results.

## Sources

- [README: rootstockos/UniXNU-asteroids](https://github.com/rootstockos/UniXNU-asteroids/blob/78d4286b8aee9e67f3022a7027f2a3c4f73dd7e3/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
