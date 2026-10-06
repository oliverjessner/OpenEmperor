#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "Usage: tools/test_gate_passages.sh <original-data-directory> [Kaifeng|Zhengzhou|Xia|Banpo]" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
map_name=${2:-Kaifeng}
case "$map_name" in Kaifeng|Zhengzhou|Xia|Banpo) ;; *) echo "Choose Kaifeng, Zhengzhou, Xia or Banpo." >&2; exit 2 ;; esac
test -f "$original_data/Cities/$map_name.map"

build_dir="$repo_dir/build-gate-passages"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel --target openemperor

mkdir -p "$repo_dir/.local/gate-passages"
app_data_dir=$(mktemp -d "$repo_dir/.local/gate-passages/player-$map_name-XXXXXX")
# Use the ordinary menu/settings writer's exact schema in a fresh private root.
# JSON escaping supports data paths containing spaces, quotes or backslashes.
python3 - "$app_data_dir/settings.json" "$original_data" "$map_name" <<'PY'
import json, pathlib, sys
pathlib.Path(sys.argv[1]).write_text(json.dumps({
    "version": 1, "data_root": sys.argv[2],
    "last_map": "Cities/" + sys.argv[3] + ".map", "last_save": "",
    "prepared_starter": False, "autosave_enabled": True,
    "profile": "sandbox-city-v16"
}, indent=2) + "\n")
PY
echo "Choose New Sandbox, then Start Sandbox: City v16 rule 3, map policy 1, $map_name."
echo "This starts a new empty city. Old cities retain their existing rules."
echo "Private settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir" \
    --great-wall-presentation auto
