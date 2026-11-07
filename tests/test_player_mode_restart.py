"""Exercise recommended paid creation and ordinary load in distinct processes."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def main():
    binary = Path(sys.argv[1]).resolve(strict=True)
    parent = Path(__file__).resolve().parents[1] / ".local" / "oe49"
    parent.mkdir(parents=True, exist_ok=True)
    evidence = Path(tempfile.mkdtemp(prefix="menu-restart-", dir=parent))
    root = evidence / "scenario"
    try:
        subprocess.run([str(binary), "--player-mode-process-save", str(root)], check=True)
        # Compare complete bytes, including all saved identities and World fields.
        saves = list((root / "app" / "saves").glob("*.json"))
        assert saves, "Creator produced no manual save"
        before = {p: hashlib.sha256(p.read_bytes()).hexdigest() for p in saves}
        subprocess.run([str(binary), "--player-mode-process-load", str(root)], check=True)
        assert all(p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest() == digest
                   for p, digest in before.items()), "Restart rewrote the creator's save bytes"
    except BaseException:
        print(f"Failed restart evidence retained at {evidence}", file=sys.stderr)
        raise
    else:
        shutil.rmtree(evidence)
        print("Distinct-process recommended Rule18 creation/load/F9 and source bytes PASS")


if __name__ == "__main__":
    main()
