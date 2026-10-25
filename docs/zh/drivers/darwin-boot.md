# DarwinBoot

先按[适配流程](/zh/porting/platform)准备板级输入、修改源码、接入构建并逐步验证。

UEFI 启动加载器。安装 loader 协议，把内核、ADT、trust cache 和 ramdisk 交给 XNU。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [DarwinBoot](https://github.com/UniXNU/DarwinBoot) | [UniXNU](https://github.com/UniXNU) | UEFI 启动加载器。安装 loader 协议，把内核、ADT、trust cache 和 ramdisk 交给 XNU。 |

## 状态

不是驱动。每个 XNU 启动路径都使用它。

## 构建

在目标固件 DSC/FDF 中加入 DarwinBoot INF，从 UniXNU 执行 `./unixnu build firmware`。按[固件集成](/zh/porting/uefi-firmware#required-integration)操作，选择 XNU 启动项前确认加载器协议。
