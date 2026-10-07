#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_fire_inspector.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/fire-inspector"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-fire-inspector-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-fire-inspector-fixture" "$original_data" "$app_data_dir"
echo "Local TECHNICAL FIXTURE: Xia, three natural fires, paid House and 2/2 Fire Watch."
echo "Prepared only through ordinary paid commands and unchanged ticks; no personal saves changed."
echo "Choose Load Sandbox, then Load selected save. The city starts paused at tick2000."
echo "Use the mouse wheel over the central House/Clay/road cluster to enlarge it before continuing."
echo "Press Space: observe the Inspector on the curved route, actual arrival/extinguish and return."
echo "F1 reports the active curated Inspector clip or a named marker fallback; F2 compares sprites/markers."
echo "Private settings, save and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
