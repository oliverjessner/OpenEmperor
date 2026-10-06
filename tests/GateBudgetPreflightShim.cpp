// Linked only by SandboxGateBudgetViewTests. Keep the actual guidance code and
// intercept its two purchase-check overloads before any source work, without a
// production fault-injection API or a corrupted World fixture.
#define starter_budget_warning actual_starter_budget_warning
#include "../src/simulation/CityStartGuidance.cpp"
#undef starter_budget_warning

#include <stdexcept>

namespace {
bool fail_next=false;
bool failure_consumed=false;
void preflight_boundary() {
    if (!fail_next) return;
    fail_next=false;failure_consumed=true;
    throw std::invalid_argument("Authored budget preflight failure");
}
}
namespace openemperor::gate_budget_test {
void fail_next_preflight() { fail_next=true;failure_consumed=false; }
bool injected_failure_consumed() { return failure_consumed; }
}
namespace openemperor::simulation {
std::optional<StarterBudgetWarning> starter_budget_warning(const World& world,Command command) {
    preflight_boundary();return actual_starter_budget_warning(world,command);
}
std::optional<StarterBudgetWarning> starter_budget_warning(
    const World& world,std::span<const Command> commands) {
    preflight_boundary();return actual_starter_budget_warning(world,commands);
}
}
