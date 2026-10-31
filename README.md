

<p align="center">
  English | <a href="README.zh-CN.md">简体中文</a>
</p>

# UniXNU Official Webpage
<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="UniXNU" src="assets/logo-light.svg" width="200">
  </picture>
</p>

<p align="center">
  <a href="https://unixnu.com"><img alt="Documentation" src="https://img.shields.io/badge/docs-unixnu.com-2A5BD7?logo=readthedocs&logoColor=white"></a>
  <a href="https://t.me/unixnu"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-@unixnu-26A5E4?logo=telegram&logoColor=white"></a>
  <a href="https://qun.qq.com/universal-share/share?ac=1&authKey=Pubz23DOCY76nxjlGuSms2zesk4jRvas4zrXJRYk7omf7CprXbGQigLEI%2BCoYR8D&busi_data=eyJncm91cENvZGUiOiIxMTI2MTE2NzU5IiwidG9rZW4iOiJwSHJsUDN1UnN2Vi8yM210TjJRbzBIcERmcUpyOS9lb0hHVVpadWdvVFdScVpSYUFLT1YzTk5TWC9mN2xWYmtIIiwidWluIjoiMzE1MTEwOTc0MSJ9&data=_cJgr-jX9yJ2-GXwVjRN5FOhDyk5B-HAfIk5LSunbW_XkkmKqNXbpH33wzP93J0p9DI1fbjgBUrUfTVhwDcxjg&svctype=4&tempid=h5_group_info"><img alt="QQ Group" src="https://img.shields.io/badge/QQ%20Group-join-12B7F5?logo=qq&logoColor=white"></a>
  <a href="https://x.com/unixnu"><img alt="X" src="https://img.shields.io/badge/X-@unixnu-000000?logo=x&logoColor=white"></a>
</p>

## Redmi Note 9 (Merlinx) port

This fork carries the independent Xiaomi Redmi Note 9 (merlinx, MT6768/MT6769Z) port maintained by [Dracen-W](https://github.com/Dracen-W). The UniXNU organization does not maintain this fork.

| Repository | Contents |
| --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | Board configuration, kernel patches, payload and boot image |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | Mu-Silicium firmware, fixed BootShim and GOP boot menu |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | Native eMMC block driver used by the userdata and SystemOS path |

The UEFI firmware, BootShim, boot menu and 64 MiB p33 boot image work. NativeEmmc enumerates userdata and SystemOS, XNU reaches the root and SystemOS cache, and the 4 KiB kernel handles the original 16 KiB CodeDirectory with `CONFIG_XNU_NATIVE_CS_HASH_CACHE`. initproc and launchd exec complete. The kernel draws `ROOT`, `VOL`, `CACHE`, `EXEC` and `LAUNCHD` status lines through the GOP framebuffer.

The repository does not include a Data seed. The current test image uses a locally generated packed seed; writable `/private/var`, the launchd service graph, userland display ownership and rendering are not verified. Data seed generation helpers are derived from the [UniXNU-fajita scripts](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-linux.py) and documented in the device notes.

Device notes, build steps, boot conditions and goals are in [docs/devices/merlinx.md](docs/devices/merlinx.md).

## Documentation

The documentation site is maintained in UniXNU:

- [merlinx device guide](https://unixnu.com/devices/merlinx)
- [Building](https://unixnu.com/building)
- [Device repository specification](https://unixnu.com/porting/device-repository)
- [Device-tree specification](https://unixnu.com/porting/device-tree)
- [Component catalog](https://unixnu.com/reference/components)
- [Migration](https://unixnu.com/porting/migration)

Report build results and device boot, storage, display, input, and GPU
observations separately, with source revisions and commands.

## License

BSD-2-Clause-Patent. See [LICENSE](LICENSE).
