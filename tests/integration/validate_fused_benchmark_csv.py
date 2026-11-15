#!/usr/bin/env python3
"""Validate fused Box benchmark CSV semantics without gating wall-time performance."""
import csv
import math
import re
import statistics
import sys
from pathlib import Path

BATCHES = (1, 32, 256, 1024)
REPEATS = range(3)
METHODS = ("cpu", "fused_validated")
HEADER = ["batch", "method", "repeat", "wall_ms", "cpu_substitutions", "dispatches"]
WALL_TIME_PATTERN = re.compile(r"(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?", re.ASCII)


def parse_count(value: str, field: str) -> int:
    """Accept only canonical unsigned decimal counters from the benchmark."""
    if not value.isascii() or not value.isdecimal() or (
        len(value) > 1 and value.startswith("0")
    ):
        raise ValueError(f"Invalid {field}: {value!r}")
    return int(value)


def validate(path: Path) -> None:
    with path.open(newline="", encoding="utf-8") as stream:
        # DictReader silently skips blank records; reject truncated or padded output.
        lines = stream.readlines()
        if any(not line.strip() for line in lines):
            raise ValueError("Blank CSV record")
        reader = csv.DictReader(lines)
        if reader.fieldnames != HEADER:
            raise ValueError(f"Unexpected CSV header: {reader.fieldnames}")
        rows = list(reader)

    expected = {(batch, method, repeat) for batch in BATCHES
                for method in METHODS for repeat in REPEATS}
    seen = set()
    for row in rows:
        if None in row or any(value is None for value in row.values()):
            raise ValueError(f"Malformed CSV row: {row}")
        batch = parse_count(row["batch"], "batch")
        method = row["method"]
        repeat = parse_count(row["repeat"], "repeat")
        key = (batch, method, repeat)
        if key not in expected or key in seen:
            raise ValueError(f"Unexpected or duplicate measurement: {key}")
        seen.add(key)
        wall_text = row["wall_ms"]
        if not WALL_TIME_PATTERN.fullmatch(wall_text):
            raise ValueError(f"Noncanonical wall time for {key}: {wall_text!r}")
        wall_ms = float(wall_text)
        if not math.isfinite(wall_ms) or math.copysign(1.0, wall_ms) < 0:
            raise ValueError(f"Invalid wall time for {key}: {wall_ms}")
        substitutions = parse_count(row["cpu_substitutions"], "cpu_substitutions")
        dispatches = parse_count(row["dispatches"], "dispatches")
        if substitutions < 0 or substitutions > batch:
            raise ValueError(f"Invalid CPU substitution count for {key}")
        if method == "cpu" and (substitutions != 0 or dispatches != 0):
            raise ValueError(f"CPU row has GPU activity: {key}")
        if method == "fused_validated" and dispatches != 1:
            raise ValueError(f"Fused row must dispatch exactly once: {key}")
        # This fixed, well-conditioned workload must not silently report
        # CPU-substituted results as successful GPU measurements.
        if method == "fused_validated" and substitutions != 0:
            raise ValueError(f"Unexpected fused CPU substitution for {key}")
    if seen != expected:
        raise ValueError(f"Missing measurements: {sorted(expected - seen)}")



def summarize(path: Path) -> list[str]:
    """Report per-batch median wall times; never use these as a CI speed gate."""
    validate(path)
    with path.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    lines = []
    for batch in BATCHES:
        times = {
            method: statistics.median(
                float(row["wall_ms"]) for row in rows
                if int(row["batch"]) == batch and row["method"] == method
            )
            for method in METHODS
        }
        ranges = {
            method: (
                min(float(row["wall_ms"]) for row in rows
                    if int(row["batch"]) == batch and row["method"] == method),
                max(float(row["wall_ms"]) for row in rows
                    if int(row["batch"]) == batch and row["method"] == method),
            )
            for method in METHODS
        }
        cpu, gpu = times["cpu"], times["fused_validated"]
        ratio = f"{cpu / gpu:.3f}x" if gpu > 0 else "n/a (zero GPU wall time)"
        lines.append(
            f"batch={batch} median_cpu_ms={cpu:.6f} "
            f"median_fused_ms={gpu:.6f} cpu_over_fused={ratio} "
            f"cpu_range_ms={ranges['cpu'][0]:.6f}..{ranges['cpu'][1]:.6f} "
            f"fused_range_ms={ranges['fused_validated'][0]:.6f}..{ranges['fused_validated'][1]:.6f}"
        )
    return lines


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: validate_fused_benchmark_csv.py PATH")
    try:
        validate(Path(sys.argv[1]))
    except (OSError, ValueError) as exc:
        sys.exit(f"Invalid fused Box benchmark CSV: {exc}")
    print("Fused Box benchmark CSV contract validated (timings not gated)")
    for line in summarize(Path(sys.argv[1])):
        print(line)
    print("Diagnostic end-to-end wall-time ratios only; no GPU kernel speedup claim")
