"""Create ordered manifests and normalize input images for a fair comparison."""

import argparse
import csv
import hashlib
from pathlib import Path

from PIL import Image, ImageOps


def sizes(text):
    values = sorted(set(int(value) for value in text.split(",")))
    if not values or min(values) < 1:
        raise argparse.ArgumentTypeError("batch sizes must be positive")
    return values


def dimensions(image, longest):
    scale = longest / max(image.size)
    return tuple(max(1, round(length * scale)) for length in image.size)


def fixture(index):
    width = 127 + index * 19
    height = 81 + index * 13
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels.extend(((3 * x + y + 17 * index) % 256,
                           (x + 5 * y + 29 * index) % 256,
                           ((x // 12 + y // 14 + index) % 2) * 255))
    return Image.frombytes("RGB", (width, height), bytes(pixels))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--source", type=Path, help="folder of PNG, JPEG or WebP files")
    parser.add_argument("--sizes", type=sizes, default=sizes("4,16,48"))
    parser.add_argument("--mixed", action="store_true", help="rescale to 720, 1120 and 1550 px long sides")
    args = parser.parse_args()
    count = max(args.sizes)
    sources = []
    if args.source:
        images = (path for path in args.source.iterdir()
                  if path.is_file() and path.suffix.lower() in {".png", ".jpg", ".jpeg", ".webp"})
        sources = sorted(images, key=lambda path: (path.name.casefold(), path.name))
        if len(sources) < count:
            parser.error(f"need {count} images, found {len(sources)}")

    folder = args.destination
    (folder / "images").mkdir(parents=True)
    records = []
    for i in range(count):
        if sources:
            with Image.open(sources[i]) as original:
                image = ImageOps.exif_transpose(original).convert("RGB")
        else:
            image = fixture(i + 1)
        if args.mixed:
            image = image.resize(dimensions(image, (720, 1120, 1550)[i % 3]), Image.Resampling.LANCZOS)
        name = f"images/input-{i + 1:03d}.png"
        path = folder / name
        # Drop embedded profiles and metadata so both decoders see plain RGB.
        Image.frombytes("RGB", image.size, image.tobytes()).save(path, compress_level=3)
        records.append((name, *image.size, path.stat().st_size, hashlib.sha256(path.read_bytes()).hexdigest()))

    with (folder / "inputs.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(("input", "width", "height", "bytes", "png_sha256"))
        writer.writerows(records)
    for size in args.sizes:
        (folder / f"batch-{size}.txt").write_text("".join(row[0] + "\n" for row in records[:size]), encoding="utf-8")


if __name__ == "__main__":
    main()
