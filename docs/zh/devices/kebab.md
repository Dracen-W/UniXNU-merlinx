# 一加 8T（kebab）

搭载 SM8250 的社区设备移植。当前状态依据下方固定版本的维护者记录。

| 仓库 | 维护者 |
| --- | --- |
| [UniXNU-kebab](https://github.com/GFander/UniXNU-kebab) | [GFander](https://github.com/GFander) |
| [UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab) | [stevedang67889](https://github.com/stevedang67889) |
| [KebabRamDisk](https://github.com/stevedang67889/KebabRamDisk) | [stevedang67889](https://github.com/stevedang67889) |

## 当前状态

GFander 移植在实机到达 MMU 启用、`arm_init`、`PE_init_platform` 和早期帧缓冲控制台。KVA 映射和 `_enable_timebase_event_stream` 仍需排查，定时器报告无效的位索引。

stevedang67889 的独立移植已验证固件、菜单、物理按键和日志，并记录内核及负载构建结果；固件 UFS 集成待完成。

## 来源

- [README: GFander/UniXNU-kebab](https://github.com/GFander/UniXNU-kebab/blob/5d609fa28397f352103510eb37efe50b8570737f/README.md)
- [README: stevedang67889/UniXNU-kebab](https://github.com/stevedang67889/UniXNU-kebab/blob/06dc6ca9704f07b2a7b5922173dba302f3a09f33/README.md)

## 使用此移植

1. 阅读上方仓库的构建说明和固定版本记录，准备对应的固件、系统镜像和板级输入。
2. 按该仓库的命令构建，通用配置见[构建与启动](/zh/building)。
3. 按[启动验证顺序](/zh/porting/bring-up)逐项验证，通过[日志抓取](/zh/porting/log-capture)保存结果。
4. 更新[移植记录](/zh/porting/records)，记录实际使用的源码版本及最后确认的阶段。
