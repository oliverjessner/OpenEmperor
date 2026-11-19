#!/usr/bin/env python3
"""Black-box refusal and publication checks for the supplied-data QA launcher.

The optional source checks earn their seed through the launcher's ordinary paid
commands. Hostile copies are rejection inputs, never accepted gameplay fixtures.
"""

import argparse
import copy
import json
from pathlib import Path
import stat
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
EXECUTABLE = None
DATA = None


def fingerprint(path):
    info = path.stat()
    return path.read_bytes(), info.st_mtime_ns, stat.S_IMODE(info.st_mode)


class CliGuards(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        private = ROOT / ".local/agri-mac04"
        private.mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(prefix="cli-guards-", dir=private)
        cls.root = Path(cls.temporary.name)
        cls.empty_data = cls.root / "empty-original-data"
        cls.empty_data.mkdir()

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def invoke(self, *arguments):
        return subprocess.run([str(EXECUTABLE), *map(str, arguments)], cwd=ROOT,
                              capture_output=True, text=True, timeout=30,
                              check=False)

    def base(self, output, data=None):
        return ["--data", data or self.empty_data, "--output", output]

    def refused(self, arguments, output=None, inputs=(), diagnostic=None):
        before = {path: fingerprint(path) for path in inputs}
        result = self.invoke(*arguments)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("FAIL", result.stderr)
        self.assertNotIn("PASS Rule29", result.stdout)
        self.assertNotIn("PARTIAL Rule29", result.stdout)
        if diagnostic:
            self.assertIn(diagnostic, result.stderr)
        for path, expected in before.items():
            self.assertEqual(fingerprint(path), expected,
                             "Refused CLI changed an explicit input")
        if output is not None:
            self.assertFalse(output.exists(), "Argument refusal published output")
        return result

    def test_help_has_no_side_effects(self):
        before = sorted(self.root.iterdir())
        result = self.invoke("--help")
        self.assertEqual(result.returncode, 0, result.stderr)
        for argument in ("--data", "--output", "--input", "--expected"):
            self.assertIn(argument, result.stdout)
        self.assertFalse(result.stderr)
        self.assertEqual(sorted(self.root.iterdir()), before)

    def test_missing_unknown_and_duplicate_options(self):
        output = self.root / "invalid-options"
        base = self.base(output)
        cases = [[], ["--data", self.empty_data], ["--output", output],
                 [*base, "--unknown", "value"], [*base, "--ticks"],
                 [*base, "--data", self.empty_data],
                 [*base, "--output", output], ["--help", "--ticks", "0"]]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                self.refused(arguments, output)

    def test_tick_and_hold_domains_refuse_before_source_loading(self):
        output = self.root / "invalid-clock"
        for flag in ("--ticks", "--hold"):
            for value in ("-1", "100001", "1.5", "nan", "+1", "1x", "",
                          "18446744073709551616"):
                with self.subTest(flag=flag, value=value):
                    self.refused([*self.base(output), flag, value], output,
                                 diagnostic="0..100000")

    def test_inclusive_clock_bounds_reach_source_validation(self):
        output = self.root / "valid-clock-missing-source"
        for flag in ("--ticks", "--hold"):
            for value in ("0", "100000"):
                with self.subTest(flag=flag, value=value):
                    self.refused([*self.base(output), flag, value], output,
                                 diagnostic="source")

    def test_output_must_be_a_new_private_child(self):
        allowed = ROOT / ".local/agri-mac04"
        outside = ROOT / ".local" / (self.root.name + "-outside")
        for output in (allowed, ROOT, outside):
            with self.subTest(output=output):
                self.refused(self.base(output), diagnostic="fresh")
        self.assertFalse(outside.exists())

    def test_existing_output_is_never_overwritten(self):
        output = self.root / "protected-output"
        output.mkdir()
        sentinel = output / "report.json"
        sentinel.write_bytes(b"protected pre-existing QA evidence\n")
        self.refused(self.base(output), inputs=(sentinel,), diagnostic="fresh")
        self.assertEqual(list(output.iterdir()), [sentinel])

    def test_output_symlink_ancestry_is_refused(self):
        target = self.root / "symlink-target"
        target.mkdir()
        link = self.root / "linked-parent"
        link.symlink_to(target, target_is_directory=True)
        output = link / "unpublished"
        self.refused(self.base(output), diagnostic="symlink")
        self.assertEqual(list(target.iterdir()), [])

    def test_missing_and_incomplete_source_publish_nothing(self):
        output = self.root / "missing-source-output"
        self.refused(self.base(output, self.root / "not-present"), output)
        self.refused(self.base(output), output, diagnostic="source")
        self.assertEqual(list(self.empty_data.iterdir()), [])

    def test_linked_source_model_is_not_opened(self):
        data = self.root / "linked-model-source"
        (data / "Model").mkdir(parents=True)
        sentinel = self.root / "source-sentinel.txt"
        sentinel.write_bytes(b"Source symlinks cannot authorize gameplay\n")
        (data / "Model/EmperorBuildingModels.txt").symlink_to(sentinel)
        output = self.root / "linked-model-output"
        self.refused(self.base(output, data), output, inputs=(sentinel,),
                     diagnostic="symlink")

    def test_replay_pair_is_required_and_preserved(self):
        explicit = self.root / "not-a-save.json"
        explicit.write_bytes(b"explicit input sentinel\n")
        output = self.root / "incomplete-replay"
        for argument in ("--input", "--expected"):
            with self.subTest(argument=argument):
                self.refused([*self.base(output), argument, explicit], output,
                             inputs=(explicit,), diagnostic="both")

    def test_replay_files_are_bounded_and_symlink_free(self):
        explicit = self.root / "replay-input.json"
        explicit.write_bytes(b"explicit replay input sentinel\n")
        linked = self.root / "linked-input.json"
        linked.symlink_to(explicit)
        oversized = self.root / "oversized-input.json"
        with oversized.open("wb") as stream:
            stream.truncate(16 * 1024 * 1024 + 1)
        output = self.root / "unsafe-replay"
        for candidate in (linked, oversized, self.root / "absent-input.json"):
            with self.subTest(candidate=candidate):
                self.refused([*self.base(output), "--input", candidate,
                              "--expected", explicit], output, inputs=(explicit,))


class SuppliedSourceCliGuards(CliGuards):
    """Paid setup and hostile copies, without injected accepted World state."""

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.model = DATA / "Model/EmperorBuildingModels.txt"
        cls.map = DATA / "Cities/Xia.map"
        cls.source_before = {path: fingerprint(path) for path in (cls.model, cls.map)}
        cls.creator = cls.root / "earned-paid-seed"
        process = subprocess.run([str(EXECUTABLE), "--data", str(DATA),
                                  "--output", str(cls.creator), "--ticks", "0",
                                  "--hold", "0"], cwd=ROOT, capture_output=True,
                                 text=True, timeout=60, check=False)
        if process.returncode != 2:
            raise AssertionError("Zero-tick paid setup must report PARTIAL, not earned luxury: "
                                 + process.stdout + process.stderr)
        cls.seed_path = cls.creator / "seed.save.json"
        cls.seed = json.loads(cls.seed_path.read_text())
        if cls.seed["schema_version"] != 45 or cls.seed["rules"]["version"] != 29:
            raise AssertionError("Paid setup did not create exact Rule29/schema45")
        report = json.loads((cls.creator / "report.json").read_text())
        if (report["status"] != "PARTIAL" or report["positive_luxury_consumption"] != [False] * 3
                or report["snapshot_or_stock_injection"]):
            raise AssertionError("Zero-tick setup falsely claims luxury consumption")

    @classmethod
    def tearDownClass(cls):
        for path, before in cls.source_before.items():
            if fingerprint(path) != before:
                raise AssertionError("CLI controls changed supplied model/map")
        super().tearDownClass()

    def rejected_replay(self, label, initial, expected):
        input_path = self.root / (label + ".input.json")
        expected_path = self.root / (label + ".expected.json")
        input_path.write_text(json.dumps(initial) + "\n")
        expected_path.write_text(json.dumps(expected) + "\n")
        output = self.root / (label + "-output")
        self.refused([*self.base(output, DATA), "--input", input_path,
                      "--expected", expected_path], inputs=(input_path, expected_path,
                                                             self.seed_path))
        if output.exists():
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(report["status"], "FAIL")
            self.assertFalse((output / "final.save.json").exists())
            self.assertFalse((output / "commands.json").exists())

    def test_hostile_historical_generation_relabel_is_refused(self):
        hostile = copy.deepcopy(self.seed)
        hostile["schema_version"] = 44
        hostile["rules"]["version"] = 28
        hostile["world"]["rule_version"] = 28
        self.rejected_replay("historical-relabel", hostile, self.seed)

    def test_expected_endpoint_must_be_a_real_later_continuation(self):
        self.rejected_replay("same-tick-expected", self.seed, self.seed)

    def test_forged_expected_generation_cannot_authorize_replay(self):
        hostile = copy.deepcopy(self.seed)
        hostile["schema_version"] = 44
        hostile["rules"]["version"] = 28
        self.rejected_replay("expected-generation", self.seed, hostile)


def main():
    global EXECUTABLE, DATA
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--data", type=Path)
    arguments = parser.parse_args()
    EXECUTABLE = arguments.executable.resolve(strict=True)
    DATA = arguments.data.absolute() if arguments.data else None
    supplied_sources = DATA is not None and all(
        (DATA / relative).is_file()
        for relative in ("Model/EmperorBuildingModels.txt", "Cities/Xia.map"))
    loader = unittest.TestLoader()
    suite = loader.loadTestsFromTestCase(CliGuards)
    if supplied_sources:
        DATA = DATA.resolve(strict=True)
        # The subclass only adds source-backed cases; inherited argument cases
        # already ran independently and do not need repeating.
        suite.addTests(SuppliedSourceCliGuards(name) for name in
                       loader.getTestCaseNames(SuppliedSourceCliGuards)
                       if name not in loader.getTestCaseNames(CliGuards))
    else:
        print("NOT_RUN: 3 source-backed CLI guards; Original Model/Xia inputs are unavailable. "
              "Running 11 independent argument/path guards.", flush=True)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == "__main__":
    main()
