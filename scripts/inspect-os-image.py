#!/usr/bin/env python3
"""Report facts about the configured OS image: launchd job definitions and dyld cache image ranges.

Runs on macOS after `./unixnu build userdata`: it extracts each window of the userdata image, attaches it read-only,
and prints what the boot actually sees -- whether the launchd plists of the services on the path to SpringBoard exist,
where their Program points, and whether that program is on the volume. It then resolves the user addresses collected
from the panel trace against the dyld shared cache's image table, so a crash or wait address becomes a library + offset.

Only derived facts are printed (labels, paths, booleans, image names and offsets): no file content of the image is kept.
"""
import json
import plistlib
import struct
import subprocess
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CHUNK = 8 << 20

# Addresses from the panel trace (panel25, ::/EFI/UniXNU/unixnu.toml boot-ui-*): unslid (slide already removed).
TRACE = {
    0x1800E9068: 'fault pc (pid 263)',
    0x1800EDCD4: 'fault lr (pid 263)',
    0x1CB780B58: 'SpringBoard pc',
    0x1CB784224: 'SpringBoard lr',
    0x1CB7831B0: 'SpringBoard f1',
    0x1CB783FB8: 'SpringBoard f2',
    0x1CB781528: 'launchd pc',
}

LABELS = [
    'com.apple.backboardd',
    'com.apple.SpringBoard',
    'com.apple.frontboardd',
    'com.apple.runningboardd',
    'com.apple.lsd',
    'com.apple.containermanagerd',
    'com.apple.fairplayd.A2',
]

PROGRAMS = [
    '/usr/libexec/backboardd',
    '/System/Library/CoreServices/SpringBoard.app/SpringBoard',
    '/usr/libexec/runningboardd',
    '/usr/libexec/lsd',
    '/usr/libexec/containermanagerd',
]


def run(*command):
    return subprocess.run([str(part) for part in command], capture_output=True, text=True)


def candidates():
    return sorted(ROOT.glob('out/*/*/*/userdata/userdata-root.json'))


def parse_images(data):
    """Find the dyld cache imagesText array without the exact header offsets: a run of {uuid, loadAddress, textSize,
    pathFileOffset} records whose path is an absolute path inside the file."""
    best = []
    limit = min(len(data), 0x40000)
    for start in range(0x100, limit, 4):
        images, offset = [], start
        while len(images) < 256 and offset + 32 <= len(data):
            address, size, path_offset = struct.unpack_from('<QII', data, offset + 16)
            if not (0x100000000 <= address < 0x400000000) or not (0 < size < 0x8000000):
                break
            if path_offset >= len(data):
                break
            path = data[path_offset:path_offset + 256].split(b'\0')[0]
            if not path.startswith(b'/'):
                break
            images.append((address, size, path.decode('utf-8', 'replace')))
            offset += 32
        if len(images) > len(best):
            best = images
    return best


def resolve(directory):
    for cache in sorted(directory.glob('dyld_shared_cache_arm64*')):
        if '.symbols' in cache.name or cache.name.endswith(('.map', '.json')):
            continue
        try:
            head = cache.open('rb').read(4 << 20)
        except OSError as error:
            print(f'    {cache.name}: unreadable ({error})')
            continue
        images = parse_images(head)
        print(f'    {cache.name}: {len(images)} images')
        for address, what in TRACE.items():
            for base, size, name in images:
                if base <= address < base + size:
                    print(f'      {what}: {address:#x} -> {name} + {address - base:#x}')
                    break


def inspect(mount):
    daemons = mount / 'System/Library/LaunchDaemons'
    agents = mount / 'System/Library/LaunchAgents'
    if daemons.is_dir():
        entries = list(daemons.glob('*.plist'))
        print(f'    LaunchDaemons: {len(entries)} plists')
    else:
        print('    LaunchDaemons: absent')
    if agents.is_dir():
        print(f'    LaunchAgents: {len(list(agents.glob("*.plist")))} plists')
    for label in LABELS:
        path = daemons / (label + '.plist')
        if not path.is_file():
            print(f'    {label}: NO PLIST')
            continue
        try:
            job = plistlib.loads(path.read_bytes())
        except Exception as error:  # noqa: BLE001 - report, never fail the run
            print(f'    {label}: unreadable ({error})')
            continue
        program = job.get('Program') or (job.get('ProgramArguments') or [''])[0]
        state = ''
        if program:
            state = 'present' if (mount / program.lstrip('/')).exists() else 'MISSING'
        keep = job.get('KeepAlive')
        if isinstance(keep, dict):
            keep = 'dict'
        print(f'    {label}: program={program} {state} RunAtLoad={job.get("RunAtLoad")} '
              f'KeepAlive={keep} MachServices={len(job.get("MachServices") or {})} '
              f'session={job.get("LimitLoadToSessionType")}')
    for program in PROGRAMS:
        path = mount / program.lstrip('/')
        print(f'    path {program}: {"present" if path.exists() else "MISSING"}')
    cache_dir = mount / 'System/Library/Caches/com.apple.dyld'
    if cache_dir.is_dir():
        resolve(cache_dir)


def main():
    reports = candidates()
    if not reports:
        print('no userdata report: run ./unixnu build userdata first')
        return 1
    report_path = reports[0]
    report = json.loads(report_path.read_text())
    image = report_path.parent / 'userdata-root.img'
    print('report', report_path, 'image', image, image.stat().st_size, 'bytes')
    for window in report['windows']:
        name, offset, size = window['name'], window['offset'], window['size']
        part = pathlib.Path('/tmp') / ('inspect-' + name + '.img')
        with image.open('rb') as source, part.open('wb') as target:
            source.seek(offset)
            remaining = size
            while remaining:
                chunk = source.read(min(remaining, CHUNK))
                if not chunk:
                    break
                target.write(chunk)
                remaining -= len(chunk)
        attached = run('hdiutil', 'attach', '-readonly', '-noverify', '-noautofsck', '-nobrowse', '-plist', part)
        if attached.returncode:
            print(f'== window {name}: attach failed: {attached.stderr.strip()[:300]}')
            continue
        try:
            info = plistlib.loads(attached.stdout.encode())
        except Exception as error:  # noqa: BLE001
            print(f'== window {name}: unreadable attach output ({error})')
            continue
        mounts = [entry['mount-point'] for entry in info.get('system-entities', []) if entry.get('mount-point')]
        print(f'== window {name}: mounts {mounts}')
        for mount in mounts:
            inspect(pathlib.Path(mount))
        run('hdiutil', 'detach', part, '-force')
    return 0


if __name__ == '__main__':
    sys.exit(main())
