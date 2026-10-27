<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark.svg">
    <source media="(prefers-color-scheme: light)" srcset="assets/logo-light.svg">
    <img alt="UniXNU" src="assets/logo-light.svg" width="200">
  </picture>
</p>

<p align="center">
  <a href="https://unixnu.com"><img alt="文档" src="https://img.shields.io/badge/docs-unixnu.com-2A5BD7?logo=readthedocs&logoColor=white"></a>
  <a href="https://t.me/unixnu"><img alt="Telegram" src="https://img.shields.io/badge/Telegram-@unixnu-26A5E4?logo=telegram&logoColor=white"></a>
  <a href="https://qun.qq.com/universal-share/share?ac=1&authKey=Pubz23DOCY76nxjlGuSms2zesk4jRvas4zrXJRYk7omf7CprXbGQigLEI%2BCoYR8D&busi_data=eyJncm91cENvZGUiOiIxMTI2MTE2NzU5IiwidG9rZW4iOiJwSHJsUDN1UnN2Vi8yM210TjJRbzBIcERmcUpyOS9lb0hHVVpadWdvVFdScVpSYUFLT1YzTk5TWC9mN2xWYmtIIiwidWluIjoiMzE1MTEwOTc0MSJ9&data=_cJgr-jX9yJ2-GXwVjRN5FOhDyk5B-HAfIk5LSunbW_XkkmKqNXbpH33wzP93J0p9DI1fbjgBUrUfTVhwDcxjg&svctype=4&tempid=h5_group_info"><img alt="QQ 群" src="https://img.shields.io/badge/QQ%20Group-join-12B7F5?logo=qq&logoColor=white"></a>
  <a href="https://x.com/unixnu"><img alt="X" src="https://img.shields.io/badge/X-@unixnu-000000?logo=x&logoColor=white"></a>
</p>

<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

# UniXNU

> **迁移通知：** 请按[迁移指南](https://unixnu.com/zh/porting/migration)更新设备仓库，接入共享 SoC 支持并统一组件名称。

一个独立项目，通过 UEFI 在非 Apple ARM64 硬件上运行 iOS 与 macOS。UniXNU 类似 OpenCore，是通用的 UEFI 环境和驱动加载系统：通过统一的 UEFI 层加载设备所需的驱动并启动系统。

UniXNU 为不同 SoC 和设备的移植提供共享构建工具、ARM64 支持、OS 配置和可复用组件。SoC 仓库提供公共平台支持，设备仓库提供板级配置和硬件集成。

**🎉 News:** Adreno740 和 AdrenoMetal 在 sheng 上提供 GPU 加速。详见 [Adreno 740 驱动文档](https://unixnu.com/zh/drivers/adreno740)。

## Redmi Note 9（merlinx）移植

这个 fork 包含由 [Dracen-W](https://github.com/Dracen-W) 维护的 Redmi Note 9（merlinx，MT6768/MT6769Z）独立移植。UniXNU 组织不维护这个 fork。

| 仓库 | 内容 |
| --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | 板级配置、内核补丁、payload 和启动镜像 |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | Mu-Silicium 固件、修复后的 BootShim 和 GOP 启动菜单 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | userdata 和 SystemOS 路径使用的原生 eMMC 块驱动 |

UEFI 固件、BootShim、启动菜单和 64 MiB p33 启动镜像可用。NativeEmmc 枚举 userdata 和 SystemOS，XNU 到达根和 SystemOS cache，4 KiB 内核使用 `CONFIG_XNU_NATIVE_CS_HASH_CACHE` 处理原版 16 KiB CodeDirectory。initproc 和 launchd exec 完成。内核通过 GOP framebuffer 绘制 `ROOT`、`VOL`、`CACHE`、`EXEC` 和 `LAUNCHD` 状态行。

当前镜像没有 Data seed。可写 `/private/var`、launchd 服务图、用户态显示接管和渲染尚未验证。

设备说明、构建步骤、启动条件和目标见 [docs/zh/devices/merlinx.md](docs/zh/devices/merlinx.md)。

文档：[unixnu.com](https://unixnu.com/zh/)。从[移植设备](https://unixnu.com/zh/porting/)或[构建与启动](https://unixnu.com/zh/building)开始。

## 仓库

| 仓库 | 内容 |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | 构建工具、组件目录、ARM64 支持、OS 配置、UEFI package、文档 |
| [UniXNU-sm8550-common](https://github.com/UniXNU/UniXNU-sm8550-common) | SM8550 共享支持 |
| [UniXNU-sheng](https://github.com/UniXNU/UniXNU-sheng) | 小米平板 6S Pro |
| [uefi-sheng](https://github.com/UniXNU/uefi-sheng) | 小米平板 6S Pro 的 UEFI 固件 |

克隆时使用 `--recurse-submodules`。

## 参与贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

BSD-2-Clause-Patent，见 [LICENSE](LICENSE)。
