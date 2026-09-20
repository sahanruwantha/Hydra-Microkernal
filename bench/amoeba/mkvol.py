#!/usr/bin/env python3
"""
mkvol.py - build Amoeba multi-volume floppy images from a byte stream.

FSD-Amoeba's installer loads software with

    readvol $FLOP | gzip -d | starch -rv -if -

so an install floppy is simply a 512-byte Amoeba volume label followed by
a raw slice of the .starchgz stream. This script produces those images so
arbitrary packages (e.g. test.starchgz, which holds perf_null, rpc_client
and rpc_server) can be fed to a VM through QEMU floppy swaps.

Format reverse-engineered from the shipped floppies and confirmed against
the original sources on the FSD-Amoeba 2002B CD:
  module/amoeba/src/h/vollabel.h          -- struct volume
  module/amoeba/src/admin/multivol/multivol.c  -- writer
  module/amoeba/src/admin/multivol/volcrc.c    -- CRC routine

struct volume, all little endian, in sector 0:
    0x00  long   vol_magic      0xABCD1234
    0x04  long   vol_mediasize  media size in 512-byte blocks (2880 = 1.44M)
    0x08  long   vol_datasize   blocks of payload in THIS volume
    0x0C  long   vol_curvol     0-based index of this volume
    0x10  long   vol_nextvol    index of next volume, or -1 on the last
    0x14  ushort vol_crc        CRC-16/CCITT over the payload, init 0xFFFF
    0x16  char[20] vol_name     volume set name, e.g. "AMOEBA"

Payload starts at sector 1. multivol.c writes whole 512-byte blocks only,
so the stream is zero-padded up to a block boundary.

Verified: regenerating the shipped AMOEBA-0/1 and RAMDISK-0/1 labels from
their own payloads reproduces the stored CRCs exactly.

Usage:
    ./mkvol.py <input-file> <output-prefix> [--name NAME] [--media-blocks N]

Produces <output-prefix>-0.image, <output-prefix>-1.image, ...
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

BLOCK = 512
VL_MAGIC = 0xABCD1234
DEFAULT_MEDIA_BLOCKS = 2880          # 1.44 MB floppy

# CRC-16/CCITT table, polynomial 0x1021 (volcrc.c).
_TAB = []
for _i in range(256):
    _c = _i << 8
    for _ in range(8):
        _c = ((_c << 1) ^ 0x1021) & 0xFFFF if _c & 0x8000 else (_c << 1) & 0xFFFF
    _TAB.append(_c)


def volcrc(data: bytes, crc: int = 0xFFFF) -> int:
    """CRC as computed by volcrc(), seeded the way multivol.c seeds it."""
    for b in data:
        crc = ((crc << 8) ^ _TAB[((crc >> 8) ^ b) & 0xFF]) & 0xFFFF
    return crc


def make_label(media_blocks: int, data_blocks: int, curvol: int,
               nextvol: int, crc: int, name: str) -> bytes:
    lab = bytearray(BLOCK)
    struct.pack_into("<IIIiiH", lab, 0,
                     VL_MAGIC, media_blocks, data_blocks,
                     curvol, nextvol, crc)
    encoded = name.encode("ascii")[:19]
    lab[0x16:0x16 + len(encoded)] = encoded
    return bytes(lab)


def build(src: Path, prefix: Path, name: str, media_blocks: int) -> list[Path]:
    data = src.read_bytes()
    payload_blocks = media_blocks - 1            # sector 0 is the label
    chunk_size = payload_blocks * BLOCK

    chunks = [data[i:i + chunk_size] for i in range(0, len(data), chunk_size)]
    if not chunks:
        chunks = [b""]

    written = []
    for idx, chunk in enumerate(chunks):
        # multivol.c writes whole blocks only; pad the tail with zeros.
        padded = chunk.ljust((len(chunk) + BLOCK - 1) // BLOCK * BLOCK, b"\0")
        data_blocks = len(padded) // BLOCK
        last = idx == len(chunks) - 1
        label = make_label(
            media_blocks=media_blocks,
            data_blocks=data_blocks,
            curvol=idx,
            nextvol=-1 if last else idx + 1,
            crc=volcrc(padded),
            name=name,
        )
        # Pad the image out to the full media size so QEMU sees a 1.44 MB floppy.
        image = label + padded
        image = image.ljust(media_blocks * BLOCK, b"\0")

        out = prefix.with_name(f"{prefix.name}-{idx}.image")
        out.write_bytes(image)
        written.append(out)
        print(f"{out}  vol={idx} next={'last' if last else idx + 1} "
              f"data_blocks={data_blocks} crc={volcrc(padded):#06x}")
    return written


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path, help="stream to split (e.g. test.starchgz)")
    ap.add_argument("prefix", type=Path, help="output prefix, e.g. ./TEST")
    ap.add_argument("--name", default="AMOEBA", help="volume set name (default: AMOEBA)")
    ap.add_argument("--media-blocks", type=int, default=DEFAULT_MEDIA_BLOCKS,
                    help="media size in 512-byte blocks (default: 2880 = 1.44MB)")
    args = ap.parse_args(argv[1:])

    if not args.input.is_file():
        print(f"ERROR: no such file: {args.input}", file=sys.stderr)
        return 1

    made = build(args.input, args.prefix, args.name, args.media_blocks)
    print(f"\n{len(made)} volume(s) written.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
