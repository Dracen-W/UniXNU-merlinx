# Redmi Note 10 Pro (sweet)

A community-maintained port for SM7150. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [UniXNU-sweet](https://github.com/CrackerCat/UniXNU-sweet) | [CrackerCat](https://github.com/CrackerCat) |

## State

The source includes the Sweet ADT, ArmCpu and DarwinBoot overrides, and an Adreno 618 HFI transport. The maintained snapshot records source-kernel and platform-archive builds, plus QEMU CPU registration and advancing timer interrupts. Self-IPI delivery remains unqualified.

Device firmware integration, Apple component prelink and a root target remain pending. Run the host checks from the repository root before preparing an image.

## Sources

- [README: CrackerCat/UniXNU-sweet](https://github.com/CrackerCat/UniXNU-sweet/blob/d3dd6fdf9e889f08b2564d92f7fef5b5c913263e/README.md)

## Build

Host checks:

```sh
python3 scripts/check-host.py
python3 scripts/make-sweet-adt.py --output out/sweet.adt
```

Device repository integration:

```sh
python3 ../UniXNU/unixnu defconfig sweet_defconfig -D . --out out/driver
python3 ../UniXNU/unixnu status -D . --out out/driver
```

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
