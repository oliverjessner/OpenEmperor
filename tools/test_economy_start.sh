#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_economy_start.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/economy-start"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-economy-start-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-economy-start-fixture" "$original_data" "$app_data_dir"
echo "LOCAL TECHNICAL FIXTURE: ordinary paid commands, no added funds, goods or workers."
echo "Choose Load Sandbox, then Load selected save: critical-before starts paused at tick0 with106 Funds."
echo "Press T for the income details. A fourth House costs80; the warning explains remaining26 Funds and missing Service Post100."
echo "Cancel is the default. Build anyway explicitly buys once."
echo "Other saves: failed-start (26 Funds, Farm0/4, Service missing, upkeep46/400t) and the unchanged paid prepared starter."
echo "These fixtures differ from the reported screenshot; fixture-report.json records exact commands and numbers."
echo "Private settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
