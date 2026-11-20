#!/usr/bin/env python3
"""Validate host-stage timing CSV semantics without speed thresholds."""
import csv
import math
import sys
from pathlib import Path

BATCHES = (1, 32, 256, 1024)
REPEATS = range(3)
TIME_FIELDS = (
    "cpu_reference_ms", "eligibility_ms", "backend_preflight_ms",
    "staging_ms", "pipeline_ms", "resources_ms", "upload_ms",
    "submit_ms", "readback_ms", "decode_ms", "reconcile_ms",
    "gpu_call_ms", "total_ms",
)
HEADER = ["batch", "repeat", *TIME_FIELDS,
          "cpu_substitutions", "dispatches"]


def read_and_validate(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != HEADER:
            raise ValueError("Unexpected stage CSV header")
        rows = list(reader)
    expected = {(n, repeat) for n in BATCHES for repeat in REPEATS}
    seen = set()
    for row in rows:
        if None in row or any(v is None for v in row.values()):
            raise ValueError("Malformed stage CSV row")
        try:
            batch = int(row["batch"])
            repeat = int(row["repeat"])
        except ValueError as error:
            raise ValueError("Invalid batch/repeat") from error
        if str(batch) != row["batch"] or str(repeat) != row["repeat"]:
            raise ValueError("Noncanonical batch/repeat")
        key = (batch, repeat)
        if key not in expected or key in seen:
            raise ValueError("Unexpected or duplicate batch/repeat")
        seen.add(key)
        if row["cpu_substitutions"] != "0" or row["dispatches"] != "1":
            raise ValueError("Unexpected substitution/dispatch count")
        try:
            durations = {k: float(row[k]) for k in TIME_FIELDS}
        except ValueError as error:
            raise ValueError("Invalid stage duration") from error
        if any(not math.isfinite(v) or
               math.copysign(1.0, v) < 0 for v in durations.values()):
            raise ValueError("Non-finite or negative host stage duration")
        if durations["gpu_call_ms"] > durations["total_ms"] + 0.02:
            raise ValueError("GPU call is not enclosed by total wall time")
        if (durations["cpu_reference_ms"] +
                durations["eligibility_ms"] +
                durations["gpu_call_ms"] +
                durations["reconcile_ms"] >
                durations["total_ms"] + 0.03):
            raise ValueError("Host sub-stages exceed enclosing wall time")
    if seen != expected:
        raise ValueError("Missing stage CSV samples")
    return rows


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: validate_fused_stage_csv.py PATH")
    try:
        result = read_and_validate(Path(sys.argv[1]))
    except (OSError, ValueError) as exc:
        sys.exit(f"Invalid fused stage timings: {exc}")
    print(f"Fused stage timing CSV validated: {len(result)} records (no speed gate)")
