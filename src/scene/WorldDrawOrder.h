#pragma once

#include <cstdint>
#include <cstddef>
#include <tuple>

namespace openemperor::scene {

// Preview painter order at a shared projected ground point. This is not an
// assertion about the original game's object/Z ordering.
enum class WorldVisualLayer : std::uint8_t {
    StoredMap, SandboxRoad, SandboxBuilding, SandboxWalker
};

struct WorldDrawKey {
    double depth = 0;
    double ground_x = 0;
    WorldVisualLayer layer = WorldVisualLayer::StoredMap;
    std::uint64_t stable_id = 0;
};

inline bool operator<(const WorldDrawKey& a, const WorldDrawKey& b) {
    return std::tie(a.depth, a.ground_x, a.layer, a.stable_id) <
           std::tie(b.depth, b.ground_x, b.layer, b.stable_id);
}

struct WorldMergeStats {
    std::size_t stored_items_visited = 0;
    std::size_t sandbox_items = 0;
    std::size_t stored_before_sandbox_count = 0;
    std::size_t sandbox_before_stored_count = 0;
};

// Both inputs are already sorted by WorldDrawKey. Draw callbacks receive the
// corresponding index and return false only on a rendering failure.
template<class StoredItems, class SandboxItems, class DrawStored, class DrawSandbox>
bool merge_world_draw_streams(const StoredItems& stored, const SandboxItems& sandbox,
                              DrawStored draw_stored, DrawSandbox draw_sandbox,
                              WorldMergeStats& stats) {
    stats = {};
    stats.sandbox_items = sandbox.size();
    std::size_t s = 0, d = 0;
    while (s < stored.size() && d < sandbox.size()) {
        if (stored[s].key < sandbox[d].key) {
            if (!draw_stored(s++)) return false;
            ++stats.stored_items_visited;
            ++stats.stored_before_sandbox_count;
        } else {
            if (!draw_sandbox(d++)) return false;
            ++stats.sandbox_before_stored_count;
        }
    }
    while (s < stored.size()) {
        if (!draw_stored(s++)) return false;
        ++stats.stored_items_visited;
    }
    while (d < sandbox.size()) if (!draw_sandbox(d++)) return false;
    return true;
}

} // namespace openemperor::scene
