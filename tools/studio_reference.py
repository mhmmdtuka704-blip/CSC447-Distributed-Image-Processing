"""Save Project 8 reference PNGs from the Creative Space Studio running on this PC."""

import argparse
import json
from pathlib import Path
from urllib.parse import urlencode
from urllib.request import Request, urlopen


STUDIO = "http://127.0.0.1:8765"


def request(path, values=None, data=None):
    url = STUDIO + path
    if values is not None:
        url += "?" + urlencode(values)
    headers = {"X-Studio-Request": "1"}
    if data is not None:
        headers["Content-Type"] = "application/octet-stream"
    with urlopen(Request(url, data=data, headers=headers), timeout=90) as response:
        return response.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("destination", type=Path)
    parser.add_argument("width", type=int)
    parser.add_argument("height", type=int)
    parser.add_argument("brightness", type=int)
    args = parser.parse_args()
    manifest = args.manifest.resolve()
    inputs = [manifest.parent / line for line in manifest.read_text(encoding="utf-8").splitlines() if line]
    options = dict(resize_width=args.width, resize_height=args.height, preserve_aspect=False,
                   interpolation="nearest", brightness=args.brightness, filters=["denoise"])
    project = json.loads(request("/api/student-path/app8"))
    print(project["title"], flush=True)
    args.destination.mkdir()
    values = dict(count=len(inputs), module="app8", options=json.dumps(options))
    batch = json.loads(request("/api/batches", values, b""))["id"]
    for index, image in enumerate(inputs, 1):
        item = json.loads(request(f"/api/batches/{batch}/images", {"name": image.name}, image.read_bytes()))
        (args.destination / f"job-{index:05d}.png").write_bytes(request(item["image_url"]))
    print(f"Saved {len(inputs)} Creative Space Studio reference outputs for batch {batch}")


if __name__ == "__main__":
    main()
