#!/usr/bin/env python3
"""Hermetic OE-07 comparison-tool contracts and independent authored goldens.

The real fork/model execution is a separate explicit integration check. These
controls run without either private input, and never download or modify them.
"""
from __future__ import annotations

import argparse
from contextlib import redirect_stdout
import copy
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import original_rules_parity as parity


class CaseControls(unittest.TestCase):
    def setUp(self):
        self.catalog = parity.cases()
        self.by_id = {case["id"]: case for case in self.catalog}

    def test_catalog_covers_each_independent_operation(self):
        self.assertEqual(len(self.by_id), len(self.catalog))
        self.assertGreaterEqual(len(self.catalog), 74)
        self.assertEqual({case["classification"] for case in self.catalog},
                         {"parity", "corrected_bug", "unverified", "unsupported"})
        self.assertTrue({"parse", "building_value", "house_value", "spread", "order",
                         "needs", "evolve", "timers", "visit", "spawn", "spawn_tick",
                         "need", "delivery", "consume", "food"}.issubset(
                             {case["input"]["operation"] for case in self.catalog}))
        for case in self.catalog:
            with self.subTest(case=case["id"]):
                self.assertTrue((parity.ROOT / case["evidence"].split("#")[0]).is_file())
                self.assertEqual(set(case["expected"]), {"fork", "current"})
                for values in case["expected"].values():
                    self.assertTrue(values)
                    self.assertTrue(all(type(value) is int for value in values))
                if case["classification"] == "parity":
                    self.assertEqual(case["expected"]["fork"], case["expected"]["current"])

    def test_independently_calculated_discriminators(self):
        # These literal results were calculated independently from the adapter:
        # signed truncation, ring geometry, alternative-stock predicates,
        # exact original gates, conservation, and strict floating thresholds.
        goldens = {
            "model.signed_percent": ([-123, -92], [-123, -92]),
            "model.house_upper_clamp": ([150, 100], [150, 100]),
            "model.house_lower_clamp": ([-110, -99], [-110, -99]),
            "rings.zero_step": ([0, 9, 9, 9, 9, 9], [0, 9, 6, 3, 0, 0]),
            "rings.negative_step": ([0, -9, -9, -9, -9, -9], [0, -9, -6, -3, 0, 0]),
            "rings.footprint_two": ([0, 0, 7, 7, 0, 0], [0, 0, 7, 7, 0, 0]),
            "rings.order_ppn": ([0], [0]),
            "rings.order_pnp": ([100], [100]),
            "needs.bronze_lacquer_not_sum": ([0], [0]),
            "needs.lacquer_alternative": ([1], [1]),
            "evolution.vacant_needs": ([3, 0], [2, 0]),
            "evolution.empty_shelter": ([4, 0], [3, 0]),
            "evolution.devolve_limit": ([3, 0], [3, 0]),
            "spawn.herbalist": ([7], [5]),
            "spawn.music": ([1], [3]),
            "spawn.no_staff": ([0, 1], [64, 0]),
            "spawn.byte_wrap": ([0, 1], [0, 0]),
            "goods.next_row": ([3], [4]),
            "goods.large_elite": ([48], [104]),
            "delivery.large_source": ([3, 97, 3], [10, 90, 10]),
            "delivery.partial_source": ([3, 2, 3], [3, 2, 3]),
            "delivery.empty_common": ([10, 90, 10], [0, 100, 0]),
            "delivery.stock_above_need_tiny_source": ([1, 0, 8], [0, 1, 7]),
            "delivery.stock_below_need_large_source": ([1, 99, 3], [8, 92, 10]),
            "consume.separate_alternatives": ([2] * 6, [2] * 6),
            "consume.distinct_fields": ([29, 25, 17, 11, 7, 3], [29, 25, 17, 11, 7, 3]),
            "consume.negative_occupants": ([5, 0, 0, 0, 0, 0], [2, 0, 0, 0, 0, 0]),
            "food.weighted_half": ([70], [70]),
            "food.weighted_above_half": ([60], [60]),
            "food.zero_delivery_boundary": ([20], [10]),
            "food.binary32_half": ([60], [70]),
            "food.binary32_third_decimal": ([70], [75]),
            "food.binary32_three": ([45], [50]),
        }
        for commodity, expected in enumerate((2, 4, 6, 8, 10, 10)):
            goldens[f"goods.distinct_field_{commodity}"] = ([expected], [expected])
        for identity, (fork, current) in goldens.items():
            with self.subTest(case=identity):
                self.assertEqual(self.by_id[identity]["expected"],
                                 {"fork": fork, "current": current})
        self.assertEqual(self.by_id["food.zero_delivery_boundary"]["classification"],
                         "unsupported")
        for identity in ("half", "third_decimal", "three"):
            self.assertEqual(self.by_id[f"food.binary32_{identity}"]["classification"],
                             "unverified")

    def test_catalog_rejects_duplicate_ids_unknown_class_and_invalid_evidence(self):
        for mutation in ("duplicate", "unknown_class", "boolean", "missing_evidence", "missing_anchor"):
            with self.subTest(mutation=mutation):
                catalog = copy.deepcopy(self.catalog)
                if mutation == "duplicate":
                    catalog.append(copy.deepcopy(catalog[0]))
                elif mutation == "unknown_class":
                    catalog[0]["classification"] = "assumed-original-parity"
                elif mutation == "boolean":
                    catalog[0]["expected"]["fork"] = [True]
                elif mutation == "missing_evidence":
                    catalog[0]["evidence"] = "docs/this-file-does-not-exist.md"
                else:
                    catalog[0]["evidence"] = "docs/reverse/original-model-contract.md#this-heading-does-not-exist"
                with self.assertRaises(ValueError):
                    parity.validate_cases(catalog)

    def test_goods_results_conserve_each_source_plus_destination(self):
        for case in self.catalog:
            if case["input"]["operation"] != "delivery":
                continue
            values = case["input"]["values"]
            commodity, initial_source = values[0], values[-1]
            initial_destination = values[2 + 15 + commodity]
            for side in ("fork", "current"):
                moved, remaining, destination = case["expected"][side]
                with self.subTest(case=case["id"], side=side):
                    self.assertGreaterEqual(moved, 0)
                    self.assertEqual(initial_source - remaining, moved)
                    self.assertEqual(destination - initial_destination, moved)
                    self.assertEqual(initial_source + initial_destination,
                                     remaining + destination)

    def test_wrong_expectation_and_missing_side_never_pass(self):
        case = self.by_id["food.binary32_half"]
        expected = copy.deepcopy(case["expected"])
        self.assertTrue(parity.compare_observations(case, expected)["passed"])
        expected["current"][0] += 1
        self.assertFalse(parity.compare_observations(case, expected)["passed"])
        with self.assertRaises(ValueError):
            parity.compare_observations(case, {"fork": [60]})
        self.assertFalse(parity.compare_observations(case, {"fork": [60], "current": [70, 0]})["passed"])

    def test_boolean_cannot_impersonate_numeric_observation(self):
        case = {"id": "boolean-negative", "expected": {"fork": [1], "current": [0]}}
        with self.assertRaises(ValueError):
            parity.compare_observations(case, {"fork": [True], "current": [False]})

    def test_probe_rejects_truncation_extra_rows_and_nonintegers(self):
        for stdout in ("", "[1]\n[2]\n", "{\"value\":1}\n", "[true]\n",
                       "[1.0]\n", "[null]\n", "not-json\n"):
            with self.subTest(stdout=stdout), mock.patch.object(
                    parity, "checked_run", return_value=subprocess.CompletedProcess(
                        [], 0, stdout, "")):
                with self.assertRaises((ValueError, RuntimeError)):
                    parity.probe(Path("unused"), Path("unused-fixture"), ["parse"])
        with mock.patch.object(parity, "checked_run", return_value=subprocess.CompletedProcess(
                [], 0, "[1,-2,2147483647]\n", "")):
            self.assertEqual(parity.probe(Path("unused"), Path("unused-fixture"), ["parse"]),
                             [[1, -2, 2147483647]])


