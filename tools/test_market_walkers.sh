#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_market_walkers.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/market-walkers"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-market-walker-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-market-walker-fixture" "$original_data" "$app_data_dir"
echo "LOCAL TECHNICAL FIXTURE: the ordinary paid1280/1300 starter,24/24 workers, unchanged production and deliveries."
echo "Choose Load Sandbox, then Load selected save. The supply city starts paused with both Market distributors travelling and actual taxes received."
echo "Press Space to continue. Zoom over the city to watch Farm -> Market, Warehouse -> Market and Market -> Houses."
echo "F1 shows active Supplier/Distributor clips or a concrete marker fallback; F2 toggles live figures and markers."
echo "Other private saves contain each loaded outbound and empty returning trip, plus the unchanged paid tick0 starter."
echo "Existing successful player cities are not touched. Settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
