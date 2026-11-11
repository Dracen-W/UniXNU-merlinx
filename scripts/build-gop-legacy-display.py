#!/usr/bin/env python3
"""Build the merlinx GOP legacy display + FocalTech FT8719 + IOHIDDevice archive.

Uses the original IOMobileFramebufferLegacy ABI (scripts/data/merlinx-legacy-display-abi.json)
and IOHIDDevice ABI (scripts/data/merlinx-hid-device-abi.json) extracted from the prelinked
iPadOS 17.7.11 kernel.
"""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/native-legacy-display'
OUT.mkdir(parents=True, exist_ok=True)
FTS_DIR = ROOT / 'drivers/FocalTechTouch'
sys.path.insert(0, str(ROOT / 'kits/XnuPrelink'))
from native_prelink import macho


def run(command, **kwargs):
    return subprocess.check_output([str(x) for x in command], timeout=120, **kwargs)


def require(value, why):
    if not value:
        raise SystemExit(why)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def extract_vtable_and_undef(obj_path: Path, vtable_sym: str, slot_count: int):
    raw = obj_path.read_bytes()
    image = macho(raw)
    st = next(p for c, p, _ in image['commands'] if c == 2)
    sy, count, strings, _ = struct.unpack_from('<4I', raw, st + 8)
    symbols = []
    for i in range(count):
        ix, kind, section, _, va = struct.unpack_from('<IBBHQ', raw, sy + i * 16)
        symbols.append((raw[strings + ix:raw.index(b'\0', strings + ix)].decode(), va, kind))
    vtable = next(va for name, va, _ in symbols if name == vtable_sym)
    relocations = {}
    for segment in image['segments']:
        for section in segment['sections']:
            if section['va'] <= vtable < section['va'] + section['size']:
                ro, rc = struct.unpack_from('<2I', raw, section['command_offset'] + 56)
                for i in range(rc):
                    offset, bits = struct.unpack_from('<2I', raw, ro + i * 8)
                    if bits >> 27 & 1 and bits >> 28 == 0 and bits >> 25 & 3 == 3:
                        relocations[section['va'] + offset] = symbols[bits & 0xffffff][0]
    compiled = [relocations.get(vtable + 16 + i * 8) for i in range(slot_count)]
    require(all(compiled), f'Exact owned {slot_count}-entry vtable for {vtable_sym}')
    return symbols, compiled


def generate_firmware_c(fw_bin: Path, out_c: Path):
    raw = fw_bin.read_bytes()
    lines = [
        '#include <stdint.h>',
        f'const uint32_t fts_board_firmware_size = {len(raw)}u;',
        'const uint8_t fts_board_firmware[] = {',
    ]
    for i in range(0, len(raw), 16):
        chunk = ', '.join(f'0x{b:02x}' for b in raw[i:i + 16])
        lines.append(f'    {chunk},')
    lines.append('};\n')
    out_c.write_text('\n'.join(lines))


abi = json.loads((ROOT / 'scripts/data/merlinx-legacy-display-abi.json').read_text())
pure = abi['pure']
methods = [int(value, 16) for value in abi['methods']]
legacy_vptr = int(abi['legacy_vptr'], 16)
require(len(methods) == 358, 'merlinx legacy ABI needs exactly 358 methods')

hid_abi = json.loads((ROOT / 'scripts/data/merlinx-hid-device-abi.json').read_text())
hid_methods = [int(value, 16) for value in hid_abi['methods']]
hid_vptr = int(hid_abi['hid_vptr'], 16)
require(len(hid_methods) == 207, 'merlinx HID ABI needs exactly 207 methods')

