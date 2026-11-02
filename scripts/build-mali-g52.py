#!/usr/bin/env python3
"""Build the Mali-G52 kernel service and its pure NativeMali layers."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--m2-writes', action='store_true')
parser.add_argument('--m3-writes', action='store_true')
parser.add_argument('--m4-writes', action='store_true')
parser.add_argument('--m6-writes', action='store_true')
args = parser.parse_args()
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/mali-g52'
OUT.mkdir(parents=True, exist_ok=True)
SOURCE = ROOT / 'drivers/MaliG52'
recipe = ROOT / 'build/xnu/ios-obj/DEVELOPMENT_ARM64_BCM2837/iokit/DEVELOPMENT/IOPlatformExpert.cpo.json'
record = json.loads(recipe.read_text().strip().rstrip(','))
sources = sorted((SOURCE / 'xnu').glob('NativeMali*.cpp'))
sources += sorted(p for p in SOURCE.glob('NativeMali*.cpp') if p.is_file())
objects = []
for source in sources:
    obj = OUT / (source.stem + '.o')
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
    command += ['-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter',
                '-Wno-c99-designator', '-Wno-reorder-init-list',
                '-Wno-deprecated-enum-enum-conversion', '-Wno-unnecessary-virtual-specifier',
                '-I', str(OUT), '-I', str(SOURCE), '-I', str(SOURCE / 'xnu'),
                '-I', str(ROOT / '.cache/xnu-work/osfmk')]
    if args.m2_writes:
        command.append('-DNATIVE_MALI_M2_WRITES=1')
    if args.m3_writes:
        command.append('-DNATIVE_MALI_M3_WRITES=1')
    if args.m4_writes:
        command.append('-DNATIVE_MALI_M4_WRITES=1')
    if args.m6_writes:
        command.append('-DNATIVE_MALI_M6_WRITES=1')
    (OUT / (source.stem + '-command.json')).write_text(json.dumps(command, indent=2) + '\n')
    result = subprocess.run(command, cwd=record['directory'], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    (OUT / (source.stem + '.log')).write_text(result.stdout)
    if result.returncode:
        raise SystemExit(result.stdout[-4000:])
    objects.append(obj)
archive = OUT / 'libMaliG52.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), *map(str, objects)], check=True)
print(f'PASS: Mali-G52 kernel archive ({len(objects)} objects)')
