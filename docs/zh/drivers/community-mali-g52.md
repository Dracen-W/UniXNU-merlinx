# Mali-G52

用于 Xiaomi Redmi Note 9（merlinx，MediaTek MT6768）ARM Mali-G52 的社区 XNU GPU 驱动。该驱动由设备项目维护，不由 UniXNU 组织维护。

| 仓库 | 维护者 | 范围 |
| --- | --- | --- |
| [Dracen-W/Mali-G52](https://github.com/Dracen-W/Mali-G52) | [Dracen-W](https://github.com/Dracen-W) | Bifrost v7 内核服务、只读探针、M2 电源和时钟、MMU、job slot 以及 MaliMetal 线协议 |

## 状态

| 层 | 状态 |
| --- | --- |
| IOKit 匹配 | `NativeMaliGpuService` personality 在生成的 ADT 上匹配 `mediatek,mali` |
| GPU 窗口 | `0x13040000`，大小 `0x4000`；GICD_PIDR2 `0x3b` 验证了固定物理映射 |
| 平台探针 | 读取 SPM、INFRACFG、topckgen、mfgcfg、pwrap 和 MT6358，不写配置寄存器 |
| PMIC 路径 | 通过 pwrap WACS2 读取 MT6358 PSEQ_ID，结果为 `0x0800` |
| M2 | 电压、时钟、四个电源域和时钟门代码已编入；执行停在第一次 VSRAM pwrap 读取 |
| M3 及之后 | GPU reset、MMU enable、job、fence 和 MaliMetal 尚未在真机执行 |
| 用户态 | 没有发布 GPU device；显示仍卡在 `IOMFB_FDR_loader` |

bring-up 使用的内核状态行包括 `MALI MATCH`、`MALI START`、`MALI RC`、`MALI M2P`、`MALI PMIC`、`MALI GIC`、`M2V` 到 `M2G`，以及电压子步骤 `V1` 到 `V4` 和 `VOK`。

## 源码

| 源码 | 用途 |
| --- | --- |
| `NativeMaliGpuService.cpp` | IOKit 服务、ADT 匹配和诊断标记 |
| `NativeMaliBringup.cpp` | M1 到 M4 顺序和分步 M2 |
| `NativeMaliPower.cpp` | SPM、topckgen、mfgcfg 和 M2 电源序列 |
| `NativeMaliMt6358.cpp` | VGPU 和 VSRAM 电压选择 |
| `NativeMaliPwrap.cpp` | PMIC WACS2 传输 |
| `NativeMaliGpuCore.cpp` | GPU 身份、特性读取、复位和核心电源 |
| `NativeMaliService.cpp` | 设备生命周期、地址空间和复位编排 |

当前源码 revision 由 `components.toml` 的 `MaliG52` 固定。

## 集成

`board.toml` 选择 `MaliG52`。`scripts/build-mali-g52.py --m2-writes` 构建归档，`patches/xnu/native-mali-personality.patch` 加入 IOKit personality，`scripts/make-merlinx-adt.py` 发布 `mediatek,mali` 节点，内核用 `-force_load` 链接。

构建使用板级常量映射 GPU、SPM、INFRACFG、topckgen、mfgcfg 和 pwrap 窗口。生成的 ADT 不描述这些地址。

## 验证

构建 kernel 和 payload，然后把 boot image 刷入 p33：

```sh
./unixnu build kernel
./unixnu build payload
fastboot flash boot out/merlinx-uefi-emmc-mali-vsteps.img
```

读取 GOP 状态行。当前预期停点是 `V1 read VSRAM`。要得到 GPU 结果，M2 必须先完成，并且 `GPU_ID` 读到 `0x70021000`。

## 维护

维护者拥有驱动仓库及其发布决定。设备集成和真机证据保留在 [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx)。
