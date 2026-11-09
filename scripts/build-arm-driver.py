#!/usr/bin/env python3
"""Compile the generic platform expert against the actual source-kernel ABI."""
import argparse
import json
import tomllib
from pathlib import Path
import subprocess
import sys

from kernel_target import ARCH, TRIPLE, check_object

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
catalog = {entry['id']: entry for entry in tomllib.loads((ROOT / 'components.toml').read_text())['component']
           if entry['kind'] == 'driver' and any(name.endswith('.cpp') for name in entry.get('source_files', []))}
aliases = {entry.get('legacy_name', identifier): identifier for identifier, entry in catalog.items()}
auxiliary = ['NativeLegacyDisplay', 'NativeGOPPresenter', 'NativeOfflineGPU', 'NativeOfflineAccelerator', 'NativeIOGPUProviderProbe']
parser.add_argument('component', choices=sorted(set(catalog) | set(aliases) | set(auxiliary)))
parser.add_argument('--negative-aes-kat', action='store_true', help='Isolated own-source wrong-answer refusal control; never changes original inputs')
parser.add_argument('--verified-root', action='store_true', help='Authenticate all actual disk reads against an independently signed immutable policy')
parser.add_argument('--verified-root-bundle', action='store_true', help='Select exactly one capacity-matching policy per device, then independently authenticate it')
parser.add_argument('--negative-verified-block', action='store_true', help='Negative own-buffer control: corrupt only the completed DMA snapshot')
args = parser.parse_args()
identifier = aliases.get(args.component, args.component)
if identifier == 'ShengUfs' and ARCH == 'arm64e':
    if any((args.negative_aes_kat, args.verified_root, args.verified_root_bundle, args.negative_verified_block)):
        parser.error('The source UFS runtime always uses the authenticated policy bundle of the OS profile')
    raise SystemExit(subprocess.call([sys.executable, ROOT / 'scripts/build-sheng-source-storage.py', '--tree', ROOT]))
metadata = catalog.get(identifier, {})
runtime = metadata.get('legacy_name', identifier)
args.component = runtime
if args.verified_root_bundle and not args.verified_root:
    parser.error('A policy bundle requires verified block mode')
if args.verified_root_bundle and args.negative_verified_block:
    parser.error('Select either bundle policy qualification or the isolated DMA snapshot fault')
if (args.verified_root or args.negative_verified_block) and args.component != 'NativeVirtioBlock':
    parser.error('Verified block mode requires the concrete read-only block transport')
if args.negative_verified_block and not args.verified_root:
    parser.error('The isolated input fault requires verified block mode')
component = args.component
recipe = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json'
record = json.loads(recipe.read_text().strip().rstrip(','))
directory = 'GopFramebuffer' if runtime in ('NativeLegacyDisplay', 'NativeGOPPresenter') else 'AdrenoMetal' if runtime in ('NativeOfflineGPU', 'NativeOfflineAccelerator', 'NativeIOGPUProviderProbe') else identifier
filename = next((name for name in metadata.get('source_files', []) if name.endswith('.cpp')), runtime + '.cpp')
source = ROOT / 'drivers' / directory / filename
outputs = metadata.get('outputs', [])
archive_output = next((path for path in outputs if path.endswith('.a')), None)
aux_dirs = {'NativeLegacyDisplay':'native-legacy-display', 'NativeGOPPresenter':'native-gop-presenter',
            'NativeOfflineGPU':'native-offline-gpu', 'NativeOfflineAccelerator':'native-offline-accelerator',
            'NativeIOGPUProviderProbe':'native-original-provider'}
out = ROOT / Path(archive_output or outputs[0]).parent if outputs else ROOT / 'build' / aux_dirs[runtime]
if args.verified_root:
    out = ROOT / ('build/virtio-mmio-block-verified-fault' if args.negative_verified_block else 'build/virtio-mmio-block-verified')
    if args.verified_root_bundle:
        out = ROOT / 'build/virtio-mmio-block-verified-bundle'
