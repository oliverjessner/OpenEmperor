#pragma once

#include "persistence/RecoveryStore.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>

namespace openemperor {

inline constexpr std::uint64_t autosave_period_simulated_seconds=60;
inline constexpr std::uint64_t autosave_interval_ticks=
    simulation::Rules::ticks_per_second*autosave_period_simulated_seconds;
static_assert(autosave_interval_ticks==1200);

struct AutosaveResult {
    enum class Kind { None, Saved, Failed } kind=Kind::None;
    std::string message;
    std::chrono::milliseconds duration{};
};

class AutosaveController {
public:
    AutosaveController(std::filesystem::path app_root,std::filesystem::path data_root,
                       bool enabled,std::uintmax_t budget=persistence::recovery_storage_budget_bytes);
    AutosaveResult begin(const persistence::SaveDocument& document,
                         const std::vector<std::uint8_t>& buildable,
                         std::optional<persistence::RecoveryParent> parent=std::nullopt,
                         persistence::RecoveryWriteFaults faults={});
    AutosaveResult poll(const persistence::SaveDocument& document,
                        const std::vector<std::uint8_t>& buildable,bool safe,
                        persistence::RecoveryWriteFaults faults={});
    bool enabled() const { return enabled_; }
    bool active() const { return !history_id_.empty(); }
    const std::string& history_id() const { return history_id_; }
    std::uint64_t next_due_tick() const { return next_due_tick_; }
    std::uint64_t last_checkpoint_tick() const { return last_checkpoint_tick_; }
    bool due(std::uint64_t tick) const {
        return enabled_ && active() && tick!=last_observed_tick_ && tick>=next_due_tick_;
    }
    const std::string& last_error() const { return last_error_; }
    persistence::RecoveryStore& store();
private:
    std::filesystem::path app_root_,data_root_;
    std::uintmax_t budget_;
    std::unique_ptr<persistence::RecoveryStore> store_;
    bool enabled_=true;
    std::string history_id_,last_error_;
    std::uint64_t next_due_tick_=0,last_observed_tick_=0,last_checkpoint_tick_=0;
};

} // namespace openemperor
