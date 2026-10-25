# Xiaomi Mi Pad 4 LTE (clover)

A community-maintained port for SDM660. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-clover](https://github.com/qine233/UniXNU-clover) | [qine233](https://github.com/qine233) |

## State

The kernel mounts a mockfs RAM root and runs a static EL0 test init. The record covers timer activity, text output and fork/exec system-call probes. Current diagnostics investigate the child process write, exit and wait paths. Full iPadOS userland remains a later integration step.

## Sources

- [README: qine233/UniXNU-clover](https://github.com/qine233/UniXNU-clover/blob/f80c740a2c418639eb8f76b31eec2af0e13af862/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
