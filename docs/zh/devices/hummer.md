# 一加 Ace 5 Pro（hummer）

搭载 SM8750 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-hummer](https://github.com/2426120557/UniXNU-hummer) | [2426120557](https://github.com/2426120557) |

## 当前状态

UEFI 显示和按键已在实机工作。XNU 通过 PSCI 启动六个 CPU，160 MiB 负载可构建。当前实机运行等待根设备。根卷 UUID 调整待实机验证，Data、GPU 和触控集成待完成。

## 来源

- [README: 2426120557/UniXNU-hummer](https://github.com/2426120557/UniXNU-hummer/blob/a24338025de2c8e1629f090ea4c3a51f980388a0/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
