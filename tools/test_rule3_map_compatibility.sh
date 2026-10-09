#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
if [ "$#" -ne 2 ]; then
    echo "Usage: sh tools/test_rule3_map_compatibility.sh <original-data-directory> <map-name>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd -P)
map_name=${2%.map}
case "$map_name" in
    ""|.|..|*/*|*\\*) echo "Choose a map name from the original Cities directory." >&2; exit 2 ;;
esac
test -f "$original_data/Cities/$map_name.map"
private_dir=$(python3 - "$repo_dir" "$original_data" <<'PY'
import pathlib, sys
path = (pathlib.Path(sys.argv[1]) / ".local/original-occupancy-next").resolve()
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
cmake --build "$build_dir" --parallel 4 --target openemperor
app_data_dir=$(mktemp -d "$private_dir/player-$map_name-XXXXXX")
python3 - "$app_data_dir/settings.json" "$original_data" "$map_name" <<'PY'
import json, pathlib, sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({
    "version": 1, "data_root": sys.argv[2],
    "last_map": "Cities/" + sys.argv[3] + ".map", "last_save": "",
    "prepared_starter": False, "autosave_enabled": True,
    "profile": "sandbox-city-v16"
}, indent=2) + "\n")
PY
echo "Choose New Sandbox, wait for Map rules checked, then Start Sandbox: $map_name."
echo "City-v16 rule 3 / Map policy 1 / Empty City; all purchases use ordinary Funds and workers."
echo "Test a paid road before buying another building, then F5/F9."
echo "Private settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir" \
    --great-wall-presentation auto
