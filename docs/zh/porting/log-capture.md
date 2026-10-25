# 抓取日志与自动启动测试

按移植进度建立日志抓取流程：先获取固件日志，再记录内核交接，随后实现持久化日志和自动重启测试。
使用[启动检查点](/zh/porting/bring-up)确定当前验证的阶段。

| 阶段 | 所需支持 | 保存结果 |
| --- | --- | --- |
| UEFI 菜单 | GOP 显示、菜单输入、Advanced Logger 和二维码插件 | 扫描屏幕导出的固件 `.log` 文件 |
| XNU 入口 | UART 或屏幕诊断 | 串口日志或最后检查点/panic 的照片 |
| 存储 | 预留日志区域、内核写入器和 Linux 读取器 | 内核消息缓冲和诊断快照 |
| 重复测试 | 可用的日志读取与返回 bootloader 功能 | 每次启动独立的证据目录 |

## 1. 集成固件日志导出器 {#firmware-setup}

按 [UEFI 集成](/zh/porting/uefi-firmware)准备固件仓库。
通用导出器位于 UniXNU 的
`package/UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf`。
它在 UEFI boot services 可用时读取 Advanced Logger 内存缓冲。

1. 在固件 DSC 的 `[Components]` 中加入：

   ```ini
   UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
   ```

   在 FDF 的 DXE 固件卷中加入：

   ```ini
   INF UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
   ```

2. 启用平台的 Advanced Logger 日志收集模块，将所需固件调试输出接入它。
   导出器在 `gAdvancedLoggerProtocolGuid` 安装后加载。
   INF 还依赖 `AdvLoggerPkg`、`MsGraphicsPkg`、`AdvancedLoggerAccessLib` 和 `QrEncoderLib`。
   从固件仓库查找当前 Mu 版本提供的库实例：

   ```sh
   find . -name '*AdvancedLogger*.inf' -o -name '*QrEncoder*.inf'
   ```

   在 DSC `[LibraryClasses]` 中映射日志访问库和二维码库，将对应包根目录加入
   `board.toml` 的 `[firmware] packages`。查不到文件时先初始化这些包所在的子模块。
   日志缓冲区和调试库映射采用该平台的 Advanced Logger 配置。
3. 在 DSC `[PcdsFixedAtBuild]` 中把 `gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel`
   设置为板型的 ASCII 名称。导出文件名使用该值。
4. 在设备 `board.toml` 已有的 `[payload]` 表中加入 `tools = true`，生成的启动菜单会包含
   **Tools**。手写的 `unixnu.toml` 使用 [`action = "tools"` 菜单项](/zh/reference/boot-configuration)。
