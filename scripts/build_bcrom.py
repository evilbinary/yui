#!/usr/bin/env python3
"""
Split mquickjs 32-bit bytecode (.bc) into a tiny RAM skeleton + a read-only
region, plus a relocation table, and bundle all read-only regions into a single
bcrom.bin flashed to a dedicated mmap'ed flash partition.

Why: the ESP32 runtime currently keeps the whole .bc in RAM for its whole life
(JS_LoadBytecode requires the buffer to outlive the context). But relocation
only patches JSValue pointers that live in JSBytecodeHeader / FUNCTION_BYTECODE
/ VALUE_ARRAY blocks. STRING / BYTE_ARRAY / FLOAT64 blocks never get patched and
can stay in flash (XIP via esp_partition_mmap), so at load time only the
pointer-bearing skeleton needs to be copied to RAM.

Block layout of a 32-bit .bc (validated: walking all blocks sums to file length):
  JSBytecodeHeader32 (16 bytes) then contiguous mblocks. Each block is:
    word0 = [gc_mark:1 | mtag:3 | payload]
    mtag 3 (STRING):  payload bits7..31 = byte len (is_unique/is_ascii at bits4..5)
    mtag 5 (VALUE_ARRAY): payload = element count, then arr[] right after word0
    mtag 6 (BYTE_ARRAY): payload = byte len, data right after word0
    mtag 4 (FUNCTION_BYTECODE): 10 words
    mtag 2 (FLOAT64): 3 words
  JSValue pointers are encoded as (target_offset + 1) (odd values), so a value
  whose low bit is 1 is a pointer and must be relocated.

New per-file payload written back over the .bc in the SPIFFS staging dir:
  BcRamHeader (28 bytes) + ram region (incl 16B JSBytecodeHeader32 + ram blocks)
  + fixup entries (each 8 bytes: site, target)
bcrom.bin = all files' rom regions concatenated; each file's rom_off/rom_len is
recorded in its BcRamHeader.
"""
import argparse
import os
import struct
import sys

MAGIC_NEW = 0x53435242  # "BCRS" little-endian
MAGIC_BC = 0xACFB

MTAG_FREE, MTAG_OBJECT, MTAG_FLOAT64, MTAG_STRING, MTAG_FUNC, MTAG_VALUE, MTAG_BYTE = range(7)
ROM_MTAG = {MTAG_FLOAT64, MTAG_STRING, MTAG_BYTE}
RAM_MTAG = {MTAG_FUNC, MTAG_VALUE}

# JSFunctionBytecode JSValue field offsets (in words, from block start)
FUNC_VALUE_FIELDS = [1, 2, 3, 4, 5, 7, 8]  # func_name, byte_code, cpool, vars, ext_vars, filename, pc2line


def parse_blocks(data, start=16):
    """Yield (mtag, data_rel_offset, size, word) for every block in the data region.

    Offsets are relative to the start of the data region (i.e. after the
    JSBytecodeHeader32), matching the pointer encoding (ptr = target_offset + 1).
    """
    n = len(data)
    pos = start
    while pos < n:
        w = struct.unpack_from('<I', data, pos)[0]
        mtag = (w >> 1) & 7
        if mtag == MTAG_STRING:
            ln = w >> 7
            sz = 4 + ((ln + 4) & ~3)
        elif mtag == MTAG_VALUE:
            sz = 4 + (w >> 4) * 4
        elif mtag == MTAG_BYTE:
            sz = 4 + (((w >> 4) + 3) & ~3)
        elif mtag == MTAG_FUNC:
            sz = 40
        elif mtag == MTAG_FLOAT64:
            sz = 12
        else:
            raise ValueError(f"unexpected mtag {mtag} at offset {pos}")
        yield mtag, pos - start, sz
        pos += sz
    assert pos == n, f"block walk ended at {pos} != {n}"


