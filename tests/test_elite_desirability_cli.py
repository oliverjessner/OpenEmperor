#!/usr/bin/env python3
"""Hermetic Elite QA boundaries: no original assets, saved game or SDL display.

Every temporary file stays in the ignored OE46 root. Rejections must happen at
the intended argument/path boundary, before an empty authored data directory can
reach source loading or create proof output.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest


EXECUTABLE: Path
REPOSITORY = Path(__file__).resolve().parents[1]
PRIVATE_ROOT = REPOSITORY / ".local/original-economy/oe46"
HELP = "elite-desirability-acceptance --mode"


class EliteDesirabilityCli(unittest.TestCase):
    def setUp(self):
        PRIVATE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="cli-guards-", dir=PRIVATE_ROOT)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.root / "authored-empty-data"
        self.data.mkdir()
        self.output = self.root / "new-proof"
        self.seed = self.root / "authored-seed.json"
        self.seed.write_text("{}\n")

    def inventory(self):
        result = {}
        for path in self.root.rglob("*"):
            info = path.lstat()
            common = (stat.S_IMODE(info.st_mode), info.st_mtime_ns)
            if path.is_symlink():
                value = ("symlink", *common, os.readlink(path))
            elif path.is_file():
                value = ("file", *common, info.st_size, hashlib.sha256(path.read_bytes()).hexdigest())
            else:
                value = ("directory", *common)
            result[str(path.relative_to(self.root))] = value
        return result

    def arguments(self, **replacements):
        values = {"--mode": "create", "--rule": "16", "--diagnosis": "oe46",
                  "--variant": "lean-med", "--paid-strategy": "oe45-grid-watch-near",
                  "--data": str(self.data), "--output": str(self.output)}
        for key, value in replacements.items():
            if value is None:
                values.pop(key, None)
            else:
                values[key] = str(value)
        return [item for pair in values.items() for item in pair]

    def run_cli(self, arguments):
        environment = os.environ.copy()
        environment["SDL_VIDEODRIVER"] = "elite-cli-must-not-open-sdl"
        return subprocess.run([str(EXECUTABLE), *arguments], cwd=self.root,
                              env=environment, capture_output=True, text=True,
                              timeout=10, check=False)

    def rejected(self, arguments, diagnostic):
        before = self.inventory()
        result = self.run_cli(arguments)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("Elite desirability proof FAIL:", result.stderr)
        self.assertIn(diagnostic, result.stderr)
        self.assertEqual(self.inventory(), before, "Rejected QA request changed input/output files")
        return result

    def test_help_preserves_historical_default_and_lists_explicit_oe46(self):
        before = self.inventory()
        result = self.run_cli(["--help"])
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("default14", result.stdout)
        self.assertIn("oe46", result.stdout)
        self.assertIn("17 requires oe46", result.stdout)
        self.assertEqual(result.stderr, "")
        self.assertEqual(self.inventory(), before)

    def test_valid_oe46_preflight_reaches_source_check_without_output(self):
        # Positive boundary control: this exact shape is admitted, then fails on
        # intentionally absent licensed input, rather than silently accepting SDL.
        self.rejected(self.arguments(), "Bounded regular source file required")
        self.assertFalse(self.output.exists())

    def test_rule16_requires_explicit_matching_diagnosis(self):
        for diagnosis in (None, "oe44"):
            with self.subTest(diagnosis=diagnosis):
                self.rejected(self.arguments(**{"--diagnosis": diagnosis}),
                              "Rule16 requires explicit")

    def test_rule17_requires_explicit_oe46_and_reaches_source_check_when_selected(self):
        for diagnosis in (None, "oe44", "oe45"):
            with self.subTest(diagnosis=diagnosis):
                self.rejected(self.arguments(**{"--rule": "17", "--diagnosis": diagnosis}),
                              "Rule17 requires explicit --rule 17 and --diagnosis oe46")
        self.rejected(self.arguments(**{"--rule": "17"}),
                      "Bounded regular source file required")
        self.rejected(self.arguments(**{"--rule": "17", "--tick-digests": "sha256"}),
                      "Bounded regular source file required")

    def test_oe46_recipe_cannot_be_selected_by_oe45(self):
        self.rejected(self.arguments(**{"--diagnosis": "oe45",
                                        "--paid-strategy": "oe46-ancestor-music-swap"}),
                      "OE46 player experiments require --diagnosis oe46")

    def test_oe46_render_refuses_historical_rule_before_creating_output(self):
        for rule in ("14", "15"):
            with self.subTest(rule=rule):
                self.rejected(self.arguments(**{"--mode": "render", "--rule": rule,
                                                "--paid-strategy": None,
                                                "--input": self.seed,
                                                "--resources": self.data}),
                              "OE46 rendering requires explicit Rule16 or Rule17")
                self.assertFalse(self.output.exists())

    def test_recipe_without_diagnosis_is_refused(self):
        self.rejected(self.arguments(**{"--rule": "14", "--diagnosis": None}), HELP)

    def test_oe46_output_cannot_use_a_prior_diagnosis_root(self):
        for generation in ("oe40", "oe44", "oe45"):
            requested = REPOSITORY / ".local/original-economy" / generation / self.root.name
            self.assertFalse(requested.exists())
            with self.subTest(generation=generation):
                self.rejected(self.arguments(**{"--output": requested}),
                              "fresh ignored output")
                self.assertFalse(requested.exists())
        self.rejected(self.arguments(**{"--output": PRIVATE_ROOT}), "fresh ignored output")

    def test_prior_diagnosis_cannot_write_in_oe46(self):
        for diagnosis in ("oe44", "oe45"):
            with self.subTest(diagnosis=diagnosis):
                self.rejected(self.arguments(**{"--rule": "14", "--diagnosis": diagnosis,
                                                "--paid-strategy": None}),
                              "fresh ignored output")

    def test_existing_output_is_preserved(self):
        self.output.mkdir()
        marker = self.output / "earlier-proof.txt"
        marker.write_bytes(b"Retain previous proof bytes.\n")
        self.rejected(self.arguments(), "fresh ignored output")
        self.rejected(self.arguments(**{"--output": marker}), "fresh ignored output")

    def test_symlinked_output_ancestor_is_refused(self):
        real = self.root / "real-parent"
        real.mkdir()
        alias = self.root / "alias"
        alias.symlink_to(real, target_is_directory=True)
        self.rejected(self.arguments(**{"--output": alias / "new-proof"}),
                      "Proof path traverses symlink")
        dangling = self.root / "dangling"
        dangling.symlink_to(self.root / "absent-parent", target_is_directory=True)
        self.rejected(self.arguments(**{"--output": dangling / "new-proof"}),
                      "Proof path traverses symlink")

    def test_output_inside_authored_data_is_refused(self):
        self.rejected(self.arguments(**{"--data": self.root}), "Output overlaps original data")

    def test_resume_inputs_outside_explicit_private_roots_are_refused(self):
        # A real authored tracked file satisfies regular-file/size checks but
        # lies outside every private input root. It is never parsed as a save.
        outside = Path(__file__).resolve()
        before = outside.read_bytes()
        for source, expected in ((outside, self.seed), (self.seed, outside)):
            with self.subTest(source=str(source), expected=str(expected)):
                self.rejected(self.arguments(**{"--mode": "resume", "--input": source,
                                                "--expected": expected}),
                              "bounded explicit protected")
        self.assertEqual(outside.read_bytes(), before)

    def test_symlinked_save_and_missing_expected_are_refused(self):
        alias = self.root / "seed-alias.json"
        alias.symlink_to(self.seed)
        self.rejected(self.arguments(**{"--mode": "resume", "--input": alias,
                                        "--expected": self.seed}), "Proof path traverses symlink")
        self.rejected(self.arguments(**{"--mode": "resume", "--input": self.seed}),
                      "Resume requires")

    def test_digest_flag_refuses_wrong_algorithm_mode_rule_and_diagnosis(self):
        cases = ({"--tick-digests": "crc32"},
                 {"--tick-digests": "sha256", "--rule": "14"},
                 {"--tick-digests": "sha256", "--rule": None},
                 {"--tick-digests": "sha256", "--mode": "render"},
                 {"--tick-digests": "sha256", "--rule": "15", "--diagnosis": "oe44"})
        for case in cases:
            with self.subTest(case=case):
                self.rejected(self.arguments(**case), "Tick digests require explicit")

    def test_duplicate_unknown_and_incomplete_arguments_are_refused(self):
        self.rejected(self.arguments() + ["--diagnosis", "oe46"], "Duplicate Elite proof argument")
        self.rejected(self.arguments() + ["--unrecognized", "x"], HELP)
        self.rejected(self.arguments() + ["--expected"], HELP)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    EXECUTABLE = options.executable.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
