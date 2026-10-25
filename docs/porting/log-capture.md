# Capture logs and automate boot tests

Set up log capture as you bring up the board. Start with firmware logs, collect
kernel evidence at handoff, then add persistent logs and automated reboot cycles.
Use the [bring-up checkpoints](/porting/bring-up) to identify the stage being tested.

| Stage | Required support | Saved result |
| --- | --- | --- |
| UEFI menu | GOP display, menu input, Advanced Logger and QR plugin | Firmware `.log` file scanned from the screen |
| XNU entry | UART or framebuffer diagnostics | Serial log or photograph of the last checkpoint/panic |
| Storage | Reserved log region, kernel writer and Linux reader | Kernel message buffer and diagnostic snapshots |
| Repeated tests | Working log retrieval and return to bootloader | One evidence directory per boot |

## 1. Include the firmware log exporter {#firmware-setup}

Prepare the firmware checkout using [UEFI integration](/porting/uefi-firmware).
The shared exporter is
`package/UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf` in UniXNU.
It reads the Advanced Logger memory buffer while UEFI boot services are active.

1. In the firmware DSC `[Components]`, add:

   ```ini
   UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
   ```

   In the FDF's DXE firmware volume, add:

   ```ini
   INF UniXNUPkg/Driver/AdvancedLogDxe/AdvancedLogDxe.inf
   ```

2. Enable the platform's Advanced Logger producer and route the desired firmware
   debug output into it. The exporter dispatches after `gAdvancedLoggerProtocolGuid`
   is installed. Its INF also requires `AdvLoggerPkg`, `MsGraphicsPkg`,
   `AdvancedLoggerAccessLib` and `QrEncoderLib`.
   From the firmware checkout, find the library instances supplied by its Mu revision:

   ```sh
   find . -name '*AdvancedLogger*.inf' -o -name '*QrEncoder*.inf'
   ```

   Map the access and QR libraries in the DSC `[LibraryClasses]` and include their
   package roots in `board.toml` `[firmware] packages`. Initialize the submodules
   containing those packages if the search returns no files. Use the platform's
   Advanced Logger configuration for its buffer and debug-library mappings.

3. Set `gUniXNUPkgTokenSpaceGuid.PcdUniXNUDeviceModel` in the DSC `[PcdsFixedAtBuild]`
   to the board's ASCII name. The export filename uses this value.
4. In the device's `board.toml`, add `tools = true` to its existing `[payload]`
   table. This adds **Tools** to the generated boot menu. A hand-authored
   `unixnu.toml` uses an [entry with `action = "tools"`](/reference/boot-configuration).
