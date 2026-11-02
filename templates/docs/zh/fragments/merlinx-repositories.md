## 仓库

| 仓库 | 维护者 | 状态 |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | XNU 挂载 SystemOS，launchd 和 SpringBoard 已运行，Data seed 已读写挂载，Mali-G52 探针到达 M2 电压；显示和 GPU 执行待处理 |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium 移植、修复后的 BootShim 和自定义 GOP 启动菜单 |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | userdata 和 SystemOS 路径使用的原生 eMMC 块驱动 |
| [Mali-G52](https://github.com/Dracen-W/Mali-G52) | [Dracen-W](https://github.com/Dracen-W) | Mali-G52 内核服务；只读探针和 GIC/PMIC 检查通过，M2 停在 VSRAM pwrap 读取 |

Mali-G52 一行以官方 [Adreno740](https://github.com/UniXNU/Adreno740) 驱动作为集成和 ABI 参考；GPU、电源、时钟和固件事实仍来自联发科与 panfrost 源码。
