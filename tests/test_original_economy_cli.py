#!/usr/bin/env python3
"""Hermetic argument contracts for Rule7 preflight and explicit original opt-ins.

Every map byte below is authored test data. No licensed model, graphics, SDL
display or existing player state is needed by this process-level check.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


EXECUTABLE: Path


def authored_map(size: int = 170) -> bytes:
    cells = 228 * 228
    terrain, heights, entities = 261455, 989275, 1093607
    logical = bytearray(entities + 6)
    logical[:8] = bytes((5, 0, 0xfe, 0xca, 0, 0, 2, 0))
    struct.pack_into("<I", logical, 84, size)
    logical[terrain:terrain + 4 * cells] = struct.pack("<I", 0x80) * cells
    logical[heights:heights + cells] = bytes((255,)) * cells
    struct.pack_into("<HI", logical, entities, 1, 0)  # Complete empty manager.
    physical = bytearray(struct.pack("<I", 0xfedcbaaa))
    for offset in range(0, len(logical), 16384):
        block = logical[offset:offset + 16384]
        compressed = zlib.compress(block)
        physical.extend(struct.pack("<III", 0, len(compressed), len(block)))
        physical.extend(compressed)
    return bytes(physical)


class OriginalEconomyCli(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="openemperor-oe08-cli-")
        cls.root = Path(cls.temporary.name).resolve()
        (cls.root / "Cities").mkdir()
        (cls.root / "Cities/authored.map").write_bytes(authored_map())
        (cls.root / "Cities/unsupported.map").write_bytes(authored_map(1))
        (cls.root / "Cities/damaged.map").write_bytes(b"authored invalid container")
        # OE-11 owns production model preparation. This option must remain a
        # map-only diagnostic even if a present model would fail preparation.
        (cls.root / "Model").mkdir()
        (cls.root / "Model/EmperorBuildingModels.txt").write_text("invalid authored model\n")
        cls.before = cls.inventory()

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    @classmethod
    def inventory(cls):
        return {str(path.relative_to(cls.root)): hashlib.sha256(path.read_bytes()).hexdigest()
                for path in cls.root.rglob("*") if path.is_file()}

    def tearDown(self):
        self.assertEqual(self.inventory(), self.before, "Preflight changed or created files")

    def arguments(self, *, root: Path | None = None, map_name="Cities/authored.map"):
        return ["--data", str(self.root if root is None else root), "--sandbox", map_name,
                "--sandbox-rules", "sandbox-city-v16",
                "--sandbox-original-economy-check", "--report-json"]

    def run_cli(self, arguments):
        environment = os.environ.copy()
        # A diagnostic that accidentally reaches SDL must fail visibly.
        environment["SDL_VIDEODRIVER"] = "openemperor-preflight-must-not-open-sdl"
        return subprocess.run([str(EXECUTABLE), *arguments], cwd=self.root,
                              env=environment, capture_output=True, text=True,
                              timeout=20, check=False)

    def diagnosis(self, arguments):
        result = self.run_cli(arguments)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertEqual(result.stderr, "")
        self.assertEqual(len(result.stdout.splitlines()), 1)
        report = json.loads(result.stdout)
        self.assertEqual(report["schema"], "openemperor-original-economy-gate-v1")
        self.assertEqual(report["profile"], "sandbox-city-v16")
        self.assertEqual(report["rule_version"], 7)
        self.assertEqual(report["policy_version"], 3)
        self.assertIs(report["runtime_ready"], False)
        self.assertIn("session start is refused", report["reason"])
        self.assertLessEqual(len(report["map_reason"]), 2048)
        self.assertLessEqual(len(report["map_detail"]), 2048)
        return report

    def test_valid_map_remains_unavailable_without_runtime(self):
        report = self.diagnosis(self.arguments())
        self.assertEqual(report["map_rules_status"], "map_rules_checked")
        self.assertEqual(report["map_sha256"], self.before["Cities/authored.map"])
        self.assertIn("session start", report["map_detail"])

    def test_invalid_inputs_have_separate_map_errors(self):
        for name in ("Cities/missing.map", "Cities/damaged.map", "../outside.map",
                     str(self.root / "Cities/authored.map")):
            with self.subTest(map=name):
                report = self.diagnosis(self.arguments(map_name=name))
                self.assertEqual(report["map_rules_status"], "input_error")
        report = self.diagnosis(self.arguments(root=self.root / "missing-root"))
        self.assertEqual(report["map_rules_status"], "input_error")

    def test_unsupported_map_has_no_runtime_fallback(self):
        report = self.diagnosis(self.arguments(map_name="Cities/unsupported.map"))
        self.assertEqual(report["map_rules_status"], "unsupported_for_rules")
        self.assertEqual(report["map_sha256"], self.before["Cities/unsupported.map"])

    def test_every_required_selection_must_be_explicit(self):
        for flag, count in (("--data", 2), ("--sandbox", 2),
                            ("--sandbox-rules", 2), ("--report-json", 1)):
            arguments = self.arguments()
            at = arguments.index(flag)
            del arguments[at:at + count]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        arguments = self.arguments()
        arguments[arguments.index("sandbox-city-v16")] = "sandbox-city-v11"
        result = self.run_cli(arguments)
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)

    def test_interactive_save_and_visual_modes_cannot_be_combined(self):
        incompatible = [
            ["--sandbox-demo"], ["--sandbox-check"], ["--sandbox-resume-check"],
            ["--sandbox-routing-check"], ["--sandbox-save", "unwritten.json"],
            ["--load-sandbox", "unread.json"], ["--sandbox-bridges"], ["--sandbox-ramps"],
            ["--sandbox-visuals", "unread.json"], ["--building-visuals", "unread.json"],
            ["--road-visuals", "unread.json"], ["--great-wall-presentation", "historical"],
            ["--authored-terrain-sides"], ["--app-root", "unwritten-app"],
            ["--browse-assets"], ["--browse-maps"], ["--list-maps"],
            ["--sandbox-original-economy-check"], ["--original-fire-protection"],
            ["--original-sustainable-economy"],
            ["--original-inspector-coordination"], ["--original-deadline-fire-dispatch"], ["--original-deadline-service-dispatch"],
        ]
        for extra in incompatible:
            with self.subTest(extra=extra):
                result = self.run_cli(self.arguments() + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)

    def test_fire_protection_requires_explicit_city16_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-fire-protection"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Usage:", result.stderr)
        arguments = base.copy()
        arguments[arguments.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(arguments).returncode, 2)

    def test_fire_protection_rejects_other_rule_and_diagnostic_modes(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-fire-protection"]
        incompatible = [
            ["--sandbox-demo"], ["--sandbox-bridges"], ["--sandbox-ramps"],
            ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
            ["--sandbox-routing-check", "--report-json"],
            ["--sandbox-original-economy-check", "--report-json"],
            ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"],
            ["--original-fire-protection"],
        ]
        for extra in incompatible:
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)


    def test_sustainable_economy_requires_own_explicit_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-sustainable-economy"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertIn("Usage:", result.stderr)
        for extra in (["--original-fire-protection"], ["--original-sustainable-economy"],
                      ["--sandbox-demo"], ["--sandbox-bridges"], ["--sandbox-ramps"],
                      ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"]):
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        base[base.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(base).returncode, 2)

    def test_inspector_coordination_requires_own_explicit_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-inspector-coordination"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        for extra in (["--original-fire-protection"], ["--original-sustainable-economy"],
                      ["--original-market-logistics"], ["--original-cardinal-water"],
                      ["--original-inspector-coordination"], ["--original-deadline-fire-dispatch"], ["--original-deadline-service-dispatch"], ["--original-deadline-food-dispatch"], ["--sandbox-demo"],
                      ["--sandbox-bridges"], ["--sandbox-ramps"],
                      ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"]):
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        base[base.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(base).returncode, 2)
    def test_deadline_fire_dispatch_requires_own_explicit_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-deadline-fire-dispatch"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        for extra in (["--original-fire-protection"], ["--original-sustainable-economy"],
                      ["--original-market-logistics"], ["--original-cardinal-water"],
                      ["--original-inspector-coordination"], ["--original-deadline-fire-dispatch"], ["--original-deadline-service-dispatch"], ["--original-deadline-food-dispatch"], ["--sandbox-demo"],
                      ["--sandbox-bridges"], ["--sandbox-ramps"],
                      ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"]):
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        base[base.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(base).returncode, 2)
    def test_deadline_service_dispatch_requires_own_explicit_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-deadline-service-dispatch"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        for extra in (["--original-fire-protection"], ["--original-sustainable-economy"],
                      ["--original-market-logistics"], ["--original-cardinal-water"],
                      ["--original-inspector-coordination"], ["--original-deadline-fire-dispatch"], ["--original-deadline-service-dispatch"], ["--original-deadline-food-dispatch"], ["--sandbox-demo"],
                      ["--sandbox-bridges"], ["--sandbox-ramps"],
                      ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"]):
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        base[base.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(base).returncode, 2)

    def test_deadline_food_dispatch_requires_own_explicit_selection(self):
        base = ["--data", str(self.root), "--sandbox", "Cities/authored.map",
                "--sandbox-rules", "sandbox-city-v16", "--original-deadline-food-dispatch"]
        for flag in ("--data", "--sandbox", "--sandbox-rules"):
            arguments = base.copy()
            at = arguments.index(flag)
            del arguments[at:at + 2]
            with self.subTest(missing=flag):
                result = self.run_cli(arguments)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        for extra in (["--original-fire-protection"], ["--original-sustainable-economy"],
                      ["--original-market-logistics"], ["--original-cardinal-water"],
                      ["--original-inspector-coordination"], ["--original-deadline-fire-dispatch"], ["--original-deadline-service-dispatch"], ["--original-deadline-food-dispatch"], ["--sandbox-demo"],
                      ["--sandbox-bridges"], ["--sandbox-ramps"],
                      ["--sandbox-check", "--report-json"], ["--sandbox-resume-check"],
                      ["--sandbox-routing-check", "--report-json"],
                      ["--sandbox-original-economy-check", "--report-json"],
                      ["--load-sandbox", "unread.json"], ["--authored-terrain-sides"]):
            with self.subTest(extra=extra):
                result = self.run_cli(base + extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
                self.assertEqual(result.stdout, "")
                self.assertIn("Usage:", result.stderr)
        base[base.index("sandbox-city-v16")] = "sandbox-city-v11"
        self.assertEqual(self.run_cli(base).returncode, 2)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    EXECUTABLE = options.executable.resolve(strict=True)
    unittest.main(argv=[sys.argv[0], *remaining])
