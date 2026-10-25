# 一加 13T（oneplus13t）

搭载 SM8750 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-Pagani_SM8750](https://github.com/IssacL04/UniXNU-Pagani_SM8750) | [IssacL04](https://github.com/IssacL04) |

## 当前状态

阶段 7 第 39 次运行记录确认实机已挂载根卷并进入系统界面，触控待完成。固件、XNU、根卷和界面结果适用于记录中的设备配置。

## 来源

- [README: IssacL04/UniXNU-Pagani_SM8750](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/9a6b057527429159a531f37c3af5b7235806caf1/README.md)

## 双系统存储

以下布局来自 256 GB 型号的实测记录。

| 项目 | 采集值 |
| --- | --- |
| Recovery 内核 | `6.6.118-android15-8-o-g4d611c32bedc-enc3l-4k` |
| 磁盘 / 逻辑扇区 | `/dev/block/sda` / 4096 字节 |
| 剩余 userdata | 分区 15，LBA 4020972 到 44928767 |
| UniXNU 分区 | 分区 16，LBA 44928768 到 61705983 |
| F2FS 缩容请求 / 预期结果 | 40907264 / 40907535 个 4096 字节块 |

按[双系统分区指南](/zh/porting/dual-system-partition)从连接的设备计算布局，不要把这些 LBA 复制到另一台手机。检查当前分区表：

```sh
python3 scripts/dual_sys/partition.py inspect
```

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
