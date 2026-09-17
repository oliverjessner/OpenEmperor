#!/usr/bin/env python3
"""Bounded, read-only PE32 string and absolute-pointer locator.

This reports locations and raw pointer occurrences, not proven code references
or semantics. It deliberately emits no original bytes or disassembly.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


def u16(data, at):
    if at < 0 or at + 2 > len(data):
        raise ValueError("truncated PE16 field")
    return struct.unpack_from("<H", data, at)[0]


def u32(data, at):
    if at < 0 or at + 4 > len(data):
        raise ValueError("truncated PE32 field")
    return struct.unpack_from("<I", data, at)[0]


def inspect_pe(path):
    data = path.read_bytes()
    if len(data) > 64 * 1024 * 1024 or data[:2] != b"MZ":
        raise ValueError("expected a bounded MZ file")
    pe = u32(data, 0x3C)
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")
    if u16(data, pe + 4) != 0x14C:
        raise ValueError("expected Intel 80386 PE")
    count = u16(data, pe + 6)
    opt = pe + 24
    opt_size = u16(data, pe + 20)
    if u16(data, opt) != 0x10B or opt + opt_size > len(data):
        raise ValueError("expected PE32 optional header")
    base = u32(data, opt + 28)
    table = opt + opt_size
    if count > 96 or table + count * 40 > len(data):
        raise ValueError("invalid section table")
    sections = []
    for i in range(count):
        at = table + i * 40
        name = data[at:at + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        raw_size, rva, raw_at = u32(data, at + 16), u32(data, at + 12), u32(data, at + 20)
        if raw_at > len(data) or raw_size > len(data) - raw_at:
            raise ValueError("section exceeds file")
        sections.append((name, raw_at, raw_size, rva))
    return data, base, sections


def location(file_offset, base, sections):
    for name, raw_at, raw_size, rva in sections:
        if raw_at <= file_offset < raw_at + raw_size:
            mapped_rva = rva + file_offset - raw_at
            return {"file_offset": file_offset, "rva": mapped_rva,
                    "va": base + mapped_rva, "section": name}
    return {"file_offset": file_offset, "rva": None, "va": None, "section": None}


def pointer_refs(data, value, base, sections, limit):
    if value is None or value > 0xFFFFFFFF:
        return []
    needle = struct.pack("<I", value)
    matches = []
    for name, at, size, _ in sections:
        if name not in (".text", ".rdata", ".data"):
            continue
        cursor = at
        while len(matches) < limit:
            found = data.find(needle, cursor, at + size)
            if found < 0:
                break
            matches.append(location(found, base, sections))
            cursor = found + 1
    return matches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("--needle", action="append", required=True)
    parser.add_argument("--max-refs", type=int, default=24)
    args = parser.parse_args()
    if not 1 <= args.max_refs <= 100:
        parser.error("--max-refs must be 1..100")
    data, base, sections = inspect_pe(args.exe)
    result = {"sha256": hashlib.sha256(data).hexdigest(), "image_base": base,
              "sections": [{"name": n, "file_offset": o, "raw_size": s, "rva": r}
                           for n, o, s, r in sections], "needles": []}
    for text in args.needle:
        encoded = text.encode("ascii") + b"\0"
        occurrences = []
        for _, start, size, _ in sections:
            cursor = start
            while len(occurrences) < 16:
                found = data.find(encoded, cursor, start + size)
                if found < 0:
                    break
                item = location(found, base, sections)
                item["direct_absolute_pointer_occurrences"] = pointer_refs(
                    data, item["va"], base, sections, args.max_refs)
                occurrences.append(item)
                cursor = found + 1
        result["needles"].append({"text": text, "occurrences": occurrences})
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
