#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_service_walker.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/service-walker"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-service-walker-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-service-walker-fixture" "$original_data" "$app_data_dir"
echo "LOCAL TECHNICAL FIXTURE: ordinary paid1280/1300 Xia starter,24/24 workers and Service Post2/2."
echo "Choose Load Sandbox, then Load selected save. The city starts paused during a real Service trip."
echo "Press Space to continue: the figure visits a House, existing Service coverage starts only on arrival, then the figure returns."
echo "Zoom over the Service Post and Houses. F1 shows the selected clip or concrete marker fallback; F2 compares markers."
echo "No funds, workers, goods or Service coverage were injected. Existing player saves remain untouched."
echo "Settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