5. 从 UniXNU 主仓库运行以下命令。先选定设备，并填好[本地输入](/zh/reference/configuration#local-build-inputs)：

   ```sh
   ./unixnu firmware
   ./unixnu payload
   ./unixnu bootimg
   ```

**结果：** 镜像包含导出器；平台日志协议可用时，菜单出现 **Tools → Advanced Logger**。

## 2. 在内核交接前扫描固件日志 {#qr-logs}

准备另一台带摄像头的手机或设备，打开 [OneSend（扫传）](https://onesend.01mvp.com)。
导出器通过连续的 OneSend 二维码帧传输日志。

1. 从主仓库启动镜像：

   ```sh
   fastboot devices
   fastboot -s <serial> boot out/<device>/<os_profile>/<config>/boot/boot.img
   ```

2. 停留在 UniXNU 菜单，选择 **Tools**，再选择 **Advanced Logger**。
   插件打开时读取一份当前固件日志快照。加载器报错并返回菜单时，在复位前打开插件抓取该错误。
3. 打开扫传的接收扫描功能，允许使用摄像头，将完整二维码置于画面内。
   保持扫描轮播帧，直到接收端完成传输。
4. **下键 / j** 降低轮播速度，**上键 / k** 提高速度。默认每秒一帧，五档间隔从 5 秒到
   250 毫秒。音量键映射为上下键的板型可使用音量键。按 **Enter** 或 **Escape** 返回菜单；
   电源键映射为 Enter 时也可用于返回。
5. 保存收到的 `<model>-<date>-<time>.log`，附上镜像版本和最后完成的检查点。
   文件名使用固件时钟。重新构建前，先查看文件中失败操作的输出。

**结果：** 在菜单可操作时获得可读的固件日志。`ExitBootServices` 之后的 XNU 执行使用步骤 3 的抓取方式。

| 现象 | 操作 |
| --- | --- |
| 没有 Tools | 检查 `[payload] tools`，重建 payload 和启动镜像 |
| 没有 Advanced Logger | 检查 DSC/FDF 中的 INF、依赖和平台日志协议 |
| 显示 `Advanced Logger: no log` | 检查日志收集模块、调试输出接入和缓冲区配置 |
| 扫描进度停止 | 降低轮播速度，继续扫描循环播放的帧 |
| 固件在菜单前停止 | 抓取平台 UART，或拍摄最后可见输出 |

## 3. 记录内核交接 {#kernel-logs}

1. 保存步骤 2 的固件日志，再选择内核启动项。
2. 按[调试步骤](/zh/guide/debugging#procedure)抓取板型 UART 或屏幕输出。
   停在绿色交接条时，按[入口探针步骤](/zh/guide/debugging#green-bar)操作。
3. 拍摄完整 panic 屏幕或检查点，保留寄存器和最后几行文字。记录构建后 payload 中的内核参数。
4. 修改参数后，重新运行 `./unixnu payload` 和 `./unixnu bootimg`。
   重复同一启动操作，对比最后确认的检查点。

**结果：** 存储适配期间也能保留内核阶段的证据。完成[存储验证](/zh/porting/ufs)后继续实现持久化抓取。

## 4. 为板型适配持久化抓取 {#persistent-logs}

参考 [sheng 实现](/zh/devices/sheng-unattended-test)的源码结构。
启用写入前完成以下板级接口：

1. 在目标存储布局中预留日志区域，记录分区、偏移、大小和块单位，并与根卷及 Data 区域一起检查。
   镜像构建器、分区提供者、写入器和读取器必须使用相同边界。
2. 将内核写入器接入存储驱动限定范围的读写与同步操作，记录内核消息缓冲、诊断快照、启动 ID 和写入状态。
   存储区域可打开后再启动写入器。
3. 用目标板型的内核、DTB、存储驱动、USB 设备控制器和 CDC ACM 支持构建 Linux 读取器。
   将预留区域复制到 RAM，再提供串口 shell 供主机取回。源分区保持只读。
4. 根据写入器的文件头和快照格式适配主机解码器。主机串口命名与自动发现规则不同时，显式指定端口。
5. 在 XNU 和 Linux 读取器中实现返回板型 bootloader 的操作，采用目标硬件的重启原因机制和复位接口。
   需要内核停滞后自动恢复时，单独验证 watchdog。

阅读 `UniXNU-sheng` 中的 `package/BootLog.c`、`BootReset.c`、`BootResetCore.h`、
`package/linux-log/` 和 `scripts/read-sheng-log.py`。
其中 PMIC 地址、watchdog 调用、userdata 预留区域和 Linux 镜像输入需要按新板型的硬件资料填写。

**结果：** 日志区域有明确记录，写入器、读取器和解码器格式一致，返回 bootloader 的路径已验证。
将这些信息写入设备仓库。

## 5. 手动完成一次抓取 {#manual-cycle}

使用选定设备文档中的日志参数和读取器镜像。
[sheng 示例](/zh/devices/sheng-unattended-test#manual-cycle)提供该实现的完整命令。

1. 启用日志参数，构建测试 payload，再构建启动镜像。
2. 运行 `fastboot -s <serial> boot <test.img>`，等待日志写入器运行。
3. 使用板型已验证的复位方式返回 fastboot，运行 `fastboot -s <serial> boot <reader.img>`。
4. 运行板型的主机读取器，将原始区域和解码文本存入按启动 ID 标识的目录。
   确认文件头属于本次启动，存在诊断快照，写入状态成功。
5. 从读取器返回 fastboot，用 `fastboot devices` 确认同一序列号重新出现。

**结果：** 一次测试完成，日志已保存，设备可以启动下一份镜像。
文件头缺失或属于旧启动时，检查写入器启动情况和区域范围。
读取器串口未出现时，检查 Linux USB gadget 和主机访问权限。

## 6. 自动执行已验证的循环 {#automated-cycle}

1. 启用板型定时返回 bootloader 的功能。超时应长于所测检查点所需时间，并留出日志同步时间。
2. 为主机执行脚本配置准确的 fastboot 序列号、测试镜像绝对路径、读取器镜像绝对路径，
   以及覆盖启动和复位所需时间的超时。
3. 脚本依次启动测试镜像、观察 USB 断开和返回、启动读取器、抓取日志，并确认最后回到 fastboot。
   板级接口匹配时，可使用 [sheng 脚本示例](/zh/devices/sheng-unattended-test#automated-cycle)。
4. 超时后记录失败阶段，按板型文档恢复到 bootloader。保留本次镜像和已获得的证据。
5. 每次修改一项内容后重复测试。在[适配记录](/zh/porting/records)中填写版本、参数、检查点、复位结果、
   输出目录和未解决错误。

**结果：** 每次测试都有可识别的证据目录和返回 bootloader 的记录。继续完成其余[启动验证阶段](/zh/porting/bring-up)。
