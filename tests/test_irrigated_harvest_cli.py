#!/usr/bin/env python3
"""Hermetic real-main generation admission; no SDL or licensed data needed."""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
EXECUTABLE: Path
FLAG = "--irrigated-harvest"


def identity(path: Path):
    stat = path.stat()
    return stat.st_size, stat.st_mode, stat.st_mtime_ns, hashlib.sha256(path.read_bytes()).hexdigest()


class IrrigatedHarvestCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="openemperor-irrigated-harvest-cli-")
        cls.root = Path(cls.temporary.name).resolve()
        cls.data = cls.root / "empty-supplied-data"
        cls.data.mkdir()
        cls.executable_before = identity(EXECUTABLE)

    @classmethod
    def tearDownClass(cls):
        try:
            if identity(EXECUTABLE) != cls.executable_before:
                raise AssertionError("Argument admission changed the actual executable")
        finally:
            cls.temporary.cleanup()

    def tearDown(self):
        self.assertEqual(list(self.root.iterdir()), [self.data], "CLI created application state")
        self.assertEqual(list(self.data.iterdir()), [], "CLI changed supplied data")

    def arguments(self):
        return ["--data", str(self.data), "--sandbox", "Cities/not-present.map",
                "--sandbox-rules", "sandbox-city-v16", FLAG]

    def run_cli(self, arguments):
        environment = os.environ.copy()
        environment["SDL_VIDEODRIVER"] = "openemperor-cli-must-not-open-sdl"
        environment["SDL_VIDEO_DRIVER"] = "openemperor-cli-must-not-open-sdl"
        return subprocess.run([str(EXECUTABLE), *arguments], cwd=self.root,
                              env=environment, capture_output=True, text=True,
                              timeout=20, check=False)

    def rejected(self, arguments):
        result = self.run_cli(arguments)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "", "Refusal reached source inspection")
        self.assertIn("Usage:", result.stderr)
        self.assertIn(FLAG, result.stderr)
        self.assertNotIn("Sandbox load failed:", result.stderr)
        self.assertNotIn("Data directory does not exist", result.stderr)

    def test_help_advertises_the_explicit_flag(self):
        # main's existing unsupported --help behavior remains Usage/exit2.
        self.rejected(["--help"])

    def test_each_required_create_context_remains_explicit(self):
        self.rejected([FLAG])
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = self.arguments()
            index = arguments.index(flag)
            del arguments[index:index + 2]
            with self.subTest(missing=flag):
                self.rejected(arguments)
        for profile in ("sandbox-city-v11", "sandbox-city-v15", "sandbox-production-v2"):
            arguments = self.arguments()
            arguments[arguments.index("sandbox-city-v16")] = profile
            with self.subTest(profile=profile):
                self.rejected(arguments)

    def test_duplicate_and_every_incompatible_lineage_are_refused(self):
        incompatible = (FLAG, "--agriculture-reconfiguration", "--food-warehouses",
                        "--food-processing", "--irrigation-productivity", "--seasonal-agriculture",
                        "--physical-food-growers", "--multi-field-agriculture", "--playable-hunting",
                        "--playable-fishing", "--playable-agriculture", "--original-irrigation",
                        "--original-agriculture", "--original-venue-house-service", "--original-music-venues",
                        "--original-safe-demolition", "--original-deadline-food-dispatch",
                        "--original-deadline-service-dispatch", "--original-deadline-fire-dispatch",
                        "--original-inspector-coordination", "--original-cardinal-water",
                        "--original-market-logistics", "--original-sustainable-economy",
                        "--original-fire-protection", "--authored-land-trade", "--authored-source-cell-ramps",
                        "--sandbox-bridges", "--sandbox-ramps", "--authored-terrain-sides", "--sandbox-demo")
        for extra in incompatible:
            for arguments in (self.arguments() + [extra], [extra] + self.arguments()):
                with self.subTest(incompatible=extra, first=arguments[0]):
                    self.rejected(arguments)

    def test_diagnostics_load_and_app_root_cannot_reselect_generation(self):
        for extra in (["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--app-root", str(self.root / "never-created-app")]):
            with self.subTest(incompatible=extra):
                self.rejected(self.arguments() + extra)
        self.rejected(["--data", str(self.data), "--load-sandbox", str(self.root / "never-read.json"), FLAG])
        # A bad combination must fail by arguments even if supplied data does
        # not exist, before attempting model/map discovery or creating output.
        arguments = self.arguments() + ["--agriculture-reconfiguration"]
        arguments[arguments.index(str(self.data))] = str(self.root / "never-read-data")
        self.rejected(arguments)

    def test_valid_selection_reaches_source_preflight_without_sdl(self):
        result = self.run_cli(self.arguments())
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertNotIn("Usage:", result.stderr)
        self.assertIn("Sandbox load failed:", result.stderr)
        self.assertNotIn("must-not-open-sdl", result.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    EXECUTABLE = arguments.executable.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