header = '''// SPDX-License-Identifier: BSD-2-Clause-Patent
// Opaque ABI storage for the exact original class, not source-level access
// to its private fields. The actual original metadata supplies its full
// Framebuffer/Service/IOService ancestry. Only qualified base prototypes may
// be called from source; opaque entries are original dispatch targets only.
#pragma once
#include <IOKit/IOService.h>
#include <libkern/libkern.h>
struct OriginalLegacySwapRequest;
struct OriginalLegacyStateRequest {uint32_t skipRequested,requested,skipPowered,powered;uint8_t changed;uint8_t pad[11];};
static constexpr unsigned kLegacyVsyncClients=5592;
static constexpr unsigned kRequestRect0=17,kRequestRect1=81,kRequestMasks=792,kRequestOptions=804,kRequestDims=816,kRequestSurfaces=2624;
#define NATIVE_LEGACY_LEAVES(LEAF) LEAF(211) LEAF(245) LEAF(246) LEAF(306) LEAF(307) LEAF(338) LEAF(345)
class IOMobileFramebufferLegacy : public IOService {
 OSDeclareAbstractStructors(IOMobileFramebufferLegacy)
 unsigned char originalPrivate[0x18a0-sizeof(IOService)];
public:
 void installBootDimensions(uint32_t width,uint32_t height) {
  *reinterpret_cast<uint32_t *>(reinterpret_cast<unsigned char *>(this)+5808)=width;
  *reinterpret_cast<uint32_t *>(reinterpret_cast<unsigned char *>(this)+5812)=height;
 }
 const OriginalLegacySwapRequest *pendingSwapRequest() const {
  const OriginalLegacySwapRequest *request=nullptr;
  memcpy(&request,reinterpret_cast<const unsigned char *>(this)+5480,sizeof(request));return request;
 }
 bool activateOnlyPendingSwap(const OriginalLegacySwapRequest *request) {
  auto self=reinterpret_cast<unsigned char *>(this);
  auto bytes=reinterpret_cast<const unsigned char *>(request);
  const void *current=nullptr,*head=nullptr,*tail=nullptr,*next=nullptr,*previous=nullptr;
  memcpy(&current,self+5472,8);memcpy(&head,self+5480,8);memcpy(&tail,self+5488,8);
  if(!request || current || head!=request)return false;
  memcpy(&next,bytes+2960,8);memcpy(&previous,bytes+2968,8);
  if(previous!=self+5480)return false;
  if(next) {
   const void *link=self+5480;
   memcpy(const_cast<unsigned char *>(reinterpret_cast<const unsigned char *>(next))+2968,&link,8);
  } else if(tail!=bytes+2960)return false;
  memcpy(self+5472,&request,8);
  if(next) {memcpy(self+5480,&next,8);}
  else {const void *empty=nullptr,*emptyTail=self+5480;memcpy(self+5480,&empty,8);memcpy(self+5488,&emptyTail,8);}
  return true;
 }
 void free() override;
 bool init(OSDictionary *properties=nullptr) override;
 IOReturn setProperties(OSObject *) override;
 void systemWillShutdown(IOOptionBits) override;
 IOService *probe(IOService *,SInt32 *) override;
 bool start(IOService *) override;
 void stop(IOService *) override;
 IOWorkLoop *getWorkLoop() const override;
 IOReturn newUserClient(task_t,void *,UInt32,IOUserClient **) override;
 IOReturn setPowerState(unsigned long,IOService *) override;
'''
for i in range(168, 358):
    if i == 184:
        header += ' virtual bool isMainDisplay();\n'
    elif i == 188:
        header += ' virtual IOReturn getDotPitch(uint32_t *);\n'
    elif i == 213:
        header += ' virtual IOReturn supportedFrameInfo(uint32_t,uint32_t *,char *,uint32_t *)=0;\n'
    elif i == 249:
        header += ' virtual void getDisplayDimensions(uint32_t *,uint32_t *);\n'
    elif i == 290:
        header += ' virtual void kickSwapQueue();\n'
    elif i == 296:
        header += ' virtual bool validateSwap(const OriginalLegacySwapRequest *);\n'
    elif i == 295:
        header += ' virtual void completeCurrentSwap();\n'
    elif i == 337:
        header += ' virtual IOReturn collectCompletionReports(uint32_t,void *,uint32_t *,uint32_t)=0;\n'
    elif i == 288:
        header += ' virtual bool startHardware();\n'
    elif i == 354:
        header += ' virtual int applyStateRequest(OriginalLegacyStateRequest *,bool);\n'
    elif i == 301:
        header += ' virtual bool getRefreshAttributes(uint64_t *,bool);\n'
    elif i in pure:
        header += f' virtual __attribute__((noreturn)) void opaque{i}()=0;\n'
    else:
        header += f' virtual __attribute__((deprecated("Unclassified ABI: never call from source"))) void opaque{i}();\n'
