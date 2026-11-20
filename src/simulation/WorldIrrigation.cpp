#include "simulation/World.h"
#include "simulation/original/IrrigationRules.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) { throw std::invalid_argument(reason); }
bool cell_less(Cell a,Cell b) { return a.y!=b.y ? a.y<b.y:a.x<b.x; }
constexpr std::array<Cell,4> cardinal{{{0,-1},{1,0},{0,1},{-1,0}}};
Cell neighbor(Cell a,Cell b) { return {a.x+b.x,a.y+b.y}; }
std::optional<std::uint64_t> last_phase27(std::uint64_t tick) {
    if(tick<28) return std::nullopt;
    return 28+(tick-28)/51*51;
}
template<class Rows> auto ditch_at(Rows& rows,Cell cell) {
    return std::lower_bound(rows.begin(),rows.end(),cell,
        [](const auto& row,Cell key){return cell_less(row.cell,key);});
}
template<class Rows> auto pump_at(Rows& rows,BuildingId id) {
    return std::lower_bound(rows.begin(),rows.end(),id,
        [](const auto& row,BuildingId key){return row.building_id<key;});
}
bool has_ditch(const IrrigationState& state,Cell cell) {
    const auto at=ditch_at(state.ditches,cell);
    return at!=state.ditches.end() && at->cell==cell;
}
bool infrastructure(const World& world,Cell cell) {
    return world.bridge_at(cell) || world.bridge_bank_at(cell) || world.ramp_endpoint(cell) ||
        world.source_cell_ramp_claim(cell) || world.fixed_passage(cell);
}
bool pump_context(const World& world,const original::PumpGeometry& shape,
                  std::optional<BuildingId> owner={}) {
    const auto permissions=world.map_permissions();const auto terrain=world.original_terrain_facts();
    if(!permissions || !terrain || !terrain->irrigation_complete()) return false;
    const auto occupied=[&](Cell cell) {
        const auto body=world.building_owner_at(cell);
        return body && (!owner || *body!=*owner);
    };
    for(const auto cell:shape.water) {
        if(!world.in_bounds(cell) || !permissions->bridge_water(cell) || terrain->raw_terrain(cell)!=4U) return false;
        // The six exterior intake cells are water context, not exclusive
        // claims. The observed exterior query tests water, not mutable bridge
        // or building occupancy. Exclusivity applies to the four body cells.
        const bool body=std::find(shape.footprint.begin(),shape.footprint.end(),cell)!=shape.footprint.end();
        if(body && (infrastructure(world,cell) || occupied(cell) ||
            (world.object_at(cell)!=Object::Empty && (!owner || world.building_owner_at(cell)!=owner)))) return false;
    }
    for(const auto cell:shape.land) {
        if(!world.in_bounds(cell) || !permissions->building_allowed(cell) || infrastructure(world,cell) || occupied(cell) ||
            (terrain->raw_terrain(cell)&0x87375a6fU)!=0) return false;
        if(world.object_at(cell)!=Object::Empty && (!owner || world.building_owner_at(cell)!=owner)) return false;
    }
    for(const auto cell:shape.footprint) {
        if(!world.in_bounds(cell)) return false;
        if(owner) {
            if(world.building_owner_at(cell)!=owner || world.object_at(cell)!=Object::OriginalIrrigationPump) return false;
        } else if(world.object_at(cell)!=Object::Empty || world.building_owner_at(cell)) return false;
        if(world.irrigation_state() && has_ditch(*world.irrigation_state(),cell)) return false;
    }
    return true;
}
bool ditch_context(const World& world,Cell cell) {
    const auto permissions=world.map_permissions();const auto terrain=world.original_terrain_facts();
    if(!permissions || !terrain || !terrain->irrigation_complete() || !world.in_bounds(cell) ||
        !terrain->irrigation_boundary_allowed(cell) || permissions->protected_original(cell) ||
        infrastructure(world,cell) || world.building_owner_at(cell)) return false;
    const auto kind=world.object_at(cell);
    if(kind!=Object::Empty && kind!=Object::Road) return false;
    const bool road=world.road_cell(cell);
    if(road ? !(permissions->road_allowed(cell) || permissions->original_road(cell)):
              !permissions->building_allowed(cell)) return false;
    const auto raw=terrain->raw_terrain(cell);
    // An immutable Original ditch is never silently imported as a paid overlay.
    return !(raw&0x800U) && original::ditch_terrain_allowed(raw,0,0);
}
bool pump_enabled_context(const World& world,const IrrigationPump& row,const BuildingState& b) {
    return b.placed && b.kind==Object::OriginalIrrigationPump && b.operating_enabled && !world.building_on_fire(b.id) &&
        pump_context(world,original::pump_geometry(b.cell,row.orientation),b.id);
}
template<class Snapshot>
void validate_retained_irrigation(const Snapshot& s) {
    if(!simulation::original_irrigation_enabled(s.profile,s.rule_version)) {
        if(s.irrigation || std::any_of(s.buildings.begin(),s.buildings.end(),
            [](const auto& b){return b.kind==Object::OriginalIrrigationPump;}))
            invalid("Irrigation authority requires the explicit Rule26 contract");
        return;
    }
    if(!s.irrigation || (!s.agriculture && !s.fishing_food && !s.hunting_food) || !s.plantations || s.width<1 || s.width>228 || s.height<1 || s.height>228)
        invalid("Irrigation requires its bounded Agriculture/plantation lineage");
    const auto& state=*s.irrigation;
    if(state.ditches.size()>irrigation_ditch_limit || state.pumps.size()>Rules::original_production_building_limit ||
        state.ditches_removed_total>state.ditches_placed_total ||
        state.ditches_placed_total-state.ditches_removed_total!=state.ditches.size() ||
        state.ditches_placed_total>s.command_sequence || state.ditches_removed_total>s.command_sequence-state.ditches_placed_total ||
        state.ditch_construction_spent<state.ditches_placed_total ||
        state.ditch_construction_spent>s.construction_spent_total ||
        std::count_if(s.buildings.begin(),s.buildings.end(),[](const auto& b){return b.kind==Object::OriginalIrrigationPump;})!=
            static_cast<std::ptrdiff_t>(state.pumps.size()))
        invalid("Irrigation installation, removal, payment or body counts disagree");
    std::vector<std::uint64_t> sequences;sequences.reserve(state.ditches.size());
    for(std::size_t i=0;i<state.ditches.size();++i) {
        const auto& row=state.ditches[i];
        if((i && !cell_less(state.ditches[i-1].cell,row.cell)) || row.cell.x<0 || row.cell.y<0 ||
            row.cell.x>=s.width || row.cell.y>=s.height || row.placed_tick>s.ticks ||
            !row.placed_sequence || row.placed_sequence>s.command_sequence)
            invalid("Irrigation ditch lost its unique paid cell/sequence or chronology");
        sequences.push_back(row.placed_sequence);
    }
    std::sort(sequences.begin(),sequences.end());
    if(std::adjacent_find(sequences.begin(),sequences.end())!=sequences.end())
        invalid("Irrigation ditch purchase sequence was reused");
    for(std::size_t i=0;i<state.pumps.size();++i) {
        const auto& row=state.pumps[i];
        const auto b=std::lower_bound(s.buildings.begin(),s.buildings.end(),row.building_id,
            [](const auto& body,BuildingId id){return body.id<id;});
        if((i && !(state.pumps[i-1].building_id<row.building_id)) || row.orientation>3 ||
            b==s.buildings.end() || b->id!=row.building_id || !b->placed || b->kind!=Object::OriginalIrrigationPump ||
            b->placed_tick>s.ticks || (row.eligible_since_tick &&
                (*row.eligible_since_tick<=b->placed_tick || *row.eligible_since_tick==0 ||
                 (*row.eligible_since_tick>s.ticks && (s.ticks==UINT64_MAX || *row.eligible_since_tick!=s.ticks+1)))))
            invalid("Irrigation Pump lost its paid body, orientation or supply epoch");
        if(b->input_clay || b->output || b->pottery_stock || b->reserved_incoming || b->progress ||
            b->active_recipe_clay || b->recipes_completed || b->demand_progress || b->fulfilled_demand ||
            b->missed_demand || b->consumed_total || b->last_demand_status || b->clay_extracted ||
            b->food_stock || b->reserved_food_incoming || b->food_consumed_total || b->food_produced ||
            b->service_until_tick || b->population || b->taxes_paid_total || b->health_risk ||
            b->health_protection_until_tick || b->sick_until_tick ||
            std::any_of(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.owner==b->id;}))
            invalid("Irrigation Pump cannot own goods, recipes, residents, service grants or a courier channel");
    }
}
} // namespace