5. From the main UniXNU checkout, with the device selected and
   [local inputs configured](/reference/configuration#local-build-inputs), run:

   ```sh
   ./unixnu firmware
   ./unixnu payload
   ./unixnu bootimg
   ```

**Result:** the rebuilt image contains the exporter, and **Tools → Advanced Logger**
appears when the platform's logging protocol is available.

## 2. Scan the firmware log before kernel handoff {#qr-logs}

Prepare a second phone or camera device with [OneSend (扫传)](https://onesend.01mvp.com).
The exporter sends a sequence of OneSend QR frames.

1. Boot the image from the main checkout:

   ```sh
   fastboot devices
   fastboot -s <serial> boot out/<device>/<os_profile>/<config>/boot/boot.img
   ```

2. Stay in the UniXNU menu. Select **Tools**, then **Advanced Logger**.
   The plugin takes a snapshot of the current firmware log when opened.
   If a loader error returns you to the menu, open the plugin then to capture it
   before resetting the device.
3. Open OneSend's receiving scanner, allow camera access and keep the complete QR
   square in view. Keep scanning as frames cycle until the receiver finishes.
4. Use **Down / j** to slow rotation and **Up / k** to speed it up. The initial
   interval is one second; five speeds range from 5 seconds to 250 milliseconds.
   Boards that map volume buttons to Up/Down use those buttons. **Enter** or
   **Escape** returns to the menu; a board's power button works when mapped to Enter.
5. Save the received `<model>-<date>-<time>.log` with the image revision and last
   completed checkpoint. The filename uses the firmware clock. Check the text for
   the failing operation before rebuilding.

**Result:** a readable firmware log captured while the menu remains accessible.
XNU execution after `ExitBootServices` uses the capture methods in step 3.

| Symptom | Action |
| --- | --- |
| Tools is missing | Check `[payload] tools`, rebuild the payload and repack the boot image |
| Advanced Logger is missing | Check the INF in DSC/FDF, its dependencies and the platform's logger protocol |
| `Advanced Logger: no log` | Check the Advanced Logger producer, debug routing and buffer configuration |
| Scanner progress stops | Reduce rotation speed and keep scanning the repeating sequence |
| Firmware stops before the menu | Capture the platform UART or photograph its last visible output |

## 3. Capture the kernel handoff {#kernel-logs}

1. Save the firmware log from step 2, then start the kernel entry.
2. Follow [debugging](/guide/debugging#procedure) to capture the board's UART or
   framebuffer output. For a green handoff bar, use the
   [entry-probe procedure](/guide/debugging#green-bar).
3. Photograph the complete panic screen or checkpoint, including registers and
   the last text lines. Record the kernel arguments from the built payload.
4. After changing arguments, run `./unixnu payload` and `./unixnu bootimg` again.
   Repeat the same boot and compare the last confirmed checkpoint.

**Result:** evidence for the kernel stage even while storage is being adapted.
Continue with persistent capture after [storage verification](/porting/ufs).

## 4. Adapt persistent capture to the board {#persistent-logs}

The [sheng implementation](/devices/sheng-unattended-test) provides a working
source layout to study. Complete these board interfaces before enabling writes:

1. Reserve a log region in the target's storage layout. Record its partition,
   offset, size and block units alongside the root and Data regions. Check that
   the image builder, partition provider, writer and reader use the same bounds.
2. Connect the kernel writer to the storage driver's bounded read/write and sync
   operations. Include the kernel message buffer, diagnostic snapshots, boot ID
   and write status. Start it after the storage region can be opened.
3. Build a Linux reader for the target board with its kernel, DTB, storage driver,
   USB device controller and CDC ACM support. Copy the reserved region into RAM,
   then expose a serial shell for retrieval. Keep the source partitions read-only.
4. Adapt the host decoder to the writer's header and snapshot format. Select the
   serial device explicitly on hosts whose device names differ from the reader's
   discovery pattern.
5. Implement return to the board's bootloader in both XNU and the Linux reader.
   Use the target's reboot-reason mechanism and reset interface. Verify watchdog
   handling separately if automatic recovery from a stalled kernel is required.

Review `package/BootLog.c`, `BootReset.c`, `BootResetCore.h`, `package/linux-log/`
and `scripts/read-sheng-log.py` in `UniXNU-sheng`. Its PMIC addresses, watchdog
calls, userdata reservation and Linux image inputs are board configuration that
the new port must supply from its own hardware sources.

**Result:** a documented log region, matching writer/reader/decoder, and a tested
route back to the bootloader. Record these in the device repository.

## 5. Complete one manual capture cycle {#manual-cycle}

Use the selected device's documented logging arguments and reader image.
The [sheng example](/devices/sheng-unattended-test#manual-cycle) supplies concrete
commands for that implementation.

1. Build a test payload with logging enabled, then rebuild its boot image.
2. Run `fastboot -s <serial> boot <test.img>`. Wait for the log writer to run.
3. Return to fastboot using the board's verified reset method. Run
   `fastboot -s <serial> boot <reader.img>`.
4. Run the board's host reader. Save the raw window and decoded text under a
   directory identified by the boot ID. Confirm the header belongs to this boot,
   snapshots are present and write status is successful.
5. Return to fastboot from the reader and confirm the same serial with
   `fastboot devices`.

**Result:** one complete test, saved logs and a device ready for the next image.
If the header is missing or stale, check writer startup and region geometry.
If the reader's serial port is absent, check its Linux USB gadget and host access.

## 6. Automate the verified cycle {#automated-cycle}

1. Enable the board's timed return to bootloader. Keep the timeout longer than
   the checkpoint being measured and allow time for log synchronization.
2. Configure a host runner with the exact fastboot serial, absolute test-image
   path, absolute reader-image path and a timeout covering the boot and reset.
3. Have it boot the test image, observe USB departure and return, boot the reader,
   retrieve logs, and confirm the final return to fastboot. Use the
   [sheng runner example](/devices/sheng-unattended-test#automated-cycle) when those
   board interfaces match.
4. On timeout, record the failed stage and recover through the board's documented
   bootloader procedure. Keep this run's images and partial evidence.
5. Repeat after one change. Record revisions, arguments, checkpoint, reset result,
   output directory and unresolved errors in the [porting record](/porting/records).

**Result:** each test produces an identifiable evidence directory and a recorded
bootloader return. Continue the remaining [bring-up stages](/porting/bring-up).
