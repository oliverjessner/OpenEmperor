#!/usr/bin/env python3
"""Hermetic real-main argument tests; no Original data or SDL session is used."""
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
FLAG = "--original-venue-house-service"


class VenueHouseServiceCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="openemperor-venue-house-cli-")
        cls.root = Path(cls.temporary.name).resolve()
        cls.data = cls.root / "empty-supplied-data"
        cls.data.mkdir()
        cls.executable_before = cls.identity(EXECUTABLE)

    @staticmethod
    def identity(path):
        stat = path.stat()
        return stat.st_size, stat.st_mode, stat.st_mtime_ns, hashlib.sha256(path.read_bytes()).hexdigest()

    @classmethod
    def tearDownClass(cls):
        try:
            if cls.identity(EXECUTABLE) != cls.executable_before:
                raise AssertionError("Argument tests changed the actual executable")
        finally:
            cls.temporary.cleanup()

    def tearDown(self):
        self.assertEqual(list(self.root.iterdir()), [self.data], "CLI created application state")
        self.assertEqual(list(self.data.iterdir()), [], "CLI changed the supplied empty directory")

    def arguments(self):
        return ["--data", str(self.data), "--sandbox", "Cities/not-present.map",
                "--sandbox-rules", "sandbox-city-v16", FLAG]

    def run_cli(self, arguments):
        environment = os.environ.copy()
        # An accidental path beyond preflight must fail visibly, without a
        # real window, preferences, licensed files or player saves.
        environment["SDL_VIDEODRIVER"] = "openemperor-cli-must-not-open-sdl"
        environment["SDL_VIDEO_DRIVER"] = "openemperor-cli-must-not-open-sdl"
        return subprocess.run([str(EXECUTABLE), *arguments], cwd=self.root,
                              env=environment, capture_output=True, text=True,
                              timeout=20, check=False)

    def rejected(self, arguments):
        result = self.run_cli(arguments)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertEqual(result.stdout, "")
        self.assertIn("Usage:", result.stderr)
        self.assertIn(FLAG, result.stderr)

    def test_usage_advertises_the_explicit_generation(self):
        # main historically reports unsupported --help with Usage/exit2.
        # This test preserves that behavior instead of inventing exit0 help.
        self.rejected(["--help"])

    def test_every_required_context_is_explicit(self):
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

    def test_duplicate_and_other_generation_flags_are_refused(self):
        for extra in (FLAG, "--original-music-venues", "--original-deadline-food-dispatch",
                      "--original-safe-demolition", "--original-deadline-service-dispatch",
                      "--original-deadline-fire-dispatch", "--original-inspector-coordination",
                      "--original-cardinal-water", "--original-market-logistics",
                      "--original-sustainable-economy", "--original-fire-protection",
                      "--authored-land-trade", "--authored-source-cell-ramps",
                      "--sandbox-bridges", "--sandbox-ramps", "--authored-terrain-sides",
                      "--sandbox-demo"):
            with self.subTest(incompatible=extra):
                self.rejected(self.arguments() + [extra])

    def test_diagnostics_and_explicit_load_cannot_reselect_generation(self):
        for extra in (["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"]):
            with self.subTest(incompatible=extra):
                self.rejected(self.arguments() + extra)
        # Loading derives the exact generation from its complete SaveDocument;
        # an explicit create-mode flag is refused BEFORE reading this sentinel.
        self.rejected(["--data", str(self.data), "--load-sandbox", str(self.root / "never-read.json"), FLAG])

    def test_valid_selection_reaches_source_preflight_without_sdl(self):
        result = self.run_cli(self.arguments())
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertNotIn("Usage:", result.stderr)
        self.assertIn("Sandbox load failed:", result.stderr)
        # This is an argument-admission control, NOT a boot/gameplay assertion.
        self.assertTrue(self.data.is_dir())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    arguments, remaining = parser.parse_known_args()
    EXECUTABLE = arguments.executable.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