def split_one(data):
    """Split one .bc into (ram_region, fixups, rom_region).

    fixups: list of (site_in_ram, is_rom, target_off) — site_in_ram and
    target_off are both relative to their region base.
    """
    assert len(data) >= 16
    magic, version, base_addr = struct.unpack_from('<HHI', data, 0)
    assert magic == MAGIC_BC, f"bad magic {magic:#x}"

    blocks = list(parse_blocks(data))
    assert sum(sz for _, _, sz in blocks) + 16 == len(data), \
        f"block walk mismatch: {sum(sz for _,_,sz in blocks) + 16} != {len(data)}"
    # ram region = [16B JSBytecodeHeader copy][ram blocks...]
    ram_buf = bytearray(data[0:16])
    rom_buf = bytearray()
    # map data-rel offset -> (is_rom, new offset in target region)
    target_map = {}
    for mtag, off, sz in blocks:
        foff = off + 16  # file offset
        if mtag in ROM_MTAG:
            target_map[off] = (1, len(rom_buf))
            rom_buf += data[foff:foff + sz]
        else:
            target_map[off] = (0, len(ram_buf))  # ram block starts after the header copy
            ram_buf += data[foff:foff + sz]

    fixups = []
    # header pointer fields (unique_strings @8, main_func @12, file offsets)
    for field_off in (8, 12):
        v = struct.unpack_from('<I', data, field_off)[0]
        if (v & 3) == 1:
            t = v - 1
            assert t in target_map, f"header ptr {t:#x} not a block start"
            fixups.append((field_off, target_map[t][0], target_map[t][1]))
        elif v != 0:
            raise ValueError(f"header field @{field_off} is not a pointer ({v:#x})")

    # per-block pointer fields
    for mtag, off, sz in blocks:
        foff = off + 16
        if mtag == MTAG_FUNC:
            new_off = target_map[off][1]
            for fw in FUNC_VALUE_FIELDS:
                site = foff + fw * 4
                v = struct.unpack_from('<I', data, site)[0]
                if (v & 3) == 1:
                    t = v - 1
                    assert t in target_map, f"func ptr {t:#x} not a block start"
                    fixups.append((new_off + fw * 4, target_map[t][0], target_map[t][1]))
        elif mtag == MTAG_VALUE:
            cnt = (struct.unpack_from('<I', data, foff)[0] >> 4)
            new_off = target_map[off][1]
            for i in range(cnt):
                site = foff + 4 + i * 4
                v = struct.unpack_from('<I', data, site)[0]
                if (v & 3) == 1:
                    t = v - 1
                    assert t in target_map, f"value_array ptr {t:#x} not a block start"
                    fixups.append((new_off + 4 + i * 4, target_map[t][0], target_map[t][1]))

    assert len(ram_buf) + len(rom_buf) == len(data), \
        f"byte conservation failed: {len(ram_buf)}+{len(rom_buf)} != {len(data)}"
    return bytes(ram_buf), fixups, bytes(rom_buf)


def pack_payload(ram_region, fixups, rom_off, rom_len):
    """BcRamHeader + ram region + fixup entries."""
    hdr = struct.pack('<IHHIIII', MAGIC_NEW, 1, 0, len(ram_region), rom_off, rom_len, len(fixups))
    fix = b''.join(struct.pack('<II', site, (0x80000000 if is_rom else 0) | toff)
                   for site, is_rom, toff in fixups)
    return hdr + ram_region + fix


def main():
    ap = argparse.ArgumentParser(description="split .bc into RAM skeleton + bcrom.bin")
    ap.add_argument("--staging", help="spiffs staging dir containing .bc files")
    ap.add_argument("--out", help="bcrom.bin output path")
    args = ap.parse_args()

    staging = args.staging
    if not staging:
        sys.exit("error: --staging required")
    bcfiles = []
    for dp, _dn, fns in os.walk(staging):
        for fn in fns:
            if fn.endswith('.bc'):
                bcfiles.append(os.path.join(dp, fn))
    bcfiles.sort()
    if not bcfiles:
        sys.exit("error: no .bc files in staging")

    rom_all = bytearray()
    per_file = []  # (path, ram_region, fixups, rom_off, rom_len)
    for path in bcfiles:
        data = open(path, 'rb').read()
        try:
            ram_region, fixups, rom_region = split_one(data)
        except Exception as e:
            print(f"ERROR: {path}: {e}", file=sys.stderr)
            sys.exit(1)
        rom_off = len(rom_all)
        rom_all += rom_region
        per_file.append((path, ram_region, fixups, rom_off, len(rom_region)))

    for path, ram_region, fixups, rom_off, rom_len in per_file:
        payload = pack_payload(ram_region, fixups, rom_off, rom_len)
        with open(path, 'wb') as f:
            f.write(payload)
        print(f"  {os.path.relpath(path, staging):38s} ram={len(ram_region):6d} "
              f"rom={rom_len:6d} fixups={len(fixups):4d}")

    rom_all = bytes(rom_all)
    out = args.out
    if out:
        with open(out, 'wb') as f:
            f.write(rom_all)
        print(f"bcrom.bin: {len(rom_all)} bytes -> {out}")

    ram_total = sum(len(r) for _, r, _, _, _ in per_file)
    print(f"totals: ram {ram_total} bytes, rom {len(rom_all)} bytes")


if __name__ == '__main__':
    main()
