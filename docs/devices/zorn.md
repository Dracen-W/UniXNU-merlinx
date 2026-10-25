# Redmi K80 (zorn)

A community port for SM8650-AB, maintained by kerrinz.

- [kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn)
- [kerrinz/uefi-zorn](https://github.com/kerrinz/uefi-zorn)

## State

The 12 GB / 256 GB device runs UEFI, the menu, DarwinBoot and XNU platform initialization. The iPadOS 17.7.11 restore RAM disk mounts as APFS `md0`. The kernel opens `/sbin/launchd` and stops before `parse_machfile`, with `PARSE` on screen.

Native UFS, touch and GPU support remain pending. Payload packaging uses the firmware repository's `scripts/make-zorn-payload.py`, including its 512-byte FAT cluster setting.

## Use this port

1. Read the pinned source and validation records below. Prepare the required firmware and system inputs.
2. Clone the device repository and follow its build guide from the checkout:

```sh
git clone --branch main https://github.com/kerrinz/UniXNU-zorn.git
cd UniXNU-zorn
git checkout af23788d9ca7d48ff1576ff83664bbac975ba3ad
git submodule update --init --recursive
```

3. Complete the build for that revision, follow the [bring-up order](/porting/bring-up), and use [log capture](/porting/log-capture) to record the last confirmed stage.
4. Save results in the [porting record](/porting/records). See [Port a device](/porting/steps) for board structure and component selection.

## Sources

- [README](https://github.com/kerrinz/UniXNU-zorn/blob/af23788d9ca7d48ff1576ff83664bbac975ba3ad/README.md)
