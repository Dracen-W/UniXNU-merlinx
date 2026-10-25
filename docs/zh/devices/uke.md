# 小米平板 7 / POCO Pad X1（uke）

搭载 SM7675 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-uke](https://github.com/Allenwdk/UniXNU-uke) | [Allenwdk](https://github.com/Allenwdk) |
| [uefi-uke](https://github.com/Allenwdk/uefi-uke) | [Allenwdk](https://github.com/Allenwdk) |

## 当前状态

设备已显示 Apple 标志、挂载 UFS 根卷并启动 `launchd`。进度记录包含 `Early boot complete`、`driverkitd` 启动和显示 surface 接入。随后发生重启，原因未查明。

GPU、物理触控和其他外设仍需分别完成实机验证。

## 来源

- [进度记录: Allenwdk/UniXNU-uke](https://github.com/Allenwdk/UniXNU-uke/blob/cf552816de501906082405ebb0e355d8c70f7b60/docs/PROGRESS.md)

## 构建

Linux 上构建固件和启动镜像：

```sh
cd /path/to/UniXNU
./unixnu defconfig uke_defconfig -D ../UniXNU-uke
./unixnu fetch -D ../UniXNU-uke
./unixnu build firmware --skip-preflight -D ../UniXNU-uke --uefi ../uefi-uke -j 8
./unixnu bootimg -D ../UniXNU-uke --uefi ../uefi-uke --dtb ../uke-port/data/vendor_boot.dtb --ramdisk none
```

内核在仓库的 GitHub Actions 中构建：

```sh
gh workflow run build-uke-kernel
```

userdata 镜像需要 macOS `hdiutil`。当前阶段、产物和剩余工作见设备仓库的 `docs/PROGRESS.md`。

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
