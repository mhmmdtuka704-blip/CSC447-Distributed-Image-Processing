"""Require exactly the expected output files and compare all decoded RGB bytes."""

import argparse
import hashlib
import struct
from pathlib import Path

from PIL import Image


def snapshot(folder, count, size):
    expected = {f"job-{index:05d}.png" for index in range(1, count + 1)}
    actual = {item.name for item in folder.iterdir()}
    if actual != expected:
        raise ValueError(f"{folder}: missing {sorted(expected - actual)}, extra {sorted(actual - expected)}")
    for name in sorted(expected):
        with Image.open(folder / name) as image:
            if image.mode != "RGB" or image.size != size:
                raise ValueError(f"{folder / name}: expected RGB {size}, got {image.mode} {image.size}")
            yield name, image.tobytes()


def compare(reference, candidate, count, size):
    fingerprint = hashlib.sha256(struct.pack(">III", count, *size))
    for (name, expected), (_, actual) in zip(snapshot(reference, count, size), snapshot(candidate, count, size), strict=True):
        if expected != actual:
            offset = next(i for i, (a, b) in enumerate(zip(expected, actual)) if a != b)
            pixel, channel = divmod(offset, 3)
            y, x = divmod(pixel, size[0])
            raise ValueError(f"{name}: first mismatch at ({x}, {y}), RGB channel {channel}: "
                             f"expected {expected[offset]}, got {actual[offset]}")
        fingerprint.update(actual)
    return fingerprint.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    for value in ("count", "width", "height"):
        parser.add_argument(value, type=int)
    args = parser.parse_args()
    if min(args.count, args.width, args.height) <= 0:
        parser.error("count and dimensions must be positive")
    try:
        digest = compare(args.reference, args.candidate, args.count, (args.width, args.height))
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(f"PASS: {args.count} files, {args.count * args.width * args.height:,} RGB pixels, zero differences")
    print(f"Decoded batch SHA-256: {digest}")


if __name__ == "__main__":
    main()
