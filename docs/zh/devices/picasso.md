# Redmi K30 5G（picasso）

搭载 SM7250 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-picasso](https://github.com/YinCQL/UniXNU-picasso) | [YinCQL](https://github.com/YinCQL) |

## 当前状态

记录确认 UEFI、菜单、GOP、物理按键和 UFS 大容量存储访问可用，XNU 已通过 UEFI 启动。根卷集成和 GPU 支持待完成。

## 来源

- [README: YinCQL/UniXNU-picasso](https://github.com/YinCQL/UniXNU-picasso/blob/879f753a01790950350bc105cb854074ffb2005b/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
