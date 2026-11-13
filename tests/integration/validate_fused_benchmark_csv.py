#!/usr/bin/env python3
"""Validate fused Box benchmark CSV semantics without gating wall-time performance."""
import csv
import math
import sys
from pathlib import Path

BATCHES = (1, 32, 256, 1024)
REPEATS = range(3)
METHODS = ("cpu", "fused_validated")
HEADER = ["batch", "method", "repeat", "wall_ms", "cpu_substitutions", "dispatches"]


def validate(path: Path) -> None:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != HEADER:
            raise ValueError(f"Unexpected CSV header: {reader.fieldnames}")
        rows = list(reader)

    expected = {(batch, method, repeat) for batch in BATCHES
                for method in METHODS for repeat in REPEATS}
    seen = set()
    for row in rows:
        if None in row or any(value is None for value in row.values()):
            raise ValueError(f"Malformed CSV row: {row}")
        batch = int(row["batch"])
        method = row["method"]
        repeat = int(row["repeat"])
        key = (batch, method, repeat)
        if key not in expected or key in seen:
            raise ValueError(f"Unexpected or duplicate measurement: {key}")
        seen.add(key)
        wall_ms = float(row["wall_ms"])
        if not math.isfinite(wall_ms) or wall_ms < 0:
            raise ValueError(f"Invalid wall time for {key}: {wall_ms}")
        substitutions = int(row["cpu_substitutions"])
        dispatches = int(row["dispatches"])
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


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: validate_fused_benchmark_csv.py PATH")
    try:
        validate(Path(sys.argv[1]))
    except (OSError, ValueError) as exc:
        sys.exit(f"Invalid fused Box benchmark CSV: {exc}")
    print("Fused Box benchmark CSV contract validated (timings not gated)")
