#include "simulation/CityStartGuidance.h"

#include <algorithm>
#include <array>
#include <limits>

namespace openemperor::simulation {
namespace {

constexpr std::array<Object,6> supply_kinds{
    Object::ClaySource,Object::Pottery,Object::Warehouse,
    Object::Farm,Object::Market,Object::ServicePost};

std::optional<CommandType> command_for(Object kind) {
    switch (kind) {
    case Object::ClaySource: return CommandType::PlaceClaySource;
    case Object::Pottery: return CommandType::PlacePottery;
    case Object::Warehouse: return CommandType::PlaceWarehouse;
    case Object::Farm: return CommandType::PlaceFarm;
    case Object::Market: return CommandType::PlaceMarket;
    case Object::ServicePost: return CommandType::PlaceServicePost;
    default: return std::nullopt;
    }
}

bool has_kind(const World& world,Object kind) {
    return std::ranges::any_of(world.buildings(),[&](const BuildingState& building) {
        return building.placed && building.kind==kind;
    });
}

StarterSupplyCondition facility_condition(const World& world,Object kind) {
    bool found=false,enabled=false,burning=false,staffed=false,no_road=false,awaiting=false,active=false;
    for (const auto& building:world.buildings()) {
        if (!building.placed || building.kind!=kind) continue;
        found=true;
        if (world.operation_controls_supported() && World::operation_controllable(building.kind) &&
            !building.operating_enabled) continue;
        enabled=true;
        if (world.building_on_fire(building.id)) { burning=true; continue; }
        if (!world.building_staffed(building.id)) continue;
        staffed=true;
        bool owns_courier=false;
        for (const auto& courier:world.couriers()) {
            if (!courier.enabled || courier.owner!=building.id) continue;
            owns_courier=true;
            const auto status=world.courier_dispatch_status(courier.id).status;
            no_road=no_road || status==CourierDispatchStatus::NoRoad ||
                status==CourierDispatchStatus::NoTarget ||
                status==CourierDispatchStatus::WaitingForRoadRevision;
            awaiting=awaiting || status==CourierDispatchStatus::NoStock;
            active=active || status==CourierDispatchStatus::Ready ||
                status==CourierDispatchStatus::AlreadyMoving ||
                status==CourierDispatchStatus::TargetFull;
        }
        if (!owns_courier) {
            if (kind==Object::Pottery)
                awaiting=awaiting || building.output==0;
            else active=true;
        }
    }
    if (!found) return StarterSupplyCondition::MissingBuilding;
    if (!enabled) return StarterSupplyCondition::Paused;
    if (!staffed && burning) return StarterSupplyCondition::OnFire;
    if (!staffed) return StarterSupplyCondition::Unstaffed;
    if (no_road && !active) return StarterSupplyCondition::NoReachableTarget;
    if (awaiting && !active) return StarterSupplyCondition::AwaitingGoods;
    return StarterSupplyCondition::DeliveringOrReady;
}

StarterFacilityInstance facility_instance(const World& world,const BuildingState& building) {
    StarterFacilityInstance result;
    result.id=building.id;
    result.kind=building.kind;
    result.workers_required=world.workforce_required(building.id);
    result.workers_assigned=world.workers_assigned(building.id);
    result.operating_enabled=building.operating_enabled;
    result.burning=world.building_on_fire(building.id);
    bool owns_courier=false,no_road=false,awaiting=false,active=false;
    for (const auto& courier:world.couriers()) {
        if (!courier.enabled || courier.owner!=building.id) continue;
        owns_courier=true;
        result.delivery_active=result.delivery_active ||
            courier.phase!=CourierPhase::IdleAtWorkshop;
        // Current staffing and operation decide the facility condition below.
        // A previously dispatched trip remains a separate fact, even on pause.
        const auto status=world.courier_dispatch_status(courier.id).status;
        no_road=no_road || status==CourierDispatchStatus::NoRoad ||
            status==CourierDispatchStatus::NoTarget ||
            status==CourierDispatchStatus::WaitingForRoadRevision;
        awaiting=awaiting || status==CourierDispatchStatus::NoStock;
        active=active || status==CourierDispatchStatus::Ready ||
            status==CourierDispatchStatus::AlreadyMoving ||
            status==CourierDispatchStatus::TargetFull;
    }
    if (world.operation_controls_supported() && World::operation_controllable(building.kind) &&
        !result.operating_enabled) result.condition=StarterSupplyCondition::Paused;
    else if (result.burning) result.condition=StarterSupplyCondition::OnFire;
    else if (!world.building_staffed(building.id)) result.condition=StarterSupplyCondition::Unstaffed;
    else {
        if (!owns_courier) {
            if (building.kind==Object::Pottery) awaiting=building.output==0;
            else active=true;
        }
        result.condition=no_road && !active ? StarterSupplyCondition::NoReachableTarget:
            awaiting && !active ? StarterSupplyCondition::AwaitingGoods:
            StarterSupplyCondition::DeliveringOrReady;
    }
    return result;
}

std::optional<std::int64_t> funds_gap(std::int64_t cost,std::int64_t funds) {
    if (funds>=cost) return 0;
    if (funds<0 && cost>INT64_MAX+funds) return std::nullopt;
    return cost-funds;
}

StarterBudgetWarning budget_warning(CityStartGuidance status,
                                    std::int64_t purchase_cost,std::int64_t after) {
    StarterBudgetWarning result;
    result.purchase_cost=purchase_cost;
    result.funds_after_purchase=after;
    result.minimum_remaining_building_funds=status.minimum_missing_building_funds;
    result.minimum_remaining_house_funds=status.minimum_house_funds_for_starter;
    result.minimum_remaining_start_cost=result.minimum_remaining_building_funds+
        result.minimum_remaining_house_funds;
    result.additional_houses_needed=status.houses_needed_for_shortfall;
    result.household_slots_remaining=status.household_slots_remaining;
    result.starter_workforce_within_house_limit=status.starter_workforce_within_house_limit;
    result.remaining_missing_supply_buildings=std::move(status.missing_supply_buildings);
    result.missing_supply_costs=std::move(status.missing_supply_costs);
    result.missing_building_funds_gap=funds_gap(result.minimum_remaining_building_funds,after);
    result.maintenance=status.maintenance;
    // Road purchases have no hypothetical World and no ownership-cost change.
    // The same arithmetic also preserves the concrete purchase's derived costs.
    result.maintenance.funds_after_interval.reset();
    if (result.maintenance.interval_cost && after>=INT64_MIN+*result.maintenance.interval_cost)
        result.maintenance.funds_after_interval=after-*result.maintenance.interval_cost;
    return result;
}

std::size_t house_limit(const World& world) {
    return scalable_profile(world.profile()) ?
        Rules::city_v10_household_limit:household_limit;
}

std::vector<std::uint8_t> copy_buildable_mask(const World& world) {
    std::vector<std::uint8_t> result;
    for (int y=0;y<world.height();++y)
        for (int x=0;x<world.width();++x)
            result.push_back(world.buildable({x,y}) ? 1U:0U);
    return result;
}

} // namespace

const char* starter_building_name(Object kind) {
    switch (kind) {
    case Object::ClaySource: return "Clay Source";
    case Object::Pottery: return "Pottery";
    case Object::Warehouse: return "Warehouse";
    case Object::Farm: return "Farm";
    case Object::Market: return "Market";
    case Object::ServicePost: return "Service Post";
    case Object::Household: return "House";
    case Object::Well: return "Well";
    case Object::HealthPost: return "Health Post";
    case Object::FireWatch: return "Fire Watch";
    default: return "Building";
    }
}

CityStartGuidance inspect_city_start(const World& world) {
    CityStartGuidance result;
    result.applicable=market_profile(world.profile());
    if (!result.applicable) return result;
    result.funds_current=world.treasury();
    result.taxes_received_total=world.taxes_collected_total();
    result.maintenance=world.maintenance_projection();
    result.workforce_supply=world.workforce_supply();
    result.workforce_required_now=world.active_workforce_required();
    result.workforce_active_demand=world.active_workforce_required();
    result.workforce_installed_demand=world.workforce_required();
    result.workforce_used=world.workforce_used();
    result.workforce_required_for_starter=result.workforce_installed_demand;
    for (const auto kind:supply_kinds) {
        const bool present=has_kind(world,kind);
        result.facilities.push_back({kind,facility_condition(world,kind)});
        if (present) continue;
        result.missing_supply_buildings.push_back(kind);
        const auto command=command_for(kind);
        if (command) {
            const auto cost=world.construction_cost(*command);
            result.missing_supply_costs.push_back({kind,cost});
            result.minimum_missing_building_funds+=cost;
            result.workforce_required_for_starter+=kind==Object::ClaySource ? Rules::clay_source_workers:
                kind==Object::Pottery ? Rules::pottery_workers:
                kind==Object::Warehouse ? Rules::warehouse_workers:
                kind==Object::Farm ? Rules::farm_workers:
                kind==Object::Market ? Rules::market_workers:Rules::service_post_workers;
        }
    }
    for (const auto& building:world.buildings()) {
        if (building.placed && building.kind!=Object::Household && building.kind!=Object::Well)
            result.facility_instances.push_back(facility_instance(world,building));
        if (!building.placed || building.kind!=Object::Household) continue;
        ++result.household_count;
        const auto demand=world.household_demand_status(building.id);
        const auto remaining=building.demand_progress>=Rules::household_demand_ticks ? 0U:
            static_cast<unsigned>(Rules::household_demand_ticks-building.demand_progress);
        result.households.push_back({building.id,demand.pottery_available,demand.food_available,
            demand.service_available,remaining,
            world.household_move_in_grace_remaining(building.id),building.last_demand_status,
            world.household_tax_contributed(building.id),demand.burning,demand.sick,demand.ready,
            world.household_service_remaining(building.id),
            building.reserved_incoming,building.reserved_food_incoming});
        result.households_ready_now+=demand.ready;
        result.households_burning+=demand.burning;
        result.households_sick+=demand.sick;
        result.households_missing_pottery+=!demand.pottery_available;
        result.households_missing_food+=demand.food_required && !demand.food_available;
        result.households_missing_service+=demand.service_required && !demand.service_available;
    }
    const auto limit=house_limit(world);
    result.household_slots_remaining=result.household_count<limit ? limit-result.household_count:0;
    result.complete_supply_chain=result.missing_supply_buildings.empty() &&
        result.household_count>0;
    result.taxes_have_been_collected=world.taxes_collected_total()>0;
    result.starter_workforce_shortfall=std::max(0,result.workforce_required_for_starter-
                                                    result.workforce_supply);
    if (result.starter_workforce_shortfall>0) {
        const auto per_house=static_cast<std::size_t>(Rules::household_initial_population);
        const auto desired=(static_cast<std::size_t>(result.starter_workforce_shortfall)+
                            per_house-1U)/per_house;
        result.houses_needed_for_shortfall=desired;
        result.starter_workforce_within_house_limit=desired<=result.household_slots_remaining;
        result.suggested_additional_houses=std::min(desired,result.household_slots_remaining);
        const auto house_cost=world.construction_cost(CommandType::PlaceHousehold);
        result.minimum_house_funds_for_starter=static_cast<std::int64_t>(desired)*house_cost;
        result.suggested_house_cost=static_cast<std::int64_t>(result.suggested_additional_houses)*
            house_cost;
        result.affordable_suggested_houses=house_cost>0 ? std::min(result.suggested_additional_houses,
            static_cast<std::size_t>(std::max<std::int64_t>(0,world.treasury())/house_cost)):0;
        const auto funds=world.treasury();
        // City-v16 debt may be INT64_MIN. This display-only shortfall saturates
        // when it cannot fit, without changing the construction reserve.
        result.suggested_house_funds_missing=funds<0 &&
            result.suggested_house_cost>std::numeric_limits<std::int64_t>::max()+funds ?
            std::numeric_limits<std::int64_t>::max():
            std::max<std::int64_t>(0,result.suggested_house_cost-funds);
    }
    result.missing_building_funds_gap=funds_gap(result.minimum_missing_building_funds,
                                               world.treasury());
    result.construction_funds_gap=funds_gap(result.minimum_missing_building_funds+
        result.minimum_house_funds_for_starter,world.treasury());
    return result;
}

std::optional<StarterBudgetWarning> starter_budget_warning(const World& world,Command command) {
    const std::array commands{command};
    return starter_budget_warning(world,std::span<const Command>(commands));
}

std::optional<StarterBudgetWarning> starter_budget_warning(
    const World& world,std::span<const Command> commands) {
    if (!market_profile(world.profile()) ||
        (world.profile()==RulesProfile::CityV11 && world.rule_version()<2) ||
        world.taxes_collected_total()>0) return std::nullopt;
    const auto snapshot=world.snapshot();
    auto hypothetical=world.map_permissions() ?
        World::restore(snapshot,world.map_permissions()):
        World::restore(snapshot,copy_buildable_mask(world));
    for (const auto command:commands) {
        const auto result=hypothetical.execute(command);
        if (!result.accepted) return std::nullopt;
    }
    if (hypothetical.treasury()>world.treasury()) return std::nullopt;
    const auto purchase_cost=world.treasury()-hypothetical.treasury();
    if (purchase_cost<=0 || purchase_cost>world.treasury()) return std::nullopt;
    auto status=inspect_city_start(hypothetical);
    const auto building_reserve=status.minimum_missing_building_funds;
    const auto house_reserve=status.minimum_house_funds_for_starter;
    if (building_reserve<0 || house_reserve<0 ||
        house_reserve>std::numeric_limits<std::int64_t>::max()-building_reserve)
        return std::nullopt;
    const auto reserve=building_reserve+house_reserve;
    const auto after=hypothetical.treasury();
    if (status.starter_workforce_within_house_limit && after>=reserve)
        return std::nullopt;
    return budget_warning(std::move(status),purchase_cost,after);
}

std::optional<StarterBudgetWarning> starter_budget_warning_for_road_purchase(
    const World& world,std::int64_t purchase_cost) {
    if (!market_profile(world.profile()) ||
        (world.profile()==RulesProfile::CityV11 && world.rule_version()<2) ||
        world.taxes_collected_total()>0 || purchase_cost<=0 || purchase_cost>world.treasury())
        return std::nullopt;
    auto status=inspect_city_start(world);
    const auto building_reserve=status.minimum_missing_building_funds;
    const auto house_reserve=status.minimum_house_funds_for_starter;
    if (building_reserve<0 || house_reserve<0 ||
        house_reserve>std::numeric_limits<std::int64_t>::max()-building_reserve)
        return std::nullopt;
    const auto reserve=building_reserve+house_reserve;
    const auto after=world.treasury()-purchase_cost;
    if (status.starter_workforce_within_house_limit && after>=reserve) return std::nullopt;
    return budget_warning(std::move(status),purchase_cost,after);
}

} // namespace openemperor::simulation
