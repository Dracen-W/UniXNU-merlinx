# Ath12kWifi

先按[适配流程](/zh/porting/network)准备板级输入、修改源码、接入构建并逐步验证。

Ath12kWifi 实现基于 ath12k 的 Wi-Fi 栈，包含 PCI、MHI、QRTR 和板级接口。
源码包含 QCC2072、QCN9274 和 WCN7850 硬件定义。

| 字段 | 值 |
| --- | --- |
| 仓库 | [UniXNU/Ath12kWifi](https://github.com/UniXNU/Ath12kWifi) |
| 功能 | ath12k Wi-Fi 传输与驱动集成 |
| 板级输入 | PCI 访问、中断、固件和板级回调 |
| 硬件验证 | 在设备页记录具体设备结果 |

选择前阅读组件 README 和 `nw_board` 接口。组件目录记录源码版本、OS 要求、构建
命令和输出。运行组件的主机检查和板级网络验证流程。
