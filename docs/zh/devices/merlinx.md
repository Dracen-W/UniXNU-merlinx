# Redmi Note 9（merlinx）

merlinx 是搭载联发科 MT6768/MT6769Z SoC 的 Redmi Note 9。下面的移植是独立社区项目，UniXNU 组织不维护它。

| 仓库 | 维护者 | 状态 |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | XNU 挂载 SystemOS，initproc 已运行，launchd exec 成功；Data 卷和用户态服务待处理 |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium 移植、修复后的 BootShim 和自定义 GOP 启动菜单 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | userdata 和 SystemOS 路径使用的原生 eMMC 块驱动 |

## 状态

| 层 | 状态 |
| --- | --- |
| 硬件 | 已测量屏幕、PMIC 按键、p33 boot 分区和 bootloader 行为 |
| UEFI 固件 | `merlinxPkg` 可从源码构建，并从 p33 启动 |
| BootShim | 自覆盖问题已修复；320 字节源码 shim 会先把 copy/clean 代码块搬到安全地址，再加载固件卷 |
| 启动菜单 | 自定义 GOP 菜单可在屏幕上显示；音量下滚动；音量上和电源键都能选中菜单项；当前源码映射已确认可用 |
| 启动镜像 | 64 MiB p33 镜像，包含修复后的 BootShim、3 MiB 固件卷、裁剪后的 DTB 和 ramdisk；用 `fastboot flash boot` 刷入 |
| 存储 | NativeEmmc 枚举 userdata 分区；OS 和 SystemOS 卷出现；owner 路径挂载 SystemOS cache carrier |
| UniXNU 和 XNU | XNU 到达根和 SystemOS cache；4 KiB 内核使用 `CONFIG_XNU_NATIVE_CS_HASH_CACHE` 处理原版 16 KiB CodeDirectory；initproc 和 launchd exec 完成 |
| 内核显示 | 内核通过 GOP framebuffer 绘制 `ROOT`、`VOL`、`CACHE`、`EXEC` 和 `LAUNCHD` 状态行 |
| Data | 未验证；仓库不包含 Data seed，因此可写 `/private/var` 和 launchd 服务图待验证 |
| 用户态 | 已确认 launchd exec；用户态服务、显示接管和渲染尚未验证 |
| 显示收尾 | 可见扫描输出从屏幕中部开始；菜单整体偏下，上半屏可能残留 OEM logo |

当前 PMIC 按键掩码同时覆盖电源键和 home/音量上键，所以音量上和电源键都会选中菜单项。两个键都能正常使用；这是当前映射，不是最终的独立按键布局。

PMIC 协议在运行时查找，没有加入 `Depex`；因此 PMIC wrapper 驱动不可用时，音量下和其他按键仍然工作。不支持开机时一直按住音量上：第一次 PMIC 读取会作为基线并返回 false。

## 构建

设备仓库让 `Mu_Basecore` 保持在上游提交。`board.toml` 列出了启动菜单补丁，共享构建驱动会在固件构建前把它应用到 `Mu_Basecore`，构建后自动反向应用。新 clone 不需要维护本地 Mu fork。

```sh
git clone --recurse-submodules https://github.com/Dracen-W/UniXNU-merlinx
git clone https://github.com/Dracen-W/uefi-merlinx
cd UniXNU-merlinx
./unixnu defconfig merlinx_defconfig -D .
./unixnu build kernel
./unixnu build payload
./unixnu build bootimg --uefi ../uefi-merlinx --dtb <merlinx.dtb>
```

payload 步骤使用设备仓库声明的板级 trust cache、平台身份和其他输入；这些输入不随仓库分发。当前镜像不包含 Data seed。

固件卷位于 `Build/merlinxPkg/DEBUG_CLANGPDB/FV/SILICIUM_UEFI.fd`。启动镜像把该固件卷、裁剪后的设备树和 ramdisk 与修复后的 BootShim 打包在一起，再补齐到 p33 所需的 64 MiB。

## Data seed

仓库不包含 Data seed。生成脚本参考 UniXNU-fajita 的 Data seed 生成器，使用 BSD-2-Clause-Patent 许可：

- [make-data-seed-linux.py](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-linux.py)
- [make-data-seed-macos.py](https://github.com/johnpanos/UniXNU-fajita/blob/main/scripts/make-data-seed-macos.py)
- [data-seed.json](https://github.com/johnpanos/UniXNU-fajita/blob/main/data-seed.json)

`data-seed.json` 描述一个区分大小写、未加密、无 role 的 APFS Data 卷，只包含空目录和偏好设置文件，不包含密钥、用户记录或 Apple 文件。

Linux 生成器使用 `mkapfs` 和 `apfsck` 格式化镜像并编辑新建的 APFS catalog，需要 UniXNU 树中的 packer：

```sh
python3 scripts/make-data-seed-linux.py \
  --packer drivers/DarwinBoot/Test/packed_ramdisk.py \
  --fetch-apfsprogs \
  --spec data-seed.json \
  --out .cache/data-seed
```

macOS 生成器使用 `diskutil` 和 `hdiutil`：

```sh
python3 scripts/make-data-seed-macos.py \
  --packer drivers/DarwinBoot/Test/packed_ramdisk.py \
  --spec data-seed.json \
  --out .cache/data-seed
```

生成器写出 `data-linux.dbr` 或 `data-macos.dbr` 以及包含 APFS 卷 UUID 的 JSON 报告。把该 UUID 用于 `CONFIG_XNU_OWNER_DATA_VOLUME_UUID`，并通过 `--data-ramdisk` 把 packed seed 传给 payload 步骤。raw 镜像、packed seed 和报告都是本地输入，不提交到仓库。

## 启动

```sh
fastboot flash boot <merlinx-boot.img>
fastboot reboot
```

`p33` 是 Android `boot` 分区的 GPT 分区号，本机为 64 MiB；`fastboot flash boot` 写入这个分区。

这个 bootloader 不支持 `fastboot boot`，会报 `usb_read failed with status e00002ed`。请使用 `fastboot flash boot`，并保留原厂 boot 镜像用于恢复。

屏幕是 1080x2340 的 FT8719。UEFI 可见扫描输出从屏幕中部开始，因此启动菜单比全屏扫描时更靠下，原厂 logo 可能残留在上半屏。

## 目标

1. 挂载 Data seed，并验证 `/private/var` 可写。
2. 确认 launchd 首先启动的服务，并找出第一个缺失的服务或资源。
3. 在早期服务图稳定后加入用户态显示路径。当前没有用户态渲染。
4. 清理屏幕偏移、背景和 logo 交接。
5. 在不依赖 UART 的前提下加入持久化日志或恢复路径。

这台设备没有 UART。早期启动证据来自屏幕显示，以及向 p33 刷入测试镜像。

## 维护

维护者拥有三个仓库及其发布决定。UniXNU 文档只记录移植和状态，不接管维护。
