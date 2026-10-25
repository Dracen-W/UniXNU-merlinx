# Redmi K30 5G (picasso)

A community-maintained port for SM7250. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-picasso](https://github.com/YinCQL/UniXNU-picasso) | [YinCQL](https://github.com/YinCQL) |

## State

UEFI, the menu, GOP, physical keys and UFS mass-storage access are recorded as working. XNU starts through UEFI. Root-volume integration and GPU support remain pending.

## Sources

- [README: YinCQL/UniXNU-picasso](https://github.com/YinCQL/UniXNU-picasso/blob/879f753a01790950350bc105cb854074ffb2005b/README.md)

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
