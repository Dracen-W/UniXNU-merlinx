#!/usr/bin/env python3
"""Derive a boot image from a built one without CI: change kernel arguments and/or the Data seed.

The payload FAT volume is the boot image's ramdisk; it is edited in place with mtools.
  scripts/respin.py BASE.img OUT.img --del-arg sheng-no-lc=1 --add-arg foo=1 --seed .cache/data-seed.dbr
  scripts/respin.py BASE.img --show
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

CONFIG = '::/EFI/UniXNU/unixnu.toml'
SEED = '::/EFI/UniXNU/Payloads/scratch-data.img'
ADT = '::/EFI/UniXNU/Payloads/kernel.macho.adt'


def layout(data):
    if data[:8] != b'ANDROID!':
        sys.exit('not an Android boot image')
    kernel, _, ramdisk, _, _, _, _, page, version = struct.unpack('<9I', data[8:44])
    if version != 2:
        sys.exit(f'unsupported boot header version {version}')
    start = page + (kernel + page - 1) // page * page
    dtb_size = struct.unpack_from('<I', data, 1648)[0]
    dtb_start = start + (ramdisk + page - 1) // page * page
    return page, kernel, start, ramdisk, dtb_start, dtb_size


def update_boot_id(data):
    page, kernel, start, ramdisk, dtb_start, dtb_size = layout(data)
    sha = hashlib.sha1()
    for blob in (data[page:page + kernel], data[start:start + ramdisk], b'', b'', data[dtb_start:dtb_start + dtb_size]):
        if blob:
            sha.update(blob)
        sha.update(struct.pack('<I', len(blob)))
    data[576:608] = struct.pack('32s', sha.digest())


def mtools(volume, *args, capture=False):
    env = dict(os.environ, MTOOLS_SKIP_CHECK='1')
    result = subprocess.run(args[:1] + ('-i', str(volume)) + args[1:], env=env, check=True, capture_output=capture)
    return result.stdout if capture else None


def read_args(volume, work):
    text = Path(work, 'unixnu.toml')
    mtools(volume, 'mcopy', '-n', CONFIG, str(text))
    lines = text.read_text().splitlines()
    for number, line in enumerate(lines):
        if line.startswith('args = "'):
            return text, lines, number, line[8:-1].split()
    sys.exit('no args line in unixnu.toml')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('base')
    parser.add_argument('output', nargs='?')
    parser.add_argument('--add-arg', action='append', default=[], metavar='NAME[=VALUE]', help='add or replace a kernel argument')
    parser.add_argument('--del-arg', action='append', default=[], metavar='NAME[=VALUE]', help='remove an argument by name (any value)')
    parser.add_argument('--seed', metavar='FILE', help='replace the packed Data seed')
    parser.add_argument('--adt', metavar='FILE', help='replace the Apple device tree (kernel.macho.adt)')
    parser.add_argument('--show', action='store_true', help='print the kernel arguments and the seed hash, write nothing')
    options = parser.parse_args()
    if not shutil.which('mcopy'):
        sys.exit('mtools is required (mcopy)')
    data = bytearray(Path(options.base).read_bytes())
    _, _, start, size, _, _ = layout(data)
    with tempfile.TemporaryDirectory() as work:
        volume = Path(work, 'payload.img')
        volume.write_bytes(data[start:start + size])
        text, lines, number, arguments = read_args(volume, work)
        if options.show:
            print(' '.join(arguments))
            seed = Path(work, 'seed')
            mtools(volume, 'mcopy', '-n', SEED, str(seed))
            print('seed', hashlib.sha256(seed.read_bytes()).hexdigest())
            return
        if not options.output:
            sys.exit('an output image is required')
        names = {item.partition('=')[0] for item in options.del_arg + options.add_arg}
        arguments = [item for item in arguments if item.partition('=')[0] not in names] + options.add_arg
        line = ' '.join(arguments)
        if len(line) > 1000:
            sys.exit(f'the kernel command line is {len(line)} bytes; the limit is 1000')
        lines[number] = f'args = "{line}"'
        text.write_text('\n'.join(lines) + '\n')
        mtools(volume, 'mcopy', '-o', str(text), CONFIG)
        if options.seed:
            mtools(volume, 'mcopy', '-o', options.seed, SEED)
        if options.adt:
            mtools(volume, 'mcopy', '-o', options.adt, ADT)
        patched = volume.read_bytes()
        if len(patched) != size:
            sys.exit('the payload volume changed size')
        data[start:start + size] = patched
        update_boot_id(data)
    Path(options.output).write_bytes(data)
    print(options.output)
    print('args', line)
    print('sha256', hashlib.sha256(data).hexdigest())


if __name__ == '__main__':
    main()
