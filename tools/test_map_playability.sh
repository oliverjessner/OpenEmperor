#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
if [ "$#" -ne 2 ]; then
    echo "Usage: sh tools/test_map_playability.sh <original-data-directory> <exact-map-name>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd -P)
map_file=$(python3 - "$original_data" "$2" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
name = sys.argv[2]
if not name or name in (".", "..") or any(c in name for c in ("/", "\\", "\n", "\r")):
    raise SystemExit("Choose an exact original Cities map name, without a directory.")
filename = name if name.endswith(".map") else name + ".map"
directory = root / "Cities"
try:
    directory.resolve().relative_to(root)
except ValueError:
    raise SystemExit("The original Cities directory must remain inside the data root.")
matches = [p for p in directory.iterdir() if p.name == filename and p.is_file()]
if len(matches) != 1:
    raise SystemExit("No unique exact Cities filename: " + filename)
path = matches[0].resolve()
try:
    path.relative_to(directory.resolve())
except ValueError:
    raise SystemExit("The selected map must remain inside the original Cities directory.")
print(matches[0].name)
PY
)
private_dir=$(python3 - "$repo_dir" "$original_data" <<'PY'
import pathlib, sys
path = (pathlib.Path(sys.argv[1]) / ".local/map-playability").resolve()
try:
    path.relative_to(pathlib.Path(sys.argv[2]))
except ValueError:
    print(path)
else:
    raise SystemExit("The test workspace must be outside the original-data directory.")
PY
)
mkdir -p "$private_dir"
build_dir="$private_dir/player-build"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-map-playability-fixture
app_data_dir=$(mktemp -d "$private_dir/player-${map_file%.map}-XXXXXX")
(cd "$repo_dir" && "$build_dir/openemperor-map-playability-fixture" \
    "$original_data" "Cities/$map_file" "$app_data_dir")
python3 - "$app_data_dir/settings.json" "$original_data" "$map_file" <<'PY'
import json, pathlib, sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({
    "version": 1, "data_root": sys.argv[2],
    "last_map": "Cities/" + sys.argv[3], "last_save": "",
    "prepared_starter": False, "autosave_enabled": True,
    "profile": "sandbox-city-v16"
}, indent=2) + "\n")
PY
echo "Choose New Sandbox, wait for Map rules checked, then Start Sandbox: ${map_file%.map}."
echo "City-v16 rule 3 / Map policy 1 / Empty City. An empty city has no sandbox couriers."
echo "Build Houses, paid roads, Clay Source and Pottery to observe a real Clay trip."
echo "Optional Load Sandbox: B-paid-courier is a normally paid 2-House city; Space starts its Clay trip."
echo "C-paid-supply is the paid compact starter; D records actual supply at tick 1600."
python3 - "$app_data_dir/road-connectivity-report.json" <<'PY'
import json, pathlib, sys
report = json.loads(pathlib.Path(sys.argv[1]).read_text())
if report["status"] == "prepared":
    print("E-road-disconnected has a normally paid separated Clay/Pottery route: select Clay, then F1 and G.")
    print("Measured gap in storage coordinates:", report["missing_storage_cell"])
    print("Buy that Road for 2, then Space observes real delivery and return.")
else:
    print("Additional E-road-disconnected unavailable:", report["reason"])
PY
echo "T / Income explains missing supply and staffing; F1 shows diagnostics; F2 compares walker markers."
echo "Private settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir" \
    --great-wall-presentation auto