header += '''};
static_assert(sizeof(IOMobileFramebufferLegacy)==0x18a0,"Actual original instance extent");

// Override pixel copy routines on merlinx so every pixel stored to the active MT6768 OVL0
// foreground layer (L3_ADDR) carries opaque Alpha = 0xFF (preventing OVL per-pixel alpha tinting),
// and bound per-frame diagnostic scans after the first 2 frames to eliminate CPU stalls.
extern "C" volatile uint32_t native_legacy_frame_count;
extern "C" volatile void *merlinx_get_active_scanout_va(volatile void *default_dst);

static inline int merlinx_fast_frame_scan(const void *source, uint64_t sourceBytes,
    uint64_t sourceStride, uint32_t width, uint32_t height, uint64_t out[5]) {
    if (native_legacy_frame_count < 2) {
        return native_legacy_frame_scan(source, sourceBytes, sourceStride, width, height, out);
    }
    out[0]=1; out[1]=0; out[2]=0; out[3]=width?width-1:0; out[4]=height?height-1:0;
    return 1;
}

static inline int merlinx_fast_frame_colors(const void *source, uint64_t sourceBytes,
    uint64_t sourceStride, uint32_t width, uint32_t height,
    uint32_t colors[4], uint64_t counts[4], uint64_t *other) {
    if (native_legacy_frame_count < 2) {
        return native_legacy_frame_colors(source, sourceBytes, sourceStride, width, height, colors, counts, other);
    }
    for (unsigned i=0;i<4;++i){colors[i]=0;counts[i]=0;}
    *other=0;
    return 1;
}

static inline int merlinx_opaque_copy_pixels(volatile void *destination,
    uint64_t destinationBytes, uint64_t destinationStride, const void *source,
    uint64_t sourceBytes, uint64_t sourceStride, uint32_t width, uint32_t height) {
    if (!native_legacy_copy_layout(destination,destinationBytes,destinationStride,
        source,sourceBytes,sourceStride,width,height)) return 0;
    volatile void *activeDst = merlinx_get_active_scanout_va(destination);
    uintptr_t dst=(uintptr_t)activeDst, src=(uintptr_t)source;
    int wide=!(dst&7) && !(destinationStride&7);
    for (uint32_t row=0;row<height;++row) {
        volatile uint32_t *out=(volatile uint32_t *)(dst+row*destinationStride);
        const unsigned char *in=(const unsigned char *)(src+row*sourceStride);
        uint32_t column=0;
        if (wide) {
            volatile uint64_t *out64=(volatile uint64_t *)out;
            for (;column+8<=width;column+=8) {
                uint64_t a,b,c,d;
                __builtin_memcpy(&a,in+(uint64_t)column*4,8);__builtin_memcpy(&b,in+(uint64_t)column*4+8,8);
                __builtin_memcpy(&c,in+(uint64_t)column*4+16,8);__builtin_memcpy(&d,in+(uint64_t)column*4+24,8);
                out64[column/2]=a | 0xFF000000FF000000ULL;
                out64[column/2+1]=b | 0xFF000000FF000000ULL;
                out64[column/2+2]=c | 0xFF000000FF000000ULL;
                out64[column/2+3]=d | 0xFF000000FF000000ULL;
            }
        }
        for (;column<width;++column) {
            uint32_t value;
            __builtin_memcpy(&value,in+(uint64_t)column*4,4);
            out[column]=value | 0xFF000000U;
        }
    }
    return 1;
}

static inline uint64_t merlinx_opaque_copy_diff(volatile void *destination,
    uint64_t destinationBytes, uint64_t destinationStride, const void *source,
    uint64_t sourceBytes, uint64_t sourceStride, uint32_t width, uint32_t height,
    void *shadow, int valid) {
    if (!native_legacy_copy_layout(destination,destinationBytes,destinationStride,
        source,sourceBytes,sourceStride,width,height)) return UINT64_MAX;
    volatile void *activeDst = merlinx_get_active_scanout_va(destination);
    uintptr_t dst=(uintptr_t)activeDst, src=(uintptr_t)source;
    if (!shadow || ((uintptr_t)shadow&7) || (dst&7) || (destinationStride&7) || (width&1)) return UINT64_MAX;
    uint64_t stored=0;
    const uint32_t words=width/2;
    for (uint32_t row=0;row<height;++row) {
        volatile uint64_t *out=(volatile uint64_t *)(dst+row*destinationStride);
        const unsigned char *in=(const unsigned char *)(src+row*sourceStride);
        uint64_t *old=(uint64_t *)((unsigned char *)shadow+(uint64_t)row*width*4);
        uint32_t w=0;
        for (;w+8<=words;w+=8) {
            uint64_t v[8];
            __builtin_memcpy(v,in+(uint64_t)w*8,64);
            for (unsigned i=0;i<8;++i) v[i] |= 0xFF000000FF000000ULL;
            if (valid && !((v[0]^old[w])|(v[1]^old[w+1])|(v[2]^old[w+2])|(v[3]^old[w+3])|
                           (v[4]^old[w+4])|(v[5]^old[w+5])|(v[6]^old[w+6])|(v[7]^old[w+7]))) continue;
            for (unsigned i=0;i<8;++i){out[w+i]=v[i];old[w+i]=v[i];}
            stored+=64;
        }
        for (;w<words;++w) {
            uint64_t v;
            __builtin_memcpy(&v,in+(uint64_t)w*8,8);
            v |= 0xFF000000FF000000ULL;
            if (valid && v==old[w]) continue;
            out[w]=v;old[w]=v;stored+=8;
        }
    }
    return stored;
}

#define native_legacy_frame_scan   merlinx_fast_frame_scan
#define native_legacy_frame_colors merlinx_fast_frame_colors
#define native_legacy_copy_pixels  merlinx_opaque_copy_pixels
#define native_legacy_copy_diff    merlinx_opaque_copy_diff
'''
(OUT / 'OriginalLegacyFramebufferABI.h').write_text(header)

