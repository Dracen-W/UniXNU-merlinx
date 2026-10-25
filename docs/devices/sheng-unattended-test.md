# sheng log capture example

Follow [Capture logs and automate boot tests](/porting/log-capture) for the porting
sequence, including QR export during firmware bring-up. This page supplies sheng's
storage, reset and reader inputs for the later capture stages.

## 1. Prepare the inputs

Use the selected [sheng repository](/devices/sheng), a built kernel/package,
configured [local build inputs](/reference/configuration#local-build-inputs),
fastboot and Python 3. Run the following commands from the main UniXNU checkout:

```sh
DEVICE_REPO=$(cd ../UniXNU-sheng && pwd)
TARGET="$PWD/out/sheng/ipados-17.7.11-21H461/sheng"
mkdir -p "$TARGET/boot"
fastboot devices
```

Record the device serial for the commands below. The reader needs these inputs:

- `boot_sheng_singleboot.img` from the
  [sheng Linux release](https://github.com/ianchb/sm8550-mainline/releases), with
  Android header version 0, appended DTB and an empty ramdisk.
- A static AArch64 Linux BusyBox with shell, mount, USB-reader utilities,
  `dd`, `gzip`, `base64` and `md5sum` applets.
- `nvmem-reboot-mode.ko` from the same kernel release's `linux-xiaomi-sheng.deb`.
- `rebootbl`, compiled from `package/linux-log/rebootbl.cpp` with an AArch64 Linux
  toolchain. It uses Linux syscalls and the board DT's reboot-mode cell.

For example, with LLVM's `clang++` and `ld.lld` installed:

```sh
clang++ --target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static \
  -fno-stack-protector -fno-exceptions -fno-rtti -Wl,-e,start \
  "$DEVICE_REPO/package/linux-log/rebootbl.cpp" -o "$TARGET/boot/rebootbl"
python3 "$DEVICE_REPO/package/linux-log/build-linux-log-boot.py" \
  --boot /path/to/boot_sheng_singleboot.img --busybox /path/to/busybox \
  --rebootbl "$TARGET/boot/rebootbl" --module /path/to/nvmem-reboot-mode.ko \
  --out "$TARGET/boot/sheng-linux-log.img"
```

**Result:** a reader image that uses sheng's Linux kernel and DTB. It reads the
last 32 MiB of `userdata` into RAM and exposes a USB CDC ACM shell. Storage access
is read-only. Its build command prints `BUILT` and the output path.

## 2. Build a logging image

`BootLog.c` writes diagnostic snapshots and the kernel message buffer through
ShengUfs into the reserved last 32 MiB of `userdata`. The writer opens the region
after ShengUfs reads the root layout. Check that this reservation matches the
installed [storage layout](/concepts/userdata).

| Argument | Behavior |
| --- | --- |
| `sheng-log=1` | Start the persistent writer from the board monitor |
| `sheng-autoreset=120` | Attempt return to fastboot after 120 seconds of log time and a successful sync; the board monitor also marks the reboot reason and starts watchdog recovery when its hardware probe succeeds |

Build a logging payload first, using the saved local inputs:

```sh
./unixnu payload --kernel-arg=sheng-log=1
./unixnu bootimg
```

**Result:** `payload/` and `boot/boot.img` under `$TARGET` contain the updated
arguments. Complete the manual cycle before adding timed reset.

## 3. Run one manual capture {#manual-cycle}

Set `SERIAL` to the identifier reported by `fastboot devices`:

```sh
SERIAL=<serial>
fastboot -s "$SERIAL" boot "$TARGET/boot/boot.img"
```

Wait for the checkpoint under test, then use the device's bootloader key sequence
to return to fastboot. Boot the reader:

```sh
fastboot -s "$SERIAL" boot "$TARGET/boot/sheng-linux-log.img"
```

Wait for its `ready: shell on USB serial` message. On macOS:

```sh
python3 "$DEVICE_REPO/scripts/read-sheng-log.py" --wait 240 --out "$TARGET/logs/captures"
```

On Linux, find the reader's actual CDC port, ensure your account can open it,
then pass it explicitly:

```sh
ls /dev/ttyACM*
python3 "$DEVICE_REPO/scripts/read-sheng-log.py" \
  --port /dev/ttyACM0 --out "$TARGET/logs/captures"
```

**Result:** `logs/captures/<tag>-<boot id>/` contains `log.bin`, `header.txt`,
`msgbuf.txt`, `slots.txt` and `latest.txt`. Check the boot ID, snapshots and write
errors. The reader sends `/bin/rebootbl` after transfer. Confirm the same serial
returns with `fastboot devices`. A `no log header` error calls for checking writer
startup and the reserved region before repeating the run.

## 4. Enable the automatic cycle {#automated-cycle}

Rebuild both outputs with timed reset:

```sh
./unixnu payload --kernel-arg=sheng-log=1 --kernel-arg=sheng-autoreset=120
./unixnu bootimg
```

The normal reset follows a successful log sync. The board monitor's watchdog
path can recover a stalled kernel: it requests a 60-second watchdog timeout and
pings until 60 seconds beyond the configured reset deadline. Record whether the
run returned normally or needed recovery.

On a macOS host with one reader serial device, run:

```sh
SERIAL="$SERIAL" READER="$TARGET/boot/sheng-linux-log.img" \
  bash "$DEVICE_REPO/scripts/run-sheng-unattended.sh" "$TARGET/boot/boot.img" 420
```

The runner changes into the device repository, so these image paths are absolute.
It boots the test image, waits for return to fastboot, starts the Linux reader and
fetches the logs. Its automatic serial discovery uses `/dev/cu.usbmodem*`; on
Linux, use the explicit-port capture in step 3 after the timed return, or adapt
the runner to pass that port. The runner saves captures under
`UniXNU-sheng/build/sheng-logs/<tag>-<boot id>/`.

If the 420-second return timeout expires, the runner reports it and waits for
manual bootloader entry. Preserve this outcome in the [porting record](/porting/records).
Confirm the final return to fastboot before starting another run.

## Reader verification

The existing `package/linux-log/check-linux-log-qemu.py` exercises partition
lookup, RAM copying, transfer and decoding with a synthetic GPT log region:

```sh
python3 "$DEVICE_REPO/package/linux-log/check-linux-log-qemu.py" \
  --image "$TARGET/boot/sheng-linux-log.img" --kernel /path/to/qemu-virt-linux-Image \
  --work "$TARGET/logs/reader-qemu"
```

Use an AArch64 QEMU `virt` Linux kernel with virtio block support and the matching
host QEMU tools. Record this host check separately from board storage writes,
USB transfer and reset observations. Continue with the
[shared test procedure](/porting/log-capture#automated-cycle).
