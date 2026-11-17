#!/usr/bin/env python3
"""Read-only compatibility audit for the recovered MM3D high-resolution overlay."""
import argparse
import json
from pathlib import Path
from inspect_inputs import apply_bps, inspect, sha


def records(patch):
    if not patch.startswith(b'PATCH'):
        raise ValueError('Invalid IPS signature')
    cursor = 5
    spans = []
    while True:
        if cursor + 3 > len(patch):
            raise ValueError('Missing IPS terminator')
        if patch[cursor:cursor + 3] == b'EOF':
            if cursor + 3 != len(patch):
                raise ValueError('Unexpected IPS trailer')
            return spans
        if cursor + 5 > len(patch):
            raise ValueError('Truncated IPS record')
        offset = int.from_bytes(patch[cursor:cursor + 3], 'big')
        count = int.from_bytes(patch[cursor + 3:cursor + 5], 'big')
        cursor += 5
        if count:
            if cursor + count > len(patch):
                raise ValueError('Truncated IPS data')
            data = patch[cursor:cursor + count]
            cursor += count
        else:
            if cursor + 3 > len(patch):
                raise ValueError('Truncated IPS run')
            count = int.from_bytes(patch[cursor:cursor + 2], 'big')
            if not count:
                raise ValueError('Empty IPS run')
            data = patch[cursor + 2:cursor + 3] * count
            cursor += 3
        if any(offset < other + len(value) and other < offset + len(data)
               for other, value in spans):
            raise ValueError('Overlapping IPS records')
        spans.append((offset, data))


def audit(source_path, bps_path, header_path, ips_path):
    report = inspect(source_path, bps_path, header_path)
    source = source_path.read_bytes()
    target = apply_bps(source, bps_path.read_bytes())
    ips = ips_path.read_bytes()
    spans = records(ips)
    result = bytearray(target)
    checked = []
    for offset, data in spans:
        end = offset + len(data)
        if end > len(source) or end > len(target):
            raise ValueError('IPS record outside source/target executable')
        agrees = source[offset:end] == target[offset:end]
        checked.append({'offset': hex(offset), 'length': len(data),
                        'restoration_matches_stock_at_site': agrees})
        if not agrees:
            raise ValueError(f'Restoration changes overlay site {offset:#x}')
        result[offset:end] = data
    report.update({
        'ips_file': str(ips_path), 'ips_sha256': sha(ips),
        'ips_records': checked, 'patched_target_sha256': sha(result),
        'overlay_has_no_conflicts_at_written_sites': True,
        'console_tested_this_reconstructed_pair': False,
        'note': 'Static compatibility only; does not validate runtime behavior or implement the new menus.'
    })
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'bps', 'header', 'ips'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(audit(args.source, args.bps, args.header, args.ips), indent=2))
