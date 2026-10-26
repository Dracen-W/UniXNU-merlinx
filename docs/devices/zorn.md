# Redmi K80 (zorn)

A community port for SM8650-AB, maintained by kerrinz.

- [kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn)
- [kerrinz/uefi-zorn](https://github.com/kerrinz/uefi-zorn)

## State

The device runs UEFI, the menu, DarwinBoot and XNU platform initialization. A read-only UFS probe reads the SHNGROOT manifest from userdata and registers its OS APFS container as `md0`; the root mounts and the screen records `APOK`.

The current stop is `MEND`, after two `vm_map_delete` calls return while loading user mappings. The payload also carries a restore RAM disk. Touch, GPU and the desktop remain pending. Use the firmware repository's payload packager and its 512-byte FAT cluster setting.

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

- [README: kerrinz/UniXNU-zorn](https://github.com/kerrinz/UniXNU-zorn/blob/8e9d3a9cce8ea1f4e0641a124bc22baf7f4fd712/README.md)
