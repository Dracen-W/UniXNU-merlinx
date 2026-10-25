# 小米 MIX Flip（ruyi）

搭载 SM8650 的社区设备移植，由 ArnoDorian123 维护。

- [ArnoDorian123/UniXNU-ruyi](https://github.com/ArnoDorian123/UniXNU-ruyi)
- [ArnoDorian123/uefi-ruyi](https://github.com/ArnoDorian123/uefi-ruyi)
- [ArnoDorian123/UniXNU-ruyi-min](https://github.com/ArnoDorian123/UniXNU-ruyi-min)

## 当前状态

主设备仓库包含 ruyi 板级配置和内核 defconfig，选择 iPad7,1 / iPadOS 17.7.11 系统输入。板级文件中的 TODO 需要补齐并实测，目标设备启动结果待发布。

## 来源

- [板级配置](https://github.com/ArnoDorian123/UniXNU-ruyi/blob/9fdf6d9dac150184b8a0fe1276c86d87d1ff1c5a/board.toml)

按[移植设备](/zh/porting/steps)完成板级输入与驱动集成，再按[启动验证顺序](/zh/porting/bring-up)记录结果。
