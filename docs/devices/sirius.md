# Xiaomi Mi 8 SE \(sirius\)

[57UU/UniXNU-sirius](https://github.com/57UU/UniXNU-sirius)

<div v-pre>

Xiaomi Mi 8 SE running the UniXNU source kernel through its own U-Boot UEFI: entry, MMU, timer, PSCI, platform expert and the boot-progress console work on hardware; IOKit is blocked on AppleARMCPU claiming /cpus.

Source record: [porting.toml](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/porting.toml), `e51069a970f81a3c48588041a0caa3087a1ebbbc`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `implemented` |
| [Firmware and boot](#firmware) | `implemented` |
| [CPU and interrupts](#cpu) | `implemented` |
| [Memory and DMA](#memory) | `implemented` |
| [Storage and root](#storage) | `planned` |
| [Display](#display) | `implemented` |
| [Input](#input) | `planned` |
| [GPU](#gpu) | `planned` |
| [Network](#network) | `planned` |
| [Audio](#audio) | `planned` |
| [Power](#power) | `planned` |
| [Image assembly](#packaging) | `implemented` |
| [Diagnostics](#diagnostics) | `implemented` |

## Configuration {#configuration}

`implemented`

Components: `ArmDTPlatform`, `DarwinBoot`.

### Hardware and dependencies

- SoC: SDM710 \(Kryo 360, 2x Cortex-A75 + 6x Cortex-A55\), GIC-500 GICv3, 19.2 MHz Qualcomm XO counter, PSCI over SMC.
- DRAM: 6 GiB model, 2 GiB at 0x80000000 plus 0xfd6c0000 at 0x100000000, 0x17d6c0000 usable.
- Target OS: the unchanged iPad7,1 iPadOS 17.7.11 \(21H461\) restore image, the same kernelcache as fajita and sheng.
- Hardware sources: mainline sdm670.dtsi, the vendor kernel tree and measurements on the phone.

### Configuration

- board.toml is format 2 \(schema\_version = 2\) and selects soc\_source = sdm710 and os\_profile = ipados-17.7.11-21H461.
- dts/sirius.dts describes memory, the eight cores and their affinities, and includes the SoC DTSI for the GIC, PSCI and counter.
- configs/sirius\_defconfig keeps the first-console kernel configuration and turns off the profile settings the image does not use yet.
- The prelink roots, slide, early security initializer and Apple device-tree identity come from the OS profile; the board adds sleep-disabled, the product model and the button set.

### Procedure

- From a UniXNU checkout: ./unixnu defconfig sirius\_defconfig -D ../UniXNU-sirius.
- Then ./unixnu fetch, ./unixnu status --json and ./unixnu tree; the output is out/sirius/ipados-17.7.11-21H461/sirius/.
- Compile the hardware tree through the payload step \(prototype: dtc -I dts -O dtb -i &lt;soc&gt;/dts dts/sirius.dts\).

### Development findings

- Format 2 replaced the hand-written ADT generator of the legacy port \(scripts/make-sirius-adt.py\) with a board DTS: the payload step compiles the tree and derives kernel.macho.adt from it.
- The arm-io base is derived from the exported nodes, not written by hand: the GIC at 0x17a00000 makes the window start at 0x17000000, which XNU reads as the SoC base.
- The legacy port set ipad\_identity = true; in format 2 the same identity is the OS profile's \[adt\] tables, so the board only states its deviations.

### Completion criteria

- status --json resolves the ARM64, sdm710, iPadOS and device layers, prints a non-empty patch order and shows the device package as kits/sirius.
- The compiled hardware tree carries one contiguous memory range equal to payload.dram\_size, eight enabled CPUs and a PSCI method.

### Current limits

- The SoC profile is new: only the GIC, PSCI, counter and the timer INTID are shared, everything else is still board-local.
- No device-verified configuration has been recorded against a commit of this repository yet; the evidence is the legacy port's handoff document.

### Next action

- Run status --json and tree against this repository and commit the resulting unixnu.lock.
- Rebuild the payload from dts/sirius.dts and compare the generated ADT with the legacy one before the next hardware boot.

### Evidence

- `source`: [Hardware values, the boot chain and the bring-up history of the legacy port, from which this configuration is carried over.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Firmware and boot {#firmware}

`implemented`

Components: `DarwinBoot`.

### Hardware and dependencies

- The device runs sirius-distro \(mainline Linux\) and its boot partition carries U-Boot 2026.07 with EFI\_LOADER, the UEFI boot manager, GOP through vidconsole and EFI block volumes over FAT.
- UniXNU.efi is built on the host by ./unixnu build package; DarwinBoot.efi is built from drivers/DarwinBoot.
- The payload volume goes to the unused vendor partition mmcblk0p80 \(3 GiB\) and is selected by a marker file.

### Configuration

- board.toml has no \[firmware\] table: there is no Mu-Silicium firmware and no boot image on this port.
- \[payload\] loader = "payload" makes the generated unixnu.toml load Payloads/DarwinBoot.efi as a \[\[driver\]\] instead of expecting it in the firmware volume.
- scripts/make-uboot-payload.py adds \\EFI\\BOOT\\BOOTAA64.EFI \(UniXNU.efi\) and the DarwinBoot.efi driver to the payload FAT volume.

### Procedure

- fastboot flash boot the U-Boot image, fastboot flash vendor the finished payload volume, then select the UniXNU menu entry in U-Boot.
- U-Boot boots UniXNU when /EFI/UniXNU/BOOTUNIXNU exists on the vendor partition and boots Linux otherwise.

### Development findings

- U-Boot's UEFI services are enough for UniXNU, DarwinBoot and XNU: no Mu-Silicium platform port is needed for this board.
- DarwinBoot's GIC-500 pre-configuration \(the iBoot role\) is what lets the kernel pass lockdown and start kexts on this SoC.
- The pre-configuration must not enable the EL1 virtual timer PPI early: on SDM710 that is INTID 19 and it fires before ArmCpu installs its handler.

### Completion criteria

- U-Boot starts UniXNU from the FAT volume, the menu lists the xnu entry, and DarwinBoot hands control to the kernel with the device tree, framebuffer and command line.

### Current limits

- The boot chain is U-Boot specific: it is not reproducible with the Mu-Silicium flow of the other ports and is not covered by ./unixnu firmware or ./unixnu bootimg.

### Next action

- Keep the U-Boot side \(sirius-distro\) pinned by revision in the handoff document; it is the firmware for this port.

### Evidence

- `source`: [Boot-chain diagram, the marker-file boot rule and the DarwinBoot GIC pre-configuration rationale.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## CPU and interrupts {#cpu}

`implemented`

Components: `ArmCpu`.

### Hardware and dependencies

- Eight Kryo 360 cores at Affinity1 0x000..0x700, boot CPU cpu0; cpu0-5 Cortex-A55, cpu6-7 Cortex-A75.
- GIC-500 distributor 0x17a00000/0x10000, redistributor 0x17a60000/0x100000, stride 0x20000.
- The EL1 virtual timer is GIC PPI 3 / INTID 19 \(Apple uses PPI 11 / INTID 27\); the SoC counter runs at 19.2 MHz.

### Configuration

- dts/sirius.dts lists all eight cores in boot order with their compatible strings; the ADT carries the same order.
- The kernel command line selects the board behaviour: sheng-smp=1, cpus=8, armcpu-self-ipi=1, armcpu-timer-intid=19.
- drivers/ArmCpu/ArmCpu.cpp is the board copy of the component: it adds the non-Apple timer INTID, the /cpus probe counters and the sheng-smp claim path.

### Procedure

- Build the kernel, then read the boot-progress TOPO line to confirm the topology and the probe decision.

### Development findings

- The boot-progress TOPO row confirms the kernel topology on hardware: boot=0 max=7 nums=01234567.
- With sheng-smp=1 the native ArmCpu claims every /cpus nub; without it the kernelcache AppleARMCPU takes the nubs and panics with "CPU configuration in device tree is invalid" because it requires /cpus/max\_cpus.
- AppleARMCPU still wins the nubs at the current stage, which is the open IOKit blocker.

### Completion criteria

- Every enabled CPU reaches its entry checkpoint and timer/IPI handling works on each enabled CPU with the board INTID.

### Current limits

- IOKit attach of the native ArmCpu is not yet proven; the last trusted snapshot shows AppleARMCPU holding /cpus.

### Next action

- Stop pre-enabling the timer PPI in DarwinBoot, re-read the probe row, and confirm ArmCpu installs its IRQ handler before the timer fires.

### Evidence

- `source`: [Timer INTID measurement, the TOPO checkpoint and the AppleARMCPU panic analysis.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Memory and DMA {#memory}

`implemented`

### Hardware and dependencies

- Usable DRAM 0x17d6c0000 from 0x80000000 \(6 GiB model: 2 GiB then 0xfd6c0000 at 0x100000000\).
- The framebuffer handed over by U-Boot sits at 0x9c000000 \(1080x2240 splash\).

### Configuration

- The DTS memory node is a single contiguous range and board.toml repeats the size as \[payload\] dram\_size; the driver refuses a mismatch.
- The ADT receives dram-base, dram-size and the CHRP NVRAM proxy from the payload step.

### Procedure

- Read the MEM row of the boot-progress panel and compare it with the DTS range.

### Development findings

- The single contiguous range is what DarwinBoot's window allocation and the ADT handoff require; the two physical banks are described as one range on purpose.

### Completion criteria

- The kernel memory map accounts for every handoff region, and no DMA engine or framebuffer range overlaps the kernel window.

### Current limits

- DMA and SMMU behaviour is untested: there is no storage or GPU driver yet.

### Next action

- Capture the kernel's memory map row after the next IOKit break-through and reconcile it with the DTS.

### Evidence

- `source`: [DRAM split, framebuffer address and the memory-map checkpoint.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Storage and root {#storage}

`planned`

### Hardware and dependencies

- The reference unit is eMMC, not UFS: the UFS transport and its sheng integration points do not apply to this board.
- No native storage driver exists yet; the expected first stop of the kernel is the root device wait.

### Configuration

- No storage component is selected in board.toml; CONFIG\_XNU\_NATIVE\_UFS is off in the defconfig.

### Procedure

- Follow the storage guide to choose an eMMC driver path, then select it and record the checkpoint.

### Development findings

- The legacy port deliberately kept every UFS and owner-root flag off; the boot-progress interlock of UniXNU had to be relaxed for that reason.

### Completion criteria

- The eMMC registers, GPT and the OS/SystemOS windows are correct, the root volume mounts and launchd starts.

### Current limits

- No storage driver and no root filesystem: the port currently stops before the root device.

### Next action

- Inventory the eMMC controller \(SDCC/MSDC\) registers and interrupt lines from the vendor tree before selecting an implementation.

### Evidence

Collect evidence using the next action.

## Display {#display}

`implemented`

### Hardware and dependencies

- U-Boot hands over a 1080x2240 splash framebuffer at 0x9c000000 through GOP \(vidconsole\).
- The panel is 1080x2244 DSI; there is no backlight or display driver in the kernel yet.

### Configuration

- CONFIG\_XNU\_BOOT\_PROGRESS=y links the boot-progress layer; boot-progress=2 on the command line draws the kernel console lines.
- package/Font.h is vendored from Mu-Silicium, so the archive builds without a UEFI checkout.

### Procedure

- Build kernel, payload and volume, boot, and read the panel: stage lines, TOPO, PROBE, MEM and panic text.

### Development findings

- The panel is the readable kernel log of this port: it renders the console, the TOPO/PROBE rows and panic\(\) output.
- Since the panic snapshot patch the panel re-renders at panic time, so the rows are not a stale frame.

### Completion criteria

- A known frame appears with correct geometry, and the kernel console and panic output stay readable on the panel.

### Current limits

- This is a firmware framebuffer text surface, not a Darwin display driver: no IOMobileFramebuffer, no backlight, no modesetting.

### Next action

- Keep the panel as the diagnostic surface until a display provider exists, and record each new row it prints.

### Evidence

- `source`: [Boot-progress panel behaviour, the panic snapshot fix and the geometry of the U-Boot framebuffer.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Input {#input}

`planned`

### Hardware and dependencies

- The board has no Home button; volume keys are reported by the button node of the ADT.
- The touch controller is not identified for this unit yet.

### Configuration

- board.toml writes \[adt.buttons\] with hold, voldown, volup and home-button-type = 2.

### Procedure

- Follow the input guide once a touch controller is known, then select an implementation.

### Development findings

- No input component is selected; the button node exists so that BSPlatform and MobileGestalt read a coherent product.

### Completion criteria

- Touches map to the display with stable contacts and the volume keys produce the expected HID reports.

### Current limits

- No touch support and no IOHIDDevice provider.

### Next action

- Identify the touch controller from the vendor DTS and the panel firmware.

### Evidence

Collect evidence using the next action.

## GPU {#gpu}

`planned`

### Hardware and dependencies

- Adreno 616 with the SDM710 GPU firmware; no GPU component is selected yet.

### Configuration

- Nothing is configured: the CPU renders the boot-progress panel through the firmware framebuffer.

### Procedure

- Follow the GPU guide to evaluate Adreno740/AdrenoMetal for this revision; the A616 is not the A740 the component targets.

### Development findings

- The Adreno family components in the catalog target the A740 of SM8550; a different implementation would be needed here.

### Completion criteria

- GPU submissions complete, fences work and a known image renders.

### Current limits

- No GPU bring-up has been attempted on this port.

### Next action

- Decide whether an A616 implementation is in scope after IOKit and storage; do not assume the A740 path applies.

### Evidence

Collect evidence using the next action.

## Network {#network}

`planned`

### Hardware and dependencies

- The Wi-Fi chip of this unit is not identified in the port record yet.

### Configuration

- No network component is selected.

### Procedure

- Follow the network guide once the chip and its firmware are known.

### Development findings

- No network work has started; the first console image does not need it.

### Completion criteria

- Scanning, association and sustained traffic pass with the selected firmware.

### Current limits

- No network support at any layer.

### Next action

- Identify the WCN chip from the vendor DTS and the firmware partition before selecting a driver.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`planned`

### Hardware and dependencies

- No audio path is documented for this unit yet.

### Configuration

- No audio component is selected.

### Procedure

- Follow the audio guide once the codec and DSP path are known.

### Development findings

- No audio work has started.

### Completion criteria

- Each declared route carries the selected format through playback or capture.

### Current limits

- No audio support at any layer.

### Next action

- Inventory the codec and the DSP layout from the vendor tree.

### Evidence

Collect evidence using the next action.

## Power {#power}

`planned`

### Hardware and dependencies

- No suspend/resume path exists; the boot-progress panel is the only power-facing behaviour so far.

### Configuration

- board.toml writes \[adt.defaults\] sleep-disabled = 1 so the CPU does not park forever.
- PSCI is declared in the SoC DTSI and used for reset and secondary-core start.

### Procedure

- Follow the power guide to add the PMIC and battery controls once the platform expert is stable.

### Development findings

- Reset works through PSCI \(the stage-3 reset loop proved it\).

### Completion criteria

- Declared power controls and battery readings match observations and sleep/resume restores the dependent peripherals.

### Current limits

- No battery reporting, no CPU frequency policy and no PMIC driver.

### Next action

- Defer until IOKit and storage are stable; keep sleep-disabled until a resume path exists.

### Evidence

- `source`: [PSCI reset evidence and the reason sleep is disabled.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Image assembly {#packaging}

`implemented`

Components: `XnuPrelink`.

### Hardware and dependencies

- The payload FAT volume is the flashed artifact of this port; there is no boot.img.
- The prelink inputs \(kernel path, roots, slide, early security initializer\) come from the OS profile; the board does not override them.

### Configuration

- \[payload\] sets volume SIRIUSESP, 256 MiB, loader = payload, dram\_size and the kernel command line.
- \[payload.adt\] keeps only chip\_id: the platform seed is supplied per build with --unique-chip-id when the userland stages need it.

### Procedure

- ./unixnu build kernel, ./unixnu build package, then bash scripts/build-darwinboot.sh \(or the CI workflow\), ./unixnu build payload, and finally make-uboot-payload.py --output the volume to flash.

### Development findings

- Format 2 generates kernel.macho.adt from the DTS, so the legacy ADT script is retired; make-uboot-payload.py still finishes the volume because U-Boot needs the EFI binaries inside it.
- scripts/build-xnu-4k-baseline.sh is a copy of the UniXNU script with one board deviation: the boot-progress interlock only requires the source crypto provider.
- The legacy build applied patches/xnu/\*.patch by glob \(56 files\); format 2 applies only what the series list names, and the ARM64 and OS series assume the two files they omit are present. native-smp-secondary.patch sets MAX\_CPUS 8 and Affinity1 core numbering; sheng-platform-power.patch supplies the bsd\_init.c block that zzz-boot-progress.patch quotes as context \(CONFIG\_XNU\_SHENG\_PMGR is off, so its code compiles to nothing\). Both now live in the sdm710 SoC series, which applies before the OS series.
- The first format 2 kernel build on Linux failed at the last patch, zzz-boot-progress.patch, with a bsd\_init.c context mismatch: the missing piece was sheng-platform-power.patch, not the moved native-smp-secondary. With both in the SoC series the whole series applies and the kernel prelinks: kernel.macho 50 MB, \_\_TEXT 0xfffffff007004000, 30 kernelcache components, 69294 bindings.
- The kernel step also runs on Linux through UniXNU's toolchain installer, which builds a patched LLVM, cctools-port ld64, MIG, iig and lzfse. That installer writes /opt/unixnu-linux, /usr/bin shims and distribution packages, so this port runs it inside a Podman image \(containers/xnu-linux/\) and leaves the host untouched. The alternative, Allenwdk/xnu-linux-toolchain, uses the system clang with portable-kalloc-type-builtins.patch and cctools-port, but needs a path-layout adapter for the driver's checks.
- The first flashed build hung on hardware with the entry probe's blue bar and no magenta bar. The cause was a defconfig override, not the kernel sources: CONFIG\_XNU\_NATIVE\_ARCH\_TIMER=n. build-xnu-4k-baseline.sh sets NATIVE\_ARCH\_TIMER=$&\#123;XNU\_NATIVE\_ARCH\_TIMER:-$&\#123;XNU\_ARMCPU:-0&\#125;&\#125;, so the legacy build ran with 1; at 0, architectural-timer-entry.patch compiles the BCM2837 local interrupt router write \(0x40000040 + cpu\*4, Raspberry Pi local peripherals\) into start.s. SDM710 has nothing at that address, and the store aborts before the MMU is on. The defconfig now leaves the ARM64 profile value \(y\) alone and documents why.
- Diagnosis method worth repeating: the legacy kernel from payload-full.img boots with this repository's new UniXNU.efi, DarwinBoot.efi and DTS-generated ADT \(it reaches \_sleh\_irq and panics on the known early timer PPI\), so the EFI side and the ADT are sound and the regression is in the kernel build. Comparing the two kernels' start\_first\_cpu bodies then showed the extra router write as the only semantic difference.
- With the timer override removed the Linux-built kernel boots past the MMU on hardware: the boot-progress panel reports TOPO boot=0 max=7 nums=01234567, the console reaches IOKit \(PE\_init\_iokit, IONVRAM initImageFormat\) and then the CPU takes an IRQ it cannot dispatch. lr 0xfffffff00741754c is \_sleh\_irq+0x70, the same NULL per-CPU handler the legacy kernel hit.
- That IRQ is DarwinBoot's early timer PPI. GIC\_SGI\_PPI\_ENABLE included bit 19, so the EL1 virtual timer fired before ArmCpu::start installed the handler. DarwinBoot now enables SGIs 0..15 only and leaves INTID 19 to ArmCpu \(armcpu-timer-intid=19\), which is what the handoff's next step asked for.
- With the timer PPI left to ArmCpu the next stop was AppleARMPlatform's CPU class: it matches cpu0..cpu7 because IODTPlatformExpert compares IONameMatch by prefix \('cpu' matches 'cpu0'\), starts first, and panics with 'CPU configuration in device tree is invalid' \(its AppleARMCPU.cpp validates max\_cpus, cpu-id, interrupts and function-\* properties that an Apple device tree carries\). The native ArmCpu personality \(IOPlatformDevice, probe score 2000\) did not win that match even after probe\(\) added 40000, so make-uboot-payload.py rewrites the AppleARMCPU IONameMatch from cpu to xpu in the personality plist inside kernel.macho \(equal length, nothing moves\).
- A frozen panel is not proof of a hang: the panel repaints from the timer interrupt, so before ArmCpu enables the timer PPI the last painted second stays on screen while the kernel may still be running. Checkpoints \(armcpu-panic=N, panics at entry/prepareGic/registration/controller/initCPU/registerService\) are what distinguish the two.
- ArmCpu::start then failed inside initDistributor: it re-ran the quiesce sequence \(GICD\_CTLR &amp;= \~3, wait, restore with ARE\) over the state DarwinBoot had already programmed, which clears the group enables and leaves the core with no interrupts at all; the boot then sits in IOKit matching with a frozen panel. The fix keeps the firmware state when GICD\_CTLR already reports affinity routing plus group 1 and only quiesces the SPIs. After it the panel advances \(time=31s, IRQ=1076\) and the kernel reaches bsd\_init with TOPO boot=0 max=7, PROBE calls=17 acc=8 num=7 reg=700 - eight CPUs online.
- The next blocker is AMFI: 'AMFI: No PMGR?' @ConfigurationSettings.cpp:235. The unchanged target AMFI finds the PMGR through IOService::serviceMatching\("ApplePMGRNub"\) + waitForMatchingService, then reads the provider at nub+0x88 and calls its readAon/writeAon slots \(OR bit 2\). The original classes are ApplePMGR \(storage 0x4b8a8\) and ApplePMGRNub \(0x740\); UniXNU/scripts/audit-amfi-pmgr-contract.py pins the exact instructions. amfi\_get\_out\_of\_my\_way=1 does not skip the check. Two routes remain: instantiate the original classes the way UniXNU-sheng does \(OSKextLoadKextWithIdentifier + a generated OriginalPMGRABI.h + a published nub\), or patch the AMFI block in the prelinked kernel.
- AMFI is cleared without touching the target kernel: the nub helpers behind AMFI's readAon/writeAon return 0 when the provider stored at nub+0x88 is NULL, so publishing a real but provider-less ApplePMGRNub satisfies the latch \(read 0, OR 4, write no-op\). package/SiriusPmgr.cpp allocates the class through OSMetaClass::getMetaClassWithName + alloc + init, attaches it to the platform and registers it from the sheng\_pmgr\_run hook in bsd\_init \(CONFIG\_XNU\_SHENG\_PMGR=1, sheng-pmgr=2\), and its build script writes the archive to build/sheng-platform-power/libShengPlatformPower.a because build-xnu-4k-baseline.sh force-loads exactly that path.
- With AMFI satisfied the boot passes bsd\_init and stops at 'Failed to mount root device' @IOKitBSDInit.cpp:984 with 8 CPUs, IRQ=2145, 61 s of live uptime: the kernel now runs to the point where it needs a storage driver and a root filesystem, which this board does not have yet \(no UFS or VirtIO block component, no APFS container on the payload volume\).

### Completion criteria

- The kernel prelinks against the pinned 21H461 kernelcache, the payload contains the kernel, the ADT and the DarwinBoot driver, and the flashed volume boots from U-Boot.

### Current limits

- The kernel step needs macOS and Xcode, or the container image with UniXNU's Linux toolchain; the board script must be re-based on the upstream script when UniXNU changes it.

### Next action

- Run the new payload once and diff the generated ADT against the legacy one before flashing.
- Boot the prelinked kernel on the phone and record the stage rows; the build side is now reproducible on Linux \(container image, see downloads\).

### Evidence

- `source`: [Build and deploy loop, the CI kernel artifact and the payload layout on the vendor partition.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

## Diagnostics {#diagnostics}

`implemented`

### Hardware and dependencies

- The panel and the serial console are the two evidence channels; there is no storage for logs yet.

### Configuration

- panic\_screen = true patches the entry probe and the panic screen into the payload kernel.
- boot-progress=2 draws the kernel console lines; debug=0x14e keeps the debug output on.

### Procedure

- Record the last checkpoint printed on the panel and in the U-Boot/XNU log for every boot, with the kernel and payload hashes of that run.

### Development findings

- Stage bands \(entry, pre-MMU, panic\) and the TOPO/PROBE/MEM rows replaced the LED and UART guessing of earlier bring-up.
- A panic snapshot forces a panel re-render at panic time; before it, rows could show a stale frame.

### Completion criteria

- Each claimed milestone has a reproducible command, a matching source and image revision and an accessible result.

### Current limits

- No persistent log capture: the evidence lives in photographs of the panel and in the run's hashes.

### Next action

- Add the boot row and hash triple to every hardware result recorded here.

### Evidence

- `source`: [Entry probe, panic screen, panel rows and the evidence convention of the port.](https://github.com/57UU/UniXNU-sirius/blob/e51069a970f81a3c48588041a0caa3087a1ebbbc/docs/HANDOFF.zh-CN.md)

</div>
