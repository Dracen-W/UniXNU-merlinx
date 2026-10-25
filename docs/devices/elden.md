# Lenovo Legion Y700 Gen 4 (elden)

A community port for SM8750P, maintained by jerry-2009.

- [jerry-2009/UNIXNU_elden](https://github.com/jerry-2009/UNIXNU_elden)

## State

TB322FC completes two cold RAM boots with the same image. The menu, 1904x3040 GOP display, volume-key input and FAT payload work. The next phase prepares an XNU-capable image.

Use the repository's pinned third-party sources, firmware overlays and phase 1 build instructions. Its device driver directory includes `EldenRamDiskDxe`.

## Use this port

1. Read the pinned source and validation records below. Prepare the required firmware and system inputs.
2. Clone the device repository and follow its build guide from the checkout:

```sh
git clone --branch main https://github.com/jerry-2009/UNIXNU_elden.git
cd UNIXNU_elden
git checkout c591b1a0a471f24978aba25c56c8f6ec9bd06668
git submodule update --init --recursive
```

3. Complete the build for that revision, follow the [bring-up order](/porting/bring-up), and use [log capture](/porting/log-capture) to record the last confirmed stage.
4. Save results in the [porting record](/porting/records). See [Port a device](/porting/steps) for board structure and component selection.

## Sources

- [README](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/README.md)
- [Phase 1 build](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/docs/PHASE1.md)
- [Hardware results](https://github.com/jerry-2009/UNIXNU_elden/blob/c591b1a0a471f24978aba25c56c8f6ec9bd06668/docs/PHASE1-STATUS.md)
