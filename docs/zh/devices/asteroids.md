# Nothing Phone (3a)（asteroids）

搭载 SM7635 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-asteroids](https://github.com/rootstockos/UniXNU-asteroids) | [rootstockos](https://github.com/rootstockos) |

| [uefi-asteroids](https://github.com/rootstockos/uefi-asteroids) | [rootstockos](https://github.com/rootstockos) |

## 当前状态

XNU 已在八个 CPU 上运行并挂载 UFS userdata 根卷。`launchd`、SystemOS 和 SpringBoard 已启动，文本控制台可用。显示客户端已打开，但尚未呈现画面，SpringBoard 会重启。

Adreno 810 和 GMU 集成正在开发，显示呈现和 GPU 执行待实机验证。

## 来源

- [板级配置: rootstockos/UniXNU-asteroids](https://github.com/rootstockos/UniXNU-asteroids/blob/3a54c7f1a5d19a2e9a79eaaeb0ed0a5c1abd1155/board.toml)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
