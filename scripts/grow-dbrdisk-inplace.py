#!/usr/bin/env python3
import hashlib
import os
import shutil
import struct
import zipfile
from pathlib import Path

OUT_DIR = Path("/working_dir/c_3b70091adbc1bc39/artifacts/file_generation/ttl=63d/output")
OUT_DIR.mkdir(parents=True, exist_ok=True)
WORK_DIR = Path("/working_dir/c_3b70091adbc1bc39/work")
WORK_DIR.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# 1. APFS Fletcher-64 & In-Place DBRDISK1 16 GiB -> 120 GiB (128.85 GB) Expander
# ---------------------------------------------------------------------------
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

def grow_dbrdisk_120gib(dbr_bytes: bytes) -> bytes:
    """Expand the 16 GiB DBRDISK1 APFS container in-place to 120 GiB (128.85 GB decimal)
    without changing the packed 413,840-byte file size or extent table."""
    dbr = bytearray(dbr_bytes)
    magic, ver, bsize, logical, ext_count, rsvd = struct.unpack_from("<8sIIQII", dbr, 0)
    assert magic == b"DBRDISK1" and ver == 1 and bsize == 4096 and ext_count == 7
    assert logical == 16 * (1024**3)

    target_gib = 120
    target_bytes = target_gib * (1024**3)       # 128,849,018,880 bytes = 128.85 GB decimal
    target_blocks = target_bytes // 4096        # 31,457,280 blocks
    target_chunks = target_blocks // 32768      # 960 chunks of 128 MiB each
    added_blocks = target_blocks - (logical // 4096)

    # Parse extents
    extents = []
    off = 32
    for _ in range(ext_count):
        boff, blen = struct.unpack_from("<QQ", dbr, off)
        extents.append((boff, blen))
        off += 16

    ext_offsets = []
    for boff, blen in extents:
        ext_offsets.append((boff, blen, off))
        off += blen
    assert off == len(dbr)

    def get_block_slice(bno: int, count: int = 1):
        pos = bno * 4096
        size = count * 4096
        for boff, blen, data_off in ext_offsets:
            if boff <= pos and pos + size <= boff + blen:
                start = data_off + (pos - boff)
                return start, start + size
        raise ValueError(f"Block {bno:#x} (count={count}) not in DBRDISK1 extents")

    def read_obj(bno: int, count: int = 1) -> bytearray:
        s, e = get_block_slice(bno, count)
        return bytearray(dbr[s:e])

    def write_obj(bno: int, data: bytearray):
        s, e = get_block_slice(bno, len(data) // 4096)
        dbr[s:e] = data

    # 1. Update DBRDISK1 header logical_bytes
    struct.pack_into("<Q", dbr, 16, target_bytes)

    # 2. Update NXSB at bno=0x0 and bno=0xa (active xid=5 superblock)
    for bno in (0x0, 0xA):
        nx = read_obj(bno, 1)
        assert nx[32:36] == b"NXSB" and checksum_ok(nx)
        struct.pack_into("<Q", nx, 40, target_blocks)
        write_obj(bno, seal(nx))

    # 3. Build CIB[1..7] covering chunks 126..959
    cib_bnos = [0x2899, 0x288E, 0x2891, 0x2892, 0x2893, 0x2894, 0x2895, 0x2896]
    xid = 5
    OBJ_CIB_PHYS = 0x40000007

    for cib_idx in range(1, 8):
        bno = cib_bnos[cib_idx]
        first_chunk = cib_idx * 126
        last_chunk = min(target_chunks, first_chunk + 126)
        ci_count = last_chunk - first_chunk
        assert 1 <= ci_count <= 126

        cib = bytearray(4096)
        struct.pack_into("<QQII", cib, 8, bno, xid, OBJ_CIB_PHYS, 0)
        struct.pack_into("<II", cib, 32, cib_idx, ci_count)
        for k in range(ci_count):
            chunk_id = first_chunk + k
            ci_xid = 1 if (cib_idx == 1 and k < 2) else xid
            ci_addr = chunk_id * 32768
            struct.pack_into("<QQIIQ", cib, 40 + k * 32, ci_xid, ci_addr, 32768, 32768, 0)
        write_obj(bno, seal(cib))

    # 4. Update SFQ_IP free_queue B-tree at bno=0x7d (oid=0x403, xid=5)
    # Remove 0x2891..0x2896 (6 blocks now active as CIB[2..7]), keeping only (xid=2, 0x288d) and (xid=5, 0x2898)
    fq_ip = read_obj(0x7D, 1)
    assert checksum_ok(fq_ip)
    toc_len = 576
    key_base = 56 + toc_len
    area_end = 4096 - 40
    fq_ip[56:area_end] = b"\0" * (area_end - 56)
    struct.pack_into("<HHI", fq_ip, 32, 0x7, 0, 2)  # flags=7 (ROOT|LEAF|FIXED_KV), level=0, nkeys=2
    struct.pack_into("<HHHHHHHH", fq_ip, 40, 0, toc_len, 32, area_end - key_base - 32, 0xFFFF, 0, 0xFFFF, 0)
    # TOC[0]: k=0, v=0xffff (ghost = count 1); key[0] = (xid=2, paddr=0x288d)
    struct.pack_into("<HH", fq_ip, 56 + 0, 0x0, 0xFFFF)
    struct.pack_into("<QQ", fq_ip, key_base + 0, 2, 0x288D)
    # TOC[1]: k=0x10, v=0xffff (ghost = count 1); key[1] = (xid=5, paddr=0x2898)
    struct.pack_into("<HH", fq_ip, 56 + 4, 0x10, 0xFFFF)
    struct.pack_into("<QQ", fq_ip, key_base + 16, 5, 0x2898)
    # Update btree_info_t footer: bt_key_count = 2, bt_node_count = 1
    struct.pack_into("<QQ", fq_ip, 4096 - 16, 2, 1)
    write_obj(0x7D, seal(fq_ip))

    # 5. Update active SPACEMAN at bno=0x7b (1 block, oid=0x400, xid=5)
    sm = read_obj(0x7B, 1)
    assert checksum_ok(sm)
    dev = 48  # sm_dev[SD_MAIN]
    old_blocks, old_chunks, old_cibs, old_cabs, old_free, addr_off = struct.unpack_from("<QQIIQI", sm, dev)
    assert old_blocks == 4194304 and old_chunks == 128 and old_cibs == 2 and old_cabs == 0
    new_free = old_free + added_blocks
    struct.pack_into("<QQIIQI", sm, dev, target_blocks, target_chunks, len(cib_bnos), 0, new_free, addr_off)
    # Bump sm_dev[SD_TIER2].sm_addr_offset (at sm+0x80) past the 8 CIB pointers of SD_MAIN
    struct.pack_into("<I", sm, 0x80, addr_off + len(cib_bnos) * 8)
    # Update sm_fq[SFQ_IP].sfq_count (at sm+0xc8) from 8 to 2
    struct.pack_into("<Q", sm, 0xC8, 2)
    for i, bno in enumerate(cib_bnos):
        struct.pack_into("<Q", sm, addr_off + i * 8, bno)
    write_obj(0x7B, seal(sm))

    # Verify all updated objects pass Fletcher-64
    for bno in (0x0, 0xA, 0x7B, 0x7D, *cib_bnos):
        assert checksum_ok(read_obj(bno, 1)), f"Checksum failed at {bno:#x}"
    return bytes(dbr)

# ---------------------------------------------------------------------------
# 2. Patch merlinx-boot-step1-step2-v2.img -> merlinx-boot-step3-step4.img
# ---------------------------------------------------------------------------
base_img_path = OUT_DIR / "merlinx-boot-step1-step2-v2.img"
boot_img = bytearray(base_img_path.read_bytes())

dbr_off = 0x188C00
dbr_len = 413840
grown_dbr = grow_dbrdisk_120gib(bytes(boot_img[dbr_off : dbr_off + dbr_len]))
assert len(grown_dbr) == dbr_len
boot_img[dbr_off : dbr_off + dbr_len] = grown_dbr

k_base = 0x200800
k_len = 52514816
k = bytearray(boot_img[k_base : k_base + k_len])

ncmds = struct.unpack_from("<I", k, 16)[0]
off = 32
segs = []
for _ in range(ncmds):
    cmd, cmdsize = struct.unpack_from("<II", k, off)
    if cmd == 0x19:
        name = k[off + 8 : off + 24].rstrip(b"\0").decode()
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<4Q", k, off + 24)
        segs.append((name, vmaddr, vmsize, fileoff, filesize))
    off += cmdsize

def va2off(va: int) -> int:
    for name, vmaddr, vmsize, fileoff, filesize in segs:
        if vmaddr <= va < vmaddr + filesize:
            return fileoff + (va - vmaddr)
    raise ValueError(f"VA {va:#x} not found")

# Patch 1: gIOKernelConfigTables at 0xfffffff0071546f7 (file offset 0x1506f7, max 1374 bytes)
cfg_va = 0xFFFFFFF0071546F7
cfg_off = va2off(cfg_va)
old_cfg_end = k.find(b"\0", cfg_off)
old_cfg_len = old_cfg_end - cfg_off
assert old_cfg_len == 1374

new_cfg = (
    "("
    "{'IOClass'=ArmCpu;'IOProviderClass'=IOPlatformDevice;'IOProbeScore'=2000:32;},"
    "{'IOClass'=ArmVirtPlatform;'IOProviderClass'=IOPlatformExpertDevice;'IONameMatch'='linux,dummy-virt';'IOProbeScore'=10000:32;},"
    "{'IOClass'=ArmDTPlatform;'IOProviderClass'=IOPlatformExpertDevice;'IONameMatch'='device-tree';'IOProbeScore'=5000:32;},"
    "{'IOClass'=IOPanicPlatform;'IOProviderClass'=IOPlatformExpertDevice;'IOProbeScore'=0:32;},"
    "{'IOClass'=IOExclaveProxy;'IOProviderClass'=IOService;'IOExclaveProxy'=1:32;},"
    "{'IOClass'=NativeAES;'IOProviderClass'=IOResources;'IOResourceMatch'=IOBSD;'IOProbeScore'=1000:32;},"
    "{'IOClass'=NativeRamBlock;'IOMatchCategory'=NativeRamBlock;'IOProviderClass'=IOResources;'IOResourceMatch'=IOBSD;'IOProbeScore'=1000:32;},"
    "{'IOClass'=NativeEmmc;'IOMatchCategory'=NativeEmmcRoot;'IOProviderClass'=IOPlatformExpertDevice;'IOProbeScore'=500:32;},"
    "{"
    "'IOClass'=NativeMaliGpuService;"
    "'IOProviderClass'=IOPlatformDevice;"
    "'IONameMatch'='mediatek,mali';"
    "'IOProbeScore'=1000:32;"
    "'MetalPluginName'='NativeMaliMetal';"
    "'MetalPluginClassName'='NativeMaliMetalDevice';"
    "'NativeSandboxGrantProcess'='*';"
    "'IOCompatibilityProperties'={'IOClass'='IOAcceleratorES';};"
    "}"
    ")"
).encode("ascii")
assert len(new_cfg) < old_cfg_len, f"new_cfg too long: {len(new_cfg)} >= {old_cfg_len}"
k[cfg_off : cfg_off + old_cfg_len] = new_cfg + b"\0" * (old_cfg_len - len(new_cfg))

# Patch 2: IOService::matchInternal at 0xfffffff0078dfbc4 -> nop (0xd503201f)
match_off = va2off(0xFFFFFFF0078DFBC4)
assert struct.unpack_from("<I", k, match_off)[0] == 0x1400004F
struct.pack_into("<I", k, match_off, 0xD503201F)

# Patch 3: native_sandbox_grant at 0xfffffff007932894 -> b 0xfffffff007932934 (0x14000028)
grant_off = va2off(0xFFFFFFF007932894)
assert struct.unpack_from("<I", k, grant_off)[0] == 0xAA0003F3
struct.pack_into("<I", k, grant_off, 0x14000028)

# Patch 4: NativeMaliGpuClient::clientMemoryForType at 0xfffffff0079c902c -> mov w9, #0x401 (kIOMapAnywhere | kIOMapWriteCombineCache)
cmap_off = va2off(0xFFFFFFF0079C902C)
assert struct.unpack_from("<I", k, cmap_off)[0] == 0x52800029
struct.pack_into("<I", k, cmap_off, 0x52808029)

boot_img[k_base : k_base + k_len] = k

step34_img_path = OUT_DIR / "merlinx-boot-step3-step4.img"
step34_img_path.write_bytes(boot_img)
step34_zip_path = OUT_DIR / "merlinx-boot-step3-step4.zip"
with zipfile.ZipFile(step34_zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    zf.write(step34_img_path, arcname="merlinx-boot-step3-step4.img")

print("Built merlinx-boot-step3-step4.img sha256:", hashlib.sha256(boot_img).hexdigest())
print("Built merlinx-boot-step3-step4.zip size:", step34_zip_path.stat().st_size)
