# 小米平板 5 Pro 5G（enuma）

搭载 SM8250-AC 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-enuma](https://github.com/wh1stl3/UniXNU-enuma) | [wh1stl3](https://github.com/wh1stl3) |
| [UniXNU-enuma](https://github.com/Tlrenhb/UniXNU-enuma) | [Tlrenhb](https://github.com/Tlrenhb) |
| [uefi-enuma](https://github.com/Tlrenhb/uefi-enuma) | [Tlrenhb](https://github.com/Tlrenhb) |

## 当前状态

wh1stl3 移植已组装包含 29 个组件的实验性预链接内核、结构化 ADT 和 96 MiB FAT 负载，宿主机加载器检查通过。存储传输、根卷集成和设备启动验证待完成。

Tlrenhb 仓库提供另一份板级配置，仍需补齐实测输入。

## 来源

- [README: wh1stl3/UniXNU-enuma](https://github.com/wh1stl3/UniXNU-enuma/blob/adfb63f9bc10532d0fe6603ea806d0fdda6fae9e/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
