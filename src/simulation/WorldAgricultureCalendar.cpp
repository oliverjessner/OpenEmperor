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
    const auto fields=agriculture_fields();
    const auto field=std::find_if(fields.begin(),fields.end(),
        [id](const auto& row){return row.field_id==id;});
    if(field!=fields.end()) {
        result.producer=field->farm_id;
        result.model=static_cast<original::ModelBuilding>(195+static_cast<unsigned>(field->crop));
        result.authored_production_active=authored_agriculture_crop_enabled(profile_,rule_version_,field->crop);
        if(result.authored_production_active)
            result.productive_progress=static_cast<std::uint32_t>(agriculture_field_progress(id));
        result.productive_target=result.authored_production_active ? 80:0;
        result.harvested=field->harvested;
        result.blocker=multi_field_agriculture_enabled() ? "AUTHORED shared Farm labor; per-field80 work units; Original harvest reconstruction is separate":
            playable_agriculture_enabled() ? "AUTHORED80-tick Food adapter; Original harvest reconstruction is separate":
            "Original Grower lifecycle, runtime yield inputs and raw-to-Food units unresolved";
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
    result.burning=building_on_fire(result.producer) || (playable_agriculture_enabled() && building_on_fire(id));
    result.assigned_workers=workers_assigned(result.producer);
    result.required_workers=workforce_required(result.producer);
    result.staffed=building_staffed(result.producer);
    result.road_connected=!building_entrances(result.producer).empty();
    const auto water=irrigation_field_status(building(id).cell);
    result.water_available=water.water_available;
    result.yield_effect_active=water.yield_effect_active;
    if(seasonal_agriculture_enabled()) {
        result.seasonal_production=true;result.growing=seasonal_growing(result.model,ticks_);
        result.next_opening_tick=seasonal_next_opening(result.model,ticks_);
        std::optional<std::uint64_t> ready;
        if(field!=fields.end()) {
            for(const auto& r:seasonal_agriculture_->fields)if(r.field_id==id){
                result.wasted_work=r.wasted_work;result.expired_ready=r.expired_ready;result.harvest_progress=r.harvest_progress;ready=r.ready_tick;break;
            }
            for(const auto& r:physical_food_growers_->fields)if(r.field_id==id)result.returned=r.returned;
            for(const auto& r:physical_food_growers_->farms)if(r.farm_id==result.producer)
                result.actual_grower_active=r.latest_trip&&!r.latest_trip->returned_tick&&r.latest_trip->field_id==id;
        } else if(result.model==original::ModelBuilding::HempField) {
            for(const auto& r:seasonal_agriculture_->hemp_farms)if(r.farm_id==result.producer){
                result.wasted_work=r.wasted_work;result.expired_ready=r.expired_ready;ready=r.ready_tick;
                result.actual_grower_active=r.latest_trip&&!r.latest_trip->returned_tick;
                result.harvested=r.picked;
            }
            for(const auto& r:original_production_->hemp_farms)if(r.farm_id==result.producer)result.returned=r.harvested;
        } else {
            for(const auto& r:seasonal_agriculture_->plants)if(r.plant_id==id){
                result.wasted_work=r.wasted_work;result.expired_ready=r.expired_ready;ready=r.ready_tick;break;
            }
        }
        result.harvest_ready=ready.has_value();result.harvest_allowed=ready&&seasonal_harvest_allowed(result.model,ticks_,*ready);
        if(ready)result.ready_expiry_tick=seasonal_ready_expiry(result.model,*ready);
        result.blocker=result.harvest_ready?"AUTHORED ready crop; physical harvest/pickup and return required":result.growing?
            "AUTHORED growing window; no Food from calendar passage":"AUTHORED seasonal rest; waiting for next growing window";
    }
    return result;
}

} // namespace openemperor::simulation
