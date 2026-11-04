"""OE35 runner guards using explicit fake children and authored JSON only.

These process/error tests provide no evidence of original gameplay. Real-source
Release/Debug/sanitizer diagnosis runs are separate acceptance requirements.
"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
SPEC = importlib.util.spec_from_file_location("economy_fire_diagnosis_runner", REPO / "tools/test_economy_fire_diagnosis.py")
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class EconomyFireDiagnosisRunnerContract(unittest.TestCase):
    def setUp(self):
        runner.PRIVATE.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="runner-contract-", dir=runner.PRIVATE)
        self.root = Path(self.temporary.name)
        self.data = self.root / "explicit-fake-original"
        (self.data / "Model").mkdir(parents=True)
        (self.data / "Cities").mkdir()
        (self.data / "Model/EmperorBuildingModels.txt").write_text("Explicit fake input; no original gameplay model.\n")
        (self.data / "Cities/Xia.map").write_text("Explicit fake map; never gameplay evidence.\n")
        self.build = self.root / "build"; self.build.mkdir()
        self.binary = self.build / "explicit-fake-child"
        self.output = self.root / "output"
        self.fake("fail")

    def tearDown(self):
        self.temporary.cleanup()

    def fake(self, behavior):
        self.binary.write_text("#!/usr/bin/env python3\nBEHAVIOR = " + repr(behavior) + "\n" + '''
import hashlib
import json
import os
from pathlib import Path
import sys
import time
a = dict(zip(sys.argv[1::2], sys.argv[2::2]))
mode = a["--mode"]; data = Path(a["--data"])
if BEHAVIOR == "fail":
    print("Explicit fake child failure; no gameplay executed")
    raise SystemExit(17)
if BEHAVIOR == "timeout":
    time.sleep(20)
out = Path(a["--output"]); out.mkdir()
ticks = int(a["--ticks"])
def summary(end):
    upkeep = (end - 816) // 1000
    value = {"first_goal_tick": None,
        "goal_at_end": {"reached": False, "tick": end, "funds": 11920-upkeep,
            "population": 1, "taxes": 0, "maintenance": upkeep, "construction": 80},
        "first_negative_tick": None, "minimum_funds": 11920-upkeep,
        "taxes_delta": 0, "maintenance_delta": upkeep, "operating_balance": -upkeep,
        "goal_loss_ticks": 0,
        "funds_at_seed_plus_20000": 11900 if end >= 20816 else None,
        "funds_at_goal_plus_20000": None,
        "funds_20k_nonnegative": True if end >= 20816 else None,
        "goal_20k_financeable": None}
    if BEHAVIOR == "goal-bool":
        value["goal_at_end"]["reached"] = "false"
    if BEHAVIOR == "invented-goal":
        value["goal_at_end"]["reached"] = True
    if BEHAVIOR in ("false-first-goal", "missing-first-goal", "wrong-first-goal-tick", "nonbool-first-goal"):
        value["first_goal_tick"] = 816
        value["first_goal"] = {"reached": True, "tick": 816}
        if BEHAVIOR == "false-first-goal": value["first_goal"]["reached"] = False
        if BEHAVIOR == "missing-first-goal": del value["first_goal"]
        if BEHAVIOR == "wrong-first-goal-tick": value["first_goal"]["tick"] = 815
        if BEHAVIOR == "nonbool-first-goal": value["first_goal"]["reached"] = 1
    return value
def checkpoint(label, end):
    upkeep = (end - 816) // 1000
    h = {"occupants": 1, "type": 3, "hemp": 0, "ceramics": 0, "tea": 0,
         "silk": 0, "bronzeware": 0, "lacquerware": 0}
    w = {"ticks":end, "treasury":11920-upkeep, "construction_spent_total":80,
        "taxes_collected_total":0, "maintenance_spent_total":upkeep,
        "command_sequence":1, "next_building_id":2, "next_courier_id":1,
        "buildings":[{"id":1,"kind":6,"placed":True,"health_risk":0,
             "health_protection_until_tick":0,"sick_until_tick":0,"taxes_paid_total":0}],
        "couriers":[], "original_economy":{"model_source":{"difficulty":2,
            "sha256":hashlib.sha256((data/"Model/EmperorBuildingModels.txt").read_bytes()).hexdigest()},
            "houses":[{"building_id":1,"house":h}]},
        "original_production":{"development_revision":27,"residents_admitted":1,
            "finance":{"houses":[{"building_id":1,"rows":[{"resident_units":0} for _ in range(15)]}]}},
        "original_goods":{"stocks":[],"cargo":[],"totals":[
            {"good":g,"produced":0,"consumed":0,"imported":0,"demolished":0} for g in range(6)]}}
    doc = {"format":"openemperor-sandbox-save","schema_version":26,
        "rules":{"id":"sandbox-city-v16","version":10},
        "profiles":{"map_permissions":{"version":3}},
        "map":{"relative_path":"Cities/Xia.map", "sha256":hashlib.sha256((data/"Cities/Xia.map").read_bytes()).hexdigest()},
        "world":w}
    if BEHAVIOR == "ledger":
        w["treasury"] += 1
    if BEHAVIOR == "source":
        w["original_economy"]["model_source"]["difficulty"] = 1
    if BEHAVIOR == "contract":
        doc["schema_version"] = 25
    if BEHAVIOR == "duplicate":
        w["buildings"].append(dict(w["buildings"][0]))
    if BEHAVIOR == "goods-grant":
        w["original_goods"]["totals"][0]["imported"] = 1
    if BEHAVIOR == "missing-owner":
        w["original_economy"]["houses"][0]["building_id"] = 99
    observation = {"format":"openemperor-economy-fire-observation-v1", "begin_tick":816,
        "end_tick":end,"observed_ticks":end-816,"summary":summary(end),
        "months":[],"invoices":[],"goal_changes":[],"fire_events":[],"controls":[],
        "supply_by_house":[],"building_costs":[],"transport":{},"explicit_fake_no_gameplay":True}
    if BEHAVIOR == "mismatch" and mode == "resume" and label == "result":
        observation["intentional_mismatch"] = True
    (out/(label+".save.json")).write_text(json.dumps(doc,sort_keys=True))
    (out/(label+".snapshot.json")).write_text(json.dumps({"wrong":True} if BEHAVIOR == "snapshot" else w,sort_keys=True))
    (out/(label+".report.json")).write_text(json.dumps(observation,sort_keys=True))
end = 816 if mode == "seed" else 816+ticks
manifest = {"format":"explicit-fake-manifest-no-gameplay", "status":"PASS", "mode":mode,
    "variant":a["--variant"], "begin_tick":0 if mode == "seed" else 816,
    "end_tick":end,"ticks":816 if mode == "seed" else ticks,
    "creator_mode":mode != "resume", "restored_live_world":mode == "resume",
    "protected_reference_compared":mode == "run", "input_preserved":True,
    "rule":10,"schema":26,"policy":3,"revision":27,"original_map":True,
    "original_model":True,"snapshot_injection":False,"summary":summary(end)}
if mode != "resume":
    manifest["initial_commands"] = [{"cost":80,"funds_before":12000,"funds_after":11920}]
if BEHAVIOR == "restore-expected" and mode == "run":
    manifest["creator_mode"] = False; manifest["restored_live_world"] = True
if BEHAVIOR == "short" and mode != "seed":
    manifest["ticks"] -= 1
if BEHAVIOR == "injected":
    manifest["snapshot_injection"] = True
if BEHAVIOR == "command-grant" and mode == "seed":
    manifest["initial_commands"][0]["funds_after"] += 1
if BEHAVIOR == "missing-commands" and mode == "seed":
    manifest["initial_commands"] = []
if mode == "seed":
    checkpoint("seed", 816)
else:
    if mode == "run":
        (out/"creator-reference.save.json").write_bytes(Path(a["--input"]).read_bytes())
        if BEHAVIOR == "creator-mismatch":
            (out/"creator-reference.save.json").write_text("Explicit fake wrong creator")
    checkpoint("midpoint",816+ticks//2)
    checkpoint("result",end)
    if BEHAVIOR == "mutate-input" and mode == "resume":
        Path(a["--input"]).write_text("Explicit fake modified input")
if BEHAVIOR == "mutate-original":
    (data/"Cities/Xia.map").write_text("Explicit fake original mutation")
if BEHAVIOR == "touch-original":
    path = data/"Cities/Xia.map"; stat = path.stat()
    os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns+1000))  # 1 us: NTFS keeps 100 ns steps
if BEHAVIOR == "mutate-binary":
    with Path(sys.argv[0]).open("a") as stream:
        stream.write("\\n# Explicit fake binary mutation\\n")
(out/"manifest.json").write_text(json.dumps(manifest,sort_keys=True))
''')
        self.binary.chmod(0o700)

    def invoke(self, behavior="ok", *, output=None, data=None, ticks=20000, variant="supplied", **kwargs):
        self.fake(behavior)
        with mock.patch.object(runner, "MODEL_SHA", runner.digest(self.data / "Model/EmperorBuildingModels.txt")), \
                mock.patch.object(runner, "prepared", return_value=([[0] * 24 for _ in range(15)], {})):
            return runner.workflow(self.binary, data or self.data, output or self.output,
                                   ticks, variant, kwargs.get("timeout", 20))

    def failure(self, behavior, pattern):
        with self.assertRaisesRegex((RuntimeError, KeyError), pattern):
            self.invoke(behavior)
        if self.output.exists():
            report = runner.load(self.output / "report.json")
            self.assertNotEqual(report["status"], "PASS")
            return report

    def test_help_requires_no_original_inputs(self):
        result = subprocess.run([sys.executable, str(REPO / "tools/test_economy_fire_diagnosis.py"), "--help"],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0)
        self.assertIn("--variant", result.stdout)
        self.assertFalse(self.output.exists())

    def test_invalid_bounds_refused_before_child(self):
        for ticks in (0, 19999, 500001, True):
            with self.subTest(ticks=ticks), self.assertRaisesRegex(RuntimeError, "Ticks"):
                self.invoke(ticks=ticks)
        for timeout in (0, 7201, True):
            with self.subTest(timeout=timeout), self.assertRaisesRegex(RuntimeError, "Timeout"):
                self.invoke(timeout=timeout)
        self.assertFalse(self.output.exists())

    def test_unknown_variant_refused(self):
        with self.assertRaisesRegex(RuntimeError, "Unknown"):
            self.invoke(variant="free-money")
        self.assertFalse(self.output.exists())

    def test_missing_original_is_blocked(self):
        with self.assertRaisesRegex(RuntimeError, "BLOCKED.*missing"):
            self.invoke(data=self.root / "absent")
        self.assertFalse(self.output.exists())

    def test_wrong_model_is_blocked(self):
        with self.assertRaisesRegex(RuntimeError, "BLOCKED.*model"):
            runner.workflow(self.binary, self.data, self.output, 20000, "supplied", 20)
        self.assertFalse(self.output.exists())

    def test_existing_output_preserved(self):
        self.output.mkdir(); original = self.output / "prior-evidence"
        original.write_bytes(b"Preserve exact prior bytes")
        with self.assertRaisesRegex(RuntimeError, "already exists"):
            self.invoke()
        self.assertEqual(original.read_bytes(), b"Preserve exact prior bytes")
        self.assertEqual(list(self.output.iterdir()), [original])

    def test_output_cannot_escape_or_overlap_inputs(self):
        for output, pattern in ((REPO / "unowned-oe35", "private OE35"),
                                (self.data / "output", "overlaps"), (self.build / "output", "overlaps")):
            with self.subTest(output=output), self.assertRaisesRegex(RuntimeError, pattern):
                self.invoke(output=output)
            self.assertFalse(output.exists())

    def test_symlink_parents_and_original_refused(self):
        target = self.root / "unowned"; target.mkdir()
        linked = self.root / "linked"; linked.symlink_to(target, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            self.invoke(output=linked / "output")
        original = self.root / "linked-original"; original.symlink_to(self.data, target_is_directory=True)
        with self.assertRaisesRegex(RuntimeError, "symlink"):
            self.invoke(data=original)
        self.assertEqual(list(target.iterdir()), [])

    def test_failed_child_recorded_with_actual_pid(self):
        report = self.failure("fail", "child failed.*17")
        self.assertEqual(report["processes"][0]["exit_code"], 17)
        self.assertGreater(report["processes"][0]["actual_pid"], 0)
        self.assertTrue(report["original_bytes_modes_mtimes_unchanged"])
        self.assertTrue(report["binary_unchanged"])
        self.assertIn("Explicit fake child failure", (self.output / "supplied/seed.log").read_text())

    def test_goal_false_is_allowed_only_as_technical_diagnosis(self):
        report = self.invoke()
        self.assertEqual(report["status"], "PASS")
        self.assertIn("not a profitable", report["pass_means"])
        self.assertEqual(report["actual_processes"], 3)
        case = report["variants"][0]
        self.assertIsNone(case["summary"]["first_goal_tick"])
        self.assertFalse(case["summary"]["goal_at_end"]["reached"])
        self.assertLess(case["summary"]["operating_balance"], 0)
        self.assertEqual(len(case["literal_pairs"]), 6)
        self.assertEqual(len({p["actual_pid"] for p in report["processes"]}), 3)
        self.assertTrue(all(runner.load(Path(pair["expected"]["path"])) is not None for pair in case["literal_pairs"]))
        raw_report = runner.load(self.output / "supplied/expected/result.report.json")
        self.assertTrue(raw_report["explicit_fake_no_gameplay"])

    def test_all_variants_are_separate_process_workflows(self):
        report = self.invoke(variant="all")
        self.assertEqual([case["variant"] for case in report["variants"]], list(runner.VARIANTS))
        self.assertEqual(report["actual_processes"], 3 * len(runner.VARIANTS))

    def test_first_goal_requires_true_productive_world_descriptor(self):
        for behavior in ("false-first-goal", "missing-first-goal", "wrong-first-goal-tick", "nonbool-first-goal"):
            with self.subTest(behavior=behavior):
                self.output = self.root / behavior
                self.failure(behavior, "true tick-bound production first-goal")

    def test_literal_pair_mismatch_refused(self):
        self.failure("mismatch", "Separate-process bytes differ: result.report")

    def test_supplied_seed_mutation_refused(self):
        self.failure("mutate-input", "modified their supplied paid input")

    def test_original_mutation_cannot_be_pass(self):
        report = self.failure("mutate-original", "Protected original")
        self.assertFalse(report["original_bytes_modes_mtimes_unchanged"])

    def test_binary_mutation_cannot_be_pass(self):
        report = self.failure("mutate-binary", "Protected original")
        self.assertFalse(report["binary_unchanged"])

    def test_original_mtime_change_cannot_be_pass(self):
        report = self.failure("touch-original", "Protected original")
        self.assertFalse(report["original_bytes_modes_mtimes_unchanged"])

    def test_timeout_kills_and_records_failed_child(self):
        with self.assertRaisesRegex(RuntimeError, "BLOCKED.*timeout"):
            self.invoke("timeout", timeout=1)
        report = runner.load(self.output / "report.json")
        self.assertEqual(report["status"], "BLOCKED")
        self.assertTrue(report["processes"][0]["timed_out"])
        self.assertNotEqual(report["processes"][0]["exit_code"], 0)

    def test_expected_creator_cannot_be_another_restore(self):
        self.failure("restore-expected", "creator and restored")

    def test_fresh_creator_literal_reference_checked(self):
        self.failure("creator-mismatch", "Fresh unrestored creator differs")

    def test_short_tick_count_refused(self):
        self.failure("short", "complete seed-relative interval")

    def test_snapshot_injection_refused(self):
        self.failure("injected", "authority contract")

    def test_missing_paid_commands_refused(self):
        self.failure("missing-commands", "paid commands")

    def test_command_funds_must_reconcile(self):
        self.failure("command-grant", "Actual paid command differs")

    def test_raw_funds_corruption_refused(self):
        self.failure("ledger", "funds conservation")

    def test_wrong_raw_source_refused(self):
        self.failure("source", "Normal model")

    def test_wrong_raw_contract_refused(self):
        self.failure("contract", "rule/schema/policy")

    def test_duplicate_raw_identity_refused(self):
        self.failure("duplicate", "identity or capacity")

    def test_snapshot_must_be_complete_raw_world(self):
        self.failure("snapshot", "complete saved World")

    def test_stock_grants_refused(self):
        self.failure("goods-grant", "unsupported commodity grants")

    def test_missing_raw_owner_refused(self):
        self.failure("missing-owner", "owner is missing")

    def test_production_goal_witness_requires_exact_boolean(self):
        self.failure("goal-bool", "goal descriptor")

    def test_reached_goal_requires_observed_first_tick(self):
        self.failure("invented-goal", "first observed tick")

    def test_duplicate_json_keys_refused(self):
        path = self.root / "ambiguous.json"; path.write_text('{"funds":1,"funds":2}')
        with self.assertRaisesRegex(RuntimeError, "Duplicate JSON key"):
            runner.load(path)


if __name__ == "__main__":
    unittest.main()
