
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
  <a href="https://x.com/unixnu"><img alt="X" src="https://img.shields.io/badge/X-@unixnu-000000?logo=x&logoColor=white"></a>
</p>

## Redmi Note 9（merlinx）移植

这个 fork 包含由 [Dracen-W](https://github.com/Dracen-W) 维护的 Redmi Note 9（merlinx，MT6768/MT6769Z）独立移植。UniXNU 组织不维护这个 fork。

| 仓库 | 内容 |
| --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | 板级配置、内核补丁、payload 和启动镜像 |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | Mu-Silicium 固件、修复后的 BootShim 和 GOP 启动菜单 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | userdata 和 SystemOS 路径使用的原生 eMMC 块驱动 |

UEFI 固件、BootShim、启动菜单和 64 MiB p33 启动镜像可用。NativeEmmc 枚举 userdata 和 SystemOS，XNU 到达根和 SystemOS cache，4 KiB 内核使用 `CONFIG_XNU_NATIVE_CS_HASH_CACHE` 处理原版 16 KiB CodeDirectory。initproc 和 launchd exec 完成。内核通过 GOP framebuffer 绘制 `ROOT`、`VOL`、`CACHE`、`EXEC` 和 `LAUNCHD` 状态行。

仓库不包含 Data seed。当前测试镜像使用本地生成的 packed seed；可写 `/private/var`、launchd 服务图、用户态显示接管和渲染尚未验证。Data seed 生成脚本参考 [UniXNU-fajita](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-linux.py)，并在设备说明中记录。

设备说明、构建步骤、启动条件和目标见 [docs/zh/devices/merlinx.md](docs/zh/devices/merlinx.md)。

## 文档

下列文件存在于UniXNU的官方网站:

- [merlinx 设备说明](https://unixnu.com/devices/merlinx)
- [构建](https://unixnu.com/building)
- [设备仓库具体结构说明](https://unixnu.com/porting/device-repository)
- [设备树具体说明](https://unixnu.com/porting/device-tree)
- [组件目录](https://unixnu.com/reference/components)
- [移植](https://unixnu.com/porting/migration)

提交报告时，请把以下几项内容分开写，并且记得加上你的代码版本（Commit ID）和执行的命令：
• 编译/构建结果（装得顺不顺利，有没有报错）
• 设备开机情况（能不能正常启动）
• 存储盘情况（认不认得出硬盘/闪存，读写正不正常）
• 屏幕显示（画面出不出得来，有没有花屏）
• 输入设备（键盘、鼠标、触摸屏好不好使）
• 显卡/GPU 表现（画面卡不卡，3D 加速正不正常）

## 许可证

BSD-2-Clause-Patent，见 [LICENSE](LICENSE)。
