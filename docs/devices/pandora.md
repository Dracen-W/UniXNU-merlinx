# Xiaomi 17 Pro (pandora)

A community-maintained port for SM8850. Current results are based on the maintainer records at the revisions linked below.

| Repository | Maintainer |
| --- | --- |
| [data-a-live](https://github.com/ShueiTsuki/data-a-live) | [ShueiTsuki](https://github.com/ShueiTsuki) |

## State

The iOS 16.7.16 system reaches the home and lock screens with touch and physical buttons. The port uses a device loader, native UFS and a file-backed root in Android userdata.

Adreno 840 integration includes Vulkan and Metal, and the maintainer records the first GPU-rendered home-screen frame. Rapid interaction can hang the system. CPU rendering remains the default while GPU stability is investigated.

## Sources

- [README: ShueiTsuki/data-a-live](https://github.com/ShueiTsuki/data-a-live/blob/1a6c05fbc03695adb41a636210b9c48f40e14563/README.md)

## Build

The kernel is built by the `xnu.yml` GitHub Actions workflow with `machine_config=PANDORA`. The scripts in the repository build the ADT and the payload in WSL and the loader with the Android NDK:

```sh
python3 tools/mkprelink.py <kernel> <kernelcache> <out>
KERNEL=<out> tools/boot_ios.sh --file /data/local/tmp/iosroot.img --run <N> --rd ufsroot0
```

`tools/boot_ios.sh` resolves the extents of the pinned root file, builds the payload and the loader image, and starts it with `fastboot boot`. The kernel, the kernelcache, the root image and the trust caches are external inputs and are not stored in the repository.

## Use this port

1. Read the repository build instructions and pinned records. Prepare its firmware, system images and board inputs.
2. Build with that repository's commands. Use [Build and boot](/building) for common configuration.
3. Follow the [bring-up order](/porting/bring-up) and save results through [log capture](/porting/log-capture).
4. Update the [porting record](/porting/records) with the source revisions and last confirmed stage.
