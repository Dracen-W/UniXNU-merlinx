## Repositories

| Repository | Maintainer | State |
| --- | --- | --- |
| [UniXNU-merlinx](https://github.com/Dracen-W/UniXNU-merlinx) | [Dracen-W](https://github.com/Dracen-W) | XNU mounts SystemOS, launchd and SpringBoard run, the Data seed is read-write, and the Mali-G52 probe reaches M2 voltage; display and GPU execution are pending |
| [uefi-merlinx](https://github.com/Dracen-W/uefi-merlinx) | [Dracen-W](https://github.com/Dracen-W) | Mu-Silicium port, fixed BootShim and custom GOP boot menu |
| [NativeEmmc](https://github.com/Dracen-W/NativeEmmc) | [Dracen-W](https://github.com/Dracen-W) | Native eMMC block driver used by the userdata and SystemOS path |
| [Mali-G52](https://github.com/Dracen-W/Mali-G52) | [Dracen-W](https://github.com/Dracen-W) | Mali-G52 kernel service; read-only probe and GIC/PMIC checks pass, M2 stops at the VSRAM pwrap read |

The Mali-G52 row uses the official [Adreno740](https://github.com/UniXNU/Adreno740) driver as the integration and ABI reference. Its GPU, power, clock and firmware facts come from MediaTek and panfrost sources.