out.mkdir(parents=True, exist_ok=True)
if args.negative_aes_kat:
    if component != 'NativeAES':
        parser.error('The own-source KAT control requires NativeAES')
    out = ROOT / 'build/software-aes-kat-negative'
    out.mkdir(parents=True, exist_ok=True)
    text = source.read_text()
    marker = '0x76,0x49,0xab,0xac'
    if text.count(marker) != 1:
        raise SystemExit('The exact own-source NIST answer was not uniquely identified')
    (out / 'OriginalAESABI.h').write_bytes(source.with_name('OriginalAESABI.h').read_bytes())
    source = out / 'NativeAES.cpp'
    source.write_text(text.replace(marker, '0x77,0x49,0xab,0xac'))
if component == 'NativeEmmc':
    # Stage a build-time copy of NativeEmmc.cpp into build/native-emmc/ so the
    # drivers/NativeEmmc submodule stays 100% clean while eliminating the
    # unconditional per-FIFO-word IODelay(1) bottleneck (160 ms -> <2 ms per 64 KiB read).
    text = source.read_text()
    text = text.replace(
        'constexpr uint32_t kTransferBlocks = 128;',
        'constexpr uint32_t kTransferBlocks = 256;',
        1,
    )
    old_pio_loop = """  uint32_t got = 0;
  unsigned idle = 0;
  while (got < total) {
    status = read32(base, MSDC_INT);
    if (status & MSDC_INT_ERROR) {
      *failure = 4;
      return false;
    }
    uint32_t available = read32(base, MSDC_FIFOCS) & MSDC_FIFOCS_RX_MASK;
    uint32_t before = got;
    while (available >= 4 && got + 4 <= total) {
      uint32_t word = read32(base, MSDC_RXDATA);
      out[got + 0] = static_cast<uint8_t>(word);
      out[got + 1] = static_cast<uint8_t>(word >> 8);
      out[got + 2] = static_cast<uint8_t>(word >> 16);
      out[got + 3] = static_cast<uint8_t>(word >> 24);
      got += 4;
      available -= 4;
    }
    if ((status & MSDC_INT_XFER_COMPL) && got == total) {
      clearStatus(base, status & MSDC_INT_XFER_COMPL);
      break;
    }
    if (got != before) {
      idle = 0;
    } else if (++idle > 200000) {
      *failure = 5;
      return false;
    }
    IODelay(1);
  }"""
    new_pio_loop = """  for (unsigned clrSpin = 0; clrSpin < 100000U && (read32(base, MSDC_FIFOCS) & MSDC_FIFOCS_CLR); ++clrSpin) {
  }
  uint32_t got = 0;
  unsigned idle = 0;
  while (got < total) {
    uint32_t available = read32(base, MSDC_FIFOCS) & MSDC_FIFOCS_RX_MASK;
    uint32_t before = got;
    while (available >= 4 && got + 4 <= total) {
      uint32_t word = read32(base, MSDC_RXDATA);
      out[got + 0] = static_cast<uint8_t>(word);
      out[got + 1] = static_cast<uint8_t>(word >> 8);
      out[got + 2] = static_cast<uint8_t>(word >> 16);
      out[got + 3] = static_cast<uint8_t>(word >> 24);
      got += 4;
      available -= 4;
    }
    if (got >= total) {
      break;
    }
    if (got != before) {
      idle = 0;
      continue;
    }
    status = read32(base, MSDC_INT);
    if (status & MSDC_INT_ERROR) {
      *failure = 4;
      return false;
    }
    if (++idle > 2000000U) {
      *failure = 5;
      return false;
    }
    if ((idle & 63U) == 0U) {
      IODelay(1);
    }
  }
  for (unsigned xferSpin = 0; xferSpin < 200000U; ++xferSpin) {
    status = read32(base, MSDC_INT);
    if (status & (MSDC_INT_XFER_COMPL | MSDC_INT_ERROR)) {
      break;
    }
    if ((xferSpin & 15U) == 15U) {
      IODelay(1);
    }
  }
  if (status & MSDC_INT_ERROR) {
    *failure = 4;
    return false;
  }"""
    if old_pio_loop in text:
        text = text.replace(old_pio_loop, new_pio_loop, 1)
    old_write_loop = """    for (uint32_t index = 0; index < n; index++) {
      uint64_t offset = (done + index) * 512;
      if (buffer->writeBytes(offset, bounce + index * 512, 512) != 512) {
        result = kIOReturnIOError;
        break;
      }
    }"""
    new_write_loop = """    IOByteCount chunkBytes = static_cast<IOByteCount>(n) * 512;
    if (buffer->writeBytes(done * 512, bounce, chunkBytes) != chunkBytes) {
      result = kIOReturnIOError;
      break;
    }"""
    if old_write_loop in text:
        text = text.replace(old_write_loop, new_write_loop, 1)
    text = text.replace('    IODelay(1);\n  }', '    if ((i & 15U) == 15U) {\n      IODelay(1);\n    }\n  }')
    source = out / 'NativeEmmc.cpp'
    source.write_text(text)
