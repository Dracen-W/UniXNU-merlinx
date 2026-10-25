# OnePlus 5 (cheeseburger)

A community-maintained port for MSM8998. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger) | [DinnerABC](https://github.com/DinnerABC) |
| [UniXNU-cheeseburger](https://github.com/lss717/UniXNU-cheeseburger) | [lss717](https://github.com/lss717) |

## State

The DinnerABC port reaches PID 1 in EL0, mounts the root and SystemOS, and provides writable APFS Data in RAM. The P18 boot record stops at `watchdogd[106]` with `SIGTRAP`; user-space PC and stack capture are the next diagnostic inputs.

The lss717 port separately records UEFI, menu and payload checks on hardware. Its XNU integration remains in progress.

## Sources

- [P18 boot result: DinnerABC/UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/c5463363de7ca0ad541b8132f29c7f0a3cef718c/docs/P18_BOOT_RESULT.md)
- [README: lss717/UniXNU-cheeseburger](https://github.com/lss717/UniXNU-cheeseburger/blob/de1af958fc2530c2626217a36ab276e1f77f57a1/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