hid_header = '''// Generated from the qualified original image; opaque private storage only.
#pragma once
#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
class IOHIDDevice : public IOService {
 OSDeclareAbstractStructors(IOHIDDevice)
 unsigned char originalPrivate[0xd0-sizeof(IOService)];
public:
 using IOService::setProperty;
 IOReturn Dispatch(IORPC) override;
 void free() override;
 bool init(OSDictionary *properties=nullptr) override;
 bool setProperty(const OSSymbol *,OSObject *) override;
 bool willTerminate(IOService *,IOOptionBits) override;
 IOReturn configureReport(IOReportChannelList *,IOReportConfigureAction,void *,void *) override;
 IOReturn updateReport(IOReportChannelList *,IOReportUpdateAction,void *,void *) override;
 bool start(IOService *) override;
 void stop(IOService *) override;
 bool handleOpen(IOService *,IOOptionBits,void *) override;
 void handleClose(IOService *,IOOptionBits) override;
 bool handleIsOpen(const IOService *) const override;
 bool matchPropertyTable(OSDictionary *,SInt32 *) override;
 IOReturn message(UInt32,IOService *,void *) override;
 IOReturn newUserClient(task_t,void *,UInt32,OSDictionary *,IOUserClient **) override;
'''
for i in range(168, 207):
    if i == 171:
        hid_header += ' virtual OSString *newTransportString() const;\n'
    elif i == 173:
        hid_header += ' virtual OSString *newProductString() const;\n'
    elif i == 180:
        hid_header += ' virtual IOReturn newReportDescriptor(IOMemoryDescriptor **) const=0;\n'
    elif i == 181:
        hid_header += ' virtual IOReturn handleReport(IOMemoryDescriptor *,unsigned,IOOptionBits);\n'
    else:
        hid_header += f' virtual __attribute__((deprecated("Unqualified prototype: never call from source"))) void opaque{i}();\n'
hid_header += '};\nstatic_assert(sizeof(IOHIDDevice)==0xd0,"Original device instance size");\n'
(OUT / 'OriginalHidDeviceABI.h').write_text(hid_header)

fw_c = OUT / 'fts_firmware_blob.c'
generate_firmware_c(FTS_DIR / 'firmware/focaltech_ts_fw_xinli.bin', fw_c)

run(['python3', ROOT / 'scripts/build-arm-driver.py', 'NativeLegacyDisplay'])
run(['python3', ROOT / 'scripts/build-arm-driver.py', 'NativeGOPPresenter'])