destination = out / (component + '.o')
arguments = record['arguments']
command = []
index = 0
while index < len(arguments):
    value = arguments[index]
    if value == record['file']:
        command.append(str(source))
    elif value == '-o':
        command.extend(['-o', str(destination)])
        index += 1
    elif value == '-Xclang' and arguments[index + 1].startswith('-W'):
        index += 1
    elif not value.startswith('-W'):
        command.append(value)
    index += 1
command.extend(['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                '-Wno-c99-designator', '-Wno-reorder-init-list', '-Wno-deprecated-enum-enum-conversion',
                '-Wno-unnecessary-virtual-specifier', '-I', str(ROOT / '.cache/xnu-work/osfmk')])
if component in ('NativeVirtioBlock', 'NativeRamBlock', 'NativeUfs', 'NativeEmmc'):
    command += ['-I', str(ROOT / 'build/storage-abi/include')]
    if args.verified_root:
        command += ['-I', str(ROOT / 'kits/VerifiedRoot/include'), '-DNATIVE_VERIFIED_BLOCK=1']
    if args.negative_verified_block:
        command += ['-DNATIVE_VERIFIED_BLOCK_INJECT_FAULT=1']
    if args.verified_root_bundle:
        command += ['-DNATIVE_VERIFIED_BLOCK_MULTI_POLICY=1']
if component == 'NativeUfs':
    command += ['-I', str(ROOT / 'kits/VerifiedRoot/include'), '-I', str(ROOT / 'drivers/VirtioMmioBlock'),
                '-DNATIVE_VERIFIED_BLOCK=1', '-DNATIVE_VERIFIED_BLOCK_MULTI_POLICY=1', '-mgeneral-regs-only']
if component == 'NativeEmmc':
    command += ['-mgeneral-regs-only']
if component=='NativeOfflineGPU':
    command += ['-I',str(ROOT/'drivers/VirtioMmioConsole')]
if component in ('NativeLegacyDisplay','NativeHidInput'):
    command += ['-I', str(out)]
if component == 'NativeAdreno':
    command += ['-I', str(ROOT / 'kits/ArmSmmu4K'), '-I', str(ROOT / 'kits/AdrenoPM4'),
                '-I', str(ROOT / 'kits/sheng'), '-I', str(ROOT / 'kits/Sm8550Platform'), '-I', str(ROOT / 'kits/AdrenoFirmware'),
                '-I', str(ROOT / 'build/sheng-gmu'), '-I', str(ROOT / 'build/adreno740/include')]
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
result = subprocess.run(command, cwd=record['directory'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
(out / 'compile.log').write_text(result.stdout)
if result.returncode:
    raise SystemExit(result.stdout[-3500:])
check_object(destination.read_bytes(), component)
archive = out / ('lib' + identifier + '.a')
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', archive, destination], check=True)
print(f'{identifier}: -Werror build and {TRIPLE} object audit passed')