class ForkPinControls(unittest.TestCase):
    def fake_fork(self, root: Path):
        fork = root / "authored-fork-control"
        fork.mkdir()
        source = fork / "authored.cpp"
        source.write_text("// independently authored pin control\n", encoding="utf-8")
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        manifest = {"fork_head": parity.FORK_REVISION, "files": [
            {"path": source.name, "bytes": source.stat().st_size, "sha256": digest}]}
        (fork / "file-manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        return fork, source, manifest, {source.name: digest}

    def test_missing_and_present_bad_sources_have_different_outcomes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(parity.SkipComparison):
                parity.validate_fork(root / "missing")
            (root / "empty").mkdir()
            with self.assertRaises(ValueError):
                parity.validate_fork(root / "empty")
            (root / "not-directory").write_text("present", encoding="utf-8")
            with self.assertRaises(ValueError):
                parity.validate_fork(root / "not-directory")
            (root / "dangling").symlink_to(root / "absent-target")
            with self.assertRaises(ValueError):
                parity.validate_fork(root / "dangling")

    def test_manifest_cannot_redefine_the_pinned_source(self):
        with tempfile.TemporaryDirectory() as directory:
            fork, source, manifest, pins = self.fake_fork(Path(directory))
            with mock.patch.object(parity, "FORK_SHA256", pins):
                self.assertEqual(parity.validate_fork(fork)["files"], pins)
                source.write_text("// edited source\n", encoding="utf-8")
                manifest["files"][0]["sha256"] = hashlib.sha256(source.read_bytes()).hexdigest()
                manifest["files"][0]["bytes"] = source.stat().st_size
                (fork / "file-manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
                with self.assertRaises(ValueError):
                    parity.validate_fork(fork)

    def test_manifest_revision_and_complete_unique_entries_are_required(self):
        for mutation in ("wrong_revision", "duplicate", "wrong_size", "missing"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as directory:
                fork, source, manifest, pins = self.fake_fork(Path(directory))
                if mutation == "wrong_revision":
                    manifest["fork_head"] = "0" * 40
                elif mutation == "duplicate":
                    manifest["files"].append(copy.deepcopy(manifest["files"][0]))
                elif mutation == "wrong_size":
                    manifest["files"][0]["bytes"] += 1
                else:
                    source.unlink()
                (fork / "file-manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
                with mock.patch.object(parity, "FORK_SHA256", pins), self.assertRaises(ValueError):
                    parity.validate_fork(fork)

    def test_symlinked_source_is_not_accepted_as_a_verified_file(self):
        with tempfile.TemporaryDirectory() as directory:
            fork, source, _, pins = self.fake_fork(Path(directory))
            saved = fork / "other.cpp"
            source.rename(saved)
            source.symlink_to(saved)
            with mock.patch.object(parity, "FORK_SHA256", pins), self.assertRaises(ValueError):
                parity.validate_fork(fork)


class HousingNamespaceControls(unittest.TestCase):
    def test_reviewed_world_and_terrain_dto_headers_are_fingerprinted(self):
        reviewed = parity.validate_housing_world_api()
        self.assertEqual(reviewed, parity.REVIEWED_HOUSING_WORLD_API_GIT_BLOBS)
        self.assertEqual(set(reviewed), {"World.h", "OriginalTerrainFacts.h", "OriginalHousingDiagnosis.h",
                                        "OriginalSafeDemolitionState.h"})
        fingerprints = parity.current_source_hashes()
        for name in reviewed:
            self.assertIn("src/simulation/" + name, fingerprints)

    def test_unreviewed_missing_or_symlinked_world_api_headers_are_rejected(self):
        for name in parity.REVIEWED_HOUSING_WORLD_API_GIT_BLOBS:
            for mutation in ("changed", "missing", "symlink"):
                with self.subTest(header=name, mutation=mutation), tempfile.TemporaryDirectory() as directory:
                    root = Path(directory)
                    for member in parity.REVIEWED_HOUSING_WORLD_API_GIT_BLOBS:
                        target = root / "src/simulation" / member
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes((parity.ROOT / "src/simulation" / member).read_bytes())
                    target = root / "src/simulation" / name
                    if mutation == "changed":
                        target.write_bytes(target.read_bytes() + b"\n// unreviewed API drift\n")
                    elif mutation == "missing":
                        target.unlink()
                    else:
                        alternate = root / "same-header-bytes"
                        target.rename(alternate)
                        target.symlink_to(alternate)
                    with self.assertRaisesRegex(ValueError, "World API drift"):
                        parity.validate_housing_world_api(root)

    def test_isolation_changes_only_exact_namespace_and_local_include_lines(self):
        source = ('#include "simulation/World.h"\n'
                  '#include "simulation/original/HouseEvolution.h"\n'
                  'namespace openemperor::original {\n'
                  'int authored_control(int value) { return value * 17 - 3; }\n'
                  '}  // namespace openemperor::original\n')
        expected = ('#include "simulation/World.h"\n'
                    '#include "HouseEvolution.h"\n'
                    'namespace openemperor::parity_fork_original {\n'
                    'int authored_control(int value) { return value * 17 - 3; }\n'
                    '}  // namespace openemperor::parity_fork_original\n')
        self.assertEqual(parity.isolate_housing_source("OriginalHousingView.cpp", source), expected)
        for suffix in ("", "\n"):
            self.assertEqual(parity.isolate_housing_source("ModelData.h", source.rstrip("\n") + suffix),
                             expected.rstrip("\n") + suffix)

    def test_unreviewed_namespace_includes_and_module_names_are_rejected(self):
        source = 'namespace openemperor::original {\n}  // namespace openemperor::original\n'
        for altered in (source.replace("namespace openemperor::original {", "namespace other {"),
                        source + source,
                        source + '#include "simulation/original/FutureRules.h"\n',
                        source + 'openemperor::original::HouseState other;\n'):
            with self.subTest(altered=altered), self.assertRaises(ValueError):
                parity.isolate_housing_source("ModelData.h", altered)
        with self.assertRaises(ValueError):
            parity.isolate_housing_source("FutureRules.cpp", source)

    def test_generated_copies_keep_input_pins_and_record_exact_transformation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fork = root / "fork"
            output = root / "output"
            output.mkdir()
            pins = {}
            originals = {}
            for module in parity.HOUSING_FORK_MODULES:
                for suffix in (".h", ".cpp"):
                    name = module + suffix
                    relative = "src/simulation/original/" + name
                    text = ('#include "simulation/World.h"\n'
                            f'#include "simulation/original/{module}.h"\n'
                            'namespace openemperor::original {\n'
                            'int authored_control = 37;\n'
                            '}  // namespace openemperor::original\n')
                    path = fork / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text(text)
                    originals[relative] = path.read_bytes()
                    pins[relative] = hashlib.sha256(originals[relative]).hexdigest()
            with mock.patch.object(parity, "FORK_SHA256", pins):
                generated = parity.prepare_housing_fork(fork, output)
                with self.assertRaises(FileExistsError):
                    parity.prepare_housing_fork(fork, output)
                manifest = json.loads((generated / "transform-manifest.json").read_text())
                self.assertEqual(len(manifest["files"]), 8)
                for relative, original in originals.items():
                    self.assertEqual((fork / relative).read_bytes(), original)
                    name = Path(relative).name
                    emitted = (generated / name).read_bytes()
                    self.assertEqual(emitted.decode(), parity.isolate_housing_source(name, original.decode()))
                    self.assertEqual(manifest["files"][name], {
                        "source_sha256": pins[relative],
                        "generated_sha256": hashlib.sha256(emitted).hexdigest()})
                changed = fork / "src/simulation/original/ModelData.h"
                changed.write_text(changed.read_text() + "// unauthorized source mutation\n")
                other = root / "other-output"
                other.mkdir()
                with self.assertRaisesRegex(ValueError, "pinned housing fork source mismatch"):
                    parity.prepare_housing_fork(fork, other)

    def test_only_fourth_probe_uses_namespace_isolated_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            isolated = root / "isolated"
            with mock.patch.object(parity, "prepare_housing_fork", return_value=isolated), \
                    mock.patch.object(parity, "checked_run", return_value=subprocess.CompletedProcess(
                        [], 0, "-I/authored/openssl/include -L/authored/openssl/lib -lcrypto", "")) as run:
                parity.compile_probes(root / "fork", root, "unused-compiler", "debug", root / "simulation.a")
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(len(commands), 4)  # pkg-config and three compilers.
            fork, current, housing = commands[1:]
            self.assertIn(str(root / "fork/src/simulation/original/HouseEvolution.cpp"), fork)
            self.assertIn(str(parity.ROOT / "src/simulation/original/HouseEvolution.cpp"), current)
            self.assertNotIn(str(root / "fork/src"), housing)
            for module in parity.HOUSING_FORK_MODULES:
                self.assertIn(str(isolated / (module + ".cpp")), housing)
            libraries = [str(path) for path in parity.housing_link_libraries(root / "simulation.a")]
            start = housing.index(str(root / "simulation.a"))
            self.assertEqual(housing[start:start + len(libraries)], libraries)
            self.assertEqual(housing[start + len(libraries):start + len(libraries) + 3],
                             ["-I/authored/openssl/include", "-L/authored/openssl/lib", "-lcrypto"])
            self.assertFalse(any(path in fork or path in current for path in libraries))


class OutputAndStatusControls(unittest.TestCase):
    def test_cli_dangling_fork_is_failure_before_path_resolution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fork = root / "dangling-fork"
            fork.symlink_to(root / "absent-target")
            output = root / "result"
            with mock.patch.object(parity, "compile_probes") as compiler, redirect_stdout(io.StringIO()):
                self.assertEqual(parity.main(["--fork", str(fork), "--output", str(output)]), 1)
            compiler.assert_not_called()
            report = json.loads((output / "report.json").read_text())
            self.assertEqual(report["status"], "FAIL")
            self.assertTrue(fork.is_symlink())

    def test_output_is_fresh_and_cannot_overlap_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo = root / "repo"
            repo.mkdir()
            source = root / "original-inputs"
            source.mkdir()
            output = repo / ".local" / "capture"
            self.assertEqual(parity.fresh_output(output, repo=repo, inputs=(source,)), output.resolve())
            marker = output / "previous-PASS.json"
            marker.write_text('{"status":"PASS"}', encoding="utf-8")
            for bad in (output, repo / "tests" / "public-result", source / "inside", root):
                with self.subTest(path=str(bad)), self.assertRaises(ValueError):
                    parity.fresh_output(bad, repo=repo, inputs=(source,))
            self.assertEqual(marker.read_text(encoding="utf-8"), '{"status":"PASS"}')
            self.assertFalse((repo / "tests").exists())

    def test_symlinked_private_parent_cannot_escape_into_read_only_data(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, source = root / "repo", root / "original-inputs"
            repo.mkdir()
            source.mkdir()
            (repo / ".local").symlink_to(source, target_is_directory=True)
            with self.assertRaises(ValueError):
                parity.fresh_output(repo / ".local" / "capture", repo=repo, inputs=(source,))
            self.assertEqual(list(source.iterdir()), [])

    def test_private_parent_symlink_cannot_publish_outside_checkout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            repo, outside = root / "repo", root / "outside"
            repo.mkdir()
            outside.mkdir()
            (repo / ".local").symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError):
                parity.fresh_output(repo / ".local" / "capture", repo=repo)
            self.assertEqual(list(outside.iterdir()), [])

    def test_implicit_read_only_and_build_paths_are_protected(self):
        with tempfile.TemporaryDirectory() as directory:
            repo = Path(directory) / "repo"
            repo.mkdir()
            for relative in (".local/gog-extracted/app/report",
                             ".local/original-economy/oe00/fork/report",
                             ".local/reswall/release-build/report"):
                with self.subTest(relative=relative), self.assertRaises(ValueError):
                    parity.fresh_output(repo / relative, repo=repo)
            self.assertEqual(list(repo.iterdir()), [])

    def test_main_maps_actual_skip_and_fail_without_reusing_previous_output(self):
        for error, expected_code, status in ((parity.SkipComparison("absent"), 77, "SKIP"),
                                             (ValueError("present malformed"), 1, "FAIL"),
                                             (FileNotFoundError("compiler missing"), 1, "FAIL")):
            with self.subTest(status=status), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                output = root / "capture"
                with mock.patch.object(parity, "run_comparison", side_effect=error), redirect_stdout(io.StringIO()):
                    code = parity.main(["--fork", str(root / "fork"), "--output", str(output)])
                self.assertEqual(code, expected_code)
                self.assertEqual(json.loads((output / "report.json").read_text())["status"], status)
                before = (output / "report.json").read_bytes()
                with mock.patch.object(parity, "run_comparison") as runner, redirect_stdout(io.StringIO()):
                    self.assertEqual(parity.main(["--fork", str(root / "fork"), "--output", str(output)]), 1)
                runner.assert_not_called()
                self.assertEqual((output / "report.json").read_bytes(), before)

    def test_public_report_removes_local_build_details(self):
        with tempfile.TemporaryDirectory() as directory:
            report = self.comparison_without_compilers(Path(directory), None)
            report["secret_extra"] = "not-public"
            public = parity.public_report(report)
            self.assertNotIn("build_provenance", public)
            self.assertNotIn("configuration", public)
            self.assertNotIn("secret_extra", public)
            self.assertEqual(public["cases"], report["cases"])
            self.assertEqual(report["configuration"], "debug")

    def test_incomplete_or_failed_report_cannot_be_exported(self):
        with tempfile.TemporaryDirectory() as directory:
            valid = self.comparison_without_compilers(Path(directory), None)
            for mutation in ("skip", "empty", "missing_area", "failed_observation", "failed_housing", "wrong_housing"):
                with self.subTest(mutation=mutation):
                    report = copy.deepcopy(valid)
                    if mutation == "skip":
                        report["status"] = "SKIP"
                    elif mutation == "empty":
                        report["cases"] = []
                    elif mutation == "missing_area":
                        report["cases"] = [case for case in report["cases"] if case["area"] != "desirability"]
                    elif mutation == "failed_observation":
                        report["cases"][0]["passed"] = False
                    elif mutation == "failed_housing":
                        report["housing_view"]["passed"] = False
                    else:
                        report["housing_view"]["fork_observed"][0] += 1
                    with self.assertRaises(ValueError):
                        parity.public_report(report)

    def test_export_rejects_rewritten_goldens_wrong_pins_and_stale_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            valid = self.comparison_without_compilers(Path(directory), None)
            for mutation in ("golden", "classification", "missing_case", "summary", "fork_revision",
                             "fork_hash", "source_hash", "housing_boolean", "housing_both_rewritten"):
                with self.subTest(mutation=mutation):
                    report = copy.deepcopy(valid)
                    if mutation == "golden":
                        rewritten = copy.deepcopy(report["cases"][0]["expected"]["current"])
                        rewritten[0] = 999
                        report["cases"][0]["expected"]["current"] = rewritten
                        report["cases"][0]["observed"]["current"] = copy.deepcopy(rewritten)
                    elif mutation == "classification":
                        report["cases"][0]["classification"] = "unverified"
                    elif mutation == "missing_case":
                        report["cases"].pop()
                    elif mutation == "summary":
                        report["summary"]["passed"] += 1
                    elif mutation == "fork_revision":
                        report["fork"]["revision"] = "0" * 40
                    elif mutation == "fork_hash":
                        first = next(iter(report["fork"]["files"]))
                        report["fork"]["files"][first] = "0" * 64
                    elif mutation == "source_hash":
                        first = next(iter(report["current"]["files"]))
                        report["current"]["files"][first] = "0" * 64
                    elif mutation == "housing_boolean":
                        report["housing_view"]["fork_observed"][-1] = True
                    else:
                        report["housing_view"]["fork_observed"][0] = 999
                        report["housing_view"]["fork_expected"][0] = 999
                    with self.assertRaises(ValueError):
                        parity.public_report(report)

    def comparison_without_compilers(self, directory: Path, data: Path | None):
        output = directory / "result"
        output.mkdir()
        library = directory / "authored-unused-library.a"
        for path in parity.housing_link_libraries(library):
            path.write_text("test-only orchestration control", encoding="utf-8")
        args = argparse.Namespace(fork=directory / "fork", data=data, compiler="unused",
                                  configuration="debug", simulation_library=library)
        binaries = {side + "-probe": Path(side + "-probe") for side in ("fork", "current", "housing")}
        catalog = parity.cases()

        def fake_probe(binary, fixture, commands):
            kind = fixture.parents[1].name
            side = "fork" if binary.name == "fork-probe" else "current"
            group = [case for case in catalog if case["input"]["fixture"] == kind]
            self.assertEqual(len(group), len(commands))
            return [copy.deepcopy(case["expected"][side]) for case in group]

        housing = [35, 43, 54, 59, 193, 72, 207, 1, 0, 3, 3, 4, 4, 1, 1, 3, 4, 1]
        with mock.patch.object(parity, "validate_fork", return_value={"revision": parity.FORK_REVISION, "files": copy.deepcopy(parity.FORK_SHA256)}), \
                mock.patch.object(parity, "compile_probes", return_value=binaries), \
                mock.patch.object(parity, "probe", side_effect=fake_probe), \
                mock.patch.object(parity, "checked_run", return_value=subprocess.CompletedProcess(
                    [], 0, json.dumps(housing), "")):
            return parity.run_comparison(args, output)

    def test_real_data_omission_and_explicit_absence_are_distinct(self):
        for supplied, expected_status, real_status in ((False, "PASS", "NOT_RUN"),
                                                       (True, "SKIP", "SKIP")):
            with self.subTest(supplied=supplied), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                report = self.comparison_without_compilers(root, root / "absent" if supplied else None)
                self.assertEqual(report["status"], expected_status)
                self.assertEqual(report["real_model"]["status"], real_status)
                self.assertEqual(report["summary"]["passed"], len(parity.cases()))
                self.assertTrue(all(case["passed"] for case in report["cases"]))

    def test_present_invalid_real_input_is_failure_rather_than_skip(self):
        for kind in ("malformed", "directory", "oversized", "symlink"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                data = root / "data"
                model = data / "Model" / "EmperorBuildingModels.txt"
                model.parent.mkdir(parents=True)
                if kind == "directory":
                    model.mkdir()
                elif kind == "oversized":
                    model.write_bytes(b"x" * (256 * 1024 + 1))
                elif kind == "symlink":
                    model.symlink_to(root / "absent-model")
                else:
                    model.write_text("present malformed original", encoding="utf-8")
                before = sorted(str(path.relative_to(data)) for path in data.rglob("*"))
                with self.assertRaises(ValueError):
                    self.comparison_without_compilers(root, data)
                self.assertEqual(sorted(str(path.relative_to(data)) for path in data.rglob("*")), before)

    def test_existing_invalid_real_data_parent_cannot_be_skipped(self):
        for kind in ("data_file", "data_dangling", "model_parent_file", "model_parent_dangling"):
            with self.subTest(kind=kind), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                data = root / "data"
                if kind == "data_file":
                    data.write_text("present file", encoding="utf-8")
                elif kind == "data_dangling":
                    data.symlink_to(root / "absent-data")
                else:
                    data.mkdir()
                    if kind == "model_parent_file":
                        (data / "Model").write_text("present file", encoding="utf-8")
                    else:
                        (data / "Model").symlink_to(root / "absent-model-directory")
                with self.assertRaises(ValueError):
                    self.comparison_without_compilers(root, data)


class CommittedEvidenceControls(unittest.TestCase):
    def test_committed_report_matches_current_sources_and_canonical_vectors(self):
        path = parity.ROOT / "docs/original-economy/core-parity.json"
        self.assertTrue(path.is_file(), "mandatory OE-07 machine-readable comparison is missing")
        self.assertFalse(path.is_symlink())
        self.assertLessEqual(path.stat().st_size, 1024 * 1024)
        report = json.loads(path.read_text(encoding="utf-8"))
        # This invokes the independent pin, current-code hashes, canonical
        # vectors, classifications, complete summary and fourth-module checks.
        self.assertEqual(parity.public_report(report), report)
        self.assertEqual(report["schema_version"], 1)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["real_model"]["status"], "PASS")
        self.assertEqual(report["real_model"]["sha256"], parity.MODEL_SHA256)
        self.assertEqual(report["summary"]["cases"], len(parity.cases()))


if __name__ == "__main__":
    unittest.main()
