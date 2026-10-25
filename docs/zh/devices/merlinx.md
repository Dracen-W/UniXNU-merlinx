# Redmi Note 9（merlinx）

merlinx 是搭载联发科 MT6768/MT6769Z SoC 的 Redmi Note 9。下面的移植是独立社区项目，UniXNU 组织不维护它。

| 仓库 | 维护者 | 状态 |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | UEFI 可启动，自定义菜单和启动镜像工作；存储初始化阻塞 UniXNU 和 XNU |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium 移植、修复后的 BootShim 和自定义 GOP 启动菜单 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | merlinx 的原生 eMMC 驱动 |

## 状态

| 层 | 状态 |
| --- | --- |
| 硬件 | 已测量屏幕、PMIC 按键、p33 boot 分区和 bootloader 行为 |
| UEFI 固件 | `merlinxPkg` 可从源码构建，并从 p33 启动 |
| BootShim | 自覆盖问题已修复；320 字节源码 shim 会先把 copy/clean 代码块搬到安全地址，再加载固件卷 |
| 启动菜单 | 自定义 GOP 菜单可在屏幕上显示；音量下滚动；音量上和电源键都能选中菜单项；当前源码映射已确认可用 |
| 启动镜像 | 64 MiB p33 镜像，包含修复后的 BootShim、3 MiB 固件卷、裁剪后的 DTB 和 Magisk ramdisk；用 `fastboot flash boot` 刷入 |
| 存储 | SdCardDxe 和 eMMC 初始化超时；Internal Storage 报 `EFI_NOT_FOUND`；USB 启动也失败 |
| UniXNU 和 XNU | 尚未到达；没有 ADT、负载或内核运行记录 |
| 显示收尾 | 可见扫描输出从屏幕中部开始；菜单整体偏下，上半屏可能残留 OEM logo |

当前 PMIC 按键掩码同时覆盖电源键和 home/音量上键，所以音量上和电源键都会选中菜单项。两个键都能正常使用；这是当前映射，不是最终的独立按键布局。

PMIC 协议在运行时查找，没有加入 `Depex`；因此 PMIC wrapper 驱动不可用时，音量下和其他按键仍然工作。不支持开机时一直按住音量上：第一次 PMIC 读取会作为基线并返回 false。

## 构建

设备仓库让 `Mu_Basecore` 保持在上游提交。`board.toml` 列出了启动菜单补丁，共享构建驱动会在固件构建前把它应用到 `Mu_Basecore`，构建后自动反向应用。新 clone 不需要维护本地 Mu fork。

```sh
git clone --recurse-submodules https://github.com/Dracen-W/UniXNU-merlinx
git clone https://github.com/Dracen-W/uefi-merlinx
cd UniXNU-merlinx
./unixnu build firmware -D . --uefi ../uefi-merlinx --edk2 ../uefi-merlinx/Mu_Basecore
```

固件卷位于 `Build/merlinxPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`。启动镜像把该固件卷、裁剪后的设备树和 ramdisk 与修复后的 BootShim 打包在一起，再补齐到 p33 所需的 64 MiB。

## 启动

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

`p33` 是 Android `boot` 分区的 GPT 分区号，本机为 64 MiB；`fastboot flash boot` 写入这个分区。

这个 bootloader 不支持 `fastboot boot`，会报 `usb_read failed with status e00002ed`。请使用 `fastboot flash boot`，并保留原厂 boot 镜像用于恢复。

屏幕是 1080x2340 的 FT8719。UEFI 可见扫描输出从屏幕中部开始，因此启动菜单比全屏扫描时更靠下，原厂 logo 可能残留在上半屏。

## 目标

1. 修复 SdCardDxe 和 eMMC 超时，让 Internal Storage 枚举出来。
2. 让 USB 存储或 USB 启动可作为恢复路径。
3. 清理屏幕偏移、背景和 logo 交接。
4. 加入 ADT 和负载，进入 XNU 入口。
5. 从 XNU 入口推进到根挂载和 iOS 用户态。

这台设备没有 UART。早期启动证据来自屏幕显示，以及向 p33 刷入测试镜像。

## 维护

维护者拥有两个仓库及其发布决定。UniXNU 文档只记录移植和状态，不接管维护。
