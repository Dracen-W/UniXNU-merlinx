
<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

# UniXNU 官方页面

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
  <a href="https://x.com/UniXNU_new"><img alt="X" src="https://img.shields.io/badge/X-@UniXNU__new-000000?logo=x&logoColor=white"></a>
</p>

**本项目始终免费、自由、开源，且不分发 Apple 和任何厂商的私有二进制以合规。请不要通过盈利性质的网盘分发或销售预编译镜像。若您执意如此，请[参见](https://baidu.ma/?q=5a2k5YS/5oCO5LmI5Yqe55CG5oi35Y+j5pys)。**

<p align="center">
  <a href="README.md">English</a> | 简体中文
</p>

# UniXNU

> **迁移通知：** 请按[迁移指南](https://unixnu.com/zh/porting/migration)更新设备仓库，接入共享 SoC 支持并统一组件名称。

一个独立项目，通过 UEFI 在非 Apple ARM64 硬件上运行 iOS 与 macOS。UniXNU 类似 OpenCore，是通用的 UEFI 环境和驱动加载系统：通过统一的 UEFI 层加载设备所需的驱动并启动系统。

UniXNU 为不同 SoC 和设备的移植提供共享构建工具、ARM64 支持、OS 配置和可复用组件。SoC 仓库提供公共平台支持，设备仓库提供板级配置和硬件集成。

**🎉 News:** Adreno740 和 AdrenoMetal 实现 A740/SM8550 GPU 加速。详见 [Adreno 740 驱动文档](https://unixnu.com/zh/drivers/adreno740)。

文档：[unixnu.com](https://unixnu.com/zh/)。从[移植设备](https://unixnu.com/zh/porting/)或[构建与启动](https://unixnu.com/zh/building)开始。

## 红米 Note 9（Merlinx）移植

UniXNU 在红米 Note 9（merlinx，MT6768）上从 p33 启动分区引导 iPadOS 17.7.11（21H461）。该机型的 bootloader 不支持 `fastboot boot`，所有镜像都用 `fastboot flash boot` 写入。

目前可用：

* **启动链**：UEFI 固件、DarwinBoot、4 KiB 用户 ABI 的 XNU、iPadOS 恢复配置和打包的 Data 种子都能从 p33 启动。
* **存储**：NativeEmmc 取代 UEFI 存储路径；SystemOS cryptex 与可写 Data 种子在 `launchd` 之前挂载。
* **用户态**：`launchd` 拉起完整服务图，`lsd` 完成首次 LaunchServices 建库，SpringBoard 画出锁屏时钟和 Dock。
* **显示**：legacy GOP 提供者重新编程 `DISP_OVL0`/`DISP_OVL0_2L`，直接扫描 XNU 的 framebuffer，消除一帧的垂直偏移和 LK 残留 logo；OSD 文字在开机帧后停绘。
* **触摸**：FocalTech FT8719 驱动经 SPI0 下发 Xinli 固件，并通过 `NativeHidInput`（`IOHIDDevice`）注入触点。
* **诊断**：EP0 MUSB 日志/命令通道（VID `0x05AC`，PID `0x12A8`）输出内核环形日志、`ps`、故障记录和显示/触摸命令，主机端用 `scripts/merlinx_usb_log.py`。
* **GPU（内核）**：Mali-G52 路径完成 M2/M3/M4/M6，读出 `GPU_ID` 并报告 `MALI RC 0`。

仍在处理：PosterBoard 因没有 GPU 退出，首个 SpringBoard 可能先触发 FrontBoard 看门狗，再由新实例重新画出锁屏；用户态 Metal/PanVK、Wi-Fi/蓝牙、音频和电池上报尚未实现。

构建与刷入：

```bash
./unixnu firmware
./unixnu kernel
./unixnu payload --kernel <kernel.macho> --data-ramdisk <seed>.dbr --trust-cache <bundle>
./unixnu bootimg
fastboot flash boot out/merlinx-uefi-emmc-mali-*.img
```

启动链和 UI 问题见 [bringup-quirks-and-pitfalls.md](bringup-quirks-and-pitfalls.md)，各模块状态见 [porting.toml](porting.toml)。

## 仓库

| 仓库 | 内容 |
| --- | --- |
| [UniXNU](https://github.com/UniXNU/UniXNU) | 构建工具、组件目录、ARM64 支持、OS 配置、UEFI package、文档 |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | 红米 Note 9 设备仓库（本仓库） |
| [UniXNU-MT6768-common](https://github.com/Dracen-W/UniXNU-MT6768-common) | MT6768 共享支持 |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | 红米 Note 9 的 UEFI 固件 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | 原生 eMMC 存储驱动 |
| [Mali-G52](https://github.com/Dracen-W/Mali-G52) | Mali-G52 内核驱动 |
| [MaliMetal](https://github.com/Dracen-W/MaliMetal) | Mali 用户态 Metal/PanVK 前端 |
| [FocalTechTouch-FT8719](https://github.com/Dracen-W/FocalTechTouch-FT8719) | FocalTech FT8719 触摸驱动 |

克隆时使用 `--recurse-submodules`。

## 参与贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

BSD-2-Clause-Patent，见 [LICENSE](LICENSE)。
