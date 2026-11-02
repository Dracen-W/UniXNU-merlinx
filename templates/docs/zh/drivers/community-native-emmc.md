# NativeEmmc

面向 Redmi Note 9（merlinx，MediaTek MT6768）的原生 eMMC 存储驱动。它向 XNU 发布设备 userdata 存储，并用于 root、OS 和 SystemOS 路径。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | merlinx 原生 eMMC 块驱动；GPT userdata 发现和 OS/SystemOS 块路径 |

## 状态

| 层 | 状态 |
| --- | --- |
| 主机和控制器 | 驱动替代超时的 UEFI 存储路径，向 XNU 发布 eMMC userdata 块设备 |
| 分区 | GPT 发现 Android userdata 分区及 APFS OS、SystemOS 卷 |
| Root 路径 | XNU 挂载 SystemOS cache carrier，运行 initproc，并到达 launchd、xpcproxy 和 SpringBoard |
| Data | 打包的 Data seed 以读写方式挂载；这些 bring-up 镜像禁用了 watchdogd |
| 持久化 | manifest backend 和持久化路径仍是 stub；Data 和 manifest 状态还不是生产可用 |
| 剩余工作 | 替换 bring-up stub，验证重启后写入仍存在，并接入真实 boot-manifest backend |

存储路径是板级相关的。eMMC 控制器、PHY、时钟、复位和电源输入保留在设备仓库，通用块逻辑保留在驱动仓库。

## 集成

`board.toml`、`components.toml` 和 `configs/merlinx_defconfig` 选择 `NativeEmmc`。设备树提供控制器和 pinctrl 资源。owner 补丁挂载 SystemOS cache carrier 和打包的 Data seed。

## 验证

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

屏幕上的 `ROOT`、`VOL`、`CACHE`、`EXEC`、`LAUNCHD` 和 `SPAWN` 显示存储与用户态检查点。Data seed 可写本身不能证明重启后仍然持久。

## 维护

维护者拥有仓库及其发布决定。UniXNU 文档记录驱动及其状态，不接管维护。
