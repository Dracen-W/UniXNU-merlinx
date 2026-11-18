#!/usr/bin/env python3
"""Bind the NativeEmmc block windows to the pinned original ordinary storage ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'kits/XnuPrelink'))
import native_prelink as np

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--objects-dir', type=Path, default=ROOT / 'build/native-emmc')
args = parser.parse_args()
out = args.objects_dir
recorded = ROOT / 'scripts/data/native-emmc-original-bindings.S'
obj_path = out / 'NativeEmmc.o'
if not obj_path.is_file() or not recorded.is_file():
    raise SystemExit('NativeEmmc.o or the recorded original bindings are missing')

obj = obj_path.read_bytes()
image = np.macho(obj)
symcmd = next(offset for command, offset, length in image['commands'] if command == 2)
symoff, symcount, stroff, _ = struct.unpack_from('<4I', obj, symcmd + 8)
undefined = set()
for index in range(symcount):
    strx, kind, _, _, value = struct.unpack_from('<IBBHQ', obj, symoff + index * 16)
    if kind & 0xe0:
        continue
    name = obj[stroff + strx:obj.index(b'\0', stroff + strx)].decode()
    if 'IOBlockStorageDevice' in name:
        undefined.add(name)
recorded_names = {line.split()[1].rstrip(',') for line in recorded.read_text().splitlines()
                  if line.startswith('.globl ')}
if undefined != recorded_names:
    raise SystemExit(f'NativeEmmc storage imports changed: missing={sorted(undefined - recorded_names)} extra={sorted(recorded_names - undefined)}')
bindings = {}
for line in recorded.read_text().splitlines():
    if line.startswith('.set '):
        name, value = line[5:].split(',')
        bindings[name.strip()] = int(value.strip(), 16)
subprocess.run(['xcrun', 'clang', '-target', 'arm64-apple-ios17.0', '-Werror',
                '-Wno-incompatible-sysroot', '-c', recorded, '-o', out / 'original-bindings.o'], check=True)
archive = out / 'libNativeEmmc.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', archive, obj_path, out / 'original-bindings.o'], check=True)
(out / 'bindings.json').write_text(json.dumps(dict(
    source_kernelcache_sha256='f18b4df865405f2a287b84977d7aca745e056d656d9fc9df1662970ce2969de7',
    ordinary_storage_abi=True, compiled_object_sha256=hashlib.sha256(obj).hexdigest(),
    bindings={name: hex(value) for name, value in sorted(bindings.items())},
    original_code_modified=False), indent=2) + '\n')
print(f'PASS: {len(bindings)} NativeEmmc storage bindings to the pinned original storage ABI')
