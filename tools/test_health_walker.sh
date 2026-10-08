#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -ne 1 ]; then
    echo "Usage: tools/test_health_walker.sh <original-data-directory>" >&2
    exit 2
fi
original_data=$(CDPATH= cd -- "$1" && pwd)
test -f "$original_data/Cities/Xia.map"
private_dir="$repo_dir/.local/health-walker"
build_dir="$private_dir/build"
mkdir -p "$private_dir"
cmake -S "$repo_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DOPENEMPEROR_USE_SYSTEM_SDL3=ON
cmake --build "$build_dir" --parallel 4 --target openemperor openemperor-health-walker-fixture
app_data_dir=$(mktemp -d "$private_dir/player-Xia-XXXXXX")
"$build_dir/openemperor-health-walker-fixture" "$original_data" "$app_data_dir"
echo "LOCAL TECHNICAL FIXTURE: production paid1280/1300 starter, actual taxes, two bought Houses and genuinely staffed2/2 Health Posts."
echo "Four dry Houses became naturally sick at3400; only real HealthWorker arrivals cure/protect. No funds, workers, goods or Health state were injected."
echo "Choose Load Sandbox, then Load selected save. The city starts paused during a real HealthWorker trip."
echo "Press Space: the figure reaches a sick House, existing Health arrival cures it, then the figure returns and hides at home."
echo "Zoom over the Health Post and Houses. F1 shows the selected clip or concrete marker fallback; F2 compares markers."
echo "Settings, saves and recovery: $app_data_dir"
exec "$build_dir/openemperor" --data "$original_data" --app-root "$app_data_dir"
