import importlib.util
import json
import pathlib
import signal
import subprocess
import sys
import tempfile
import unittest


RUNNER = pathlib.Path(__file__).resolve().parents[1] / "tools" / "verify_map_corpus.py"
SPEC = importlib.util.spec_from_file_location("verify_map_corpus", RUNNER)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CorpusRunnerTests(unittest.TestCase):
    def test_timeout_and_invalid_json(self):
        with tempfile.TemporaryDirectory() as directory:
            helper = pathlib.Path(directory) / "helper.py"
            helper.write_text("import time; time.sleep(2)\n", encoding="utf-8")
            timed = MODULE.invoke([sys.executable, str(helper)], 0.1)
            self.assertEqual(MODULE.classify_invocation(timed)["status"], "timeout")
            helper.write_text("print('not json')\n", encoding="utf-8")
            invalid = MODULE.invoke([sys.executable, str(helper)], 2)
            self.assertEqual(MODULE.classify_invocation(invalid)["status"], "process_failed")
            helper.write_text("import sys; sys.exit(7)\n", encoding="utf-8")
            failed = MODULE.invoke([sys.executable, str(helper)], 2)
            self.assertEqual(MODULE.classify_invocation(failed)["exit_code"], 7)
            helper.write_text("print('x' * 3000000)\n", encoding="utf-8")
            limited = MODULE.invoke([sys.executable, str(helper)], 2)
            self.assertEqual(MODULE.classify_invocation(limited)["status"], "process_failed")
            self.assertTrue(limited["output_limited"])

    def test_incremental_report_and_paths_with_spaces(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fake = root / "fake binary"
            fake.write_text(
                f"#!{sys.executable}\n"
                "import json, sys\n"
                "if '--list-maps' in sys.argv:\n"
                " print(json.dumps({'entries':[{'relative_path':'Cities/One Map.map'},"
                "{'relative_path':'Cities/Bad.map'}], 'scan_errors':[]}))\n"
                "elif any('One Map.map' in part for part in sys.argv):\n"
                " profile=sys.argv[sys.argv.index('--graphics-profile')+1]\n"
                " print(json.dumps({'status':'snapshot_complete','candidate_cells':4,"
                "'covered_cells':4,'graphics_profile':profile}))\n"
                "else:\n"
                " print('broken output'); sys.exit(9)\n", encoding="utf-8")
            fake.chmod(0o755)
            report = root / "reports" / "results.json"
            result = subprocess.run([sys.executable, str(RUNNER), "--binary", str(fake),
                                     "--data", str(root / "data with spaces"),
                                     "--report", str(report), "--timeout", "2",
                                     "--graphics-profile", "exe-6373328b-v213-slot8-runtime-table"],
                                    capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            saved = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(saved["discovered_files"], 2)
            self.assertEqual(saved["checked_files"], 2)
            self.assertEqual(saved["summary"], {"snapshot_complete": 1, "process_failed": 1})
            self.assertEqual(saved["results"][0]["relative_path"], "Cities/One Map.map")
            self.assertEqual(saved["results"][0]["graphics_profile"],
                             "exe-6373328b-v213-slot8-runtime-table")

    def test_timed_map_keeps_previous_result(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fake = root / "fake_timeout"
            fake.write_text(
                f"#!{sys.executable}\n"
                "import json, sys, time\n"
                "if '--list-maps' in sys.argv:\n"
                " print(json.dumps({'entries':[{'relative_path':'A.map'},"
                "{'relative_path':'B.map'}], 'scan_errors':[]}))\n"
                "elif 'A.map' in sys.argv:\n"
                " print(json.dumps({'status':'snapshot_complete'}))\n"
                "else: time.sleep(2)\n", encoding="utf-8")
            fake.chmod(0o755)
            report = root / "report.json"
            result = subprocess.run([sys.executable, str(RUNNER), "--binary", str(fake),
                                     "--data", str(root), "--report", str(report),
                                     "--timeout", "1"], capture_output=True,
                                    text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            saved = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(saved["checked_files"], 2)
            self.assertEqual([item["status"] for item in saved["results"]],
                             ["snapshot_complete", "timeout"])

    def test_interrupt_keeps_unchecked_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fake = root / "fake_interrupt"
            fake.write_text(
                f"#!{sys.executable}\n"
                "import json, sys, time\n"
                "if '--list-maps' in sys.argv:\n"
                " print(json.dumps({'entries':[{'relative_path':'A.map'},"
                "{'relative_path':'B.map'}], 'scan_errors':[]}))\n"
                "elif 'A.map' in sys.argv:\n"
                " print(json.dumps({'status':'snapshot_complete'}))\n"
                "else: time.sleep(10)\n", encoding="utf-8")
            fake.chmod(0o755)
            report = root / "report.json"
            process = subprocess.Popen([sys.executable, str(RUNNER), "--binary", str(fake),
                                        "--data", str(root), "--report", str(report),
                                        "--timeout", "5"], stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
            self.assertIn("1/2 A.map", process.stdout.readline())
            process.send_signal(signal.SIGINT)
            process.communicate(timeout=5)
            self.assertEqual(process.returncode, 130)
            saved = json.loads(report.read_text(encoding="utf-8"))
            self.assertEqual(saved["checked_files"], 1)
            self.assertEqual([item["status"] for item in saved["results"]],
                             ["snapshot_complete", "not_checked"])


if __name__ == "__main__":
    unittest.main()
