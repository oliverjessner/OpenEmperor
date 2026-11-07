"""Independent synthetic CLI checks; no original model fixture or World authority."""

import argparse
import hashlib
import json
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest


def synthetic_model():
    difficulties = ["Very Easy Modifiers", "Easy Modifiers", "Normal Modifier",
                    "Hard Modifiers", "Very Hard Modifiers"]
    lines = ["Authored inspector fixture", "Model data starts here:-", "BUILDING MODS"]
    for number, name in enumerate(difficulties):
        lines.append(f"{number},{name},{{," + ",".join(["100"] * 13) + ",}")
    lines.append("ALL BUILDINGS")
    for number, label, cost, employees, resource in [
            (0, "Authored zero", -2147483648, 2147483647, -9),
            (37, "Authored middle", 17, 3, 42),
            (4096, "Authored upper bound", 89, 0, 0)]:
        fields = [cost, 0, 0, 0, 0, employees, 0, 0, resource, 0, 0, 0, 0]
        lines.append(f"{number},{label},{{," + ",".join(map(str, fields)) + ",}")
    lines.append("HOUSE MODS")
    lines.extend(name + ",{," + ",".join(["0"] * 24) + ",}" for name in difficulties)
    lines.append("ALL HOUSES")
    lines.extend(f"Authored house {i},{{," + ",".join(["0"] * 24) + ",}" for i in range(15))
    lines.append("End of model data.")
    return ("\r\n".join(lines) + "\r\n").encode()


class InspectorTests(unittest.TestCase):
    def setUp(self):
        parent = Path(".local/oe55")
        parent.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="model-cli-", dir=parent)
        self.root = Path(self.temporary.name).resolve()
        self.source = self.root / "Model/EmperorBuildingModels.txt"
        self.source.parent.mkdir()
        self.source.write_bytes(synthetic_model())
        self.source.chmod(stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
        self.before = (self.source.read_bytes(), self.source.stat())

    def tearDown(self):
        self.assertEqual(self.source.read_bytes(), self.before[0])
        after = self.source.stat()
        self.assertEqual((after.st_size, after.st_mtime_ns, after.st_mode),
                         (self.before[1].st_size, self.before[1].st_mtime_ns, self.before[1].st_mode))
        self.assertEqual(sorted(p.relative_to(self.root).as_posix() for p in self.root.rglob("*")),
                         ["Model", "Model/EmperorBuildingModels.txt"])
        self.temporary.cleanup()

    def run_cli(self, *args, root=None):
        return subprocess.run([str(EXECUTABLE), str(root or self.root), *args],
                              capture_output=True, text=True, check=False, timeout=10)

    def test_legacy_default_has_no_observations(self):
        result = self.run_cli()
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        data = json.loads(result.stdout)
        self.assertEqual(data, {"status": "validated", "evidence": "RAW MODEL FACT",
            "reader_format_version": 1, "source_kind": "file", "source_path": str(self.source),
            "sha256": hashlib.sha256(self.before[0]).hexdigest(), "byte_count": len(self.before[0]),
            "building_rows": 3, "house_rows": 15, "building_modifier_rows": 5,
            "house_modifier_rows": 5, "building_fields": 13, "house_fields": 24})

    def test_sparse_rows_and_signed_columns_are_raw(self):
        result = self.run_cli("--building", "4096", "--building", "0", "--building", "37")
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        data = json.loads(result.stdout)
        self.assertEqual(data["building_observation_contract"], "raw-building-columns-v1")
        self.assertEqual(data["raw_buildings"], [
            {"model_id": 4096, "label": "Authored upper bound", "raw_cost": 89,
             "raw_employees": 0, "raw_resource": 0},
            {"model_id": 0, "label": "Authored zero", "raw_cost": -2147483648,
             "raw_employees": 2147483647, "raw_resource": -9},
            {"model_id": 37, "label": "Authored middle", "raw_cost": 17,
             "raw_employees": 3, "raw_resource": 42}])
        self.assertIn("recipe and commodity identity", data["unresolved_runtime_semantics"])
        self.assertEqual(data["sha256"], hashlib.sha256(self.before[0]).hexdigest())

    def test_missing_sparse_row_rejects_whole_query(self):
        result = self.run_cli("--building", "37", "--building", "1")
        self.assertEqual((result.returncode, result.stdout), (1, ""))
        self.assertEqual(json.loads(result.stderr)["model_id"], 1)
        self.assertEqual(json.loads(result.stderr)["section"], "query")

    def test_invalid_queries_refuse_before_source_read(self):
        cases = [("--building",), ("--unknown", "0"), ("--building", ""),
                 ("--building", "-1"), ("--building", "+1"), ("--building", "4097"),
                 ("--building", "42949672960"), ("--building", "3x"),
                 ("--building", " 37"), ("--building", "37", "--building", "037")]
        for args in cases:
            with self.subTest(args=args):
                result = self.run_cli(*args, root=self.root / "absent")
                self.assertEqual((result.returncode, result.stdout), (2, ""))
                self.assertTrue(result.stderr.startswith("Usage:"))

    def test_missing_source_keeps_loader_failure(self):
        result = self.run_cli("--building", "0", root=self.root / "absent")
        self.assertEqual((result.returncode, result.stdout), (1, ""))
        self.assertEqual(json.loads(result.stderr)["status"], "error")

    def test_non_utf8_label_is_not_replaced_or_partially_published(self):
        # A second independently owned input, never a mutation of the read-only fixture.
        other = self.root / "non-utf8"
        source = other / "Model/EmperorBuildingModels.txt"
        source.parent.mkdir(parents=True)
        payload = synthetic_model().replace(b"Authored middle", b"Authored \xff label")
        source.write_bytes(payload)
        try:
            result = self.run_cli("--building", "37", root=other)
            self.assertEqual((result.returncode, result.stdout), (1, ""))
            self.assertIn("UTF-8", json.loads(result.stderr)["reason"])
            self.assertEqual(source.read_bytes(), payload)
        finally:
            source.unlink()
            source.parent.rmdir()
            other.rmdir()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    EXECUTABLE = args.executable.resolve(strict=True)
    unittest.main(argv=[__file__, *remaining])
