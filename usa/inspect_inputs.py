#!/usr/bin/env python3
"""Validate user-supplied MM3D files without altering or installing them."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zlib


def sha(data):
    return hashlib.sha256(data).hexdigest()


def apply_bps(source, patch):
    if len(patch) < 19 or patch[:4] != b"BPS1":
        raise ValueError("Not a BPS patch")
    source_crc, target_crc, patch_crc = struct.unpack_from("<3I", patch, len(patch) - 12)
    if zlib.crc32(patch[:-4]) != patch_crc:
        raise ValueError("Patch CRC mismatch")
    pos = 4

    def number():
        nonlocal pos
        value, shift = 0, 1
        for _ in range(10):
            if pos >= len(patch) - 12:
                raise ValueError("Truncated BPS number")
            byte = patch[pos]
            pos += 1
            value += (byte & 127) * shift
            if byte & 128:
                return value
            shift <<= 7
            value += shift
        raise ValueError("Oversized BPS number")

    source_size, target_size, metadata_size = number(), number(), number()
    if source_size != len(source) or zlib.crc32(source) != source_crc:
        raise ValueError("Source executable does not match this BPS")
    if target_size > 32 * 1024 * 1024:
        raise ValueError("Unexpectedly large executable")
    pos += metadata_size
    if pos > len(patch) - 12:
        raise ValueError("Truncated BPS metadata")
    target = bytearray()
    source_relative = target_relative = 0
    while len(target) < target_size:
        command = number()
        kind, count = command & 3, (command >> 2) + 1
        if len(target) + count > target_size:
            raise ValueError("BPS action exceeds target size")
        if kind == 0:
            offset = len(target)
            if offset + count > len(source):
                raise ValueError("SourceRead out of bounds")
            target.extend(source[offset:offset + count])
        elif kind == 1:
            if pos + count > len(patch) - 12:
                raise ValueError("Truncated literal")
            target.extend(patch[pos:pos + count])
            pos += count
        else:
            encoded = number()
            delta = -(encoded >> 1) if encoded & 1 else encoded >> 1
            if kind == 2:
                source_relative += delta
                if source_relative < 0 or source_relative + count > len(source):
                    raise ValueError("SourceCopy out of bounds")
                target.extend(source[source_relative:source_relative + count])
                source_relative += count
            else:
                target_relative += delta
                for _ in range(count):
                    if not 0 <= target_relative < len(target):
                        raise ValueError("TargetCopy out of bounds")
                    target.append(target[target_relative])
                    target_relative += 1
    if pos != len(patch) - 12 or zlib.crc32(target) != target_crc:
        raise ValueError("Target size/CRC mismatch")
    return bytes(target)


def inspect(source_path, bps_path, header_path):
    source, bps, header = (p.read_bytes() for p in (source_path, bps_path, header_path))
    if len(header) not in (0x400, 0x800):
        raise ValueError("Expected a 0x400- or 0x800-byte exheader")
    target = apply_bps(source, bps)
    segments = {}
    for name, offset in (("text", 16), ("ro", 32), ("data", 48)):
        address, pages, size = struct.unpack_from("<3I", header, offset)
        segments[name] = {"address": hex(address), "pages": pages, "size": size}
    capacity = sum(s["pages"] * 4096 for s in segments.values())
    return {
        "files": {"source": str(source_path), "bps": str(bps_path), "header": str(header_path)},
        "source_sha256": sha(source), "bps_sha256": sha(bps), "header_sha256": sha(header),
        "target_sha256": sha(target), "target_size": len(target),
        "bps_crcs_pass": True, "segments": segments,
        "mapped_code_capacity": capacity,
        "header_can_hold_target": capacity >= len(target),
        "matches_previously_audited_restoration_target": sha(target) ==
            "f3d8b649adeaef0eab43288726a824f607d301bbf5a1b48ff9873404e241be3f",
        "integration_status": "No START menu, third page or live renderer adapter installed",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--bps", type=Path, required=True)
    parser.add_argument("--header", type=Path, required=True)
    args = parser.parse_args()
    report = inspect(args.source, args.bps, args.header)
    print(json.dumps(report, indent=2))
