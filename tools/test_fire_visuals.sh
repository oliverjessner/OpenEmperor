#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_fire_visuals.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/fire-visuals"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-fire-visual-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-fire-visual-fixture" "$original_data" "$app_data_dir"
echo "Local TECHNICAL FIXTURE: Xia, four naturally burning buildings at tick 2000."
echo "Created with ordinary paid commands/ticks; no personal saves or settings were changed."
echo "Choose Load Sandbox, then Load selected save. The city starts paused."
echo "Press Space to animate; use the mouse wheel or Z to zoom near the buildings."
echo "F1 reports Original animated clip or a named orange-marker Fallback."
echo "Private settings, save and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
