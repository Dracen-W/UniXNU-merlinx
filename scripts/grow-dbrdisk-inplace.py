#!/usr/bin/env python3
"""Expand a 16 GiB DBRDISK1 APFS Data seed in-place to N GiB (default 120 GiB = 128.85 GB decimal)
without requiring macOS hdiutil/diskutil, while satisfying all XNU APFS spaceman_sanity_check invariants."""
import argparse
import json
import struct
from pathlib import Path


def fletcher64(block: bytes) -> int:
    words = struct.unpack_from(f"<{(len(block) - 8) // 4}I", block, 8)
    count = len(words)
    modulus = 0xFFFFFFFF
    sum1 = sum(words) % modulus
    sum2 = sum(word * (count - index) for index, word in enumerate(words)) % modulus
    check1 = modulus - ((sum1 + sum2) % modulus)
    check2 = modulus - ((sum1 + check1) % modulus)
    return (check2 << 32) | check1


def seal(obj: bytearray) -> bytearray:
    struct.pack_into("<Q", obj, 0, fletcher64(obj))
    return obj


def checksum_ok(obj: bytes) -> bool:
    return struct.unpack_from("<Q", obj, 0)[0] == fletcher64(obj)


def grow_dbrdisk_exact(dbr_bytes: bytes, target_gib: int = 120) -> bytes:
    dbr = bytearray(dbr_bytes)
    magic, ver, bsize, logical, ext_count, rsvd = struct.unpack_from("<8sIIQII", dbr, 0)
    if magic != b"DBRDISK1" or ver != 1 or bsize != 4096 or ext_count != 7:
        raise ValueError("Input is not a valid 7-extent 4 KiB DBRDISK1 image")
    if logical != 16 * (1024**3):
        raise ValueError(f"Expected 16 GiB base DBRDISK1, got {logical // (1024**3)} GiB")
    if not 17 <= target_gib <= 126:
        raise ValueError(f"target_gib must be between 17 and 126 GiB, got {target_gib}")

    target_bytes = target_gib * (1024**3)
    target_blocks = target_bytes // 4096
    target_chunks = target_blocks // 32768
    added_blocks = target_blocks - (logical // 4096)
    num_cibs = (target_chunks + 125) // 126
    extra_cibs = num_cibs - 2

    extents = []
    off = 32
    for _ in range(ext_count):
        boff, blen = struct.unpack_from("<QQ", dbr, off)
        extents.append((boff, blen))
        off += 16

    ext_data = []
    for boff, blen in extents:
        ext_data.append([boff, bytearray(dbr[off : off + blen])])
        off += blen
    if off != len(dbr):
        raise ValueError("DBRDISK1 extent payload size mismatch")

    # Extend Extent 3 (boff=0x288d000, 14 blocks 0x288d..0x289a) by extra_cibs blocks (0x289b..)
    if ext_data[3][0] != 0x288D * 4096 or len(ext_data[3][1]) != 14 * 4096:
        raise ValueError("Unexpected Extent 3 layout in base DBRDISK1")
    ext_data[3][1].extend(bytes(extra_cibs * 4096))

    def read_obj(bno: int) -> bytearray:
        pos = bno * 4096
        for boff, data in ext_data:
            if boff <= pos < boff + len(data):
                s = pos - boff
                return bytearray(data[s : s + 4096])
        raise ValueError(f"Block {bno:#x} not in extents")

    def write_obj(bno: int, blk: bytearray) -> None:
        assert len(blk) == 4096
        pos = bno * 4096
        for boff, data in ext_data:
            if boff <= pos < boff + len(data):
                s = pos - boff
                data[s : s + 4096] = blk
                return
        raise ValueError(f"Block {bno:#x} not in extents")

    # 1. Update ALL NXSB checkpoint superblocks (bno = 0x0, 0x2, 0x4, 0x6, 0x8, 0xa)
    for bno in (0x0, 0x2, 0x4, 0x6, 0x8, 0xA):
        nx = read_obj(bno)
        assert nx[32:36] == b"NXSB" and checksum_ok(nx)
        struct.pack_into("<Q", nx, 40, target_blocks)
        write_obj(bno, seal(nx))

    # 2. Mark new CIB blocks (0x289b .. 0x289b + extra_cibs - 1, IP indices 14 .. 14 + extra_cibs - 1) in active IP bitmap (0x2881)
    ip_bm = read_obj(0x2881)
    for bit in range(14, 14 + extra_cibs):
        ip_bm[bit // 8] |= 1 << (bit % 8)
    write_obj(0x2881, ip_bm)

    # 3. Expand Internal Pool from 390 to (target_chunks + 8) * 3 blocks in bmap0 (0x288f) and cib0 (0x2899)
    old_ip_blocks = 390
    new_ip_blocks = (target_chunks + 8) * 3
    extra_ip_blocks = new_ip_blocks - old_ip_blocks
    bmap0 = read_obj(0x288F)
    for b in range(0x288D + old_ip_blocks, 0x288D + new_ip_blocks):
        assert ((bmap0[b // 8] >> (b % 8)) & 1) == 0
        bmap0[b // 8] |= 1 << (b % 8)
    write_obj(0x288F, bmap0)

    cib0 = read_obj(0x2899)
    assert checksum_ok(cib0)
    xid0, addr0, bcnt0, fcnt0, bm0 = struct.unpack_from("<QQIIQ", cib0, 40)
    assert addr0 == 0 and bcnt0 == 32768 and fcnt0 == 21997 and bm0 == 0x288F
    struct.pack_into("<QQIIQ", cib0, 40, xid0, addr0, bcnt0, fcnt0 - extra_ip_blocks, bm0)
    write_obj(0x2899, seal(cib0))

    # 4. Build CIB[1 .. num_cibs - 1]
    cib_bnos = [0x2899, 0x288E] + [0x289B + i for i in range(extra_cibs)]
    xid = 5
    OBJ_CIB_PHYS = 0x40000007
    for cib_idx in range(1, num_cibs):
        bno = cib_bnos[cib_idx]
        first_chunk = cib_idx * 126
        last_chunk = min(target_chunks, first_chunk + 126)
        ci_count = last_chunk - first_chunk
        cib = bytearray(4096)
        struct.pack_into("<QQII", cib, 8, bno, xid, OBJ_CIB_PHYS, 0)
        struct.pack_into("<II", cib, 32, cib_idx, ci_count)
        for k in range(ci_count):
            chunk_id = first_chunk + k
            ci_xid = 1 if (cib_idx == 1 and k < 2) else xid
            ci_addr = chunk_id * 32768
            struct.pack_into("<QQIIQ", cib, 40 + k * 32, ci_xid, ci_addr, 32768, 32768, 0)
        write_obj(bno, seal(cib))

    # 5. Update active SPACEMAN at bno=0x7b including sm_ip_block_count (+0x98) and sm_fq[SFQ_IP].sfq_tree_len (+224)
    sm = read_obj(0x7B)
    assert checksum_ok(sm)
    dev = 48
    old_blocks, old_chunks, old_cibs, old_cabs, old_free, addr_off = struct.unpack_from("<QQIIQI", sm, dev)
    assert old_blocks == 4194304 and old_chunks == 128 and old_cibs == 2
    new_free = old_free + added_blocks - extra_ip_blocks
    struct.pack_into("<QQIIQI", sm, dev, target_blocks, target_chunks, len(cib_bnos), 0, new_free, addr_off)
    struct.pack_into("<I", sm, 0x80, addr_off + len(cib_bnos) * 8)
    struct.pack_into("<Q", sm, 0x98, new_ip_blocks)

    fq_ip_entries = (new_ip_blocks + 15) >> 4
    sfq_ip_tree_len = (fq_ip_entries + 164) // 165
    if sfq_ip_tree_len == 2:
        sfq_ip_tree_len = 3
    struct.pack_into("<H", sm, 224, sfq_ip_tree_len)

    for i, bno in enumerate(cib_bnos):
        struct.pack_into("<Q", sm, addr_off + i * 8, bno)
    write_obj(0x7B, seal(sm))

    for bno in (0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0x7B, 0x7D, *cib_bnos):
        assert checksum_ok(read_obj(bno)), f"Checksum failed at {bno:#x}"

    out = bytearray(struct.pack("<8sIIQII", b"DBRDISK1", 1, 4096, target_bytes, len(ext_data), 0))
    for boff, data in ext_data:
        out += struct.pack("<QQ", boff, len(data))
    for _, data in ext_data:
        out += data
    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gib", type=int, default=120, help="Target APFS container size in GiB (default: 120)")
    parser.add_argument("input", type=Path, help="Input 16 GiB DBRDISK1 file")
    parser.add_argument("output", type=Path, nargs="?", help="Output DBRDISK1 file (defaults to overwriting input in-place)")
    args = parser.parse_args()

    out_path = args.output or args.input
    grown = grow_dbrdisk_exact(args.input.read_bytes(), target_gib=args.gib)
    out_path.write_bytes(grown)
    print(json.dumps({
        "input": str(args.input),
        "output": str(out_path),
        "gib": args.gib,
        "logical_bytes": args.gib * (1024**3),
        "packed_bytes": len(grown),
    }, indent=2))


if __name__ == "__main__":
    main()
