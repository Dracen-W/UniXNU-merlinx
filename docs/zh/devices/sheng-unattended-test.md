# sheng 日志抓取示例

按[抓取日志与自动启动测试](/zh/porting/log-capture)执行移植步骤，其中包含固件阶段的二维码导出。
本页提供后续抓取阶段所需的 sheng 存储、复位和读取器输入。

## 1. 准备输入

准备已选定的 [sheng 仓库](/zh/devices/sheng)、已构建的内核与 package、
已填写的[本地构建输入](/zh/reference/configuration#local-build-inputs)、fastboot 和 Python 3。
从 UniXNU 主仓库运行以下命令：

```sh
DEVICE_REPO=$(cd ../UniXNU-sheng && pwd)
TARGET="$PWD/out/sheng/ipados-17.7.11-21H461/sheng"
mkdir -p "$TARGET/boot"
fastboot devices
```

记录设备序列号，用于后续命令。读取器需要以下输入：

- [sheng Linux release](https://github.com/ianchb/sm8550-mainline/releases) 中的
  `boot_sheng_singleboot.img`，包含 Android header v0、追加的 DTB 和空 ramdisk。
- 静态链接的 AArch64 Linux BusyBox，包含 shell、mount、USB 读取器所需工具，
  以及 `dd`、`gzip`、`base64` 和 `md5sum` applet。
- 同一内核 release 的 `linux-xiaomi-sheng.deb` 中的 `nvmem-reboot-mode.ko`。
- 用 AArch64 Linux 工具链从 `package/linux-log/rebootbl.cpp` 编译的 `rebootbl`。
  它使用 Linux 系统调用和板型 DT 的 reboot-mode cell。

例如，安装 LLVM 的 `clang++` 和 `ld.lld` 后：

```sh
clang++ --target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static \
  -fno-stack-protector -fno-exceptions -fno-rtti -Wl,-e,start \
  "$DEVICE_REPO/package/linux-log/rebootbl.cpp" -o "$TARGET/boot/rebootbl"
python3 "$DEVICE_REPO/package/linux-log/build-linux-log-boot.py" \
  --boot /path/to/boot_sheng_singleboot.img --busybox /path/to/busybox \
  --rebootbl "$TARGET/boot/rebootbl" --module /path/to/nvmem-reboot-mode.ko \
  --out "$TARGET/boot/sheng-linux-log.img"
```

**结果：** 生成使用 sheng Linux 内核和 DTB 的读取器镜像。它将 `userdata` 最后 32 MiB
复制到 RAM，提供 USB CDC ACM shell，存储访问保持只读。构建命令打印 `BUILT` 和输出路径。

## 2. 构建日志镜像

`BootLog.c` 通过 ShengUfs 将诊断快照和内核消息缓冲写入 `userdata` 末尾预留的 32 MiB。
ShengUfs 读取根布局后，写入器才能打开该区域。先确认预留区域与已安装的[存储布局](/zh/concepts/userdata)一致。

| 参数 | 行为 |
| --- | --- |
| `sheng-log=1` | 从板级监控启动持久化写入器 |
| `sheng-autoreset=120` | 日志时间达到 120 秒且同步成功后尝试返回 fastboot；硬件探测成功时，板级监控还会设置重启原因并启动 watchdog 恢复 |

先使用已保存的本地输入构建日志 payload：

```sh
./unixnu payload --kernel-arg=sheng-log=1
./unixnu bootimg
```

**结果：** `$TARGET` 下的 `payload/` 和 `boot/boot.img` 包含更新后的参数。
完成手动抓取后再加入定时复位。

## 3. 手动抓取一次 {#manual-cycle}

将 `SERIAL` 设置为 `fastboot devices` 显示的标识：

```sh
SERIAL=<serial>
fastboot -s "$SERIAL" boot "$TARGET/boot/boot.img"
```

等待所测检查点，再使用设备的 bootloader 按键组合返回 fastboot。启动读取器：

```sh
fastboot -s "$SERIAL" boot "$TARGET/boot/sheng-linux-log.img"
```

等待显示 `ready: shell on USB serial`。macOS 上运行：

```sh
python3 "$DEVICE_REPO/scripts/read-sheng-log.py" --wait 240 --out "$TARGET/logs/captures"
```

Linux 上找到读取器的实际 CDC 端口，确认当前账号有访问权限，再显式指定：

```sh
ls /dev/ttyACM*
python3 "$DEVICE_REPO/scripts/read-sheng-log.py" \
  --port /dev/ttyACM0 --out "$TARGET/logs/captures"
```

**结果：** `logs/captures/<tag>-<boot id>/` 包含 `log.bin`、`header.txt`、`msgbuf.txt`、
`slots.txt` 和 `latest.txt`。检查启动 ID、快照和写入错误。
传输结束后读取器发送 `/bin/rebootbl`，用 `fastboot devices` 确认同一序列号返回。
出现 `no log header` 时，先检查写入器是否启动、预留区域是否正确，再重复测试。

## 4. 启用自动循环 {#automated-cycle}

加入定时复位后，重新构建两项输出：

```sh
./unixnu payload --kernel-arg=sheng-log=1 --kernel-arg=sheng-autoreset=120
./unixnu bootimg
```

正常复位在日志同步成功后执行。板级监控的 watchdog 路径可恢复停滞的内核：
它请求 60 秒 watchdog 超时，持续喂狗到配置的复位时限后 60 秒。
记录本次属于正常返回还是超时恢复。

macOS 主机连接一个读取器串口时运行：

```sh
SERIAL="$SERIAL" READER="$TARGET/boot/sheng-linux-log.img" \
  bash "$DEVICE_REPO/scripts/run-sheng-unattended.sh" "$TARGET/boot/boot.img" 420
```

脚本会切换到设备仓库，因此这里使用镜像绝对路径。
它启动测试镜像，等待返回 fastboot，启动 Linux 读取器并取回日志。
自动串口发现使用 `/dev/cu.usbmodem*`；Linux 上可在定时返回后按步骤 3 显式指定端口，
或为执行脚本加入端口传参。自动脚本将日志保存到
`UniXNU-sheng/build/sheng-logs/<tag>-<boot id>/`。

超过 420 秒仍未返回时，脚本会报告超时并等待手动进入 bootloader。
将该结果写入[适配记录](/zh/porting/records)。下一次测试前确认最终已回到 fastboot。

## 读取器验证

现有 `package/linux-log/check-linux-log-qemu.py` 使用合成 GPT 日志区域，
检查分区查找、复制到 RAM、传输和解码：

```sh
python3 "$DEVICE_REPO/package/linux-log/check-linux-log-qemu.py" \
  --image "$TARGET/boot/sheng-linux-log.img" --kernel /path/to/qemu-virt-linux-Image \
  --work "$TARGET/logs/reader-qemu"
```

使用支持 virtio block 的 AArch64 QEMU `virt` Linux 内核，并安装相应主机 QEMU 工具。
将这项主机检查与板级存储写入、USB 传输和复位观察分别记录。
继续按[通用测试步骤](/zh/porting/log-capture#automated-cycle)操作。
