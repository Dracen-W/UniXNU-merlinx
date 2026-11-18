#!/usr/bin/env python3
"""
Merlinx XNU Mode A MUSB EP0 PIO Log Streamer & Live Command Client
==================================================================
Communicates with the MediaTek MT6768 MUSB EP0 PIO diagnostic interface
(VID 0x05AC / PID 0x12A8, MS OS 1.0 WinUSB auto-bound) inside the XNU kernel.

Requirements:
  pip install pyusb

Usage examples:
  # 1. Dump entire buffered boot log and keep streaming live (`dmesg -w` style):
  python3 scripts/merlinx_usb_log.py watch -o merlinx-boot.log

  # 2. One-shot dump of the 4 MiB kernel log ring buffer to a file:
  python3 scripts/merlinx_usb_log.py dump -o merlinx-ring.log

  # 3. Show USB / MUSB hardware register state & ring buffer counters:
  python3 scripts/merlinx_usb_log.py status

  # 4. Run live kernel diagnostic commands over USB EP0:
  python3 scripts/merlinx_usb_log.py cmd "help"
  python3 scripts/merlinx_usb_log.py cmd "ps"
  python3 scripts/merlinx_usb_log.py cmd "allbt"
  python3 scripts/merlinx_usb_log.py cmd "bt lsd"
  python3 scripts/merlinx_usb_log.py cmd "bt backboardd"
  python3 scripts/merlinx_usb_log.py cmd "bt SpringBoard"
  python3 scripts/merlinx_usb_log.py cmd "kill 91 9"
  python3 scripts/merlinx_usb_log.py cmd "icblock 0"
"""

import argparse
import struct
import sys
import time

try:
    import usb.core
    import usb.util
except ImportError:
    print("Error: pyusb is not installed. Run: pip install pyusb", file=sys.stderr)
    sys.exit(1)

VID = 0x05AC
PID = 0x12A8

REQ_GET_STATUS   = 0xF0
REQ_READ_LOG     = 0xF1
REQ_EXEC_CMD     = 0xF2
REQ_READ_CMD_OUT = 0xF3

STATUS_FMT = "<IIQQIIIII4B"
STATUS_SIZE = struct.calcsize(STATUS_FMT)  # 48 bytes
MAGIC_MXLG = 0x4D584C47


def find_device(wait=True):
    printed = False
    while True:
        dev = usb.core.find(idVendor=VID, idProduct=PID)
        if dev is not None:
            try:
                dev.set_configuration()
            except Exception:
                pass
            return dev
        if not wait:
            return None
        if not printed:
            print(f"[*] Waiting for Merlinx XNU USB device ({VID:04x}:{PID:04x})...", file=sys.stderr)
            printed = True
        time.sleep(0.25)


def get_status(dev):
    raw = bytes(dev.ctrl_transfer(0xC0, REQ_GET_STATUS, 0, 0, STATUS_SIZE, timeout=1000))
    if len(raw) < STATUS_SIZE:
        raise RuntimeError(f"Short status packet ({len(raw)} bytes)")
    (
        magic,
        ring_size,
        write_seq,
        uptime_cs,
        usb_polls,
        usb_resets,
        usb_setups,
        cmd_seq,
        cmd_len,
        faddr,
        power,
        devctl,
        config,
    ) = struct.unpack(STATUS_FMT, raw[:STATUS_SIZE])
    if magic != MAGIC_MXLG:
        raise RuntimeError(f"Invalid status magic: 0x{magic:08x}")
    return {
        "ring_size": ring_size,
        "write_seq": write_seq,
        "uptime_s": uptime_cs / 100.0,
        "usb_polls": usb_polls,
        "usb_resets": usb_resets,
        "usb_setups": usb_setups,
        "cmd_seq": cmd_seq,
        "cmd_len": cmd_len,
        "faddr": faddr,
        "power": power,
        "devctl": devctl,
        "config": config,
    }


def read_ring_range(dev, start_seq, end_seq, ring_size, chunk_size=1024):
    if end_seq <= start_seq:
        return b""
    if end_seq - start_seq > ring_size:
        start_seq = end_seq - ring_size
    out = bytearray()
    cur = start_seq
    while cur < end_seq:
        to_read = min(chunk_size, end_seq - cur)
        off32 = cur & (ring_size - 1)
        wValue = off32 & 0xFFFF
        wIndex = (off32 >> 16) & 0xFFFF
        data = bytes(dev.ctrl_transfer(0xC0, REQ_READ_LOG, wValue, wIndex, to_read, timeout=2000))
        if not data:
            break
        out.extend(data)
        cur += len(data)
    return bytes(out)


def do_status(args):
    dev = find_device(wait=args.wait)
    if dev is None:
        print("Device not found.", file=sys.stderr)
        sys.exit(1)
    st = get_status(dev)
    print(f"Merlinx XNU Mode A MUSB Status:")
    print(f"  Uptime       : {st['uptime_s']:.2f} s")
    print(f"  Ring Buffer  : {st['write_seq']} bytes written / {st['ring_size'] // 1024} KiB capacity")
    print(f"  MUSB State   : FADDR={st['faddr']}CFG={st['config']} POWER=0x{st['power']:02x} DEVCTL=0x{st['devctl']:02x}")
    print(f"  USB Counters : polls={st['usb_polls']} setups={st['usb_setups']} resets={st['usb_resets']}")
    print(f"  Last Command : seq={st['cmd_seq']} out_len={st['cmd_len']} bytes")


