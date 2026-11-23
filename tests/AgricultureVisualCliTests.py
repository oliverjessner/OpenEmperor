#!/usr/bin/env python3
"""Finite argument boundaries; no Original data or World execution is needed."""
import pathlib
import subprocess
import sys
import tempfile


def run(executable, arguments, success):
    result = subprocess.run([str(executable), *arguments], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, timeout=30)
    if (result.returncode == 0) != success:
        raise AssertionError(f"Unexpected CLI result for {arguments}: {result.stdout} {result.stderr}")
    return result


def main():
    if len(sys.argv) != 3:
        raise AssertionError("backend and normal-view executable paths required")
    for path in map(pathlib.Path, sys.argv[1:]):
        help_result = run(path, ["--help"], True)
        if "--data" not in help_result.stdout or "--output" not in help_result.stdout:
            raise AssertionError("Incomplete explicit-input help")
        for args in ([], ["--unknown", "value"], ["--data"], ["--help", "extra"],
                     ["--data", "x", "--data", "y"], ["--ticks", "NaN"], ["--id", "-1"]):
            run(path, args, False)
    backend, view = map(pathlib.Path, sys.argv[1:])
    repo = pathlib.Path(__file__).resolve().parents[1]
    private = repo / ".local/agri-mac12"
    private.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="numeric-cli-", dir=private) as temporary:
        root = pathlib.Path(temporary).resolve()
        data, resources = root / "data", root / "resources"
        (data / "Cities").mkdir(parents=True)
        resources.mkdir()
        (data / "Cities/Chengdu.map").write_bytes(b"numeric parser stops before map preparation")
        seed, checkpoint = root / "plan.json", root / "input.save.json"
        seed.write_text("{}")
        checkpoint.write_text("{}")
        backend_args = ["--data", str(data), "--output", str(root / "backend-output"), "--seed-plan", str(seed)]
        view_args = ["--data", str(data), "--resources", str(resources), "--input", str(checkpoint),
                     "--output", str(root / "view-output"), "--domain", "Building", "--id", "1"]
        controls = [(backend, backend_args + ["--ticks", "NaN"], "Invalid tick count"),
                    (backend, backend_args + ["--ticks", "0"], "Require12000..50000 actual ticks"),
                    (view, view_args[:-1] + ["-1"], "Invalid numeric argument"),
                    (view, view_args + ["--frames", "2"], "Out-of-range visual request")]
        for executable, arguments, expected in controls:
            result = run(executable, arguments, False)
            if expected not in result.stderr:
                raise AssertionError(f"Numeric guard did not run: wanted {expected!r}, got {result.stderr!r}")
        if (root / "backend-output").exists() or (root / "view-output").exists():
            raise AssertionError("Numeric rejection performed output publication")
    print("Agriculture visual backend/native CLI boundaries PASS")


if __name__ == "__main__":
    main()
