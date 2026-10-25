# sheng 上的 macOS

搭载 SM8550 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [macOS-ForSheng](https://github.com/Jelly10086/macOS-ForSheng) | [Jelly10086](https://github.com/Jelly10086) |

## 当前状态

macOS 26.5 的 QEMU 路径已运行原始 arm64e `launchd`、`sh` 和 `bash`，使用临时 Data 卷，并到达 Aqua 初始化。WindowServer 的 HID 和显示集成待完成。

sheng 实机记录的结果为 UEFI EL1 探针，QEMU 用户态结果使用独立的配置和验证记录。

## 来源

- [README: Jelly10086/macOS-ForSheng](https://github.com/Jelly10086/macOS-ForSheng/blob/daec3714f8b4e12f5cd129efa79be894511353e5/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
