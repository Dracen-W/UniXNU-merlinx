# 一加 5（cheeseburger）

搭载 MSM8998 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger) | [DinnerABC](https://github.com/DinnerABC) |
| [UniXNU-cheeseburger](https://github.com/lss717/UniXNU-cheeseburger) | [lss717](https://github.com/lss717) |

## 当前状态

DinnerABC 移植已在 EL0 执行 PID 1，挂载根卷和 SystemOS，并提供内存中的可写 APFS Data。P18 启动记录停止于 `watchdogd[106]` 的 `SIGTRAP`，下一步需要获取用户态 PC 和调用栈。

lss717 的独立移植已记录 UEFI、菜单和负载的实机检查，XNU 集成仍在进行。

## 来源

- [P18 启动结果: DinnerABC/UniXNU-cheeseburger](https://github.com/DinnerABC/UniXNU-cheeseburger/blob/c5463363de7ca0ad541b8132f29c7f0a3cef718c/docs/P18_BOOT_RESULT.md)
- [README: lss717/UniXNU-cheeseburger](https://github.com/lss717/UniXNU-cheeseburger/blob/de1af958fc2530c2626217a36ab276e1f77f57a1/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
