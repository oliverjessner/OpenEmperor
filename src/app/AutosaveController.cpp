#include "app/AutosaveController.h"
#include "core/PerformanceDiagnostics.h"

#include <limits>
#include <stdexcept>

namespace openemperor {
namespace {
std::uint64_t due_after(std::uint64_t tick) {
    if (tick>std::numeric_limits<std::uint64_t>::max()-autosave_interval_ticks)
        return std::numeric_limits<std::uint64_t>::max();
    return tick+autosave_interval_ticks;
}
}
AutosaveController::AutosaveController(std::filesystem::path app_root,
                                       std::filesystem::path data_root,bool enabled,
                                       std::uintmax_t budget)
    : app_root_(std::move(app_root)),data_root_(std::move(data_root)),budget_(budget),
      enabled_(enabled) {}

persistence::RecoveryStore& AutosaveController::store() {
    if (!store_) store_=std::make_unique<persistence::RecoveryStore>(app_root_,data_root_,budget_);
    return *store_;
}

AutosaveResult AutosaveController::begin(const persistence::SaveDocument& document,
                                         const std::vector<std::uint8_t>& buildable,
                                         std::optional<persistence::RecoveryParent> parent,
                                         persistence::RecoveryWriteFaults faults) {
    history_id_.clear();last_error_.clear();last_observed_tick_=document.world.ticks;
    last_checkpoint_tick_=document.world.ticks;next_due_tick_=due_after(document.world.ticks);
    if (!enabled_) return {};
    const auto before=std::chrono::steady_clock::now();
    performance::ScopedTimer write_timer(performance::Timing::AutosaveWrite);
    try {
        history_id_=store().create_history(document,buildable,std::move(parent),faults);
        return {AutosaveResult::Kind::Saved,"Recovery start point saved",
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-before)};
    } catch (const std::exception& error) {
        last_error_=error.what();
        return {AutosaveResult::Kind::Failed,
            "Autosave failed; previous recovery points are unchanged. "+last_error_,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-before)};
    }
}

AutosaveResult AutosaveController::poll(const persistence::SaveDocument& document,
                                        const std::vector<std::uint8_t>& buildable,bool safe,
                                        persistence::RecoveryWriteFaults faults) {
    if (!enabled_ || history_id_.empty()) return {};
    const auto tick=document.world.ticks;
    if (tick==last_observed_tick_ || tick<next_due_tick_ || !safe) return {};
    last_observed_tick_=tick;
    const auto before=std::chrono::steady_clock::now();
    performance::ScopedTimer write_timer(performance::Timing::AutosaveWrite);
    try {
        store().write_checkpoint(history_id_,document,buildable,faults);
        last_checkpoint_tick_=tick;next_due_tick_=due_after(tick);last_error_.clear();
        return {AutosaveResult::Kind::Saved,"Recovery checkpoint saved at tick "+std::to_string(tick),
                std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-before)};
    } catch (const std::exception& error) {
        last_error_=error.what();
        // One interval before retry prevents a failing filesystem operation every frame.
        next_due_tick_=due_after(tick);
        return {AutosaveResult::Kind::Failed,
            "Autosave failed; previous recovery points are unchanged. "+last_error_,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-before)};
    }
}
} // namespace openemperor
