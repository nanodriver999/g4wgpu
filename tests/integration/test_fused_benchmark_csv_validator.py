#!/usr/bin/env python3
"""Negative and positive contract tests for the fused benchmark CSV validator."""
import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

VALIDATOR = Path(__file__).with_name("validate_fused_benchmark_csv.py")
spec = importlib.util.spec_from_file_location("fused_csv_validator", VALIDATOR)
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)


def rows():
    return [
        {"batch": str(batch), "method": method, "repeat": str(repeat),
         "wall_ms": "0.01", "cpu_substitutions": "0",
         "dispatches": "1" if method == "fused_validated" else "0"}
        for batch in validator.BATCHES
        for method in validator.METHODS
        for repeat in validator.REPEATS
    ]


class FusedCsvValidatorTest(unittest.TestCase):
    def check(self, data, header=None, error=False):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "benchmark.csv"
            with path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=header or validator.HEADER)
                writer.writeheader()
                writer.writerows(data)
            if error:
                with self.assertRaises(ValueError):
                    validator.validate(path)
            else:
                validator.validate(path)

    def test_valid(self):
        self.check(rows())

    def test_missing_row(self):
        self.check(rows()[:-1], error=True)

    def test_duplicate_row(self):
        data = rows()
        data[-1] = data[0].copy()
        self.check(data, error=True)

    def test_nonfinite_and_negative_times(self):
        for value in ("nan", "inf", "-0.01"):
            with self.subTest(value=value):
                data = rows()
                data[0]["wall_ms"] = value
                self.check(data, error=True)

    def test_invalid_cpu_counts(self):
        for field, value in (("dispatches", "1"), ("cpu_substitutions", "1")):
            with self.subTest(field=field):
                data = rows()
                data[0][field] = value
                self.check(data, error=True)

    def test_invalid_fused_dispatch(self):
        data = rows()
        fused = next(row for row in data if row["method"] == "fused_validated")
        fused["dispatches"] = "0"
        self.check(data, error=True)

    def test_fused_cpu_substitution_is_rejected(self):
        data = rows()
        fused = next(row for row in data if row["method"] == "fused_validated")
        fused["cpu_substitutions"] = "1"
        self.check(data, error=True)

    def test_out_of_range_substitutions(self):
        data = rows()
        fused = next(row for row in data if row["method"] == "fused_validated")
        fused["cpu_substitutions"] = str(int(fused["batch"]) + 1)
        self.check(data, error=True)

    def test_unexpected_header(self):
        self.check(rows(), header=validator.HEADER[::-1], error=True)

    def test_unexpected_measurement(self):
        data = rows()
        data[0]["repeat"] = "3"
        self.check(data, error=True)


if __name__ == "__main__":
    unittest.main()
