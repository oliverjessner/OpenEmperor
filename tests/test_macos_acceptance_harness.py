"""Acceptance-tool boundaries; no compiler, original game or desktop is executed."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))
import alpha_check as alpha
import package_macos as package
import verify_macos_bundle as bundle

PACK = REPO / "resources/compatibility" / bundle.COMPATIBILITY_ID


class OutputIsolation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="oe-macos-harness-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def test_fresh_paths_do_not_create_or_replace_anything(self):
        paths = {"build": self.root / "build", "report": self.root / "report.json"}
        self.assertEqual(package.fresh_output_paths(paths), paths)
        self.assertEqual(list(self.root.iterdir()), [])

    def test_prior_evidence_is_preserved(self):
        prior = self.root / "report.json"
        prior.write_bytes(b"retained evidence")
        with self.assertRaisesRegex(ValueError, "existing output is preserved"):
            package.fresh_output_paths({"report": prior})
        self.assertEqual(prior.read_bytes(), b"retained evidence")

    def test_nested_outputs_and_original_overlap_refused(self):
        for paths, data in [({"build": self.root / "new", "report": self.root / "new/report.json"}, None),
                            ({"dist": self.root / "data/new"}, self.root / "data"),
                            ({"dist": self.root / "new"}, self.root / "new/data")]:
            with self.subTest(paths=paths), self.assertRaises(ValueError):
                package.fresh_output_paths(paths, data)

    def test_linked_and_non_directory_components_refused(self):
        (self.root / "link").symlink_to(self.root / "missing")
        (self.root / "file").write_text("input")
        for path in [self.root / "link/out", self.root / "file/out"]:
            with self.subTest(path=path), self.assertRaises(ValueError):
                package.fresh_output_paths({"build": path})

    def test_repository_and_maintained_source_outputs_refused(self):
        for path in [REPO, REPO.parent, REPO / "src/new-build", REPO / ".git/new-report"]:
            with self.subTest(path=path), self.assertRaises(ValueError):
                package.fresh_output_paths({"build": path})

    def test_package_arguments_are_effective_and_fresh(self):
        data = self.root / "original"
        data.mkdir()
        args = package.parse_args(["--build-dir", str(self.root / "build"), "--dist-dir",
            str(self.root / "dist"), "--data", str(data), "--jobs", "2"])
        self.assertEqual((args.build_dir, args.dist_dir, args.data, args.jobs),
                         (self.root / "build", self.root / "dist", data, 2))
        self.assertEqual(args.min_os, "26.0")
        with self.assertRaises(ValueError):
            package.parse_args(["--jobs", "0"])

    def test_alpha_forwards_original_model_root_to_every_configuration(self):
        for kind, sanitized in [("Debug", False), ("Release", False), ("RelWithDebInfo", True)]:
            with self.subTest(kind=kind), mock.patch.object(alpha, "run", return_value=("", 0)) as call:
                alpha.configure(self.root / kind, kind, 2, True, sanitizers=sanitized,
                                data=self.root / "original")
                cmake = call.call_args_list[0].args[0]
                self.assertIn("-DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=" + str(self.root / "original"), cmake)
                self.assertIn("-DOPENEMPEROR_ENABLE_SANITIZERS=" + ("ON" if sanitized else "OFF"), cmake)
                self.assertEqual(call.call_args_list[1].args[0][-2:], ["--parallel", "2"])

    def test_optional_configuration_does_not_override_default_with_invalid_empty_root(self):
        with mock.patch.object(alpha, "run", return_value=("", 0)) as call:
            alpha.configure(self.root / "debug", "Debug", 2, True)
        self.assertFalse(any(arg.startswith("-DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=")
                             for arg in call.call_args_list[0].args[0]))

    def test_package_optional_data_default_and_explicit_forwarding(self):
        for data in (None, self.root / "supplied-original"):
            args = argparse.Namespace(build_dir=self.root / "new-build", dist_dir=self.root / "new-dist",
                                      data=data, jobs=2, min_os="26.0")
            with self.subTest(data=data), mock.patch.object(package, "parse_args", return_value=args), \
                 mock.patch.object(package.sys, "platform", "darwin"), \
                 mock.patch.object(package.shutil, "which", return_value="/test/tool"), \
                 mock.patch.object(package, "call", side_effect=["arm64\n", "configured", "built"]) as call, \
                 mock.patch("builtins.print"):
                # Stop at the deliberately absent build output; no subprocess/build ran.
                with self.assertRaises(FileNotFoundError):
                    package.main()
            command = call.call_args_list[1].args
            overrides = [v for v in command if v.startswith("-DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=")]
            self.assertEqual(overrides, [] if data is None else
                             ["-DOPENEMPEROR_ORIGINAL_MODEL_TEST_ROOT=" + str(data)])

    def test_explicit_minimum_os_is_validated_without_changing_the_default(self):
        args = package.parse_args(["--build-dir", str(self.root / "build"), "--dist-dir",
            str(self.root / "dist"), "--min-os", "27.0"])
        self.assertEqual(args.min_os, "27.0")
        self.assertEqual(package.MIN_OS, "26.0")
        for value in ("26.0", "26.0.0", "26.1", "27.0", "27.0.1"):
            with self.subTest(value=value):
                self.assertEqual(package.validated_min_os(value), value)
        for value in ("", "27", "v27.0", "27.0-beta", " 27.0", "27.0 ", "027.0",
                      "27.00", "27..0", "27.0.0.0", "27.-1", "NaN", "27.0\n", "２７.0",
                      "9" * 33 + ".0", None, True):
            with self.subTest(value=value), self.assertRaises(ValueError):
                package.validated_min_os(value)
        for value in ("0.0", "25.0", "25.99.99"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "lower than"):
                package.validated_min_os(value)
        for value in ("25.99.99", "27", "27.0-beta"):
            with self.subTest(cli_value=value), self.assertRaises(ValueError):
                package.parse_args(["--build-dir", str(self.root / "build"), "--dist-dir",
                    str(self.root / "dist"), "--min-os=" + value])
        self.assertEqual(list(self.root.iterdir()), [])

    def test_selected_minimum_os_drives_cmake_deployment(self):
        for selected in ("26.0", "27.0"):
            args = argparse.Namespace(build_dir=self.root / "new-build", dist_dir=self.root / "new-dist",
                                      data=None, jobs=2, min_os=selected)
            with self.subTest(selected=selected), mock.patch.object(package, "parse_args", return_value=args), \
                 mock.patch.object(package.sys, "platform", "darwin"), \
                 mock.patch.object(package.shutil, "which", return_value="/test/tool"), \
                 mock.patch.object(package, "call", side_effect=["arm64\n", "configured", "built"]) as call, \
                 mock.patch("builtins.print"):
                with self.assertRaises(FileNotFoundError):
                    package.main()
            command = call.call_args_list[1].args
            self.assertEqual([item for item in command if item.startswith("-DCMAKE_OSX_DEPLOYMENT_TARGET=")],
                             ["-DCMAKE_OSX_DEPLOYMENT_TARGET=" + selected])

    def test_build_info_must_match_selected_minimum_os_exactly(self):
        for selected in ("26.0", "27.0"):
            info = {"display_version": "authored", "project_version": "1.0", "revision": "test",
                    "dirty": True, "architecture": "arm64", "build_type": "Release",
                    "deployment_target": selected}
            with self.subTest(selected=selected):
                package.validate_build_info(info, selected)
                for conflicting in ("25.0", "26.0" if selected == "27.0" else "27.0", "27.0.0"):
                    wrong = dict(info, deployment_target=conflicting)
                    with self.assertRaises(RuntimeError):
                        package.validate_build_info(wrong, selected)

    def test_notice_is_owned_by_actual_linked_version(self):
        actual = self.root / "Cellar/openssl@4/4.0.3"
        (actual / "lib").mkdir(parents=True)
        library = actual / "lib/libcrypto.4.dylib"
        library.write_bytes(b"independently authored dependency identity")
        (actual / "LICENSE.txt").write_text("actual notice")
        alias = self.root / "libcrypto.dylib"
        alias.symlink_to(library)
        result = package.dependency_license(alias, "LICENSE.txt")
        self.assertEqual(result["notice"], actual / "LICENSE.txt")
        self.assertEqual(result["version"], "4.0.3")
        (actual / "LICENSE.txt").unlink()
        with self.assertRaises(RuntimeError):
            package.dependency_license(alias, "LICENSE.txt")

    def test_header_license_follows_configured_cmake_package(self):
        prefix = self.root / "json/3.12.0"
        config = prefix / "share/cmake/nlohmann_json"
        config.mkdir(parents=True)
        header = prefix / "include/nlohmann/detail/abi_macros.hpp"
        header.parent.mkdir(parents=True)
        header.write_text("#define NLOHMANN_JSON_VERSION_MAJOR 3\n#define NLOHMANN_JSON_VERSION_MINOR 12\n#define NLOHMANN_JSON_VERSION_PATCH 0\n")
        (prefix / "LICENSE.MIT").write_text("configured notice")
        build = self.root / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text("nlohmann_json_DIR:PATH=" + str(config) + "\n")
        self.assertEqual(package.json_license(build)["prefix"], prefix)
        header.write_text(header.read_text().replace("MINOR 12", "MINOR 13"))
        with self.assertRaises(RuntimeError):
            package.json_license(build)

    def test_alpha_package_forwards_private_paths_without_falling_back(self):
        args = argparse.Namespace(package_build_dir=self.root / "package-build",
            package_dist_dir=self.root / "dist", data=self.root / "original", jobs=2)
        with mock.patch.object(alpha, "run", return_value=("", 0)) as call:
            with self.assertRaisesRegex(alpha.CheckFailure, "did not produce"):
                alpha.package_check(args)
        command = call.call_args.args[0]
        for flag, expected in [("--build-dir", args.package_build_dir),
                               ("--dist-dir", args.package_dist_dir), ("--data", args.data)]:
            self.assertEqual(command[command.index(flag) + 1], str(expected))
        self.assertEqual(command[command.index("--jobs") + 1], "2")

    def test_ctest_skip_cannot_count_as_acceptance(self):
        build = self.root / "skip-build"
        build.mkdir()
        outputs = [(json.dumps({"tests": [{"name": "original-real"}]}), 0),
                   ("1/1 Test #1: original-real ...***Skipped\n100% tests passed, 0 tests failed out of 1", 1)]
        with mock.patch.object(alpha, "run", side_effect=outputs):
            with self.assertRaisesRegex(alpha.CheckFailure, "skipped or omitted"):
                alpha.test_build(build, 2)

    def test_outer_budget_preserves_each_registered_timeout(self):
        registration = {"tests": [
            {"name": "original-menu-session-flow", "properties": [{"name": "TIMEOUT", "value": 120.0}]},
            {"name": "long-check", "properties": [{"name": "TIMEOUT", "value": 1800.0}]},
            {"name": "untimed-check", "properties": []}]}
        before = copy.deepcopy(registration)
        budget = alpha.ctest_outer_budget(registration)
        self.assertEqual(budget["seconds"], 3720)
        self.assertEqual(registration, before)
        self.assertEqual(registration["tests"][0]["properties"][0]["value"], 120.0)
        self.assertFalse(budget["individual_timeouts_modified"])
        self.assertEqual(alpha.ctest_outer_budget({"tests": []})["seconds"], 1800)

    def test_invalid_declared_case_budget_fails_closed(self):
        for value in (True, -1, "not-a-duration", float("inf"), float("nan")):
            with self.subTest(value=value), self.assertRaises(alpha.CheckFailure):
                alpha.ctest_outer_budget({"tests": [{"name": "bad", "properties":
                    [{"name": "TIMEOUT", "value": value}]}]})


class ProjectLicenseProvenance(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="oe-license-provenance-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()

    def test_absent_license_remains_a_factual_publication_blocker(self):
        result = package.project_license_report(self.root)
        self.assertEqual(result["project_license"], "missing; publication blocker")
        provenance = result["project_license_provenance"]
        self.assertFalse(provenance["exists"])
        self.assertFalse(provenance["regular_file"])
        self.assertIsNone(provenance["sha256"])
        self.assertEqual(provenance["distribution_review"], "NOT_RUN")
        self.assertEqual(list(self.root.iterdir()), [])

    def test_present_regular_license_records_bytes_without_legal_acceptance(self):
        content = b"Independently authored license-provenance fixture\n"
        path = self.root / "LICENSE"
        path.write_bytes(content)
        result = package.project_license_report(self.root)
        self.assertEqual(result["project_license"], "present; distribution review NOT_RUN")
        self.assertEqual(result["project_license_provenance"], {
            "path": "LICENSE", "exists": True, "regular_file": True,
            "bytes": len(content), "sha256": hashlib.sha256(content).hexdigest(),
            "distribution_review": "NOT_RUN"})
        self.assertEqual(path.read_bytes(), content)

    def test_symlink_license_is_refused_without_reading_its_target(self):
        target = self.root / "outside-notice"
        target.write_bytes(b"protected authored target\n")
        (self.root / "LICENSE").symlink_to(target)
        with mock.patch.object(package.os, "open", side_effect=AssertionError("must not open a symlink")):
            with self.assertRaisesRegex(ValueError, "symbolic link"):
                package.project_license_report(self.root)
        self.assertEqual(target.read_bytes(), b"protected authored target\n")

    def test_license_provenance_without_posix_nofollow_preserves_binary_bytes(self):
        payload = b"Independently authored notice\r\nExact bytes\r\n"
        (self.root / "LICENSE").write_bytes(payload)
        with mock.patch.object(package.os, "O_NOFOLLOW", 0, create=True):
            del package.os.O_NOFOLLOW
            result = package.project_license_report(self.root)
        self.assertEqual(result["project_license_provenance"]["sha256"],
                         hashlib.sha256(payload).hexdigest())
        self.assertEqual(result["project_license_provenance"]["bytes"], len(payload))

    def test_opened_identity_mismatch_is_refused_before_read_without_nofollow(self):
        path = self.root / "LICENSE"
        path.write_bytes(b"original authored notice\n")
        other = self.root / "other"
        other.write_bytes(b"different authored file\n")
        original_open = package.os.open
        original_fdopen = package.os.fdopen

        class NoContentRead:
            def __init__(self, descriptor, mode):
                self.stream = original_fdopen(descriptor, mode)

            def __enter__(self):
                return self

            def __exit__(self, *_):
                self.stream.close()

            def fileno(self):
                return self.stream.fileno()

            def read(self, *_):
                raise AssertionError("conflicting file content must not be read")

        with mock.patch.object(package.os, "O_NOFOLLOW", 0, create=True), \
             mock.patch.object(package.os, "open", side_effect=lambda _path, flags: original_open(other, flags)), \
             mock.patch.object(package.os, "fdopen", side_effect=NoContentRead) as fdopen:
            del package.os.O_NOFOLLOW
            with self.assertRaisesRegex(ValueError, "changed during"):
                package.project_license_report(self.root)
        self.assertEqual(fdopen.call_count, 1)
        self.assertEqual(path.read_bytes(), b"original authored notice\n")

    def test_nonregular_or_oversized_license_is_refused_before_open(self):
        path = self.root / "LICENSE"
        path.mkdir()
        with self.assertRaisesRegex(ValueError, "bounded regular file"):
            package.project_license_report(self.root)
        path.rmdir()
        with path.open("wb") as stream:
            stream.truncate(package.PROJECT_LICENSE_LIMIT + 1)
        with mock.patch.object(package.os, "open", side_effect=AssertionError("must not open an oversized file")):
            with self.assertRaisesRegex(ValueError, "bounded regular file"):
                package.project_license_report(self.root)


class OptionalOriginalScope(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="oe-alpha-scope-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.model = self.root / "missing-original"
        self.registration = {"tests": [
            {"name": "synthetic-check", "command": ["synthetic"]},
            {"name": "original-model-data-real", "command": ["model-test", "--real", str(self.model)],
             "properties": [{"name": "SKIP_RETURN_CODE", "value": 77}]}]}
        self.junit = self.root / "results.xml"
        self.junit.write_text('<testsuite><testcase name="synthetic-check"/>'
            '<testcase name="original-model-data-real"><skipped message="SKIP_RETURN_CODE=77"/>'
            '</testcase></testsuite>')

    def scope(self, registration=None, configured=False):
        return alpha.ctest_result_scope(registration or self.registration, self.junit,
            original_data_configured=configured, model_root=self.model)

    def test_only_declared_missing_original_is_reported_not_run(self):
        result = self.scope()
        self.assertEqual((result["result"], result["passed"], result["skipped"]), ("synthetic_pass", 1, 1))
        self.assertEqual(result["original_data_status"], "NOT_CONFIGURED")
        self.assertEqual(result["not_run_original_cases"][0]["status"], "NOT_RUN")

    def test_explicit_data_never_permits_a_skip(self):
        with self.assertRaises(alpha.CheckFailure):
            self.scope(configured=True)

    def test_unexpected_skip_contracts_fail_closed(self):
        variants = []
        changed = copy.deepcopy(self.registration)
        changed["tests"][1]["properties"][0]["value"] = 0
        variants.append(changed)
        changed = copy.deepcopy(self.registration)
        changed["tests"][1]["command"][-1] = "different-input"
        variants.append(changed)
        changed = copy.deepcopy(self.registration)
        changed["tests"][1]["properties"].append({"name": "DISABLED", "value": True})
        variants.append(changed)
        changed = copy.deepcopy(self.registration)
        changed["tests"][1]["command"][-2] = "--unrelated"
        variants.append(changed)
        for registration in variants:
            with self.subTest(registration=registration), self.assertRaises(alpha.CheckFailure):
                self.scope(registration)

    def test_existing_model_cannot_explain_an_optional_skip(self):
        (self.model / "Model").mkdir(parents=True)
        (self.model / "Model/EmperorBuildingModels.txt").write_text("independent test marker")
        with self.assertRaises(alpha.CheckFailure):
            self.scope()

    def test_partial_junit_cannot_pass(self):
        self.junit.write_text('<testsuite><testcase name="synthetic-check"/></testsuite>')
        with self.assertRaises(alpha.CheckFailure):
            self.scope()

    def test_non_original_skip_is_unexpected_even_without_data(self):
        self.junit.write_text('<testsuite><testcase name="synthetic-check"><skipped/></testcase>'
                              '<testcase name="original-model-data-real"/></testsuite>')
        with self.assertRaises(alpha.CheckFailure):
            self.scope()

    def test_optional_main_reports_partial_not_global_original_pass(self):
        args = argparse.Namespace(data=None, jobs=1, package=False, system_sdl=True,
            build_dir=self.root / "debug", release_build_dir=self.root / "release",
            sanitize_build_dir=self.root / "sanitize", report=self.root / "report.json")
        result = {"result": "synthetic_pass", "passed": 1, "count": 2, "skipped": 1,
                  "original_data_status": "NOT_CONFIGURED"}
        with mock.patch.object(alpha, "parse_args", return_value=args), \
             mock.patch.object(alpha, "run", side_effect=[("test-revision", 0), ("", 0), ("Mach-O arm64", 0)]), \
             mock.patch.object(alpha, "configure", return_value={}), \
             mock.patch.object(alpha, "test_build", return_value=result) as tests, \
             mock.patch.object(alpha, "endurance", return_value={"json_roundtrips": 12}), \
             mock.patch.object(alpha, "render_stress", return_value={}), \
             mock.patch.object(alpha, "session_stress", return_value={}), \
             mock.patch.object(alpha, "original_check", return_value={"configured": False, "result": "not_configured"}), \
             mock.patch("builtins.print"):
            self.assertEqual(alpha.main(), 0)
        report = json.loads(args.report.read_text())
        self.assertEqual(report["result"], "partial")
        self.assertEqual(report["acceptance_scope"], "synthetic_only")
        self.assertFalse(report["full_original_acceptance"])
        self.assertTrue(all(call.kwargs["original_data_configured"] is False for call in tests.call_args_list))


class VisualMetadata(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((PACK / "manifest.json").read_text())
        self.documents = {name: bundle.validate_compatibility_json(PACK / name)
                          for name in bundle.COMPATIBILITY_FILES}

    def test_current_maintained_house_contract_is_complete(self):
        bundle.validate_original_house_metadata(self.manifest, self.documents)

    def test_house_semantic_and_geometry_counterexamples_refused(self):
        mutations = [
            lambda d: d["stages"].pop(),
            lambda d: d["stages"][1].update(house_type=2),
            lambda d: d["stages"][-1].update(original_side=2),
            lambda d: d["stages"][-1].update(ground_anchor=[0, 0]),
            lambda d: d["stages"][-1].update(image_index=0),
            lambda d: d["stages"][-1].update(group_key=1),
            lambda d: d["stages"][-1].update(variant=True),
            lambda d: d["stages"][-1].update(base_bytes=1),
            lambda d: d["stages"][0].update(group_key=0x471),
            lambda d: d["files"][0].update(sha256="0" * 64),
            lambda d: d.update(selection="invented"),
        ]
        for index, mutate in enumerate(mutations):
            with self.subTest(index=index):
                documents = copy.deepcopy(self.documents)
                mutate(documents["original-houses.json"])
                with self.assertRaises(ValueError):
                    bundle.validate_original_house_metadata(self.manifest, documents)

    def test_shipped_type2_requires_the_evidenced_purchase_image(self):
        mutations = [
            {"image_index": 1509}, {"image_index": 1510},
            {"group_key": 0x471}, {"variant": 1}, {"variant": True},
            {"width": 160}, {"height": 89}, {"base_bytes": 3200},
            {"ground_anchor": [79, 63]}, {"archive": "DATA/SprMain.sg3"},
            # A coherent Shelter registration is still not the fresh Type2
            # purchase, even though its own geometry and anchor are valid.
            {"image_index": 1509, "group_key": 0x471, "height": 83,
             "ground_anchor": [79, 63]},
            # Internally coherent Type30 geometry cannot replace exact88px.
            {"height": 89, "ground_anchor": [79, 69]},
        ]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                documents = copy.deepcopy(self.documents)
                documents["original-houses.json"]["stages"][0].update(mutation)
                with self.assertRaises(ValueError):
                    bundle.validate_original_house_metadata(self.manifest, documents)

    def test_shipped_type2_cannot_downgrade_to_a_runtime_fallback(self):
        # Historical fallback-only profiles remain valid runtime inputs. The
        # maintained bundle must ship its complete current native registration.
        documents = copy.deepcopy(self.documents)
        documents["original-houses.json"]["stages"][0] = {
            "house_type": 2, "original_side": 2, "fallback": "Historical authored fallback"}
        with self.assertRaises(ValueError):
            bundle.validate_original_house_metadata(self.manifest, documents)
        documents["original-houses.json"]["stages"][0]["archive"] = "DATA/China_General.sg3"
        with self.assertRaises(ValueError):
            bundle.validate_original_house_metadata(self.manifest, documents)

    def test_house_supplement_cannot_substitute_source_authority(self):
        changed = copy.deepcopy(self.manifest)
        changed["optional_original_houses"]["core_files"] = ["DATA/SprMain.sg3"]
        with self.assertRaises(ValueError):
            bundle.validate_original_house_metadata(changed, self.documents)

    def test_duplicate_metadata_key_refused(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / "metadata.json"
            path.write_text('{"schema_version":1,"schema_version":2}')
            with self.assertRaises(ValueError):
                bundle.validate_compatibility_json(path)

    def test_visual_count_is_derived_from_declared_physical_identities(self):
        args = argparse.Namespace(walker_visuals=None, building_visuals=None, road_visuals=None)
        result = {"walker_visuals": {"unique_assets": len(alpha.profile_assets(PACK / "walkers.json"))},
                  "building_visuals": {"decoded_unique_assets": len(alpha.profile_assets(PACK / "buildings.json")),
                      "draws_by_role": {r: 1 for r in ("clay_source", "pottery", "warehouse", "household")},
                      "placeholder_fallbacks_by_role": {r: 0 for r in ("clay_source", "pottery", "warehouse", "household")}},
                  "road_visuals": {"unique_assets": len(alpha.profile_assets(PACK / "roads.json")),
                                   "draws": 1, "configured_masks": list(range(16))}}
        alpha.validate_smoke_visuals(result, args, PACK)
        for mutate in [lambda d: d["building_visuals"].update(decoded_unique_assets=0),
                       lambda d: d["road_visuals"].update(unique_assets=0),
                       lambda d: d["building_visuals"]["placeholder_fallbacks_by_role"].update(pottery=1),
                       lambda d: d["building_visuals"]["draws_by_role"].update(pottery=0)]:
            candidate = copy.deepcopy(result)
            mutate(candidate)
            with self.assertRaises(alpha.CheckFailure):
                alpha.validate_smoke_visuals(candidate, args, PACK)


if __name__ == "__main__":
    unittest.main()