record = json.loads((ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json').read_text().strip().rstrip(','))


def compile_cxx_with_xnu_record(src_path: Path, dst_obj: Path, extra_flags=None):
    args = record['arguments']
    command = []
    index = 0
    while index < len(args):
        value = args[index]
        if value == record['file']:
            command.append(str(src_path))
        elif value == '-o':
            command += ['-o', str(dst_obj)]
            index += 1
        elif value == '-Xclang' and args[index + 1].startswith('-W'):
            index += 1
        elif not value.startswith('-W'):
            command.append(value)
        index += 1
    command += [
        '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
        '-Wno-unnecessary-virtual-specifier', '-Wno-c99-designator',
        '-Wno-reorder-init-list', '-Wno-deprecated-enum-enum-conversion',
        '-I', str(ROOT / '.cache/xnu-work/osfmk'),
        '-I', str(OUT),
        '-I', str(FTS_DIR),
    ]
    if extra_flags:
        command += extra_flags
    run(command, cwd=record['directory'])


hid_obj = OUT / 'MerlinxHidInput.o'
compile_cxx_with_xnu_record(ROOT / 'package/MerlinxHidInput.cpp', hid_obj)
compile_cxx_with_xnu_record(ROOT / 'package/MerlinxLegacyDisplay.cpp', OUT / 'MerlinxLegacyDisplay.o')
compile_cxx_with_xnu_record(FTS_DIR / 'FtsProbe.cpp', OUT / 'FtsProbe.o')

obj = OUT / 'NativeLegacyDisplay.o'
symbols, compiled = extract_vtable_and_undef(obj, '__ZTV19NativeLegacyDisplay', 358)
hid_symbols, hid_compiled = extract_vtable_and_undef(hid_obj, '__ZTV14NativeHidInput', 207)

bindings = {
    '__ZN25IOMobileFramebufferLegacyC2EPK11OSMetaClass': 0xfffffff0057865cc,
    '__ZN25IOMobileFramebufferLegacyD2Ev': 0xfffffff005786618,
    '__ZN25IOMobileFramebufferLegacy10gMetaClassE': 0xfffffff006b35940,
    '__ZTV25IOMobileFramebufferLegacy': legacy_vptr - 16,
    '_native_legacy_surface_descriptor': 0xfffffff004b9ead0,
    '_native_legacy_surface_width': 0xfffffff004b9ebbc,
    '_native_legacy_surface_height': 0xfffffff004b9ebc4,
    '_native_legacy_surface_stride': 0xfffffff004b9ebd4,
    '_native_legacy_surface_allocation': 0xfffffff004b9ebfc,
    '_native_legacy_surface_format': 0xfffffff004b9ebac,
    '_native_legacy_surface_planes': 0xfffffff004b9ebf4,
    '_native_legacy_vsync_notify_original': 0xfffffff005796720,
    '_native_legacy_set_refresh_properties': 0xfffffff00579ebb8,
    '__ZN11IOHIDDeviceC2EPK11OSMetaClass': int(hid_abi['c2'], 16),
    '__ZN11IOHIDDeviceD2Ev': int(hid_abi['d2'], 16),
    '__ZN11IOHIDDevice10gMetaClassE': int(hid_abi['gmeta'], 16),
    '__ZTV11IOHIDDevice': hid_vptr - 16,
}
for i, name in enumerate(compiled):
    if name and (name.startswith('__ZN25IOMobileFramebufferLegacy') or
                 name.startswith('__ZNK25IOMobileFramebufferLegacy')):
        bindings[name] = methods[i]

hid_owned = []
for i, name in enumerate(hid_compiled):
    if name and (name.startswith('__ZN14NativeHidInput') or name.startswith('__ZNK14NativeHidInput')):
        hid_owned.append(i)
    elif name and '11IOHIDDevice' in name:
        bindings[name] = hid_methods[i]
require(hid_owned == [0, 1, 7, 14, 86, 87, 111, 171, 173, 180], f'Unexpected NativeHidInput owned slots: {hid_owned}')

undefined = {n for n, _, kind in symbols if kind == 1 and 'IOMobileFramebufferLegacy' in n}
missing = undefined - set(bindings)
require(not missing, 'Unbound original Legacy symbols: ' + ', '.join(sorted(missing)))

hid_undefined = {n for n, _, kind in hid_symbols if kind == 1 and '11IOHIDDevice' in n}
hid_missing = hid_undefined - set(bindings)
require(not hid_missing, 'Unbound original IOHIDDevice symbols: ' + ', '.join(sorted(hid_missing)))

asm = OUT / 'original-bindings.S'
asm.write_text(''.join(f'.globl {name}\n.set {name}, {value:#x}\n' for name, value in sorted(bindings.items())))
sdk = run(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-isysroot', sdk, '-Werror',
     '-c', asm, '-o', OUT / 'original-bindings.o'])

c_common = [
    'xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-isysroot', sdk,
    '-std=c99', '-O2', '-fno-builtin', '-fno-common', '-mkernel',
    '-Wall', '-Wextra', '-Werror', '-I', str(FTS_DIR),
]
run([*c_common, '-c', FTS_DIR / 'FtsProtocol.c', '-o', OUT / 'FtsProtocol.o'])
run([*c_common, '-c', FTS_DIR / 'FtsTouchCore.c', '-o', OUT / 'FtsTouchCore.o'])
run([*c_common, '-c', FTS_DIR / 'FtsHid.c', '-o', OUT / 'FtsHid.o'])
run([*c_common, '-c', fw_c, '-o', OUT / 'fts_firmware_blob.o'])

llvm = Path('/opt/homebrew/opt/llvm/bin')
if not (llvm / 'llvm-objcopy').is_file():
    llvm = Path(shutil.which('llvm-objcopy')).parent
late = OUT / 'NativeLegacyDisplay.late.o'
run([llvm / 'llvm-objcopy', '--remove-section=__DATA,__mod_init_func',
     '--globalize-symbol=__GLOBAL__sub_I_NativeLegacyDisplay.cpp', obj, late])
hid_late = OUT / 'MerlinxHidInput.late.o'
run([llvm / 'llvm-objcopy', '--remove-section=__DATA,__mod_init_func',
     '--globalize-symbol=__GLOBAL__sub_I_MerlinxHidInput.cpp', hid_obj, hid_late])
gop_obj = ROOT / 'build/native-gop-presenter/NativeGOPPresenter.o'
gop_late = OUT / 'NativeGOPPresenter.late.o'
run([llvm / 'llvm-objcopy', '--remove-section=__DATA,__mod_init_func',
     '--globalize-symbol=__GLOBAL__sub_I_NativeGOPPresenter.cpp', gop_obj, gop_late])

archive = OUT / 'libNativeLegacyDisplay.late.a'
archive.unlink(missing_ok=True)
run([
    'xcrun', 'ar', 'rcs', archive,
    late,
    hid_late,
    OUT / 'original-bindings.o',
    OUT / 'MerlinxLegacyDisplay.o',
    OUT / 'FtsProbe.o',
    OUT / 'FtsProtocol.o',
    OUT / 'FtsTouchCore.o',
    OUT / 'FtsHid.o',
    OUT / 'fts_firmware_blob.o',
    gop_late,
])
sources = [
    ROOT / 'drivers/GopFramebuffer/NativeLegacyDisplay.cpp',
    ROOT / 'drivers/GopFramebuffer/NativeLegacyRequest.h',
    ROOT / 'drivers/GopFramebuffer/NativeGOPPresenter.cpp',
    ROOT / 'drivers/GopFramebuffer/NativeGOPWire.h',
    ROOT / 'drivers/FocalTechTouch/FtsProtocol.c',
    ROOT / 'drivers/FocalTechTouch/FtsTouchCore.c',
    ROOT / 'drivers/FocalTechTouch/FtsHid.c',
    ROOT / 'drivers/FocalTechTouch/FtsProbe.cpp',
    ROOT / 'package/MerlinxHidInput.cpp',
    ROOT / 'package/MerlinxLegacyDisplay.cpp',
    ROOT / 'scripts/data/merlinx-legacy-display-abi.json',
    ROOT / 'scripts/data/merlinx-hid-device-abi.json',
    ROOT / 'scripts/build-gop-legacy-display.py',
]
(OUT / 'build-report.json').write_text(json.dumps(dict(
    source_sha256={str(p.relative_to(ROOT)): sha(p) for p in sources if p.exists()},
    generated_abi_sha256=sha(OUT / 'OriginalLegacyFramebufferABI.h'),
    generated_hid_abi_sha256=sha(OUT / 'OriginalHidDeviceABI.h'),
    compiled_object_sha256=sha(obj),
    late_object_sha256=sha(late),
    hid_late_object_sha256=sha(hid_late),
    archive_sha256=sha(archive),
    virtual_entries=358,
    hid_virtual_entries=207,
    owned_entries=[i for i, name in enumerate(compiled) if name and 'NativeLegacyDisplay' in name],
    hid_owned_entries=hid_owned,
    original_instructions_modified=False,
    display_presented=False,
    hardware_tested=False), indent=2) + '\n')
print(f'PASS: merlinx GOP legacy display + FT8719 HID archive ({len(bindings)} original bindings)')
