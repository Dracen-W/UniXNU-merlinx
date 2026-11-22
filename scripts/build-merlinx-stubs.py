#!/usr/bin/env python3
"""Build link stubs required by the merlinx kernel bring-up."""
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from kernel_target import TRIPLE
DEVICE = Path(os.environ.get('UNIXNU_DEVICE', ROOT))
OUT = ROOT / 'build/merlinx-stubs'
OUT.mkdir(parents=True, exist_ok=True)
sdk = subprocess.check_output(['xcrun', '--sdk', 'iphoneos', '--show-sdk-path'], text=True).strip()
obj = OUT / 'MerlinxStubs.o'
subprocess.run(
    ['xcrun', 'clang', '-target', TRIPLE, '-isysroot', sdk,
     '-mgeneral-regs-only', '-Wall', '-Wextra', '-Werror', '-c',
     str((ROOT / 'drivers/MerlinxStubs/MerlinxStubs.c') if (ROOT / 'drivers/MerlinxStubs/MerlinxStubs.c').is_file() else (DEVICE / 'drivers/MerlinxStubs/MerlinxStubs.c')), '-o', str(obj)],
    check=True,
)
archive = OUT / 'libMerlinxStubs.a'
archive.unlink(missing_ok=True)
subprocess.run(['xcrun', 'ar', 'rcs', str(archive), str(obj)], check=True)
print('PASS: merlinx link stubs')
