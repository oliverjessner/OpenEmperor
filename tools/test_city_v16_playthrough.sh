#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd -P)
if [ "$#" -ne 1 ]; then
    echo "Usage: sh tools/test_city_v16_playthrough.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd -P)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/city-v16-playthrough"
build_dir="$private_dir/build"
case "$private_dir/" in
    "$original_data/"*)
        echo "The test workspace must be outside the original-data directory." >&2
        exit 2
        ;;
esac
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-city-v16-playthrough-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-city-v16-playthrough-fixture" "$original_data" "$app_data_dir"
echo "Choose Load Sandbox, then Load selected save: A-paid-starter starts paused at tick 0."
echo "City-v16 rule 3 / map policy 1: 1,280 paid, 20 Funds, four Houses, 24/24 workers, no Well or Health Post."
echo "F-goal and G-stability are separate technical checkpoints earned by the logged paid command replay."
echo "Press Space to run; F5 saves and F9 restores the selected city paused."
echo "Recipe and measured results: $app_data_dir/fixture-report.json"
echo "Settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
