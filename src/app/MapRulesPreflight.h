#pragma once

#include "maps/MapRulesCheck.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace openemperor::menu {

// A session-local selection controller. All public methods belong to the menu
// thread. The worker owns only copied requests and the SDL-free checker.
class MapRulesPreflight {
public:
    using Checker=std::function<maps::MapRulesResult(const maps::MapRulesRequest&)>;
    static constexpr std::size_t cache_capacity=8;

    explicit MapRulesPreflight(Checker checker=maps::check_map_rules);
    ~MapRulesPreflight();
    MapRulesPreflight(const MapRulesPreflight&)=delete;
    MapRulesPreflight& operator=(const MapRulesPreflight&)=delete;

    // Always check current content, even if this selection has a remembered
    // result. At most one active check and the latest pending request survive.
    std::uint64_t select(maps::MapRulesRequest request);
    std::uint64_t recheck();
    // Leaving setup invalidates results without waiting for old parser work.
    void invalidate();
    // The only place a completed worker result enters current menu state.
    // No file access or parser work occurs here.
    bool poll();
    // Shutdown drops pending work and joins the single worker. It is idempotent.
    void shutdown();

    const maps::MapRulesResult& result() const { return result_; }
    bool has_selection() const { return selected_.has_value(); }
    bool can_start() const;
    std::uint64_t generation() const { return generation_; }

    // Historical list annotation only: this is never start authorization.
    // Cache records include the checked input SHA as part of their identity.
    std::optional<maps::MapRulesResult> remembered_result(
        const maps::MapRulesRequest& request) const;
    std::size_t cache_size() const { return cache_.size(); }
    std::size_t active_check_count() const;
    std::size_t pending_check_count() const;

private:
    struct Worker;
    void remember(const maps::MapRulesResult& result);
    std::uint64_t next_generation();
    std::unique_ptr<Worker> worker_;
    std::optional<maps::MapRulesRequest> selected_;
    maps::MapRulesResult result_;
    std::vector<maps::MapRulesResult> cache_;
    std::uint64_t generation_=0;
};

} // namespace openemperor::menu