World World::create_original_irrigation(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    return create_irrigation_generation(kOriginalIrrigationRuleVersion,std::move(permissions),std::move(model),std::move(terrain));
}
World World::create_playable_agriculture(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    return create_irrigation_generation(kPlayableAgricultureRuleVersion,std::move(permissions),std::move(model),std::move(terrain));
}
World World::create_playable_fishing(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_irrigation_generation(kPlayableFishingRuleVersion,std::move(permissions),std::move(model),std::move(terrain));
    world.initialize_fishing_sites();world.validate_fishing_context();return world;
}
World World::restore_playable_fishing(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=restore_irrigation_generation(kPlayableFishingRuleVersion,s,std::move(permissions),std::move(model),std::move(terrain));
    world.validate_fishing_context();return world;
}
World World::create_playable_hunting(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_irrigation_generation(kPlayableHuntingRuleVersion,std::move(permissions),std::move(model),std::move(terrain));
    world.initialize_fishing_sites();world.initialize_hunting_habitats();
    world.validate_fishing_context();world.validate_hunting_context();return world;
}
World World::restore_playable_hunting(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=restore_irrigation_generation(kPlayableHuntingRuleVersion,s,std::move(permissions),std::move(model),std::move(terrain));
    world.validate_fishing_context();world.validate_hunting_context();return world;
}
World World::create_multi_field_agriculture(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_irrigation_generation(kMultiFieldAgricultureRuleVersion,std::move(permissions),std::move(model),std::move(terrain));
    world.initialize_fishing_sites();world.initialize_hunting_habitats();
    world.validate_fishing_context();world.validate_hunting_context();return world;
}
World World::restore_multi_field_agriculture(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    validate_multi_field_agriculture_snapshot(s);
    auto world=restore_irrigation_generation(kMultiFieldAgricultureRuleVersion,s,std::move(permissions),std::move(model),std::move(terrain));
    world.validate_fishing_context();world.validate_hunting_context();return world;
}
World World::create_irrigation_generation(std::uint32_t version,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if(!permissions || permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !model || !terrain ||
        !terrain->irrigation_complete() || !terrain->fertility_complete())
        invalid("Irrigation creation requires policy3 and complete source-bound model/terrain facts");
    const auto width=permissions->width(),height=permissions->height();auto mask=permissions->building_mask();
    World world(width,height,std::move(mask),RulesProfile::CityV16,version,
        std::move(permissions),true,std::move(model),std::move(terrain));
    world.original_production_->fire_protection.emplace();
    world.validate_original_hemp_model();world.validate_agriculture_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();
    world.validate_original_industry_model();world.validate_original_finance_model();world.validate_irrigation_model();
    world.rebuild_original_influence_cache();world.refresh_irrigation();return world;
}
World World::restore_original_irrigation(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    return restore_irrigation_generation(kOriginalIrrigationRuleVersion,s,std::move(permissions),std::move(model),std::move(terrain));
}
World World::restore_playable_agriculture(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    return restore_irrigation_generation(kPlayableAgricultureRuleVersion,s,std::move(permissions),std::move(model),std::move(terrain));
}
World World::restore_irrigation_generation(std::uint32_t version,const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if(s.profile!=RulesProfile::CityV16 || s.rule_version!=version || !permissions || !model || !terrain ||
        permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !terrain->fertility_complete() || !terrain->irrigation_complete())
        invalid("Irrigation restore requires its exact generation, policy3 and complete source-bound model/terrain");
    validate_original_production_snapshot(s);validate_irrigation_snapshot(s);
    for(const auto& row:(s.hunting_food ? s.hunting_food->fields:s.fishing_food ? s.fishing_food->fields:s.agriculture->fields)) if(row.crop==AgricultureCrop::Rice) {
        const auto at=std::find_if(s.buildings.begin(),s.buildings.end(),[&](const auto& b){return b.id==row.field_id;});
        if(at==s.buildings.end() || terrain->fertility(at->cell)!=100)
            invalid("Irrigation generation retains Rice fertility100 construction admission");
    }
    auto world=restore_impl(s,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_agriculture_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();
    world.validate_original_industry_model();world.validate_original_finance_model();world.validate_irrigation_model();
    world.validate_irrigation_state();world.rebuild_irrigation_cache();return world;
}
void World::validate_irrigation_model() const {
    if(!irrigation_enabled()) return;
    if(!original_rules_model_ || !original_terrain_facts_ || !original_terrain_facts_->irrigation_complete() || !permissions_ ||
        permissions_->policy_version()!=kBridgesMapPermissionsPolicyVersion)
        invalid("Irrigation requires complete immutable source context");
    for(const auto model:{original::ModelBuilding::IrrigationDitch,original::ModelBuilding::IrrigationPump})
        if(!original_rules_model_->supports_building(model) || original_rules_model_->building_value(model,original::BuildingField::Cost)<=0)
            invalid("Irrigation requires separately prepared positive-price Ditch202 and Pump203 models");
    const auto workers=original_rules_model_->building_value(original::ModelBuilding::IrrigationPump,original::BuildingField::Employees);
    if(workers<=0 || workers>INT16_MAX ||
        original_rules_model_->building_value(original::ModelBuilding::IrrigationDitch,original::BuildingField::Employees)!=0)
        invalid("Irrigation requires a positive bounded Pump workforce and zero-worker ditch overlays");
}
void World::validate_irrigation_snapshot(const WorldSnapshot& s) { validate_retained_irrigation(s); }

bool World::irrigation_crossings_valid(std::span<const Cell> added,std::optional<Cell> removed,
    std::span<const Cell> added_roads,std::optional<Cell> removed_road) const {
    if(!irrigation_) return added.empty();
    for(const auto cell:added) if(!in_bounds(cell)) return false;
    for(const auto cell:added_roads) if(!in_bounds(cell)) return false;
    if((removed && !in_bounds(*removed)) || (removed_road && !in_bounds(*removed_road))) return false;
    const auto contains=[&](Cell cell) {
        if(removed && cell==*removed) return false;
        return has_ditch(*irrigation_,cell) || std::find(added.begin(),added.end(),cell)!=added.end();
    };
    const auto has_road=[&](Cell cell) {
        return (!removed_road || cell!=*removed_road) &&
            (road_cell(cell) || std::find(added_roads.begin(),added_roads.end(),cell)!=added_roads.end());
    };
    const auto valid=[&](Cell cell) {
        if(!contains(cell) || !has_road(cell)) return true;
        std::uint8_t roads=0,ditches=0;
        for(std::size_t n=0;n<cardinal.size();++n) {
            const auto at=neighbor(cell,cardinal[n]);
            if(in_bounds(at) && has_road(at)) roads|=static_cast<std::uint8_t>(1U<<n);
            if(in_bounds(at) && contains(at)) ditches|=static_cast<std::uint8_t>(1U<<n);
        }
        return original::crossing_allowed(roads,ditches);
    };
    // Only changed cells and their cardinal neighbors can change a crossing.
    if(added.empty() && !removed && added_roads.empty() && !removed_road) {
        for(const auto& row:irrigation_->ditches) if(!valid(row.cell)) return false;
    } else {
        for(const auto cell:added) {
            if(!valid(cell)) return false;
            for(const auto delta:cardinal) if(!valid(neighbor(cell,delta))) return false;
        }
        if(removed) for(const auto delta:cardinal) if(!valid(neighbor(*removed,delta))) return false;
        for(const auto cell:added_roads) {
            if(!valid(cell)) return false;
            for(const auto delta:cardinal) if(!valid(neighbor(cell,delta))) return false;
        }
        if(removed_road) {
            if(!valid(*removed_road)) return false;
            for(const auto delta:cardinal) if(!valid(neighbor(*removed_road,delta))) return false;
        }
    }
    return true;
}
IrrigationBatchValidation World::validate_irrigation_batch(std::span<const Cell> cells) const {
    IrrigationBatchValidation result;
    if(!irrigation_enabled() || !irrigation_) {result.reason="Ditch construction requires explicit Agriculture irrigation";return result;}
    if(cells.empty() || cells.size()>irrigation_gesture_limit) {result.reason="Irrigation gesture requires1..256 cells";return result;}
    std::vector<Cell> added;added.reserve(cells.size());
    for(const auto cell:cells) {
        if(!has_ditch(*irrigation_,cell) && (grower_navigation_cell_claimed(cell) || fishing_boat_cell_claimed(cell) || hunting_actor_cell_claimed(cell))) {
            result.reason="Ditch overlaps navigation experiment or its begun edge";return result;
        }
        if(!ditch_context(*this,cell)) {
            result.reason="Ditch requires permitted empty land or a supported road crossing";
            result.diagnostic={in_bounds(cell) ? BuildBlocker::UnsupportedTerrain:BuildBlocker::OutsideMap,cell,{}};return result;
        }
        if(!has_ditch(*irrigation_,cell) && std::find(added.begin(),added.end(),cell)==added.end()) added.push_back(cell);
    }
    if(!irrigation_crossings_valid(added)) {result.reason="Ditch crossing requires a single perpendicular road axis";return result;}
    const auto count=static_cast<std::uint64_t>(added.size());
    result.new_ditch_count=added.size();
    if(count>irrigation_ditch_limit-irrigation_->ditches.size() || command_sequence_>=UINT64_MAX-1 ||
        count>UINT64_MAX-1-command_sequence_ || count>UINT64_MAX-irrigation_->ditches_placed_total || count>UINT64_MAX-road_revision_) {
        result.reason="Irrigation installation counter exhausted";return result;
    }
    const auto unit=construction_cost(CommandType::PlaceIrrigationDitch);
    if(unit<=0 || count>static_cast<std::uint64_t>(INT64_MAX/unit)) {result.reason="Irrigation construction cost overflow";return result;}
    result.total_cost=static_cast<std::int64_t>(count)*unit;
    const auto cost=static_cast<std::uint64_t>(result.total_cost);
    if(cost>UINT64_MAX-construction_spent_total_ || cost>UINT64_MAX-irrigation_->ditch_construction_spent) {
        result.reason="Irrigation spending counter exhausted";return result;
    }
    if(result.total_cost>treasury_) {
        result.reason="Not enough money for the complete irrigation gesture";
        result.diagnostic={BuildBlocker::InsufficientFunds,{},{}};return result;
    }
    result.accepted=true;result.reason=count ? "Irrigation ditch ready":"Ditch already installed";return result;
}
CommandResult World::execute_irrigation_batch(std::span<const Cell> cells) {
    const auto checked=validate_irrigation_batch(cells);
    if(!checked.accepted || !checked.new_ditch_count)
        return {checked.accepted,false,checked.reason,command_sequence_,ticks_,checked.diagnostic};
    auto candidate=*this;
    for(const auto cell:cells) if(!has_ditch(*candidate.irrigation_,cell)) {
        const IrrigationDitch row{cell,ticks_,++candidate.command_sequence_};
        candidate.irrigation_->ditches.insert(ditch_at(candidate.irrigation_->ditches,cell),row);
        ++candidate.irrigation_->ditches_placed_total;
    }
    candidate.treasury_-=checked.total_cost;
    candidate.construction_spent_total_+=static_cast<std::uint64_t>(checked.total_cost);
    candidate.irrigation_->ditch_construction_spent+=static_cast<std::uint64_t>(checked.total_cost);
    candidate.road_revision_+=checked.new_ditch_count;candidate.cached_route_.reset();candidate.refresh_routes();
    candidate.rebuild_irrigation_cache();*this=std::move(candidate);
    return {true,true,"Paid irrigation ditch installed",command_sequence_,ticks_};
}
CommandResult World::validate_irrigation_command(Command command) const {
    CommandResult result{false,false,"Irrigation requires explicit Agriculture irrigation",command_sequence_,ticks_};
    if(!irrigation_enabled() || !irrigation_) return result;
    if(command.type==CommandType::PlaceIrrigationDitch) {
        const std::array<Cell,1> cells{{command.cell}};const auto batch=validate_irrigation_batch(cells);
        return {batch.accepted,batch.accepted && batch.new_ditch_count>0,batch.reason,command_sequence_,ticks_,batch.diagnostic};
    }
    if(command_sequence_>=UINT64_MAX-1) {result.reason="Irrigation command counter exhausted";return result;}
    if(command.type==CommandType::RemoveIrrigationDitch) {
        if((grower_navigation_cell_claimed(command.cell) || fishing_boat_cell_claimed(command.cell) || hunting_actor_cell_claimed(command.cell))) {
            result.reason="Ditch occupied by navigation experiment or its begun edge";return result;
        }
        if(!has_ditch(*irrigation_,command.cell)) {result.reason="Only an installed paid World ditch can be removed";return result;}
        if(irrigation_->ditches_removed_total==UINT64_MAX || road_revision_==UINT64_MAX || !irrigation_crossings_valid({},command.cell)) {
            result.reason="Ditch removal would invalidate a supported road crossing";return result;
        }
        return {true,true,"Ditch may be removed without refund",command_sequence_,ticks_};
    }
    if(command.type!=CommandType::PlaceIrrigationPump) {result.reason="Unsupported irrigation command";return result;}
    if(command.orientation>3) {result.reason="Pump requires a physical orientation in0..3";return result;}
    original::PumpGeometry shape;
    try {shape=original::pump_geometry(command.cell,command.orientation);} catch(const std::exception&) {
        result.reason="Pump geometry exceeds coordinate bounds";return result;
    }
    if(!pump_context(*this,shape)) {result.reason="Pump requires its complete empty shoreline body and eight intake-water cells";return result;}
    for(const auto cell:shape.footprint) if((grower_navigation_cell_claimed(cell) || fishing_boat_cell_claimed(cell) || hunting_actor_cell_claimed(cell))) {
        result.reason="Pump overlaps navigation experiment or its begun edge";return result;
    }
    if(buildings_.size()>=Rules::original_production_building_limit || !next_building_id_ || next_building_id_==UINT32_MAX ||
        road_revision_==UINT64_MAX) {result.reason="Pump paid building capacity or revision exhausted";return result;}
    for(const auto cell:shape.footprint) for(const auto& c:couriers_)
        if(c.role==CourierRole::OriginalPlantationGrower && c.phase!=CourierPhase::IdleAtWorkshop && !c.path.empty() &&
            (c.path.at(c.path_vertex)==cell || (c.edge_progress && c.path.at(c.path_vertex+1)==cell))) {
            result.reason="Pump construction overlaps a real worker or its begun edge";return result;
        }
    const auto cost=construction_cost(CommandType::PlaceIrrigationPump);
    if(cost<=0 || static_cast<std::uint64_t>(cost)>UINT64_MAX-construction_spent_total_) {
        result.reason="Pump construction spending exhausted";return result;
    }
    if(cost>treasury_) {result.reason="Not enough money for Pump construction";result.diagnostic={BuildBlocker::InsufficientFunds,command.cell,{}};return result;}
    return {true,true,"Pump shoreline and payment ready",command_sequence_,ticks_};
}
CommandResult World::execute_irrigation_command(Command command) {
    if(command.type==CommandType::PlaceIrrigationDitch) {
        const std::array<Cell,1> cells{{command.cell}};return execute_irrigation_batch(cells);
    }
    auto result=validate_irrigation_command(command);if(!result.accepted) return result;
    if(command.type==CommandType::RemoveIrrigationDitch) {
        auto candidate=*this;
        candidate.irrigation_->ditches.erase(ditch_at(candidate.irrigation_->ditches,command.cell));
        ++candidate.irrigation_->ditches_removed_total;++candidate.command_sequence_;++candidate.road_revision_;
        candidate.cached_route_.reset();candidate.refresh_routes();candidate.rebuild_irrigation_cache();*this=std::move(candidate);
        result.sequence=command_sequence_;result.reason="Ditch removed. No refund.";return result;
    }
    auto candidate=*this;const auto id=static_cast<BuildingId>(candidate.next_building_id_++);
    candidate.buildings_.push_back(BuildingState{id,Object::OriginalIrrigationPump,command.cell,true});
    candidate.buildings_.back().placed_tick=ticks_;
    for(const auto cell:original::pump_geometry(command.cell,command.orientation).footprint) {
        candidate.objects_[candidate.index(cell)]=Object::OriginalIrrigationPump;
        candidate.owners_[candidate.index(cell)]=static_cast<std::uint32_t>(id);
    }
    candidate.irrigation_->pumps.push_back({id,command.orientation,{}});
    const auto cost=construction_cost(CommandType::PlaceIrrigationPump);
    candidate.treasury_-=cost;candidate.construction_spent_total_+=static_cast<std::uint64_t>(cost);
    ++candidate.command_sequence_;++candidate.road_revision_;
    candidate.tick_staffed_.clear();candidate.tick_staffing_active_=false;candidate.cached_route_.reset();
    for(auto& c:candidate.couriers_) {
        if(c.role!=CourierRole::OriginalPlantationGrower || c.phase==CourierPhase::IdleAtWorkshop || c.route_pending) continue;
        for(std::size_t i=c.path_vertex+1;i<c.path.size();++i)
            if(!candidate.plantation_edge_allowed(c.path[i-1],c.path[i],c.owner,c.target)) {candidate.mark_route_pending(c);break;}
    }
    candidate.refresh_routes();candidate.refresh_irrigation();*this=std::move(candidate);
    result.sequence=command_sequence_;result.reason="Paid irrigation Pump installed";return result;
}
bool World::irrigation_pump_eligible(BuildingId id) const {
    if(!irrigation_enabled() || !irrigation_) return false;
    const auto row=pump_at(irrigation_->pumps,id);
    const auto b=std::lower_bound(buildings_.begin(),buildings_.end(),id,[](const auto& body,BuildingId key){return body.id<key;});
    return row!=irrigation_->pumps.end() && row->building_id==id && b!=buildings_.end() && b->id==id &&
        workers_assigned(id)>0 && pump_enabled_context(*this,*row,*b);
}
void World::refresh_irrigation() {
    if(!irrigation_enabled() || !irrigation_) return;
    bool changed=irrigation_wet_.size()!=objects_.size() || irrigation_supplier_.size()!=objects_.size() ||
        irrigation_cache_phase_tick_!=last_phase27(ticks_);
    // Use the existing sole allocation once. Its all-or-zero assignment is
    // the inherited authored workforce rule; no irrigation allocator exists.
    const auto assigned=irrigation_->pumps.empty() ? std::vector<bool>{}:workforce_allocation();
    for(auto& row:irrigation_->pumps) {
        const auto b=std::lower_bound(buildings_.begin(),buildings_.end(),row.building_id,
            [](const auto& body,BuildingId id){return body.id<id;});
        const bool eligible=b!=buildings_.end() && b->id==row.building_id && assigned[static_cast<std::size_t>(b-buildings_.begin())] &&
            workforce_required(b->id)>0 && pump_enabled_context(*this,row,*b);
        if(!eligible && row.eligible_since_tick) {row.eligible_since_tick.reset();changed=true;}
        else if(eligible && !row.eligible_since_tick && ticks_!=UINT64_MAX) {row.eligible_since_tick=ticks_+1;changed=true;}
    }
    if(changed) rebuild_irrigation_cache();
}
void World::rebuild_irrigation_cache() {
    irrigation_wet_.assign(objects_.size(),0);irrigation_supplier_.assign(objects_.size(),BuildingId{});
    irrigation_cache_phase_tick_=last_phase27(ticks_);
    if(!irrigation_enabled() || !irrigation_ || !irrigation_cache_phase_tick_) return;
    const auto phase=*irrigation_cache_phase_tick_;
    std::vector<Cell> queue;queue.reserve(irrigation_->ditches.size());
    const auto admit=[&](Cell cell,BuildingId supplier) {
        if(!in_bounds(cell) || irrigation_wet_[index(cell)]) return;
        const auto row=ditch_at(irrigation_->ditches,cell);
        if(row==irrigation_->ditches.end() || row->cell!=cell || row->placed_tick>=phase) return;
        irrigation_wet_[index(cell)]=1;irrigation_supplier_[index(cell)]=supplier;queue.push_back(cell);
    };
    for(const auto& row:irrigation_->pumps) {
        if(!row.eligible_since_tick || *row.eligible_since_tick>phase) continue;
        queue.clear();const auto& b=building(row.building_id);
        for(const auto cell:original::pump_geometry(b.cell,row.orientation).seeds) admit(cell,row.building_id);
        for(std::size_t cursor=0;cursor<queue.size();++cursor)
            for(const auto delta:cardinal) admit(neighbor(queue[cursor],delta),row.building_id);
    }
}
bool World::irrigation_ditch_wet(Cell cell) const {
    return irrigation_enabled() && in_bounds(cell) && irrigation_wet_.size()==objects_.size() && irrigation_wet_[index(cell)]!=0;
}
IrrigationFieldStatus World::irrigation_field_status(Cell field) const {
    IrrigationFieldStatus result;
    if(!irrigation_enabled() || !irrigation_ || !in_bounds(field)) {result.reason="Irrigation requires the explicit Agriculture irrigation contract";return result;}
    result.supported=true;
    for(int ring=1;ring<=5;++ring) for(int dy=-ring;dy<=ring;++dy) for(int dx=-ring;dx<=ring;++dx) {
        if(std::abs(dx)!=ring && std::abs(dy)!=ring) continue;
        const Cell at{field.x+dx,field.y+dy};
        if(irrigation_ditch_wet(at) && original::field_in_irrigation_range(field,at)) {
            result.water_available=true;result.wet_ditch=at;
            result.supplying_pump=irrigation_supplier_[index(at)];
            result.reason=playable_agriculture_enabled() ? "Water supplied by an active paid Pump; AUTHORED base yield has no irrigation bonus":
                "Water supplied by an active paid Pump; numeric crop effect remains locked";return result;
        }
    }
    result.reason="No currently supplied paid ditch within the observed five-cell query range";return result;
}
IrrigationPumpStatus World::irrigation_pump_status(BuildingId id) const {
    IrrigationPumpStatus result;
    if(!irrigation_enabled() || !irrigation_) {result.reason="Pump requires explicit Agriculture irrigation";return result;}
    const auto row=pump_at(irrigation_->pumps,id);
    if(row==irrigation_->pumps.end() || row->building_id!=id) {result.reason="No installed paid Pump";return result;}
    const auto& b=building(id);const auto shape=original::pump_geometry(b.cell,row->orientation);
    result.supported=true;result.placed=b.placed;result.orientation=row->orientation;
    result.footprint=shape.footprint;result.seeds=shape.seeds;result.intake_water=shape.water;
    result.intake_valid=pump_context(*this,shape,id);result.operating=b.operating_enabled;result.burning=building_on_fire(id);
    result.assigned_workers=workers_assigned(id);result.staffed=result.assigned_workers>0;
    result.eligible_since_tick=row->eligible_since_tick;result.last_phase_tick=last_phase27(ticks_);
    result.supplying=row->eligible_since_tick && result.last_phase_tick && *row->eligible_since_tick<=*result.last_phase_tick;
    result.reason=!result.intake_valid ? "Invalid complete shoreline/intake context":!result.operating ? "Pump paused":
        result.burning ? "Pump on fire":!result.staffed ? "Pump has no assigned workers":
        !result.supplying ? "Waiting for the next irrigation phase":"Pump supplies connected paid ditches";
    return result;
}
void World::validate_irrigation_state() const {
    if(!irrigation_enabled()) {if(irrigation_) invalid("Historical World cannot own irrigation state");return;}
    validate_irrigation_snapshot(snapshot());
    if(!irrigation_ || !original_terrain_facts_ || !original_terrain_facts_->irrigation_complete())
        invalid("Irrigation restore lacks complete immutable source context");
    const auto unit=construction_cost(CommandType::PlaceIrrigationDitch);
    if(unit<=0 || irrigation_->ditches_placed_total>UINT64_MAX/static_cast<std::uint64_t>(unit) ||
        irrigation_->ditch_construction_spent!=irrigation_->ditches_placed_total*static_cast<std::uint64_t>(unit))
        invalid("Irrigation ditch history disagrees with exact model/difficulty construction cost");
    for(const auto& row:irrigation_->ditches) if(!ditch_context(*this,row.cell))
        invalid("Restored ditch conflicts with source terrain, physical occupancy or infrastructure");
    if(!irrigation_crossings_valid()) invalid("Restored ditch has an unsupported road crossing");
    for(const auto& row:irrigation_->pumps) {
        const auto& b=building(row.building_id);
        if(!pump_context(*this,original::pump_geometry(b.cell,row.orientation),b.id) ||
            irrigation_pump_eligible(b.id)!=row.eligible_since_tick.has_value())
            invalid("Restored Pump intake or current supply epoch contradicts World eligibility");
    }
}

} // namespace openemperor::simulation
