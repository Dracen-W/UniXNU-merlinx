#!/usr/bin/env python3
"""Build the minimal ApplePMGRNub provider used to satisfy AMFI on merlinx.

The provider has no hardware access: the iPad7,1 AppleT8011PMGR read returns
0xffffffff and its write is a no-op. This publishes the original nub class so
AMFI's ConfigurationSettings check can complete.
"""
import json
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEVICE = Path(os.environ.get('UNIXNU_DEVICE', ROOT))
OUT = ROOT / 'build/sheng-platform-power'
OUT.mkdir(parents=True, exist_ok=True)

header = '''// Generated minimal ApplePMGR ABI for the merlinx bring-up provider.
#pragma once
#include <IOKit/IOPMGR.h>
class ApplePMGR : public IOPMGR {
 OSDeclareAbstractStructors(ApplePMGR)
 unsigned char originalPrivate[0x4b8a8-sizeof(IOPMGR)];
public:
'''
guards = ''
for slot in range(177, 340):
    if slot == 320:
        header += ' virtual uint32_t readAon();\n'
    elif slot == 321:
        header += ' virtual void writeAon(uint64_t);\n'
    else:
        header += f' virtual void opaque{slot}() = 0;\n'
        guards += f' void opaque{slot}() override {{ unavailable({slot}); }}\n'
header += '};\nstatic_assert(sizeof(ApplePMGR)==0x4b8a8,"ApplePMGR ABI size");\n'
(OUT / 'OriginalPMGRABI.h').write_text(header)
(OUT / 'GuardedPMGRSlots.inc').write_text(guards)

source = ROOT / 'kits/merlinx/ShengPlatformPower.cpp'
if not source.is_file():
    source = DEVICE / 'package/ShengPlatformPower.cpp'
record = json.loads((ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))
obj = OUT / 'ShengPlatformPower.o'
command = []
arguments = record['arguments']
index = 0
while index < len(arguments):
    value = arguments[index]
    if value == record['file']:
        command.append(str(source))
    elif value == '-o':
        command += ['-o', str(obj)]
        index += 1
    elif value == '-Xclang' and arguments[index + 1].startswith('-W'):
        index += 1
    elif not value.startswith('-W'):
        command.append(value)
    index += 1
command += ['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-Wno-c99-designator',
            '-Wno-reorder-init-list', '-Wno-deprecated-enum-enum-conversion',
            '-Wno-unnecessary-virtual-specifier', '-I', str(OUT),
            '-I', str(ROOT / '.cache/xnu-work/osfmk')]
subprocess.run(command, cwd=record['directory'], check=True)

late = OUT / 'ShengPlatformPower.late.o'
subprocess.run(['llvm-objcopy', '--remove-section=__DATA,__mod_init_func',
                '--globalize-symbol=__GLOBAL__sub_I_ShengPlatformPower.cpp',
                str(obj), str(late)], check=True)

bindings = {
    '__ZN9ApplePMGRC2EPK11OSMetaClass': 0xfffffff0049f003c,
    '__ZN9ApplePMGRD2Ev': 0xfffffff0049f019c,
    '__ZN9ApplePMGR10gMetaClassE': 0xfffffff006adf490,
    '_sheng_original_pmgr_nub_create': 0xfffffff004a22568,
    '_sheng_original_pmgr_nub_read': 0xfffffff004a24adc,
    '_sheng_original_pmgr_nub_write': 0xfffffff004a230ec,
}
asm = OUT / 'original-bindings.S'
asm.write_text(''.join(f'.globl {name}\n.set {name}, {value:#x}\n' for name, value in sorted(bindings.items())))
sdk = subprocess.check_output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
bindings_obj = OUT / 'original-bindings.o'
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-isysroot', sdk,
                '-c', str(asm), '-o', str(bindings_obj)], check=True)
archive = OUT / 'libShengPlatformPower.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), str(late), str(bindings_obj)], check=True)
print('PASS: merlinx ApplePMGRNub provider archive')
