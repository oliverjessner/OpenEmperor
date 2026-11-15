#include "simulation/World.h"
#include "simulation/original/SeasonRules.h"

#include <algorithm>

namespace openemperor::simulation {
namespace {
std::optional<std::uint64_t> next_phase_tick(std::uint64_t tick,std::uint8_t phase) {
    const auto current=original::scheduler_position(tick).phase;
    const auto remaining=static_cast<std::uint64_t>(
        (phase+original::scheduler_phase_count-current)%original::scheduler_phase_count)+1;
    if(tick>=original::scheduler_max_tick || remaining>original::scheduler_max_tick-tick)
        return std::nullopt;
    return tick+remaining;
}
}

std::optional<AgricultureCalendarStatus> World::agriculture_calendar() const {
    if(!original_agriculture_enabled()) return std::nullopt;
    return AgricultureCalendarStatus{ticks_,original::scheduler_position(ticks_),
        original::scheduler_mapping_version,next_phase_tick(ticks_,20),
        irrigation_enabled() ? next_phase_tick(ticks_,27):std::nullopt};
}

std::optional<AgricultureSeasonStatus> World::agriculture_season_status(BuildingId id) const {
    const auto calendar=agriculture_calendar();
    if(!calendar) return std::nullopt;
    AgricultureSeasonStatus result;
    result.subject=id;
    result.calendar=*calendar;
    const auto field=std::find_if(agriculture_->fields.begin(),agriculture_->fields.end(),
        [id](const auto& row){return row.field_id==id;});
    if(field!=agriculture_->fields.end()) {
        result.producer=field->farm_id;
        result.model=static_cast<original::ModelBuilding>(195+static_cast<unsigned>(field->crop));
        result.authored_production_active=original_plantations_enabled() &&
            (field->crop==AgricultureCrop::Wheat || field->crop==AgricultureCrop::Millet);
        if(result.authored_production_active)
            result.productive_progress=static_cast<std::uint32_t>(building(field->farm_id).progress);
        result.productive_target=result.authored_production_active ? 80:0;
        result.harvested=field->harvested;
        result.blocker="Original Grower lifecycle, runtime yield inputs and raw-to-Food units unresolved";
    } else {
        const auto hemp=std::find_if(original_production_->hemp_farms.begin(),original_production_->hemp_farms.end(),
            [id](const auto& row){return row.field_id && *row.field_id==id;});
        if(hemp!=original_production_->hemp_farms.end()) {
            result.producer=hemp->farm_id;
            result.model=original::ModelBuilding::HempField;
            result.authored_production_active=true;
            result.productive_progress=static_cast<std::uint32_t>(building(hemp->farm_id).progress);
            result.productive_target=80;
            result.harvested=hemp->harvested;
            result.blocker="Original tending and harvest binding unresolved";
        } else {
            if(!plantations_) return std::nullopt;
            const auto plant=std::find_if(plantations_->plants.begin(),plantations_->plants.end(),
                [id](const auto& row){return row.plant_id==id;});
            if(plant==plantations_->plants.end()) return std::nullopt;
            result.producer=plant->source_id;
            switch(building(id).kind) {
            case Object::OriginalTeaBush26: result.model=original::ModelBuilding::TeaBush;break;
            case Object::OriginalLacquerTree27: result.model=original::ModelBuilding::LacquerTree;break;
            case Object::OriginalMulberryTree28: result.model=original::ModelBuilding::MulberryTree;break;
            default: return std::nullopt;
            }
            result.authored_production_active=true;
            result.productive_progress=plant->growth;
            result.productive_target=plantation_growth_ticks;
            result.harvest_ready=plant->growth==plantation_growth_ticks;
            result.harvested=plant->harvested;
            result.returned=plant->returned;
            const auto source=std::find_if(plantations_->sources.begin(),plantations_->sources.end(),
                [&](const auto& row){return row.building_id==plant->source_id;});
            result.actual_grower_active=source!=plantations_->sources.end() && source->latest_trip &&
                source->latest_trip->plant_id==id && !source->latest_trip->returned_tick;
            result.blocker="Original seasonal yield and tending binding unresolved";
        }
    }
    const auto slot=calendar->position.month;
    result.reference_growing_period=original::season_in_growing_period(result.model,slot);
    result.reference_growth_suppressed=original::season_growth_suppressed(result.model,slot);
    const auto& producer=building(result.producer);
    result.operating=producer.operating_enabled && building(id).operating_enabled;
    result.burning=building_on_fire(result.producer);
    result.assigned_workers=workers_assigned(result.producer);
    result.required_workers=workforce_required(result.producer);
    result.staffed=building_staffed(result.producer);
    result.road_connected=!building_entrances(result.producer).empty();
    const auto water=irrigation_field_status(building(id).cell);
    result.water_available=water.water_available;
    result.yield_effect_active=water.yield_effect_active;
    return result;
}

} // namespace openemperor::simulation
