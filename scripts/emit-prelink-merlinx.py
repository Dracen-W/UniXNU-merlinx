#!/usr/bin/env python3
"""Wrapper around kits/XnuPrelink/emit_prelink.py for iOS 16.7.16 (20H392).

com.apple.kec.Libm in 20H392 embeds IEEE-754 floating-point constant tables inside
__TEXT_EXEC (e.g. 0xf09d5f48 at 0xfffffff005b8c1cc), which falsely match the ARM64
ADRP opcode mask in emit_prelink.instruction_inventory(). Skip only Libm during
instruction_inventory while keeping full strict checks on all other kexts.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'kits/XnuPrelink'))
import emit_prelink as ep

_SKIP_INVENTORY = {'com.apple.kec.Libm'}
_orig_inventory = ep.instruction_inventory


def _bundle_id(mod):
    if isinstance(mod, dict):
        for key in ('bundle', 'identifier', 'id', 'CFBundleIdentifier', 'name'):
            if key in mod and isinstance(mod[key], str):
                return mod[key]
    return ''


def _filtered_inventory(original_data, parent, modules):
    if isinstance(modules, dict):
        filtered = {k: v for k, v in modules.items() if k not in _SKIP_INVENTORY}
    elif isinstance(modules, list):
        filtered = [m for m in modules if _bundle_id(m) not in _SKIP_INVENTORY]
    else:
        filtered = modules
    return _orig_inventory(original_data, parent, filtered)


ep.instruction_inventory = _filtered_inventory

if __name__ == '__main__':
    ep.main()
