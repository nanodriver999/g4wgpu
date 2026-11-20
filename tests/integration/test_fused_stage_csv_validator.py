import csv
import tempfile
import unittest
from pathlib import Path

from validate_fused_stage_csv import BATCHES, HEADER, REPEATS, TIME_FIELDS, read_and_validate


class StageCsvValidatorTest(unittest.TestCase):
    def write_rows(self, path, bad=None):
        rows = []
        for batch in BATCHES:
            for repeat in REPEATS:
                row = {"batch": str(batch), "repeat": str(repeat)}
                row.update({name: "0.0001" for name in TIME_FIELDS})
                row.update({"gpu_call_ms": "0.01", "total_ms": "0.03",
                            "cpu_substitutions": "0", "dispatches": "1"})
                rows.append(row)
        if bad is not None:
            rows[0].update(bad)
        with path.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=HEADER)
            writer.writeheader()
            writer.writerows(rows)

    def test_valid_contract(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / "stages.csv"
            self.write_rows(p)
            self.assertEqual(len(read_and_validate(p)), 12)

    def test_rejects_negative_and_nonfinite(self):
        for wrong in ("-1", "nan", "inf"):
            with self.subTest(wrong=wrong), tempfile.TemporaryDirectory() as folder:
                p = Path(folder) / "stages.csv"
                self.write_rows(p, {"staging_ms": wrong})
                with self.assertRaises(ValueError):
                    read_and_validate(p)

    def test_rejects_dispatch_count_or_duplicate(self):
        with tempfile.TemporaryDirectory() as folder:
            p = Path(folder) / "stages.csv"
            self.write_rows(p, {"dispatches": "0"})
            with self.assertRaises(ValueError):
                read_and_validate(p)


if __name__ == "__main__":
    unittest.main()
