#!/usr/bin/env python3
"""Build the SHAIDX1 index from byte offsets in the exact installed title file."""
import argparse
from pathlib import Path
import struct


def build_index(data):
    records = {}
    offset = 0
    for line in data.splitlines(keepends=True):
        key, sep, name = line.rstrip(b"\r\n").partition(b";")
        if sep and name:
            try:
                title_id = int(key.strip(), 16)
                if 0 < title_id <= 0xFFFFFFFFFFFFFFFF:
                    records.setdefault(title_id, offset)
            except ValueError:
                pass
        offset += len(line)
    output = bytearray(struct.pack("<8sII", b"SHAIDX1\0", 1, len(records)))
    for title_id, offset in sorted(records.items()):
        output.extend(struct.pack("<QQ", title_id, offset))
    return bytes(output)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=Path("romfs/titles/titles.txt"))
    parser.add_argument("--output", type=Path, default=Path("romfs/titles/titles.idx"))
    args = parser.parse_args()
    index = build_index(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(index)
    print(f"Wrote {(len(index)-16)//16} title index records to {args.output}")
