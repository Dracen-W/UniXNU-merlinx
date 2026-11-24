#!/usr/bin/env python3
"""Wrapper around kits/XnuPrelink/emit_prelink.py for iOS 16.7.16 (20H392).

1. com.apple.kec.Libm in 20H392 embeds IEEE-754 floating-point constant tables
   inside __TEXT_EXEC (e.g. 0xf09d5f48 at 0xfffffff005b8c1cc), which falsely
   match the ARM64 ADRP opcode mask in emit_prelink.instruction_inventory().
   Skip only Libm during instruction_inventory while keeping full strict checks
   on all other kexts.
2. iPhone10,6 (20H392) IOHIDFamily strips the Keyboard/Digitizer IOHIDEventDriver
   personalities and leaves only "HID Game Controller Pointing Driver" with
   IOMatchDefer=<true/> (blocked under dk=0) and HIDDefaultBehavior=<string/>.
   Patch that personality in-place inside __PRELINK_INFO (exact same byte length,
   space-padded) to match Touch Screen Digitizer (0xd/0x4) + Keyboard (0x1/0x6)
   directly so NativeHidInput attaches IOHIDEventDriver on boot without any
   GamePad workaround.
"""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

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

_IOHID_PERSONALITY_KEY = b'<key>HID Game Controller Pointing Driver</key>'
_IOHID_PATCHED_DICT = (
    b'<dict>'
    b'<key>CFBundleIdentifier</key><string>com.apple.iokit.IOHIDFamily</string>'
    b'<key>DeviceUsagePairs</key><array>'
    b'<dict><key>DeviceUsage</key><integer size="64">0x4</integer>'
    b'<key>DeviceUsagePage</key><integer size="64">0xd</integer></dict>'
    b'<dict><key>DeviceUsage</key><integer size="64">0x6</integer>'
    b'<key>DeviceUsagePage</key><integer size="64">0x1</integer></dict>'
    b'</array>'
    b'<key>IOClass</key><string>IOHIDEventDriver</string>'
    b'<key>IOProviderClass</key><string>IOHIDInterface</string>'
    b'</dict>'
)


def _find_matching_dict_end(buf: bytes, dict_start: int) -> int:
    """Return index immediately after the closing </dict> matching dict_start."""
    depth = 0
    pos = dict_start
    while pos < len(buf):
        nxt_open = buf.find(b'<dict>', pos)
        nxt_close = buf.find(b'</dict>', pos)
        if nxt_close == -1:
            return -1
        if nxt_open != -1 and nxt_open < nxt_close:
            depth += 1
            pos = nxt_open + 6
        else:
            depth -= 1
            pos = nxt_close + 7
            if depth == 0:
                return pos
    return -1


def patch_iohid_prelink_info(macho_path: Path) -> bool:
    """Patch IOHIDFamily personality in-place inside emitted Mach-O __PRELINK_INFO."""
    data = bytearray(macho_path.read_bytes())
    key_pos = data.find(_IOHID_PERSONALITY_KEY)
    if key_pos == -1:
        print(f'emit-prelink-merlinx: {_IOHID_PERSONALITY_KEY.decode()} not found in {macho_path.name}; skipping IOHID patch')
        return False

    dict_start = data.find(b'<dict>', key_pos + len(_IOHID_PERSONALITY_KEY))
    if dict_start == -1 or dict_start - key_pos > 128:
        raise RuntimeError('emit-prelink-merlinx: malformed IOHID personality dict header')

    dict_end = _find_matching_dict_end(bytes(data), dict_start)
    if dict_end == -1:
        raise RuntimeError('emit-prelink-merlinx: unterminated IOHID personality <dict>')

    old_dict = bytes(data[dict_start:dict_end])
    if b'<integer size="64">0xd</integer>' in old_dict and b'IOMatchDefer' not in old_dict:
        print(f'emit-prelink-merlinx: IOHIDFamily personality already patched in {macho_path.name}')
        return True

    if len(_IOHID_PATCHED_DICT) > len(old_dict):
        raise RuntimeError(
            f'emit-prelink-merlinx: replacement dict ({len(_IOHID_PATCHED_DICT)}B) exceeds original ({len(old_dict)}B)'
        )

    padded = _IOHID_PATCHED_DICT + b' ' * (len(old_dict) - len(_IOHID_PATCHED_DICT))
    # Validate XML well-formedness before writing back
    ET.fromstring(b'<root>' + _IOHID_PERSONALITY_KEY + padded + b'</root>')

    data[dict_start:dict_end] = padded
    macho_path.write_bytes(data)
    print(
        f'emit-prelink-merlinx: patched IOHIDFamily personality in {macho_path.name} '
        f'at {dict_start:#x} ({len(old_dict)}B in-place, Digitizer 0xd/0x4 + Keyboard 0x1/0x6)'
    )
    return True


def _extract_output_path(argv):
    for i, arg in enumerate(argv):
        if arg in ('--output', '-o') and i + 1 < len(argv):
            return Path(argv[i + 1])
        if arg.startswith('--output='):
            return Path(arg.split('=', 1)[1])
    return None


def main():
    ep.main()
    out_path = _extract_output_path(sys.argv[1:])
    if out_path and out_path.is_file():
        patch_iohid_prelink_info(out_path)


if __name__ == '__main__':
    main()
