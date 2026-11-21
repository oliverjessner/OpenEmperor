#!/usr/bin/env python3
"""Own-application argument validation; never opens Original data or starts SDL."""

from pathlib import Path
import subprocess
import sys
import tempfile


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: IrrigationProductivityCliTests.py OPENEMPEROR_EXECUTABLE")
    executable = str(Path(sys.argv[1]).resolve(strict=True))
    with tempfile.TemporaryDirectory(prefix="openemperor-irrigation-productivity-cli-") as temporary:
        root = Path(temporary)
        # These exact nonexistent paths must never be inspected: each invocation
        # must fail in argument validation, before data, SDL, prefs or save setup.
        data = root / "unused-original-data"
        base = ["--data", str(data), "--sandbox", "Cities/NotOpened.map",
                "--sandbox-rules", "sandbox-city-v16", "--irrigation-productivity"]
        incompatible = [
            "--sandbox-bridges", "--sandbox-ramps", "--authored-source-cell-ramps",
            "--authored-land-trade", "--seasonal-agriculture", "--original-fire-protection",
            "--original-sustainable-economy", "--original-market-logistics",
            "--original-cardinal-water", "--original-inspector-coordination",
            "--original-deadline-fire-dispatch", "--original-deadline-service-dispatch",
            "--original-deadline-food-dispatch", "--original-safe-demolition",
            "--original-music-venues", "--original-venue-house-service",
            "--original-agriculture", "--original-irrigation", "--physical-food-growers", "--playable-agriculture", "--playable-fishing", "--playable-hunting", "--multi-field-agriculture", "--sandbox-demo",
            "--authored-terrain-sides", "--sandbox-check", "--sandbox-original-economy-check",
        ]
        cases = [["--irrigation-productivity"], base + ["--irrigation-productivity"]]
        cases.extend(base + [flag] for flag in incompatible)
        cases.append(base + ["--load-sandbox", str(root / "not-opened.save.json")])
        cases.append(["--data", str(data), "--load-sandbox", str(root / "not-opened.save.json"),
                      "--irrigation-productivity"])
        cases.append(["--data", str(data), "--sandbox", "Cities/NotOpened.map",
                      "--sandbox-rules", "sandbox-city-v11", "--irrigation-productivity"])
        for arguments in cases:
            result = subprocess.run([executable, *arguments], cwd=root, capture_output=True,
                                    text=True, timeout=15, check=False)
            # The existing CLI prints its session-only Great Wall selection
            # before argument validation. Require the actual usage line, rather
            # than treating that harmless established prefix as a failure.
            usage_line = any(line.startswith("Usage:") for line in result.stderr.splitlines())
            if result.returncode != 2 or not usage_line:
                raise AssertionError(f"Irrigation Productivity mode conflict escaped argument validation: {arguments!r}\n"
                                     f"exit={result.returncode}\nstdout={result.stdout}\nstderr={result.stderr}")
            if "--irrigation-productivity" not in result.stderr:
                raise AssertionError("CLI usage omits the explicit playable mode")
            if result.stdout or list(root.iterdir()):
                raise AssertionError("Invalid CLI changed data/settings/save state before rejection")
        print(f"Irrigation Productivity CLI rejects {len(cases)} conflicting/missing-context cases before data or SDL")


if __name__ == "__main__":
    main()
