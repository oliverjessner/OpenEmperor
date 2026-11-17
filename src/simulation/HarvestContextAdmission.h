#pragma once

#include "simulation/GrowerLifecycleState.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace openemperor::simulation {

class World;

// Read-only observations captured by World for one actual worker invocation.
// A binding alone supplies no verified numeric operand or harvest authority.
struct HarvestContextBindings {
    FoodGrowerId actor_id{};
    BuildingId farm_id{},field_id{};
    std::uint64_t tick=0,road_revision=0;
    std::uint8_t slot=0;
    std::optional<original::ModelBuilding> crop;
    std::string source_map_sha256,model_sha256;
    std::optional<original::Difficulty> difficulty;
    std::optional<std::int16_t> source_region_operand;
    std::optional<std::uint8_t> construction_fertility;
    std::optional<bool> irrigated,event_doubling;
    std::uint16_t checked_irrigation_cells=0;
    std::optional<Cell> unsupported_terrain_cell;
    std::uint16_t quantity=0,work_word=0,carried_word=0;
    bool operator==(const HarvestContextBindings&) const = default;
};

// Only World's preparation can construct a result. The current source domain
// has unresolved live operands and therefore returns precise refusals. The
// freely constructible pure CropHarvestContext cannot become an admitted one.
class HarvestContextAdmission final {
public:
    const HarvestContextBindings& bindings() const noexcept { return bindings_; }
    const std::vector<HarvestContextFailure>& failures() const noexcept { return failures_; }
    const std::optional<original::CropHarvestContext>& context() const noexcept { return context_; }
    bool accepted() const noexcept { return context_.has_value() && failures_.empty(); }

private:
    friend class World;
    HarvestContextAdmission(HarvestContextBindings bindings,std::vector<HarvestContextFailure> failures)
        : bindings_(std::move(bindings)),failures_(std::move(failures)) {}

    const HarvestContextBindings bindings_;
    const std::vector<HarvestContextFailure> failures_;
    const std::optional<original::CropHarvestContext> context_;
};

std::string_view harvest_context_failure_message(HarvestContextFailure) noexcept;

} // namespace openemperor::simulation
