# 小米平板 4 LTE（clover）

搭载 SDM660 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-clover](https://github.com/qine233/UniXNU-clover) | [qine233](https://github.com/qine233) |

## 当前状态

内核已挂载 mockfs 内存根卷，并运行静态 EL0 测试 init。记录覆盖定时器活动、文本输出和 fork/exec 系统调用探针。当前诊断集中在子进程写入、退出和等待路径，完整 iPadOS 用户态仍待集成。

## 来源

- [README: qine233/UniXNU-clover](https://github.com/qine233/UniXNU-clover/blob/f80c740a2c418639eb8f76b31eec2af0e13af862/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
