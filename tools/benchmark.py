"""Measure the C programs, then verify the output of every run outside the timer."""

import argparse
import csv
import os
import random
import statistics
import subprocess
import tempfile
from pathlib import Path

from prepare import sizes
from verify import compare


CONFIGURATIONS = (1, 2, 4, 8)


def parse_fields(line):
    return dict(field.split("=", 1) for field in line.split() if "=" in field)


def run(root, ranks, manifest, output, recipe):
    args = [str(manifest), str(output), *map(str, recipe)]
    if ranks == 1:
        cmd = [str(root / "build/sequential"), *args]
    else:
        cmd = ["mpirun", "--map-by", "slot", "--bind-to", "none"]
        if os.geteuid() == 0:
            cmd.append("--allow-run-as-root")
        cmd += ["-np", str(ranks), str(root / "build/distributed"), *args]
    process = subprocess.run(cmd, text=True, capture_output=True, timeout=180)
    if process.returncode:
        raise RuntimeError(f"{cmd[0]} exited with {process.returncode}\n{process.stdout}{process.stderr}")
    lines = process.stdout.splitlines()
    totals, = (parse_fields(line) for line in lines if line.startswith("batch "))
    workers = [parse_fields(line) for line in lines if line.startswith("rank=")]
    return totals, workers


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as stream:
        output = csv.DictWriter(stream, fieldnames=tuple(rows[0]))
        output.writeheader()
        output.writerows(rows)


def summarize(records):
    cases = sorted({(row["images"], row["ranks"]) for row in records})
    medians = {}
    for case in cases:
        samples = [row["seconds"] for row in records if (row["images"], row["ranks"]) == case and row["run"] > 0]
        medians[case] = statistics.median(samples), min(samples), max(samples)
    return [dict(images=n, ranks=p, workers=max(1, p - 1), median_seconds=f"{median:.9f}",
                 min_seconds=f"{low:.9f}", max_seconds=f"{high:.9f}",
                 speedup=f"{medians[n, 1][0] / median:.6f}", images_per_second=f"{n / median:.6f}")
            for (n, p), (median, low, high) in medians.items()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset", type=Path)
    parser.add_argument("results", type=Path)
    parser.add_argument("--sizes", type=sizes, default=sizes("4,16,48"))
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--height", type=int, default=640)
    parser.add_argument("--brightness", type=int, default=18)
    args = parser.parse_args()
    if args.repetitions < 1:
        parser.error("repetitions must be positive")
    root = Path(__file__).resolve().parents[1]
    recipe = args.width, args.height, args.brightness
    cases, per_rank = [], []
    rng = random.Random(44708)
    args.results.mkdir(parents=True, exist_ok=False)
    for count in args.sizes:
        manifest = args.dataset.resolve() / f"batch-{count}.txt"
        with tempfile.TemporaryDirectory(prefix="pixel-dispatch-") as directory:
            reference = Path(directory) / "run-0-ranks-1"
            for repeat in range(args.repetitions + 1):
                order = list(CONFIGURATIONS)
                if repeat:
                    rng.shuffle(order)
                for ranks in order:
                    output = Path(directory) / f"run-{repeat}-ranks-{ranks}"
                    totals, worker_rows = run(root, ranks, manifest, output, recipe)
                    seconds = float(totals["seconds"])
                    counts = sum(int(row["completed"]) for row in worker_rows)
                    if (int(totals["completed"]) != count or int(totals["jobs"]) != count or counts != count
                            or [int(row["rank"]) for row in worker_rows] != list(range(ranks))
                            or any(int(row["failed"]) for row in worker_rows)
                            or (ranks > 1 and int(worker_rows[0]["completed"]) != 0)):
                        raise ValueError(f"unexpected completion counts: {totals} {worker_rows}")
                    digest = compare(reference, output, count, recipe[:2])
                    cases.append(dict(images=count, ranks=ranks, run=repeat, seconds=seconds,
                                      completed=count, rgb_channels_checked=count * args.width * args.height * 3,
                                      decoded_sha256=digest))
                    for row in worker_rows:
                        per_rank.append(dict(images=count, ranks=ranks, run=repeat, **row))
                    print(f"{count:2} images | {ranks} ranks | run {repeat} | {seconds:.4f} s | exact", flush=True)
        write_csv(args.results / "runs.csv", cases)
        write_csv(args.results / "per_rank.csv", per_rank)
        write_csv(args.results / "summary.csv", summarize(cases))


if __name__ == "__main__":
    main()
