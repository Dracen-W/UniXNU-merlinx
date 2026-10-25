# Nothing Phone (2)（pong）

pong 是搭载 SM8475 的 Nothing Phone (2)。下面的移植是独立的社区项目，UniXNU 组织不维护它。目标是 iOS 16.6.1（20G81），使用 iPad7,1 / J120 身份。

| 仓库 | 维护者 | 状态 |
| --- | --- | --- |
| [UniXNU-pong](https://github.com/Chengdong-Yu/UniXNU-pong) | [Chengdong-Yu](https://github.com/Chengdong-Yu) | 已进入 iOS 16.6.1 主屏幕；显示、触控、UFS/APFS 启动、UFS Data 挂载及 Adreno 730 硬件加速与 Metal 已接通 |
| [uefi-pong](https://github.com/Chengdong-Yu/uefi-pong) | [Chengdong-Yu](https://github.com/Chengdong-Yu) | 带 `pongPkg` 的 Mu-Silicium 固件 |

## 状态

| 区域 | 状态 |
| --- | --- |
| iOS 16.6.1 启动 | 已进入主屏幕 |
| 显示 | 1080x2412 面板显示系统界面 |
| 触控和按键 | Pong 触控路径、电源键和音量键已接通 |
| UFS 和 APFS | 原生 UFS 和 APFS 启动路径已接通，Data 在 UFS 上正常挂载，部分挂载能力仍在完善 |
| 可写 Data | Data 在 UFS 上正常挂载 |
| Adreno 730 和 Metal | Adreno 730 硬件加速与 Metal 渲染已接通 |
| Wi-Fi、蓝牙、相机、音频、传感器 | 尚未完成设备适配 |

设备仓库按 HTTP Range 从恢复镜像中只获取需要的 iOS 16.6.1 组件。Apple 恢复文件、生成的 userdata 镜像和 GPU 固件不进入仓库。

## 维护

维护者拥有仓库和发布决定。UniXNU 文档记录移植及其状态，不接管维护。
