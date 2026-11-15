"""Independent synthetic CLI checks; no original model fixture or World authority."""

import argparse
from contextlib import contextmanager
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


def synthetic_harvest_model(source_ids=(31, 33)):
    """Keep source columns independent of the observed runtime selectors."""
    source_rows = []
    for model_id, label, cost, employees, resource in [
            (31, "Authored Quay", 137, 11, 91),
            (33, "Authored Hunter", 73, 19, -17)]:
        if model_id not in source_ids:
            continue
        fields = [cost, 0, 0, 0, 0, employees, 0, 0, resource, 0, 0, 0, 0]
        source_rows.append(f"{model_id},{label},{{," + ",".join(map(str, fields)) + ",}\r\n")
    marker = b"37,Authored middle"
    return synthetic_model().replace(marker, "".join(source_rows).encode() + marker)


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

    @contextmanager
    def harvest_input(self, source_ids=(31, 33), *, non_utf8=False):
        # Own every generated input and guard the same source bytes and metadata
        # used by the CLI. The ordinary three-row fixture remains untouched.
        with tempfile.TemporaryDirectory(prefix="harvest-", dir=self.root) as directory:
            root = Path(directory)
            source = root / "Model/EmperorBuildingModels.txt"
            source.parent.mkdir()
            payload = synthetic_harvest_model(source_ids)
            if non_utf8:
                payload = payload.replace(b"Authored Quay", b"Authored \xff Quay")
            source.write_bytes(payload)
            source.chmod(stat.S_IRUSR | stat.S_IRGRP | stat.S_IROTH)
            before = source.stat()
            try:
                yield root, payload
            finally:
                self.assertEqual(source.read_bytes(), payload)
                after = source.stat()
                self.assertEqual((after.st_size, after.st_mtime_ns, after.st_mode),
                                 (before.st_size, before.st_mtime_ns, before.st_mode))
                self.assertEqual(sorted(p.relative_to(root).as_posix() for p in root.rglob("*")),
                                 ["Model", "Model/EmperorBuildingModels.txt"])

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
        self.assertEqual(set(data), {
            "status", "evidence", "reader_format_version", "source_kind", "source_path",
            "sha256", "byte_count", "building_rows", "house_rows", "building_modifier_rows",
            "house_modifier_rows", "building_fields", "house_fields",
            "building_observation_contract", "raw_buildings", "unresolved_runtime_semantics"})
        self.assertEqual(data["unresolved_runtime_semantics"], [
            "World admission", "footprint", "field/plant ownership", "final purchase price",
            "recipe and commodity identity", "quantity and capacity", "production/calendar timing",
            "routes and demand"])
        self.assertEqual(result.stdout, json.dumps(data, indent=2, sort_keys=True) + "\n")

    def test_harvest_source_identity_and_raw_columns_remain_distinct(self):
        with self.harvest_input() as (root, payload):
            result = self.run_cli("--harvest-source", "33", "--harvest-source", "31", root=root)
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            data = json.loads(result.stdout)
            self.assertEqual(data["harvest_source_observation_contract"], "harvest-source-rules-v1")
            self.assertEqual(data["sha256"], hashlib.sha256(payload).hexdigest())
            self.assertEqual(data["byte_count"], len(payload))
            self.assertNotIn("raw_buildings", data)
            sources = data["harvest_sources"]
            self.assertEqual([row["model_id"] for row in sources], [33, 31])
            expected = {
                31: ("Authored Quay", 137, 11, 91, 2, "Fish", 49, 179),
                33: ("Authored Hunter", 73, 19, -17, 4, "Meat", 40, 183)}
            for row in sources:
                with self.subTest(model_id=row["model_id"]):
                    values = (row["label"], row["raw_cost"], row["raw_employees"],
                              row["raw_resource"], row["output_resource_id"],
                              row["output_resource_name"], row["work_actor_id"],
                              row["source_point_model_id"])
                    self.assertEqual(values, expected[row["model_id"]])
                    self.assertEqual(row["delivery_actor_id"], 19)
                    self.assertEqual(row["raw_return_amount"], 100 if row["model_id"] == 31 else 50)
                    self.assertEqual(row["raw_receipt_operation"],
                                     "assign" if row["model_id"] == 31 else "add")
                    self.assertEqual(row["ordinary_raw_cart_amount"], 100)
                    self.assertEqual(row["raw_source_capacity"],
                                     None if row["model_id"] == 31 else 400)
                    self.assertIs(row["external_effect_supported"], False)
                    self.assertEqual(row["rule_evidence"], "EXE_OBSERVED")
                    self.assertEqual(row["observed_exe_sha256"],
                                     "6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e")
                    self.assertIs(row["gameplay_enabled"], False)
                    self.assertIs(row["food_conversion_verified"], False)
                    self.assertIs(row["clock_mapping_verified"], False)
                    self.assertTrue(row["blockers"])
                    self.assertTrue(all(isinstance(item, str) and item for item in row["blockers"]))
                    self.assertEqual(row["geometry"]["body_cells"],
                                     [[0, 0], [1, 0], [0, 1], [1, 1]])
            self.assertEqual(sources[0]["staffing_tiers"], [
                {"minimum_percent": 100, "attempt_threshold": 1, "active_hunter_cap": 3},
                {"minimum_percent": 75, "attempt_threshold": 4, "active_hunter_cap": 2},
                {"minimum_percent": 50, "attempt_threshold": 16, "active_hunter_cap": 1},
                {"minimum_percent": 25, "attempt_threshold": 32, "active_hunter_cap": 1},
                {"minimum_percent": 1, "attempt_threshold": 64, "active_hunter_cap": 0}])
            self.assertEqual(sources[1]["work_tiers"], [
                {"minimum_percent": 100, "fishing_comparison": 408, "dock_comparison": 204},
                {"minimum_percent": 75, "fishing_comparison": 816, "dock_comparison": 408},
                {"minimum_percent": 50, "fishing_comparison": 1224, "dock_comparison": 612},
                {"minimum_percent": 25, "fishing_comparison": 1632, "dock_comparison": 816},
                {"minimum_percent": 1, "fishing_comparison": 2040, "dock_comparison": 1020}])
            self.assertTrue({"world", "world_snapshot", "schema_version", "rule_version",
                             "agriculture", "irrigation", "save"}.isdisjoint(data))

    def test_resolved_navigation_domains_and_conditional_prey_metadata(self):
        with self.harvest_input() as (root, _):
            result = self.run_cli("--harvest-source", "31", "--harvest-source", "33", root=root)
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            quay, hunter = json.loads(result.stdout)["harvest_sources"]
            self.assertEqual(quay["navigation_byte_examples"], [
                {"navigation_byte": 0, "origin_flood_allowed": True, "distance_preparation_extra": 0},
                {"navigation_byte": 254, "origin_flood_allowed": False, "distance_preparation_extra": 4},
                {"navigation_byte": 255, "origin_flood_allowed": False, "distance_preparation_extra": None}])
            initial = hunter["prey_initialization"]
            self.assertEqual(initial["attempts_per_eligible_point"], 7)
            self.assertEqual(initial["raw_selector_examples"], [
                {"raw_selector": 0, "actor_id": 77, "actor_identifier": "FIG_ANTELOPE"},
                {"raw_selector": 1, "actor_id": 76, "actor_identifier": "FIG_PHEASANT"},
                {"raw_selector": 2, "actor_id": 75, "actor_identifier": "FIG_WILD_PIG"}])
            self.assertTrue(initial["prerequisites"])
            self.assertTrue(all(initial["prerequisites"]))
            self.assertIs(hunter["gameplay_enabled"], False)

    def test_quay_observation_retains_every_oriented_dock_claim(self):
        with self.harvest_input() as (root, _):
            result = self.run_cli("--harvest-source", "31", root=root)
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            geometry = json.loads(result.stdout)["harvest_sources"][0]["geometry"]
            body = {tuple(cell) for cell in geometry["body_cells"]}
            orientations = geometry["orientations"]
            self.assertEqual([row["orientation"] for row in orientations], [0, 1, 2, 3])
            for row, dock in zip(orientations, [[0, -1], [2, 0], [1, 2], [-1, 1]]):
                with self.subTest(orientation=row["orientation"]):
                    occupied = {tuple(cell) for cell in row["occupied_cells"]}
                    water = {tuple(cell) for cell in row["intake_water_cells"]}
                    land = {tuple(cell) for cell in row["land_cells"]}
                    self.assertEqual(row["dock_cell"], dock)
                    self.assertEqual(occupied, body | {tuple(dock)})
                    self.assertEqual(len(row["occupied_cells"]), 5)
                    self.assertEqual(len(water), 8)
                    self.assertEqual(len(land), 2)
                    self.assertEqual(len(body & water), 2)
                    self.assertEqual(body & land, land)
                    self.assertTrue(water.isdisjoint(land))

    def test_mixed_observations_have_independent_duplicate_sets(self):
        with self.harvest_input() as (root, _):
            result = self.run_cli("--building", "31", "--harvest-source", "31",
                                  "--building", "33", "--harvest-source", "33", root=root)
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            data = json.loads(result.stdout)
            self.assertEqual([row["model_id"] for row in data["raw_buildings"]], [31, 33])
            self.assertEqual([row["model_id"] for row in data["harvest_sources"]], [31, 33])
            self.assertEqual(data["building_observation_contract"], "raw-building-columns-v1")

    def test_harvest_invalid_selectors_refuse_before_source_read(self):
        cases = [
            ("--harvest-source",), ("--harvest-source", ""),
            ("--harvest-source", "30"), ("--harvest-source", "32"),
            ("--harvest-source", "0"), ("--harvest-source", "4096"),
            ("--harvest-source", "-31"), ("--harvest-source", "+31"),
            ("--harvest-source", "31x"), ("--harvest-source", " 31"),
            ("--harvest-source", "31 "), ("--harvest-source", "42949672960"),
            ("--harvest-source", "31", "--harvest-source", "031"),
            ("--harvest-source", "33", "--harvest-source", "33"),
            ("--building", "37", "--harvest-source", "34")]
        for args in cases:
            with self.subTest(args=args):
                result = self.run_cli(*args, root=self.root / "absent")
                self.assertEqual((result.returncode, result.stdout), (2, ""))
                self.assertTrue(result.stderr.startswith("Usage:"))

    def test_missing_harvest_source_rejects_whole_mixed_query(self):
        with self.harvest_input(source_ids=(31,)) as (root, _):
            result = self.run_cli("--building", "37", "--harvest-source", "31",
                                  "--harvest-source", "33", root=root)
            self.assertEqual((result.returncode, result.stdout), (1, ""))
            error = json.loads(result.stderr)
            self.assertEqual((error["status"], error["section"], error["model_id"]),
                             ("error", "query", 33))

    def test_harvest_source_non_utf8_label_refuses_atomic_publication(self):
        with self.harvest_input(non_utf8=True) as (root, _):
            result = self.run_cli("--harvest-source", "33", "--harvest-source", "31", root=root)
            self.assertEqual((result.returncode, result.stdout), (1, ""))
            self.assertIn("UTF-8", json.loads(result.stderr)["reason"])

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
