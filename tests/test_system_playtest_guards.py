#!/usr/bin/env python3
"""Exercise the diagnostic's actual output guards without SDL or original data."""
from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import tempfile
import unittest


EXECUTABLE: Path
REPOSITORY = Path(__file__).resolve().parents[1]
PRIVATE_ROOT = REPOSITORY / ".local" / "oe50"


class SystemPlaytestGuards(unittest.TestCase):
    def setUp(self):
        PRIVATE_ROOT.mkdir(parents=True, exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="path-guards-", dir=PRIVATE_ROOT)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.data = self.root / "authored-data"
        self.resources = self.root / "authored-resources"
        self.data.mkdir()
        self.resources.mkdir()
        (self.data / "read-only-input.txt").write_bytes(b"authored guard input\n")
        (self.resources / "metadata.txt").write_bytes(b"authored metadata\n")

    def inventory(self):
        return {str(path.relative_to(self.root)): (
            "link", str(path.readlink())) if path.is_symlink() else (
                "directory", None) if path.is_dir() else ("file", path.read_bytes())
            for path in self.root.rglob("*")}

    def check_paths(self, app: Path, output: Path, *, accepted=False):
        before = self.inventory()
        result = subprocess.run([str(EXECUTABLE), "--check-paths", str(self.data),
                                 str(self.resources), str(app), str(output)],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(self.inventory(), before, "Path-only validation mutated an input or output")
        if accepted:
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("no output created", result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("System playtest FAIL:", result.stderr)
        return result

    def test_fresh_disjoint_oe50_paths_validate_without_writes(self):
        self.check_paths(self.root / "app", self.root / "captures", accepted=True)

    def test_identical_or_nested_destinations_are_rejected(self):
        for app, output in ((self.root / "same", self.root / "same"),
                            (self.root / "app", self.root / "app" / "captures"),
                            (self.root / "captures" / "app", self.root / "captures")):
            with self.subTest(app=app, output=output):
                self.check_paths(app, output)

    def test_existing_destinations_are_rejected(self):
        existing = self.root / "existing"
        existing.mkdir()
        self.check_paths(existing, self.root / "new-captures")
        self.check_paths(self.root / "new-app", existing)

    def test_task_root_and_outside_destinations_are_rejected(self):
        outside = REPOSITORY / ".local" / "oe49" / ("guard-" + self.root.name)
        self.check_paths(PRIVATE_ROOT, self.root / "captures")
        self.check_paths(self.root / "app", PRIVATE_ROOT)
        self.check_paths(outside / "app", outside / "captures")
        self.assertFalse(outside.exists())

    def test_symlink_destinations_and_ancestors_are_rejected(self):
        link = self.root / "linked"
        link.symlink_to(self.root / "unused-target", target_is_directory=True)
        self.check_paths(link, self.root / "captures")
        self.check_paths(link / "app", self.root / "captures")
        self.check_paths(self.root / "app", link / "captures")

    def test_data_and_resource_overlap_are_rejected(self):
        self.check_paths(self.data / "app", self.root / "captures")
        self.check_paths(self.root / "app", self.resources / "captures")
        self.check_paths(self.resources, self.root / "captures")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    options, remaining = parser.parse_known_args()
    EXECUTABLE = options.executable.resolve(strict=True)
    unittest.main(argv=[__file__, *remaining])
