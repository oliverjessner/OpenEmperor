#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <ostream>
#include <string_view>
#include <vector>

namespace openemperor::performance {

enum class Counter : std::size_t {
    WorldCopies,
    WorldRestores,
    WorldExecutes,
    RoadPlans,
    RouteRefreshes,
    BfsCalls,
    BfsVisitedCells,
    AssetDecodes,
    TextureUploads,
    FileReads,
    FileWrites,
    SimulationTicks,
    Count
};

enum class Timing : std::size_t {
    EventHandling,
    HoverPicking,
    RoadPlanning,
    BudgetCheck,
    RoadCommit,
    RouteRefresh,
    SimulationUpdate,
    WorldRender,
    HudRender,
    Present,
    FrameWait,
    AutosaveCapture,
    AutosaveWrite,
    Count
};

inline constexpr std::size_t sample_limit=4096;
inline std::atomic_bool enabled_flag=false;
inline std::array<std::atomic_uint64_t,static_cast<std::size_t>(Counter::Count)> counters{};
inline std::array<std::vector<std::uint64_t>,static_cast<std::size_t>(Timing::Count)> samples{};
inline std::mutex samples_mutex;

inline void set_enabled(bool enabled) { enabled_flag.store(enabled,std::memory_order_relaxed); }
inline bool enabled() { return enabled_flag.load(std::memory_order_relaxed); }

inline void increment(Counter counter,std::uint64_t amount=1) {
    if (enabled()) counters[static_cast<std::size_t>(counter)].fetch_add(amount,std::memory_order_relaxed);
}

inline std::uint64_t counter(Counter value) {
    return counters[static_cast<std::size_t>(value)].load(std::memory_order_relaxed);
}

inline void record(Timing timing,std::uint64_t nanoseconds) {
    if (!enabled()) return;
    std::lock_guard lock(samples_mutex);
    auto& values=samples[static_cast<std::size_t>(timing)];
    if (values.size()<sample_limit) values.push_back(nanoseconds);
}

class ScopedTimer {
public:
    explicit ScopedTimer(Timing timing) : timing_(timing),active_(enabled()) {
        if (active_) start_=std::chrono::steady_clock::now();
    }
    ~ScopedTimer() {
        if (!active_) return;
        const auto elapsed=std::chrono::steady_clock::now()-start_;
        record(timing_,static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()));
    }
    ScopedTimer(const ScopedTimer&)=delete;
    ScopedTimer& operator=(const ScopedTimer&)=delete;
private:
    Timing timing_;
    bool active_=false;
    std::chrono::steady_clock::time_point start_{};
};

// Kept as a World member so implicit copies, including temporary validation
// Worlds, cannot evade the global diagnostics counter.
struct WorldCopyProbe {
    WorldCopyProbe()=default;
    WorldCopyProbe(const WorldCopyProbe&) { increment(Counter::WorldCopies); }
    WorldCopyProbe& operator=(const WorldCopyProbe&) {
        increment(Counter::WorldCopies); return *this;
    }
    WorldCopyProbe(WorldCopyProbe&&) noexcept=default;
    WorldCopyProbe& operator=(WorldCopyProbe&&) noexcept=default;
};

inline void reset() {
    for (auto& value:counters) value.store(0,std::memory_order_relaxed);
    std::lock_guard lock(samples_mutex);
    for (auto& value:samples) value.clear();
}

inline constexpr std::array<std::string_view,static_cast<std::size_t>(Counter::Count)> counter_names{
    "world_copies","world_restores","world_executes","road_plans","route_refreshes",
    "bfs_calls","bfs_visited_cells","asset_decodes","texture_uploads","file_reads",
    "file_writes","simulation_ticks"};
inline constexpr std::array<std::string_view,static_cast<std::size_t>(Timing::Count)> timing_names{
    "event_handling","hover_picking","road_planning","budget_check","road_commit",
    "route_refresh","simulation_update","world_render","hud_render","present","frame_wait",
    "autosave_capture","autosave_write"};

inline void print_summary(std::ostream& out) {
    if (!enabled()) return;
    out<<"Performance diagnostics (bounded to "<<sample_limit<<" samples per timing):\n";
    for (std::size_t i=0;i<counter_names.size();++i)
        out<<"  "<<counter_names[i]<<"="<<counters[i].load(std::memory_order_relaxed)<<'\n';
    std::array<std::vector<std::uint64_t>,static_cast<std::size_t>(Timing::Count)> copied;
    {
        std::lock_guard lock(samples_mutex);
        copied=samples;
    }
    out<<std::fixed<<std::setprecision(3);
    for (std::size_t i=0;i<copied.size();++i) {
        auto& values=copied[i];
        if (values.empty()) continue;
        std::sort(values.begin(),values.end());
        const auto percentile=[&](double p) {
            const auto index=static_cast<std::size_t>(p*static_cast<double>(values.size()-1));
            return static_cast<double>(values[index])/1'000'000.0;
        };
        out<<"  "<<timing_names[i]<<" samples="<<values.size()
           <<" median_ms="<<percentile(0.5)<<" p95_ms="<<percentile(0.95)
           <<" max_ms="<<static_cast<double>(values.back())/1'000'000.0<<'\n';
    }
}

} // namespace openemperor::performance
