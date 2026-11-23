#!/usr/bin/env python3
"""Unified CONFIG_XNU_DW_* resolver for UniXNU-merlinx.

Merges configuration layers in strict Android/Kconfig fragment precedence order:
  1. SoC baseline:    soc/mt6768/configs/mt6768.config (if present)
  2. Board general:   configs/merlinx_defconfig
  3. OS override:     configs/merlinx-iOS16.7.config (or UNIXNU_DW_OS_CONFIG)

The OS override config ALWAYS wins over general and SoC configs.
"""
import argparse
import os
from pathlib import Path
import re
import shlex
import sys

ROOT = Path(__file__).resolve().parents[1]
LINE_RE = re.compile(r'^(CONFIG_[A-Za-z0-9_]+)=(.*)$')
NOT_SET_RE = re.compile(r'^#\s*(CONFIG_[A-Za-z0-9_]+)\s+is not set\s*$')


def parse_config_file(path: Path) -> dict:
    values = {}
    if not path.is_file():
        return values
    for raw in path.read_text().splitlines():
        line = raw.strip()
        if not line:
            continue
        m_ns = NOT_SET_RE.match(line)
        if m_ns:
            values[m_ns.group(1)] = False
            continue
        if line.startswith('#'):
            continue
        m = LINE_RE.match(line)
        if not m:
            continue
        key, val = m.group(1), m.group(2).strip()
        if val == '':
            continue  # blank example placeholder: do not overwrite lower layer
        if val == 'y':
            values[key] = True
        elif val == 'n':
            values[key] = False
        elif len(val) >= 2 and val[0] == '"' and val[-1] == '"':
            values[key] = val[1:-1]
        elif re.fullmatch(r'-?(?:0x[0-9a-fA-F]+|[0-9]+)', val):
            values[key] = int(val, 0)
        else:
            values[key] = val
    return values


def default_layers(root: Path = ROOT) -> list[Path]:
    os_override = os.environ.get('UNIXNU_DW_OS_CONFIG', 'configs/merlinx-iOS16.7.config')
    layers = [
        root / 'soc/mt6768/configs/mt6768.config',
        root / 'configs/merlinx_defconfig',
        root / os_override if not Path(os_override).is_absolute() else Path(os_override),
    ]
    return layers


def load_resolved_config(root: Path = ROOT) -> dict:
    merged = {}
    for layer in default_layers(root):
        merged.update(parse_config_file(layer))
    # Propagate unified APFS UUIDs as single source of truth when component-specific keys are unset
    apfs_os = merged.get('CONFIG_XNU_DW_APFS_OS_UUID')
    apfs_sys = merged.get('CONFIG_XNU_DW_APFS_SYSTEMOS_UUID')
    apfs_data = merged.get('CONFIG_XNU_DW_APFS_DATA_UUID')
    if apfs_os:
        merged.setdefault('CONFIG_XNU_OWNER_ROOT_VOLUME_UUID', apfs_os)
        merged.setdefault('CONFIG_XNU_DW_NATIVEEMMC_OS_UUID', apfs_os)
    if apfs_sys:
        merged.setdefault('CONFIG_XNU_OWNER_CACHE_VOLUME_UUID', apfs_sys)
        merged.setdefault('CONFIG_XNU_DW_NATIVEEMMC_SYSTEMOS_UUID', apfs_sys)
    if apfs_data:
        merged['CONFIG_XNU_OWNER_DATA_VOLUME_UUID'] = apfs_data
    return merged


def cflags_for(component: str, cfg: dict | None = None) -> list[str]:
    cfg = cfg if cfg is not None else load_resolved_config()
    prefix = f'CONFIG_XNU_DW_{component.upper()}_'
    flags = []
    for key in sorted(cfg):
        if not (key.startswith(prefix) or key.startswith('CONFIG_XNU_DW_APFS_') or key == 'CONFIG_XNU_DW_OS_BUILD'):
            continue
        val = cfg[key]
        if isinstance(val, bool):
            flags.append(f'-D{key}={1 if val else 0}')
        elif isinstance(val, int):
            flags.append(f'-D{key}={val}')
        elif isinstance(val, str):
            flags.append(f'-D{key}="{val}"')
    return flags


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cflags', help='Emit compiler -D flags for component (e.g. NATIVEEMMC, MALIG52, DISPLAY)')
    ap.add_argument('--get', help='Print resolved value for a single CONFIG_* key')
    ap.add_argument('--shell-exports', action='store_true', help='Emit POSIX shell exports for XNU build')
    ap.add_argument('--dump', action='store_true', help='Dump all resolved CONFIG_XNU_DW_* keys')
    args = ap.parse_args()
    cfg = load_resolved_config()
    if args.cflags:
        print(' '.join(shlex.quote(f) for f in cflags_for(args.cflags, cfg)))
    elif args.get:
        val = cfg.get(args.get, '')
        if isinstance(val, bool):
            print('1' if val else '0')
        else:
            print(val)
    elif args.shell_exports:
        mapping = {
            'XNU_OWNER_ROOT_VOLUME_UUID': cfg.get('CONFIG_XNU_DW_APFS_OS_UUID', ''),
            'XNU_OWNER_CACHE_VOLUME_UUID': cfg.get('CONFIG_XNU_DW_APFS_SYSTEMOS_UUID', ''),
            'XNU_OWNER_DATA_VOLUME_UUID': cfg.get('CONFIG_XNU_DW_APFS_DATA_UUID', ''),
            'XNU_DW_OS_BUILD': cfg.get('CONFIG_XNU_DW_OS_BUILD', ''),
        }
        for k, v in mapping.items():
            if v:
                print(f'export {k}={shlex.quote(str(v))}')
    elif args.dump:
        for k in sorted(cfg):
            if k.startswith('CONFIG_XNU_DW_'):
                print(f'{k}={cfg[k]!r}')
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