def do_dump(args):
    dev = find_device(wait=True)
    st = get_status(dev)
    ring_size = st["ring_size"]
    write_seq = st["write_seq"]
    start_seq = max(0, write_seq - ring_size)
    data = read_ring_range(dev, start_seq, write_seq, ring_size, chunk_size=args.chunk)
    text = data.decode("utf-8", errors="replace")
    sys.stdout.write(text)
    sys.stdout.flush()
    if args.output:
        Path(args.output).write_bytes(data)
        print(f"\n[+] Saved {len(data)} bytes to {args.output}", file=sys.stderr)


def do_watch(args):
    out_file = open(args.output, "ab") if args.output else None
    cur_seq = 0
    dev = None
    print("[*] Streaming Merlinx XNU kernel log over USB EP0 (Ctrl+C to stop)...", file=sys.stderr)
    try:
        while True:
            if dev is None:
                dev = find_device(wait=True)
                st = get_status(dev)
                print(
                    f"[+] Connected (uptime={st['uptime_s']:.2f}s, ring_written={st['write_seq']} B, "
                    f"POWER=0x{st['power']:02x}, FADDR={st['faddr']})",
                    file=sys.stderr,
                )
                if cur_seq == 0 and st["write_seq"] > st["ring_size"]:
                    cur_seq = st["write_seq"] - st["ring_size"]
            try:
                st = get_status(dev)
                wseq = st["write_seq"]
                rsize = st["ring_size"]
                if wseq < cur_seq:
                    # Device rebooted
                    cur_seq = max(0, wseq - rsize)
                if wseq > cur_seq:
                    if wseq - cur_seq > rsize:
                        cur_seq = wseq - rsize
                    chunk = read_ring_range(dev, cur_seq, wseq, rsize, chunk_size=args.chunk)
                    if chunk:
                        cur_seq += len(chunk)
                        sys.stdout.write(chunk.decode("utf-8", errors="replace"))
                        sys.stdout.flush()
                        if out_file:
                            out_file.write(chunk)
                            out_file.flush()
                else:
                    time.sleep(args.interval)
            except Exception as e:
                print(f"\n[!] USB connection interrupted ({e}), reconnecting...", file=sys.stderr)
                dev = None
                time.sleep(0.3)
    except KeyboardInterrupt:
        print("\n[*] Stopped.", file=sys.stderr)
    finally:
        if out_file:
            out_file.close()


def do_cmd(args):
    dev = find_device(wait=True)
    cmd_str = " ".join(args.command).strip()
    if not cmd_str:
        cmd_str = "help"
    payload = cmd_str.encode("utf-8")[:120]
    dev.ctrl_transfer(0x40, REQ_EXEC_CMD, 0, 0, payload, timeout=2000)
    st = get_status(dev)
    total_len = st["cmd_len"]
    out = bytearray()
    off = 0
    while off < total_len:
        to_read = min(1024, total_len - off)
        part = bytes(dev.ctrl_transfer(0xC0, REQ_READ_CMD_OUT, off, 0, to_read, timeout=2000))
        if not part:
            break
        out.extend(part)
        off += len(part)
    sys.stdout.write(out.decode("utf-8", errors="replace"))
    sys.stdout.flush()


def main():
    parser = argparse.ArgumentParser(description="Merlinx XNU Mode A MUSB EP0 PIO Tool")
    sub = parser.add_subparsers(dest="subcmd")

    p_watch = sub.add_parser("watch", help="Dump buffered boot log and stream live (`dmesg -w`)")
    p_watch.add_argument("-o", "--output", help="Append raw log stream to file")
    p_watch.add_argument("--chunk", type=int, default=1024, help="EP0 read chunk size (default 1024)")
    p_watch.add_argument("--interval", type=float, default=0.05, help="Poll interval seconds (default 0.05)")

    p_dump = sub.add_parser("dump", help="One-shot dump of current 4 MiB ring buffer")
    p_dump.add_argument("-o", "--output", help="Save raw dump to file")
    p_dump.add_argument("--chunk", type=int, default=1024, help="EP0 read chunk size (default 1024)")

    p_status = sub.add_parser("status", help="Show MUSB registers and log ring counters")
    p_status.add_argument("--no-wait", dest="wait", action="store_false", help="Exit immediately if not connected")
    p_status.set_defaults(wait=True)

    p_cmd = sub.add_parser("cmd", help="Run a live kernel command (help, ps, bt, allbt, kill, icblock, musb)")
    p_cmd.add_argument("command", nargs="+", help="Command string, e.g. 'allbt' or 'bt lsd'")

    args = parser.parse_args()
    if args.subcmd in (None, "watch"):
        if not hasattr(args, "output"):
            args.output = None
            args.chunk = 1024
            args.interval = 0.05
        do_watch(args)
    elif args.subcmd == "dump":
        do_dump(args)
    elif args.subcmd == "status":
        do_status(args)
    elif args.subcmd == "cmd":
        do_cmd(args)


if __name__ == "__main__":
    main()
