# ArmSmmu4K

Start with the [adaptation procedure](/porting/platform) for board inputs, source changes, build integration and verification order.

ARM SMMUv2 native 4 KiB table kit. Builds and programs stage-1/stage-2 context tables.

| Repository | Maintainer | Scope |
| --- | --- | --- |
| [ArmSmmu4K](https://github.com/UniXNU/ArmSmmu4K) | [UniXNU](https://github.com/UniXNU) | ARM SMMUv2 native 4 KiB table kit. Builds and programs stage-1/stage-2 context tables. |

## State

Used by Adreno740 and the storage drivers.

## Build

The consuming driver's recipe compiles the kit sources. sheng includes `ArmSmmu4K*.c` in its GPU archive. Supply MMIO, context/stream ownership and allocation callbacks as described in [platform adaptation](/porting/platform).
