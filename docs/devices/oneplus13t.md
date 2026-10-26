# OnePlus 13T \(oneplus13t\)

[IssacL04/UniXNU-Pagani_SM8750](https://github.com/IssacL04/UniXNU-Pagani_SM8750)

<div v-pre>

OnePlus 13T PKX110: format-2 migration of the single-core, read-only UFS root and CPU display port; volatile RAM Data.

Source record: [porting.toml](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/porting.toml), `fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac`.

This page is generated from the device record.

| Area | Status |
| --- | --- |
| [Configuration](#configuration) | `host-verified` |
| [Firmware and boot](#firmware) | `host-verified` |
| [CPU and interrupts](#cpu) | `host-verified` |
| [Memory and DMA](#memory) | `host-verified` |
| [Storage and root](#storage) | `host-verified` |
| [Display](#display) | `host-verified` |
| [Input](#input) | `host-verified` |
| [GPU](#gpu) | `planned` |
| [Network](#network) | `planned` |
| [Audio](#audio) | `planned` |
| [Power](#power) | `host-verified` |
| [Image assembly](#packaging) | `host-verified` |
| [Diagnostics](#diagnostics) | `host-verified` |

## Configuration {#configuration}

`host-verified`

Components: `PaganiArmDTPlatform`, `NativeAES`, `NativeKeyStoreEvents`.

### Hardware and dependencies

- Four layers: ARM64, pinned sm8750, ipados-17.7.11-21H461, OnePlus 13T. Arrays are complete replacements.

### Configuration

- schema\_version=2; soc\_source=sm8750; complete component/fragment/archive arrays; configs/oneplus13t\_defconfig.

### Procedure

- From UniXNU: defconfig oneplus13t\_defconfig -D ../UniXNU-Pagani\_SM8750; fetch; status --json; tree; run scripts/test-format2.py with the main scripts on PYTHONPATH.

### Development findings

- Four profiles select 18 components. PaganiInput and PaganiPanel are embedded in the ABI-qualified display archive and depend on QcomGeniSpi plus Sm8750Platform. Build 48 passed the complete configuration checks, driver compilation, final kernel link and prelink. Its source pins are device e6435b2bda6e91431f3bc0c442d1ff9c47be1451, main 42ffa07e9b82f7ffc37499392b2751e146647a6a, drivers 143e958bea8ae8ad7f47723cd105a589084f913d and SoC 9e90b16861612c0251c453ccbf6cf10592ea87e2.
- The documentation workflow checks out pinned driver submodules so repository-relative evidence links resolve during validation.

### Completion criteria

- All source pins resolve; every selected component has a record; status shows the intended overrides and patch order.

### Current limits

- The main fork carries community catalog entries; stock upstream alone cannot resolve this selection.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [All 18 selected components resolved at their pinned revisions; archive ordering, ADT checks, full kernel link and prelink passed. Downloaded kernel and report agree at 127778816 bytes; hardware remains untested.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40183635568)
  Command: <code>test-format2.py; ./unixnu status --json; ./unixnu kernel -j 3</code>. Revision: `e6435b2bda6e91431f3bc0c442d1ff9c47be1451`.
- `host`: [Four layers, 15 pinned components, complete archive order and matching required ADT properties passed.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-format2.py; ./unixnu status --json</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Firmware and boot {#firmware}

`host-verified`

Components: `PaganiDarwinBoot`.

### Hardware and dependencies

- PKX110 ABL uses init\_boot ramdisk during fastboot boot; embedded payload plus the BootPayload HOB remains necessary.
- Private firmware checkout: IssacL04/UEFI-Pagani\_SM8750 at 50b7313f40684aa25481cd109d7f48b9a50663fa.

### Configuration

- firmware.config=oneplus13t; firmware.packages points to PaganiDarwinBoot; bootimg.embed\_payload=true.

### Procedure

- From UniXNU after kernel inputs pass: build package; firmware; payload; bootimg using ignored local input paths.

### Development findings

- PKX110 ABL uses init\_boot ramdisk during fastboot boot; embedded payload plus the BootPayload HOB remains necessary.
- UniXNURamDiskDxe, DiskIoDxe and FAT support the standard FDT handoff; the rebuilt FD contains both paths.

### Completion criteria

- Firmware image builds, enters UniXNU, publishes the framebuffer and preserves the documented memory reservations.

### Current limits

- The migrated FD and new standard initrd driver have not been booted on hardware.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [AARCH64 DEBUG package and 3145728-byte FD built locally, including UniXNURamDiskDxe and the retained embedded payload path; hardware boot remains untested.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/MIGRATION.md)
  Command: <code>./unixnu build package; ./unixnu firmware</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## CPU and interrupts {#cpu}

`host-verified`

Components: `PaganiArmCpu`.

### Hardware and dependencies

- Only MPIDR affinity 0 is enabled. GICD=0x16000000/0x10000, GICR=0x16080000/0x200000, stride=0x40000, CNTFRQ=19200000.

### Configuration

- cpus=1; cpu0 status=okay; XNU\_NATIVE\_ARCH\_TIMER and XNU\_NATIVE\_PAN enabled.

### Procedure

- Compile the board DTS, compare the generated ADT, run test-stage6.py and test-kernel-source.py, then obtain a fresh boot log for CPU0/timer.

### Development findings

- Only MPIDR affinity 0 is enabled. GICD=0x16000000/0x10000, GICR=0x16080000/0x200000, stride=0x40000, CNTFRQ=19200000.
- The counter conversion uses the firmware CNTFRQ; the shared ARM64 patch preserves the Stage-6 timer correction. The source-kernel suite also executes the real RAMBlock link section with an alternate archive directory.

### Completion criteria

- Every enabled CPU reaches its entry checkpoint and timer/IPI handling works on each enabled CPU.

### Current limits

- SMP, other CPU affinities and CPU frequency control are unverified.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [Eight source-kernel regressions and production ARM64 assembly checks passed; the enabled CPU0 code linked successfully. IRQ/IPI behavior was not tested on hardware.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-kernel-source.py; test-stage6.py; ./unixnu kernel -j 3</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Memory and DMA {#memory}

`host-verified`

Components: `Sm8750Platform`.

### Hardware and dependencies

- The captured live DT has contiguous RAM at 0x980000000 of size 0x180000000. DarwinBoot requests 0x100000000 and rewrites chosen dram-base/size to its actual allocation.

### Configuration

- payload.dram\_size=0x180000000; payload.adt.owned\_memory\_size=0x100000000.

### Procedure

- Run check-memory-map.py against the captured live.dtb and current UEFI map; inspect actual DarwinBoot allocation before allowing more RAM or DMA.

### Development findings

- The captured live DT has contiguous RAM at 0x980000000 of size 0x180000000. DarwinBoot requests 0x100000000 and rewrites chosen dram-base/size to its actual allocation.
- GIC register offsets remain unchanged; the common ADT converter rounds the arm-io window to 16 MiB. UFS uses the extracted SM8750 SMMU operations.

### Completion criteria

- All handoff regions are accounted for and each enabled DMA engine accesses its allocated range with the documented cache policy.

### Current limits

- Only the selected contiguous arena is handed to XNU; allocation size is a request. Other devices need their own RAM/reservation evidence.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [DTS/ADT memory and GIC contracts and RAMBlock handoff address bounds passed on the host. DMA behavior was not tested on hardware.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-format2.py; test-stage6.py</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Storage and root {#storage}

`host-verified`

Components: `NativeUfs`, `NativeVirtioBlock`, `NativeRamBlock`, `PaganiVerifiedRoot`.

### Hardware and dependencies

- The physical root is unixnu on LUN 0, 4096-byte LBAs 44928768..61705983; runtime UFS writes are disabled.

### Configuration

- userdata.partition=unixnu; XNU\_NATIVE\_UFS, OWNER\_ROOT\_AUDIT, TRUST\_CACHE\_RUNTIME, OWNER\_UNION\_SYSTEMOS, NATIVE\_RAM\_BLOCK enabled.

### Procedure

- Prepare UFS policy archives and original trust; prepare RAM Data on macOS; run test-stage6.py and test-root-audit.py; build the kernel.

### Development findings

- The physical root is unixnu on LUN 0, 4096-byte LBAs 44928768..61705983; runtime UFS writes are disabled.
- Read-only OS/SystemOS windows use the original APFS/AMFI path. Data is a 1 GiB RAM seed and loses changes on reboot. Virtio is an ABI dependency, not tested 13T hardware. The RAMBlock addon query is synchronized with upstream RamBlock 25c57e0e9b68bf452dd1986914525edc9d2320bc for the shared kernel ABI; missing and invalid handoff ranges are checked on the host. No GPU addon is packaged.

### Completion criteria

- Host: register, clock, link and SMMU facts match the board inventory.
- Reads: SCSI capacity, GPT and known blocks are correct; OS/SystemOS windows stay within the allocated unixnu partition.
- Root: the configured volume mounts and launchd starts. Persistence: writes survive reboot within the allocated Data region.

### Current limits

- Physical writes and persistent Data are unsupported. Never deploy the historical userdata-root.img filename to Android userdata.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [Read-only partition bounds, root-window contracts, RAM Data preparation and original storage ABI qualification passed; UFS, Virtio and RAMBlock linked successfully.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-stage6.py; test-root-audit.py; test-ram-data.py; ./unixnu kernel -j 3</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Display {#display}

`host-verified`

Components: `NativeMobileDisplay`, `PaganiPanel`.

### Hardware and dependencies

- Firmware GOP: 1216x2640, 32 bpp, stride 4864. The active AA594\_P\_7\_A0020 command-mode panel uses DSI0 at 0xae94000 and low-byte-first DCS brightness; normal max 0xdbb, initial level 0x599.

### Configuration

- Native Legacy CPU framebuffer, sheng-vsync=60 and pagani-panel=1. Board constants are in PaganiPeripherals.h.

### Procedure

- Run test-stage7.py and test-peripherals.py against the resolved tree; run build-pagani-peripherals.py --audit-only against the pinned kernelcache, then build the full display archive with XNU compiler records. After a separately authorized temporary boot, verify first brightness completion before testing slider and power transitions.

### Development findings

- PaganiPanel derives from the genuine original AppleARMBacklight class with qualified 0x370 storage and 169 virtual entries. It sends DCS 0x51/0x28/0x29 through the command FIFO and stops after a transport failure. The controller setup, DSC and panel timing stay as handed off by firmware.
- The 60 Hz software presentation timer pauses for logical display-off and resumes with a fresh deadline. Build 48 passed MMIO/timer tests, original and compiled-object ABI checks, full kernel link and prelink. PaganiPanel symbols are present in the downloaded kernel; physical display control remains unverified.

### Completion criteria

- A known frame appears with correct geometry and colors, then userland presentation and backlight controls work.

### Current limits

- No new hardware verification, physical refresh measurement, TE/vblank synchronization, high-refresh switching, brightness calibration, fade interpolation or full panel suspend/resume. CoreBrightness property access and coexistence with firmware scanout remain unverified.

### Next action

- Assemble a boot image from the build-48 kernel and matching inputs, then temporarily boot after authorization. Compare UI output with build 39, test brightness within the normal range, blank/unblank and inspect command errors. Measure TE cadence before adding physical refresh control.

### Evidence

- `host`: [Display timer and DSI transport checks passed. The owned panel object passed the original/compiled ABI audit, entered the display archive and linked into the prelinked kernel; no hardware result claimed.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40183635568)
  Command: <code>test-stage7.py; test-peripherals.py; ./unixnu kernel -j 3</code>. Revision: `e6435b2bda6e91431f3bc0c442d1ff9c47be1451`.
- `source`: [DSI command transport, genuine backlight ancestry, timer PM integration and explicit runtime limits; not a complete-kernel or device result.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/drivers/PaganiPanel/README.md)
- `host`: [Display lifecycle checks and original Legacy ABI qualification passed; the CPU presentation archive compiled and linked. The migrated framebuffer has not been observed on hardware.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-stage7.py; ./unixnu kernel -j 3</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Input {#input}

`host-verified`

Components: `PaganiInput`, `QcomGeniSpi`.

### Hardware and dependencies

- S3910 HBP on GENI SPI QUP1 S4 at 0xa90000, wrapper 0xac0000, mode 0, maximum 19 MHz. GPIOs 48..51 carry SPI; reset 161 and ATTN 162 are active low. TLMM GPIO windows include the SM8750 0x100000 offset.
- SPMI v7 observer: PON SID 0 peripheral 0x13, bits 7/6 = Power/Volume Up; GPIO SID 1 peripheral 0x8d bit 0 active low = Volume Down.

### Configuration

- pagani-touch=1 and pagani-buttons=1 select two original-ABI HID devices. Board wiring and timing are defined in PaganiPeripherals.h.

### Procedure

- Run test-peripherals.py --tree &lt;resolved-tree&gt; and the original HID ABI audit; build the full archive. On hardware first check SPI protocol, ID/application status and LBP/configuration acknowledgements, then single contacts, coordinate orientation, simultaneous contacts, releases and physical key holds.

### Development findings

- The stock HBP driver exposes an LBP fallback through dynamic config 0x11=2. The new transport drains queued raw frames, installs and reads back a custom report format, filters palms and emits explicit releases with stable IDs. TouchComm CRC command suffixes are detected from identification.
- PMIC reads are bounded and keys use 32 ms debounce. Failed key reads release all keys; touch transport failure releases held contacts and disables further touch reads. ATTN is polled at 8 ms, keys at 16 ms. Build 48 passed portable tests, the original 207-slot HID and compiled-object ABI audits, C/C++ compilation, final link and prelink. Input, S3910 and GENI SPI symbols are present in the downloaded kernel; hardware input remains unverified.

### Completion criteria

- Touches map to the display, simultaneous contacts keep stable IDs, release events arrive, and buttons produce expected HID reports.

### Current limits

- Retained regulator and GENI firmware state is required. No RPMh cold rail enable, GIC IRQ path, GPI DMA, TouchComm v2, oversized-command fragmentation, firmware recovery or system-sleep wake. Hardware must confirm LBP availability, FIFO access, coordinates and event routing. System edge gestures are not implemented.

### Next action

- Assemble the build-48 boot image, then capture the first S3910 identification and application response during an authorized temporary boot. Use the reported failure stage to choose rail/microcode, FIFO or protocol work; do not treat display output as touch verification.

### Evidence

- `host`: [TouchComm, contact lifecycle, key debounce/failure, SPMI and GENI tests passed. The HID object passed ABI checks and the S3910/GENI sources compiled as C; all three objects linked into the prelinked kernel. Hardware input remains untested.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40183635568)
  Command: <code>test-peripherals.py; test-stage7.py; ./unixnu kernel -j 3</code>. Revision: `e6435b2bda6e91431f3bc0c442d1ff9c47be1451`.
- `source`: [Physical HID adapters, bounded TouchComm/SPMI paths, contact and key lifecycle tests; no hardware result claimed.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/drivers/PaganiInput/README.md)

## GPU {#gpu}

`planned`

### Hardware and dependencies

- Adreno 830/SM8750 differs from the catalog A740/SM8550 implementation; no GPU component is selected.

### Configuration

- No driver is enabled for this subsystem.

### Procedure

- Follow the subsystem guide to identify hardware, firmware and ABI inputs before enabling a component.

### Development findings

- Adreno 830/SM8750 differs from the catalog A740/SM8550 implementation; no GPU component is selected.

### Completion criteria

- GPU submissions complete, mapping/fence handling is correct, a known image renders, and the selected Metal client presents it.

### Current limits

- No implemented or hardware-verified path is available for this subsystem.

### Next action

- Capture SM8750 GPU/GMU resources and the firmware manifest; qualify a matching implementation before enabling GPU components.

### Evidence

Collect evidence using the next action.

## Network {#network}

`planned`

### Hardware and dependencies

- No verified Wi-Fi or cellular path is selected for PKX110.

### Configuration

- No driver is enabled for this subsystem.

### Procedure

- Follow the subsystem guide to identify hardware, firmware and ABI inputs before enabling a component.

### Development findings

- No verified Wi-Fi or cellular path is selected for PKX110.

### Completion criteria

- Scanning, association, address configuration and sustained receive/transmit traffic pass with the selected firmware.

### Current limits

- No implemented or hardware-verified path is available for this subsystem.

### Next action

- Identify the WLAN transport, firmware and IRQ/DMA requirements from the stock DTB before selecting a driver.

### Evidence

Collect evidence using the next action.

## Audio {#audio}

`planned`

### Hardware and dependencies

- DSP/codec firmware interfaces, routes and playback/capture remain unimplemented.

### Configuration

- No driver is enabled for this subsystem.

### Procedure

- Follow the subsystem guide to identify hardware, firmware and ABI inputs before enabling a component.

### Development findings

- DSP/codec firmware interfaces, routes and playback/capture remain unimplemented.

### Completion criteria

- Each declared route carries the selected audio format through playback or capture and handles start/stop correctly.

### Current limits

- No implemented or hardware-verified path is available for this subsystem.

### Next action

- Map the DSP, codec, clock and firmware dependencies from the stock platform before implementing an audio route.

### Evidence

Collect evidence using the next action.

## Power {#power}

`host-verified`

Components: `ShengPlatformPower`.

### Hardware and dependencies

- The component preserves the original Apple PMGR ABI and restricted boot-time operations.

### Configuration

- XNU\_SHENG\_PMGR=y; sheng-pmgr=2; sleep-disabled=1.

### Procedure

- Build the compatibility PMGR archive against the exact kernelcache; observe boot transitions before changing any firmware-owned vote.

### Development findings

- The existing PMGR compatibility behavior is unchanged. PaganiInput adds QUP1 S4 clock votes and GPIO reset while retaining firmware regulator state. PaganiPanel requests display blanking without turning off rails or replacing full suspend/resume.
- Battery, charging, suspend and frequency scaling remain unverified.

### Completion criteria

- Declared power controls and battery readings match observations; each claimed sleep/resume transition restores its dependent peripherals.

### Current limits

- Battery, charging, sleep and full regulator/clock control are unsupported.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [The PMGR builder qualified the pinned original imports/provider dispatch and linked its archive. Physical power transitions, battery and frequency control remain unverified.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>./unixnu kernel -j 3</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Image assembly {#packaging}

`host-verified`

Components: `NativePrelink`.

### Hardware and dependencies

- Kernel ABI and IPSW stay at 21H461. NativePrelink stays pinned in the community bundle; the OS series selects its matching patches.

### Configuration

- kernel.prelink roots preserve the previously qualified subset; payload.adt.root\_match\_volume\_uuid=04769D26-2CA0-4825-B443-E2E5F88300F2.

### Procedure

- Run ipsw fetch, kernel, build package, firmware, payload, bootimg in order; inspect payload.json and the ADT in FAT.

### Development findings

- Kernel ABI and IPSW stay at 21H461. NativePrelink stays pinned in the community bundle; the OS series selects its matching patches.
- Four byte-qualified userspace fixups now come from the shared OS profile and common validated-page implementation. Board root UUID and PackedRAMDisk/TrustCache placeholders are preserved.
- Android boot v4 packages gzip\(BootShim+3 MiB FD+160 MiB FAT\) followed by all stock DTBs; Android ramdisk remains empty.

### Completion criteria

- Kernel and prelink succeed, payload contains all declared inputs, and the boot image matches the bootloader format and size limit.

### Current limits

- Host construction does not establish runtime equivalence to build 39.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [Kernel/prelink, 160 MiB FAT payload and 87928832-byte Android v4 boot image succeeded. Extracted ADT and payload kernel hashes match the reports; no GPU addon is supplied.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/MIGRATION.md)
  Command: <code>./unixnu kernel -j 3; ./unixnu build package; ./unixnu firmware; ./unixnu payload; ./unixnu bootimg</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

## Diagnostics {#diagnostics}

`host-verified`

Components: `BootProgress`, `Stage7Probe`.

### Hardware and dependencies

- Early framebuffer, root checkpoints and panic rendering remain enabled. Source-only diagnostics are separate from the prelinked userspace kernel.

### Configuration

- XNU\_BOOT\_PROGRESS=y; XNU\_EARLY\_BOOT\_PROGRESS=y; optional native-root-audit triggers the binding report.

### Procedure

- Run the panic/ADT/boot container checks; preserve old run artifacts; capture the last completed checkpoint in a future explicitly authorized temporary boot.

### Development findings

- Early framebuffer, root checkpoints and panic rendering remain enabled. Source-only diagnostics are separate from the prelinked userspace kernel.
- Build 39 reached the iPadOS UI; see STAGE6.md. The complete driver archive plan passes on macOS. The early boot-logo builder now reads the IPSW from the format-2 output root and compiles successfully. A host regression checks that the final link selects the RAMBlock object beside the configured archive; the final kernel/prelink and local firmware, payload and boot-image assembly have now passed; the image remains untested on hardware.

### Completion criteria

- Each claimed milestone has a reproducible command, a matching source/image revision and an accessible result.

### Current limits

- No device command is issued by the migration. Older observations are identified separately.

### Next action

- Temporarily boot the exact migrated image only after separate authorization; record its image/source revisions and compare the affected runtime checkpoints with build 39.

### Evidence

- `host`: [Source-kernel regressions, early console compilation and panic-entry contract passed in CI; runtime checkpoints still require a hardware boot.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/actions/runs/40174199304)
  Command: <code>test-kernel-source.py; drivers/BootProgress/Test/panic-entry.py</code>. Revision: `af2ba2975a5f0f6dcedfdcc719a1c646876f5804`.
- `source`: [Current format-2 selection and board policy.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/board.toml)
- `source`: [Historical PKX110 observations through build 39; no claim of migrated-image hardware verification.](https://github.com/IssacL04/UniXNU-Pagani_SM8750/blob/fcab49e2ea2f9ddfaa08ee6e8109ba7ed67029ac/STAGE6.md)

</div>
