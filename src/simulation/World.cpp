#include "simulation/World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace openemperor::simulation {
namespace {
constexpr std::array<Cell,4> neighbors{{{0,-1},{-1,0},{1,0},{0,1}}};
Cell add(Cell a,Cell b) { return {a.x+b.x,a.y+b.y}; }
}

const char* courier_phase_name(CourierPhase phase) {
    switch (phase) {
    case CourierPhase::IdleAtWorkshop: return "At workshop";
    case CourierPhase::ToWarehouse: return "To warehouse";
    case CourierPhase::Returning: return "Returning";
    }
    return "Unknown";
}
const char* delivery_phase_name(CourierPhase phase) {
    switch (phase) {
    case CourierPhase::IdleAtWorkshop: return "Idle at source";
    case CourierPhase::ToWarehouse: return "Delivering";
    case CourierPhase::Returning: return "Returning";
    }
    return "Unknown";
}
const char* courier_dispatch_status_name(CourierDispatchStatus status) {
    switch (status) {
    case CourierDispatchStatus::Ready: return "Ready";
    case CourierDispatchStatus::NoStock: return "No stock";
    case CourierDispatchStatus::NoRoad: return "No road connection";
    case CourierDispatchStatus::TargetFull: return "Target buffer full";
    case CourierDispatchStatus::NoTarget: return "No target building";
    case CourierDispatchStatus::AlreadyMoving: return "Already moving";
    case CourierDispatchStatus::WaitingForRoadRevision: return "Waiting for road revision";
    case CourierDispatchStatus::Unstaffed: return "Building unstaffed";
    case CourierDispatchStatus::Disabled: return "Disabled";
    }
    return "Unknown";
}
const char* rules_profile_name(RulesProfile profile) {
    switch (profile) {
    case RulesProfile::LogisticsV1: return profile_name;
    case RulesProfile::ProductionV2: return production_profile_name;
    case RulesProfile::HouseholdV3: return household_profile_name;
    case RulesProfile::SettlementV4: return settlement_profile_name;
    case RulesProfile::IndustryV5: return industry_profile_name;
    case RulesProfile::CityV6: return city_profile_name;
    }
    return "unknown";
}

World::World(int width,int height,std::vector<std::uint8_t> buildable,RulesProfile profile)
    : width_(width),height_(height),profile_(profile),buildable_(std::move(buildable)) {
    if (width<=0 || height<=0 || width>512 || height>512 ||
        buildable_.size()!=static_cast<std::size_t>(width)*static_cast<std::size_t>(height))
        throw std::invalid_argument("invalid bounded sandbox grid");
    if (profile!=RulesProfile::LogisticsV1 && profile!=RulesProfile::ProductionV2 &&
        profile!=RulesProfile::HouseholdV3 && profile!=RulesProfile::SettlementV4 &&
        profile!=RulesProfile::IndustryV5 && profile!=RulesProfile::CityV6)
        throw std::invalid_argument("unknown sandbox rules profile");
    treasury_=profile==RulesProfile::CityV6 ? Rules::starting_treasury:0;
    objects_.assign(buildable_.size(),Object::Empty);
    owners_.assign(buildable_.size(),0);
    buildings_[0].id=BuildingId::ClaySource;
    buildings_[1].id=BuildingId::Pottery;
    buildings_[2].id=BuildingId::Warehouse;
    buildings_[3].id=BuildingId::Household;
    for (std::size_t i=4;i<buildings_.size();++i)
        buildings_[i].id=static_cast<BuildingId>(i+1);
    couriers_[0].id=CourierId::Clay;
    couriers_[0].role=CourierRole::Clay;
    couriers_[0].owner=BuildingId::ClaySource;
    couriers_[0].target=BuildingId::Pottery;
    couriers_[0].good=Good::Clay;
    couriers_[1].id=CourierId::Pottery;
    couriers_[1].role=CourierRole::Pottery;
    couriers_[1].owner=BuildingId::Pottery;
    couriers_[1].target=BuildingId::Warehouse;
    couriers_[1].good=Good::Pottery;
    couriers_[2].id=CourierId::Household;
    couriers_[2].role=CourierRole::Household;
    couriers_[2].owner=BuildingId::Warehouse;
    couriers_[2].target=BuildingId::Household;
    couriers_[2].good=Good::Pottery;
    for (std::size_t i=3;i<couriers_.size();++i) {
        couriers_[i].id=static_cast<CourierId>(i+1);
        couriers_[i].owner=static_cast<BuildingId>(i+5);
    }
}
bool World::in_bounds(Cell cell) const {
    return cell.x>=0 && cell.y>=0 && cell.x<width_ && cell.y<height_;
}
std::size_t World::index(Cell cell) const {
    return static_cast<std::size_t>(cell.y)*static_cast<std::size_t>(width_)+
           static_cast<std::size_t>(cell.x);
}
bool World::buildable(Cell cell) const { return in_bounds(cell) && buildable_[index(cell)]!=0; }
Object World::object_at(Cell cell) const {
    return in_bounds(cell) ? objects_[index(cell)] : Object::Empty;
}
std::optional<BuildingId> World::building_owner_at(Cell cell) const {
    if (!in_bounds(cell)) return std::nullopt;
    const auto owner=owners_[index(cell)];
    if (owner==0) return std::nullopt;
    return static_cast<BuildingId>(owner);
}
const BuildingState& World::building(BuildingId id) const {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>9) throw std::out_of_range("invalid building ID");
    return buildings_[value-1];
}
BuildingState& World::mutable_building(BuildingId id) {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>9) throw std::out_of_range("invalid building ID");
    return buildings_[value-1];
}
const CourierState& World::courier(CourierId id) const {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>5) throw std::out_of_range("invalid courier ID");
    return couriers_[value-1];
}
CourierState& World::mutable_courier(CourierId id) {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>5) throw std::out_of_range("invalid courier ID");
    return couriers_[value-1];
}

std::int64_t World::construction_cost(CommandType type) const {
    if (profile_!=RulesProfile::CityV6) return 0;
    switch (type) {
    case CommandType::PlaceRoad: return Rules::road_cost;
    case CommandType::PlaceClaySource: return Rules::clay_source_cost;
    case CommandType::PlacePottery: return Rules::pottery_cost;
    case CommandType::PlaceWarehouse: return Rules::warehouse_cost;
    case CommandType::PlaceHousehold: return Rules::household_cost;
    case CommandType::PlaceWorkshop:
    case CommandType::RemoveRoad: return 0;
    }
    return 0;
}

int World::workforce_required(BuildingId id) const {
    const auto& value=building(id);
    if (!value.placed) return 0;
    switch (value.kind) {
    case Object::ClaySource: return Rules::clay_source_workers;
    case Object::Pottery: return Rules::pottery_workers;
    case Object::Warehouse: return Rules::warehouse_workers;
    default: return 0;
    }
}

int World::workforce_supply() const {
    if (profile_!=RulesProfile::CityV6) return 0;
    int households=0;
    for (unsigned id=first_household_id;id<household_id_end;++id)
        if (building(static_cast<BuildingId>(id)).placed) ++households;
    return households*Rules::household_workers;
}

int World::workforce_required() const {
    if (profile_!=RulesProfile::CityV6) return 0;
    int total=0;
    for (unsigned id=1;id<=9;++id) total+=workforce_required(static_cast<BuildingId>(id));
    return total;
}

bool World::building_staffed(BuildingId id) const {
    if (profile_!=RulesProfile::CityV6) return true;
    const auto requested=static_cast<unsigned>(id);
    int available=workforce_supply();
    for (unsigned value=1;value<=9;++value) {
        const int need=workforce_required(static_cast<BuildingId>(value));
        const bool staffed=need==0 || available>=need;
        if (need!=0 && staffed) available-=need;
        if (value==requested) return staffed;
    }
    throw std::out_of_range("invalid building ID");
}

int World::workforce_used() const {
    if (profile_!=RulesProfile::CityV6) return 0;
    int used=0;
    for (unsigned id=1;id<=9;++id) {
        const auto building_id=static_cast<BuildingId>(id);
        if (building_staffed(building_id)) used+=workforce_required(building_id);
    }
    return used;
}

int World::settlement_goal_households_ready() const {
    if (profile_!=RulesProfile::CityV6) return 0;
    int ready=0;
    for (unsigned id=first_household_id;id<household_id_end;++id) {
        const auto& home=building(static_cast<BuildingId>(id));
        if (home.placed && home.fulfilled_demand>=Rules::settlement_goal_fulfilled_demands) ++ready;
    }
    return ready;
}

bool World::settlement_goal_reached() const {
    return profile_==RulesProfile::CityV6 && next_household_id_==household_id_end &&
        settlement_goal_households_ready()==household_limit;
}

bool World::city_economy_valid() const {
    if (profile_!=RulesProfile::CityV6)
        return treasury_==0 && taxes_collected_total_==0 && construction_spent_total_==0;
    if (treasury_<0 || taxes_collected_total_>
        static_cast<std::uint64_t>(INT64_MAX-Rules::starting_treasury)) return false;
    std::uint64_t fulfilled=0;
    for (unsigned id=first_household_id;id<household_id_end;++id) {
        const auto count=building(static_cast<BuildingId>(id)).fulfilled_demand;
        if (fulfilled>UINT64_MAX-count) return false;
        fulfilled+=count;
    }
    const auto tax_rate=static_cast<std::uint64_t>(Rules::tax_income_per_fulfilled_demand);
    if (fulfilled>UINT64_MAX/tax_rate || taxes_collected_total_!=fulfilled*tax_rate) return false;
    std::uint64_t expected_spending=0;
    const auto add_cost=[&](std::uint64_t count,std::uint64_t cost) {
        if (count>UINT64_MAX/cost || expected_spending>UINT64_MAX-count*cost) return false;
        expected_spending+=count*cost;
        return true;
    };
    if (!add_cost(roads_placed_total_,static_cast<std::uint64_t>(Rules::road_cost))) return false;
    for (const auto& value:buildings_) if (value.placed) {
        const auto cost=value.kind==Object::ClaySource ? Rules::clay_source_cost:
            value.kind==Object::Pottery ? Rules::pottery_cost:
            value.kind==Object::Warehouse ? Rules::warehouse_cost:
            value.kind==Object::Household ? Rules::household_cost:0;
        if (cost<=0 || !add_cost(1,static_cast<std::uint64_t>(cost))) return false;
    }
    if (construction_spent_total_!=expected_spending) return false;
    const auto available=static_cast<std::uint64_t>(Rules::starting_treasury)+taxes_collected_total_;
    if (construction_spent_total_>available ||
        treasury_!=static_cast<std::int64_t>(available-construction_spent_total_) ||
        workforce_used()>workforce_supply() || workforce_used()>workforce_required()) return false;
    return true;
}
CommandResult World::validate(Command command) const {
    if (command_sequence_==UINT64_MAX) throw std::overflow_error("sandbox command counter exhausted");
    CommandResult result{false,false,"",command_sequence_+1,ticks_};
    if (command.type!=CommandType::PlaceRoad && command.type!=CommandType::PlaceWorkshop &&
        command.type!=CommandType::PlaceWarehouse && command.type!=CommandType::PlaceClaySource &&
        command.type!=CommandType::PlacePottery && command.type!=CommandType::RemoveRoad &&
        command.type!=CommandType::PlaceHousehold) {
        result.reason="Unknown placement command"; return result;
    }
    if ((profile_==RulesProfile::LogisticsV1 &&
        (command.type==CommandType::PlaceClaySource || command.type==CommandType::PlacePottery ||
         command.type==CommandType::RemoveRoad || command.type==CommandType::PlaceHousehold)) ||
        (production_profile(profile_) && command.type==CommandType::PlaceWorkshop) ||
        (profile_==RulesProfile::ProductionV2 && command.type==CommandType::PlaceHousehold)) {
        result.reason="Command unavailable in selected rules profile"; return result;
    }
    if (!in_bounds(command.cell)) { result.reason="Outside sandbox grid"; return result; }
    if (command.type==CommandType::RemoveRoad) {
        if (object_at(command.cell)!=Object::Road) {
            result.reason="No sandbox road at cell"; return result;
        }
        for (const auto& courier:couriers_) {
            if (courier.phase==CourierPhase::IdleAtWorkshop) continue;
            const auto current=courier.path.at(courier.path_vertex);
            if (current==command.cell || (courier.edge_progress>0 &&
                courier.path.at(courier.path_vertex+1)==command.cell)) {
                result.reason="Road occupied by courier"; return result;
            }
        }
        if (road_revision_==UINT64_MAX || roads_removed_total_==UINT64_MAX) {
            result.reason="Road counter exhausted"; return result;
        }
        return {true,true,"Road removed",result.sequence,ticks_};
    }
    if (!buildable(command.cell)) { result.reason="Not sandbox-buildable"; return result; }
    const auto occupied=object_at(command.cell);
    if (occupied!=Object::Empty) {
        if (occupied==Object::Road && command.type==CommandType::PlaceRoad)
            return {true,false,"Road already present",result.sequence,ticks_};
        result.reason="Cell already occupied";
        return result;
    }
    if (command.type==CommandType::PlaceWorkshop && workshop_) {
        result.reason="Workshop already exists"; return result;
    }
    if (command.type==CommandType::PlaceWarehouse && warehouse_) {
        result.reason="Warehouse already exists"; return result;
    }
    if (command.type==CommandType::PlaceWarehouse && production_profile(profile_) &&
        building(BuildingId::Warehouse).placed) {
        result.reason="Warehouse already exists"; return result;
    }
    if (command.type==CommandType::PlaceClaySource || command.type==CommandType::PlacePottery) {
        const Object kind=command.type==CommandType::PlaceClaySource ?
            Object::ClaySource:Object::Pottery;
        unsigned count=0;
        for (const auto& b:buildings_) if (b.placed && b.kind==kind) ++count;
        if (count>=(industry_profile(profile_) ? 2U:1U)) {
            result.reason=kind==Object::ClaySource ? "Clay source limit reached":
                "Pottery limit reached"; return result;
        }
        if (count>0 && (next_production_id_>=10 || next_courier_id_>=6)) {
            result.reason="Production ID limit reached"; return result;
        }
    }
    if (command.type==CommandType::PlaceHousehold &&
        (profile_==RulesProfile::HouseholdV3 ? building(BuildingId::Household).placed :
         next_household_id_>=household_id_end)) {
        result.reason=profile_==RulesProfile::SettlementV4 ?
            "Four-household limit reached":"Household already exists"; return result;
    }
    if (road_revision_==UINT64_MAX ||
        (command.type==CommandType::PlaceRoad && roads_placed_total_==UINT64_MAX)) {
        result.reason="Road counter exhausted"; return result;
    }
    const auto cost=construction_cost(command.type);
    if (cost>treasury_) { result.reason="Not enough money"; return result; }
    if (cost>0 && construction_spent_total_>UINT64_MAX-static_cast<std::uint64_t>(cost)) {
        result.reason="Construction spending counter exhausted"; return result;
    }
    return {true,true,"Placed",result.sequence,ticks_};
}
CommandResult World::execute(Command command) {
    auto result=validate(command);
    if (profile_==RulesProfile::CityV6 && !result.accepted) return result;
    ++command_sequence_;
    result.sequence=command_sequence_;
    if (!result.accepted || !result.changed) return result;
    switch (command.type) {
    case CommandType::PlaceRoad:
        objects_[index(command.cell)]=Object::Road;
        ++roads_placed_total_;
        break;
    case CommandType::RemoveRoad:
        objects_[index(command.cell)]=Object::Empty;
        ++roads_removed_total_;
        for (auto& courier:couriers_) {
            if (courier.phase==CourierPhase::IdleAtWorkshop || courier.route_pending) continue;
            // Historical points are harmless. Only a removed future point
            // invalidates a route that has not yet been traversed.
            for (std::size_t i=courier.path_vertex+1;i<courier.path.size();++i)
                if (courier.path[i]==command.cell) { mark_route_pending(courier); break; }
        }
        break;
    case CommandType::PlaceWorkshop:
        objects_[index(command.cell)]=Object::Workshop;
        workshop_=command.cell;
        break;
    case CommandType::PlaceWarehouse:
        objects_[index(command.cell)]=Object::Warehouse;
        if (production_profile(profile_)) {
            auto& b=mutable_building(BuildingId::Warehouse);
            b.placed=true; b.kind=Object::Warehouse; b.cell=command.cell;
            owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        } else warehouse_=command.cell;
        break;
    case CommandType::PlaceClaySource: {
        objects_[index(command.cell)]=Object::ClaySource;
        const auto id=building(BuildingId::ClaySource).placed ?
            static_cast<BuildingId>(next_production_id_++):BuildingId::ClaySource;
        auto& b=mutable_building(id);
        b.placed=true; b.kind=Object::ClaySource; b.cell=command.cell;
        if (industry_profile(profile_)) b.placed_tick=ticks_;
        owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        auto& courier=mutable_courier(id==BuildingId::ClaySource ? CourierId::Clay:
            static_cast<CourierId>(next_courier_id_++));
        courier.role=CourierRole::Clay; courier.owner=id; courier.target=BuildingId::Pottery;
        courier.good=Good::Clay; courier.enabled=true;
        break;
    }
    case CommandType::PlacePottery: {
        objects_[index(command.cell)]=Object::Pottery;
        const auto id=building(BuildingId::Pottery).placed ?
            static_cast<BuildingId>(next_production_id_++):BuildingId::Pottery;
        auto& b=mutable_building(id);
        b.placed=true; b.kind=Object::Pottery; b.cell=command.cell;
        if (industry_profile(profile_)) b.placed_tick=ticks_;
        owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        auto& courier=mutable_courier(id==BuildingId::Pottery ? CourierId::Pottery:
            static_cast<CourierId>(next_courier_id_++));
        courier.role=CourierRole::Pottery; courier.owner=id; courier.target=BuildingId::Warehouse;
        courier.good=Good::Pottery; courier.enabled=true;
        break;
    }
    case CommandType::PlaceHousehold: {
        objects_[index(command.cell)]=Object::Household;
        const auto id=static_cast<BuildingId>(next_household_id_);
        auto& b=mutable_building(id);
        b.placed=true; b.kind=Object::Household; b.cell=command.cell; b.placed_tick=ticks_;
        owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        mutable_courier(CourierId::Household).enabled=true;
        ++next_household_id_;
        break;
    }
    }
    if (profile_==RulesProfile::CityV6) {
        const auto cost=construction_cost(command.type);
        treasury_-=cost;
        construction_spent_total_+=static_cast<std::uint64_t>(cost);
    }
    ++road_revision_;
    cached_route_.reset();
    if (production_profile(profile_)) refresh_routes();
    return result;
}

std::optional<std::vector<Cell>> World::find_route() const {
    if (!workshop_ || !warehouse_) return std::nullopt;
    return find_route(*workshop_,*warehouse_);
}

std::optional<std::vector<Cell>> World::find_route(Cell start,Cell goal) const {
    if (!in_bounds(start) || !in_bounds(goal) || start==goal) return std::nullopt;
    const auto start_kind=object_at(start),goal_kind=object_at(goal);
    if (start_kind==Object::Empty || goal_kind==Object::Empty ||
        goal_kind==Object::Road) return std::nullopt;
    const auto start_index=index(start);
    std::vector<std::size_t> previous(objects_.size(),objects_.size());
    std::deque<Cell> queue;
    previous[start_index]=start_index;
    queue.push_back(start);
    while (!queue.empty()) {
        const auto current=queue.front(); queue.pop_front();
        for (const auto delta:neighbors) {
            const auto next=add(current,delta);
            if (!in_bounds(next) || previous[index(next)]!=objects_.size()) continue;
            const auto kind=object_at(next);
            if (next==goal) {
                if (current==start && start_kind!=Object::Road) continue;
                // New dispatches require an intervening road; a courier
                // already on a road may finish with one adjacent edge.
                previous[index(next)]=index(current);
                std::vector<Cell> route;
                auto at=index(next);
                while (at!=start_index) {
                    route.push_back({static_cast<int>(at%static_cast<std::size_t>(width_)),
                                     static_cast<int>(at/static_cast<std::size_t>(width_))});
                    at=previous[at];
                }
                route.push_back(start);
                std::reverse(route.begin(),route.end());
                return route;
            }
            if (kind!=Object::Road) continue;
            previous[index(next)]=index(current);
            queue.push_back(next);
        }
    }
    return std::nullopt;
}

const std::vector<Cell>* World::route_for_revision() {
    if (cached_revision_!=road_revision_) {
        cached_route_=find_route();
        cached_revision_=road_revision_;
    }
    return cached_route_ ? &*cached_route_ : nullptr;
}

void World::refresh_routes() {
    for (auto& courier:couriers_) {
        const auto& source=building(courier.owner);
        const auto& target=building(courier.target);
        courier.cached_route=(source.placed && target.placed &&
                              !(profile_==RulesProfile::SettlementV4 &&
                                courier.id==CourierId::Household)) ?
            find_route(source.cell,target.cell) : std::nullopt;
        courier.cached_revision=road_revision_;
        for (auto& candidate:courier.target_routes) candidate.reset();
        if (industry_profile(profile_) && courier.enabled &&
            courier.role==CourierRole::Clay && source.placed)
            for (const auto& pottery:buildings_)
                if (pottery.placed && pottery.kind==Object::Pottery)
                    courier.target_routes[static_cast<unsigned>(pottery.id)-1]=
                        find_route(source.cell,pottery.cell);
    }
    if (profile_==RulesProfile::SettlementV4 || industry_profile(profile_)) {
        const auto& warehouse=building(BuildingId::Warehouse);
        for (std::size_t i=0;i<household_routes_.size();++i) {
            const auto& home=building(static_cast<BuildingId>(first_household_id+i));
            household_routes_[i]=warehouse.placed && home.placed ?
                find_route(warehouse.cell,home.cell):std::nullopt;
        }
        household_routes_revision_=road_revision_;
    }
}

std::optional<BuildingId> World::next_household_candidate() const {
    if (profile_!=RulesProfile::SettlementV4 && !industry_profile(profile_))
        return std::nullopt;
    const auto decision=courier_dispatch_status(CourierId::Household);
    return decision.status==CourierDispatchStatus::Ready ? decision.selected_target:std::nullopt;
}

std::optional<BuildingId> World::next_pottery_candidate(CourierId id) const {
    if (!industry_profile(profile_) || courier(id).role!=CourierRole::Clay)
        return std::nullopt;
    const auto decision=courier_dispatch_status(id);
    return decision.status==CourierDispatchStatus::Ready ? decision.selected_target:std::nullopt;
}

CourierDispatchDecision World::courier_dispatch_status(CourierId id) const {
    const auto& c=courier(id);
    if (!c.enabled) return {CourierDispatchStatus::Disabled,std::nullopt};
    if (c.route_pending) return {CourierDispatchStatus::NoRoad,c.target};
    if (c.phase!=CourierPhase::IdleAtWorkshop)
        return {CourierDispatchStatus::AlreadyMoving,c.target};
    if (c.cached_revision!=road_revision_ ||
        (c.role==CourierRole::Household &&
         (profile_==RulesProfile::SettlementV4 || industry_profile(profile_)) &&
         household_routes_revision_!=road_revision_))
        return {CourierDispatchStatus::WaitingForRoadRevision,std::nullopt};
    const auto& source=building(c.owner);
    if (!source.placed) return {CourierDispatchStatus::Disabled,std::nullopt};
    if (profile_==RulesProfile::CityV6 && !building_staffed(c.owner))
        return {CourierDispatchStatus::Unstaffed,std::nullopt};
    const int source_stock=c.role==CourierRole::Household ? source.pottery_stock:source.output;
    if (source_stock<=0) return {CourierDispatchStatus::NoStock,std::nullopt};

    if (c.role==CourierRole::Clay && industry_profile(profile_)) {
        std::array<BuildingId,2> targets{};
        unsigned count=0;
        for (const auto& b:buildings_)
            if (b.placed && b.kind==Object::Pottery) targets[count++]=b.id;
        if (!count) return {CourierDispatchStatus::NoTarget,std::nullopt};
        unsigned start=0;
        if (c.last_dispatched_pottery)
            for (unsigned i=0;i<count;++i)
                if (targets[i]==*c.last_dispatched_pottery) start=(i+1)%count;
        bool any_free=false;
        for (unsigned step=0;step<count;++step) {
            const auto candidate=targets[(start+step)%count];
            const auto route_index=static_cast<unsigned>(candidate)-1U;
            const auto& target=building(candidate);
            if (target.input_clay+target.reserved_incoming>=Rules::pottery_input_capacity)
                continue;
            any_free=true;
            if (c.target_routes[route_index])
                return {CourierDispatchStatus::Ready,candidate};
        }
        return {any_free ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,
                std::nullopt};
    }

    if (c.role==CourierRole::Household &&
        (profile_==RulesProfile::SettlementV4 || industry_profile(profile_))) {
        if (next_household_id_==first_household_id)
            return {CourierDispatchStatus::NoTarget,std::nullopt};
        const unsigned start=last_dispatched_household_ ?
            static_cast<unsigned>(*last_dispatched_household_) : household_id_end-1U;
        bool any_free=false;
        for (unsigned step=1;step<=household_limit;++step) {
            const unsigned value=first_household_id+
                (start-first_household_id+step)%household_limit;
            const auto candidate=static_cast<BuildingId>(value);
            const auto& target=building(candidate);
            if (!target.placed || target.kind!=Object::Household) continue;
            if (target.pottery_stock+target.reserved_incoming>=Rules::household_capacity)
                continue;
            any_free=true;
            if (household_routes_[value-first_household_id])
                return {CourierDispatchStatus::Ready,candidate};
        }
        return {any_free ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::TargetFull,
                std::nullopt};
    }

    const auto& target=building(c.target);
    if (!target.placed) return {CourierDispatchStatus::NoTarget,std::nullopt};
    if (!c.cached_route) return {CourierDispatchStatus::NoRoad,std::nullopt};
    const int target_stock=c.role==CourierRole::Clay ? target.input_clay:target.pottery_stock;
    const int capacity=c.role==CourierRole::Clay ? Rules::pottery_input_capacity :
        c.role==CourierRole::Pottery ? Rules::warehouse_capacity:Rules::household_capacity;
    if (target_stock+target.reserved_incoming>=capacity)
        return {CourierDispatchStatus::TargetFull,std::nullopt};
    return {CourierDispatchStatus::Ready,c.target};
}

bool World::household_route_available(BuildingId id) const {
    const auto value=static_cast<unsigned>(id);
    if ((profile_!=RulesProfile::SettlementV4 && !industry_profile(profile_)) ||
        value<first_household_id ||
        value>=household_id_end || household_routes_revision_!=road_revision_)
        return false;
    return building(id).placed && household_routes_[value-first_household_id].has_value();
}

void World::tick_production_v2() {
    if (ticks_==UINT64_MAX) throw std::overflow_error("sandbox tick counter exhausted");
    ++ticks_;
    // Stable IDs within each kind: all Clay sources, then all Pottery works.
    // A newly started recipe gains no progress on its start tick.
    for (auto& source:buildings_) if (source.placed && source.kind==Object::ClaySource) {
        if (profile_==RulesProfile::CityV6 && !building_staffed(source.id)) continue;
        if (source.output<Rules::clay_output_capacity) {
            if (++source.progress==Rules::clay_ticks) {
                if (clay_extracted_total_==UINT64_MAX)
                    throw std::overflow_error("Clay total exhausted");
                source.progress=0;
                ++source.output;
                ++clay_extracted_total_;
                ++source.clay_extracted;
            }
        } else source.progress=0;
    }
    for (auto& pottery:buildings_) if (pottery.placed && pottery.kind==Object::Pottery) {
        if (profile_==RulesProfile::CityV6 && !building_staffed(pottery.id)) continue;
        if (pottery.active_recipe_clay>0) {
            if (++pottery.progress==Rules::pottery_recipe_ticks) {
                if (pottery_completed_total_==UINT64_MAX || pottery.recipes_completed==UINT64_MAX)
                    throw std::overflow_error("Pottery total exhausted");
                pottery.progress=0;
                pottery.active_recipe_clay=0;
                ++pottery.output;
                ++pottery.recipes_completed;
                ++pottery_completed_total_;
            }
        } else if (pottery.input_clay>=Rules::pottery_recipe_clay &&
                   pottery.output<Rules::pottery_output_capacity) {
            pottery.input_clay-=Rules::pottery_recipe_clay;
            pottery.active_recipe_clay=Rules::pottery_recipe_clay;
            pottery.progress=0;
        }
    }
    if (household_profile(profile_)) for (unsigned id=first_household_id;
                                      id<next_household_id_;++id) {
        auto& home=mutable_building(static_cast<BuildingId>(id));
        if (home.placed) {
            if (++home.demand_progress==Rules::household_demand_ticks) {
                home.demand_progress=0;
                if (home.pottery_stock>0) {
                    if (home.fulfilled_demand==UINT64_MAX || home.consumed_total==UINT64_MAX)
                        throw std::overflow_error("household fulfilled counter exhausted");
                    if (profile_==RulesProfile::CityV6 &&
                        (taxes_collected_total_>UINT64_MAX-
                             static_cast<std::uint64_t>(Rules::tax_income_per_fulfilled_demand) ||
                         treasury_>INT64_MAX-Rules::tax_income_per_fulfilled_demand))
                        throw std::overflow_error("City tax counter exhausted");
                    --home.pottery_stock;
                    ++home.fulfilled_demand;
                    ++home.consumed_total;
                    home.last_demand_status=1;
                    if (profile_==RulesProfile::CityV6) {
                        taxes_collected_total_+=
                            static_cast<std::uint64_t>(Rules::tax_income_per_fulfilled_demand);
                        treasury_+=Rules::tax_income_per_fulfilled_demand;
                    }
                } else {
                    if (home.missed_demand==UINT64_MAX)
                        throw std::overflow_error("household missed counter exhausted");
                    ++home.missed_demand;
                    home.last_demand_status=2;
                }
            }
        }
    }
    // Stable courier ID order. Arrival is after production and household demand.
    for (auto& courier:couriers_) dispatch_v2(courier);
    for (auto& courier:couriers_) move_v2(courier);
    if (!production_balance_valid() || !navigation_valid() || !city_economy_valid())
        throw std::logic_error("sandbox production or navigation invariant violated");
}

void World::dispatch_v2(CourierState& courier) {
    const auto decision=courier_dispatch_status(courier.id);
    if (decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const bool cycle_house=courier.role==CourierRole::Household &&
        (profile_==RulesProfile::SettlementV4 || industry_profile(profile_));
    const bool cycle_pottery=courier.role==CourierRole::Clay &&
        industry_profile(profile_);
    const auto selected_home=cycle_house ? decision.selected_target:std::nullopt;
    const auto selected_pottery=cycle_pottery ? decision.selected_target:std::nullopt;
    const auto target_id=*decision.selected_target;
    auto& source=mutable_building(courier.owner);
    auto& target=mutable_building(target_id);
    const std::vector<Cell>* route=nullptr;
    if (selected_home) {
        const auto& cached=household_routes_[static_cast<unsigned>(*selected_home)-first_household_id];
        if (cached) route=&*cached;
    } else if (selected_pottery) {
        const auto& cached=courier.target_routes[static_cast<unsigned>(*selected_pottery)-1];
        if (cached) route=&*cached;
    } else if (courier.cached_route) route=&*courier.cached_route;
    if (!source.placed || !target.placed ||
        courier.cached_revision!=road_revision_ || !route) return;
    int* source_stock=nullptr;
    int destination_stock=0,capacity=0;
    switch (courier.role) {
    case CourierRole::Clay:
        if (source.kind!=Object::ClaySource || target.kind!=Object::Pottery ||
            courier.good!=Good::Clay) throw std::logic_error("invalid Clay delivery relation");
        source_stock=&source.output; destination_stock=target.input_clay;
        capacity=Rules::pottery_input_capacity; break;
    case CourierRole::Pottery:
        if (source.kind!=Object::Pottery || target.kind!=Object::Warehouse ||
            courier.good!=Good::Pottery) throw std::logic_error("invalid Pottery delivery relation");
        source_stock=&source.output; destination_stock=target.pottery_stock;
        capacity=Rules::warehouse_capacity; break;
    case CourierRole::Household:
        if (!household_profile(profile_) || source.kind!=Object::Warehouse ||
            target.kind!=Object::Household || courier.good!=Good::Pottery ||
            (profile_==RulesProfile::HouseholdV3 && target_id!=BuildingId::Household))
            throw std::logic_error("invalid household delivery relation");
        source_stock=&source.pottery_stock; destination_stock=target.pottery_stock;
        capacity=Rules::household_capacity; break;
    default: throw std::logic_error("unknown courier delivery relation");
    }
    const int free=capacity-destination_stock-target.reserved_incoming;
    const int amount=std::min({*source_stock,Rules::courier_capacity,free});
    if (amount<=0) return;
    // Route and all operands are validated before any authoritative mutation.
    courier.path=*route;
    *source_stock-=amount;
    courier.cargo=amount;
    courier.reserved=amount;
    target.reserved_incoming+=amount;
    if (selected_home) {
        courier.target=*selected_home;
        last_dispatched_household_=*selected_home;
    } else if (selected_pottery) {
        courier.target=*selected_pottery;
        courier.last_dispatched_pottery=*selected_pottery;
    }
    courier.path_vertex=0;
    courier.edge_progress=0;
    courier.route_pending=false;
    courier.route_checked_revision.reset();
    courier.phase=CourierPhase::ToWarehouse;
}

void World::mark_route_pending(CourierState& courier) {
    const auto current=courier.path.at(courier.path_vertex);
    if (courier.edge_progress>0) {
        const auto next=courier.path.at(courier.path_vertex+1);
        courier.path={current,next};
    } else courier.path={current};
    courier.path_vertex=0;
    courier.route_pending=true;
    courier.route_checked_revision.reset();
}

void World::reroute_v2(CourierState& courier) {
    if (!courier.route_pending || courier.edge_progress!=0 ||
        courier.route_checked_revision==road_revision_) return;
    const auto& destination=building(courier.phase==CourierPhase::ToWarehouse ?
        courier.target:courier.owner);
    if (courier.reroute_attempts==UINT64_MAX)
        throw std::overflow_error("courier reroute counter exhausted");
    ++courier.reroute_attempts;
    courier.route_checked_revision=road_revision_;
    const auto route=find_route(courier.path.at(courier.path_vertex),destination.cell);
    if (!route) return;
    courier.path=*route;
    courier.path_vertex=0;
    courier.edge_progress=0;
    courier.route_pending=false;
    courier.route_checked_revision.reset();
}

bool World::valid_return_path(const CourierState& courier) const {
    const auto& source=building(courier.owner);
    const auto& target=building(courier.target);
    if (courier.path.size()<3 || courier.path.front()!=source.cell ||
        courier.path.back()!=target.cell) return false;
    for (std::size_t i=1;i+1<courier.path.size();++i)
        if (object_at(courier.path[i])!=Object::Road) return false;
    return true;
}

void World::move_v2(CourierState& courier) {
    if (!courier.enabled || courier.phase==CourierPhase::IdleAtWorkshop) return;
    if (courier.route_pending && courier.edge_progress==0) {
        reroute_v2(courier);
        if (courier.route_pending) return;
    }
    if (courier.path.size()<2 || courier.path_vertex+1>=courier.path.size())
        throw std::logic_error("invalid active courier route");
    if (++courier.edge_progress<Rules::edge_ticks) return;
    courier.edge_progress=0;
    ++courier.path_vertex;
    if (courier.route_pending) {
        const auto reached=courier.path.at(courier.path_vertex);
        courier.path={reached};
        courier.path_vertex=0;
        return;
    }
    if (courier.path_vertex+1<courier.path.size()) return;
    if (courier.phase==CourierPhase::ToWarehouse) {
        auto& target=mutable_building(courier.target);
        if (courier.reserved!=courier.cargo || target.reserved_incoming<courier.reserved)
            throw std::logic_error("courier reservation mismatch");
        target.reserved_incoming-=courier.reserved;
        if (courier.role==CourierRole::Clay) target.input_clay+=courier.cargo;
        else target.pottery_stock+=courier.cargo;
        courier.cargo=0;
        courier.reserved=0;
        courier.phase=CourierPhase::Returning;
        if (valid_return_path(courier)) {
            std::reverse(courier.path.begin(),courier.path.end());
            courier.path_vertex=0;
        } else {
            courier.path={target.cell};
            courier.path_vertex=0;
            courier.route_pending=true;
            courier.route_checked_revision.reset();
            reroute_v2(courier);
        }
    } else {
        courier.path.clear();
        courier.path_vertex=0;
        courier.phase=CourierPhase::IdleAtWorkshop;
        courier.route_pending=false;
        courier.route_checked_revision.reset();
    }
}

void World::move_courier() {
    if (phase_==CourierPhase::IdleAtWorkshop || path_.size()<2) return;
    ++edge_progress_;
    if (edge_progress_<Rules::edge_ticks) return;
    edge_progress_=0;
    ++path_vertex_;
    if (path_vertex_+1<path_.size()) return;
    if (phase_==CourierPhase::ToWarehouse) {
        const auto space=Rules::warehouse_capacity-warehouse_stock_;
        if (courier_cargo_>space) throw std::logic_error("warehouse capacity violated");
        warehouse_stock_+=courier_cargo_;
        courier_cargo_=0;
        std::reverse(path_.begin(),path_.end());
        path_vertex_=0;
        phase_=CourierPhase::Returning;
    } else {
        path_.clear();
        path_vertex_=0;
        phase_=CourierPhase::IdleAtWorkshop;
    }
}

void World::tick() {
    if (production_profile(profile_)) { tick_production_v2(); return; }
    if (ticks_==UINT64_MAX) throw std::overflow_error("sandbox tick counter exhausted");
    ++ticks_;
    if (workshop_) {
        if (workshop_stock_<Rules::workshop_capacity) {
            if (++production_progress_==Rules::production_ticks) {
                production_progress_=0;
                ++workshop_stock_;
                ++total_produced_;
            }
        } else production_progress_=0;
    }
    if (phase_==CourierPhase::IdleAtWorkshop && workshop_stock_>0 && warehouse_ &&
        warehouse_stock_<Rules::warehouse_capacity) {
        if (const auto* route=route_for_revision()) {
            const int amount=std::min({workshop_stock_,Rules::courier_capacity,
                                       Rules::warehouse_capacity-warehouse_stock_});
            if (amount>0) {
                workshop_stock_-=amount;
                courier_cargo_=amount;
                path_=*route;
                path_vertex_=0;
                edge_progress_=0;
                phase_=CourierPhase::ToWarehouse;
            }
        }
    }
    move_courier();
    if (!goods_balance_valid()) throw std::logic_error("sandbox goods balance violated");
}

std::optional<Position> World::courier_position(CourierId id) const {
    const auto& c=courier(id);
    if (!c.enabled) return std::nullopt;
    const auto& source=building(c.owner);
    if (!source.placed) return std::nullopt;
    if (c.phase==CourierPhase::IdleAtWorkshop)
        return Position{static_cast<double>(source.cell.x),static_cast<double>(source.cell.y)};
    if (c.route_pending && c.edge_progress==0)
        return Position{static_cast<double>(c.path.at(c.path_vertex).x),
                        static_cast<double>(c.path.at(c.path_vertex).y)};
    const auto from=c.path[c.path_vertex],to=c.path[c.path_vertex+1];
    const double t=static_cast<double>(c.edge_progress)/Rules::edge_ticks;
    return Position{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
}
const char* World::courier_blockage(CourierId id) const {
    const auto& c=courier(id);
    if (c.route_pending) return "Waiting for road connection";
    if (c.phase==CourierPhase::ToWarehouse) return "Delivery underway";
    if (c.phase==CourierPhase::Returning) return "Returning";
    return courier_dispatch_status_name(courier_dispatch_status(id).status);
}
const char* World::pottery_blockage() const {
    const auto& b=building(BuildingId::Pottery);
    if (!b.placed) return "No pottery building";
    if (profile_==RulesProfile::CityV6 && !building_staffed(b.id)) return "Building unstaffed";
    if (b.active_recipe_clay>0) return "Processing Clay";
    if (b.output>=Rules::pottery_output_capacity) return "Pottery output full";
    if (b.input_clay<Rules::pottery_recipe_clay) return "No Clay input";
    return "Ready to process";
}
bool World::production_balance_valid() const {
    if (!production_profile(profile_)) return false;
    if (industry_profile(profile_)) return industry_balance_valid();
    for (unsigned id=1;id<household_id_end;++id) {
        const auto& instance=building(static_cast<BuildingId>(id));
        if (instance.id!=static_cast<BuildingId>(id) ||
            (instance.placed && (!in_bounds(instance.cell) ||
                object_at(instance.cell)!=instance.kind ||
                building_owner_at(instance.cell)!=instance.id))) return false;
    }
    const auto& source=building(BuildingId::ClaySource);
    const auto& pottery=building(BuildingId::Pottery);
    const auto& warehouse=building(BuildingId::Warehouse);
    const auto& clay=courier(CourierId::Clay);
    const auto& pots=courier(CourierId::Pottery);
    const auto& delivery=courier(CourierId::Household);
    if (next_household_id_<first_household_id || next_household_id_>household_id_end ||
        (profile_==RulesProfile::ProductionV2 && next_household_id_!=first_household_id) ||
        (profile_==RulesProfile::HouseholdV3 && next_household_id_>first_household_id+1) ||
        pottery_completed_total_>(UINT64_MAX-64U)/Rules::pottery_recipe_clay ||
        source.output<0 || source.output>Rules::clay_output_capacity ||
        source.progress<0 || source.progress>=Rules::clay_ticks ||
        pottery.input_clay<0 || pottery.input_clay>Rules::pottery_input_capacity ||
        pottery.output<0 || pottery.output>Rules::pottery_output_capacity ||
        pottery.progress<0 || pottery.progress>=Rules::pottery_recipe_ticks ||
        (pottery.active_recipe_clay!=0 && pottery.active_recipe_clay!=Rules::pottery_recipe_clay) ||
        (pottery.active_recipe_clay==0 && pottery.progress!=0) ||
        warehouse.pottery_stock<0 || warehouse.pottery_stock>Rules::warehouse_capacity ||
        clay.cargo<0 || clay.cargo>Rules::courier_capacity ||
        pots.cargo<0 || pots.cargo>Rules::courier_capacity ||
        delivery.cargo<0 || delivery.cargo>Rules::courier_capacity ||
        clay.reserved<0 || clay.reserved>Rules::courier_capacity ||
        pots.reserved<0 || pots.reserved>Rules::courier_capacity ||
        delivery.reserved<0 || delivery.reserved>Rules::courier_capacity ||
        pottery.reserved_incoming<0 || warehouse.reserved_incoming<0) return false;
    std::uint64_t home_stock=0,consumed=0;
    int household_reservations=0;
    for (unsigned id=first_household_id;id<household_id_end;++id) {
        const auto& home=building(static_cast<BuildingId>(id));
        if (home.id!=static_cast<BuildingId>(id) ||
            home.pottery_stock<0 || home.pottery_stock>Rules::household_capacity ||
            home.reserved_incoming<0 || home.reserved_incoming>Rules::household_capacity ||
            home.pottery_stock+home.reserved_incoming>Rules::household_capacity ||
            home.fulfilled_demand>UINT64_MAX-home.missed_demand ||
            home.consumed_total!=home.fulfilled_demand ||
            home.consumed_total>UINT64_MAX-consumed || ticks_<home.placed_tick ||
            home.input_clay || home.output || home.progress || home.clay_extracted ||
            home.active_recipe_clay || home.recipes_completed ||
            (home.placed && (id>=next_household_id_ || home.kind!=Object::Household)) ||
            (!home.placed && (home.kind!=Object::Empty || home.pottery_stock ||
                home.reserved_incoming || home.placed_tick || home.demand_progress ||
                home.fulfilled_demand || home.missed_demand || home.last_demand_status)) ||
            (id<next_household_id_ && !home.placed) ||
            home.fulfilled_demand+home.missed_demand!=
                (home.placed ? (ticks_-home.placed_tick)/Rules::household_demand_ticks:0) ||
            home.demand_progress!=static_cast<int>(home.placed ?
                (ticks_-home.placed_tick)%Rules::household_demand_ticks:0) ||
            (home.fulfilled_demand+home.missed_demand==0 ? home.last_demand_status!=0 :
                home.last_demand_status==1 ? home.fulfilled_demand==0 :
                home.last_demand_status!=2 || home.missed_demand==0) ||
            (home.reserved_incoming>0 &&
             (delivery.phase!=CourierPhase::ToWarehouse ||
              delivery.target!=static_cast<BuildingId>(id)))) return false;
        home_stock+=static_cast<std::uint64_t>(home.pottery_stock);
        consumed+=home.consumed_total;
        household_reservations+=home.reserved_incoming;
    }
    if (consumed>UINT64_MAX-64U ||
        (profile_==RulesProfile::SettlementV4 && last_dispatched_household_ &&
         (static_cast<unsigned>(*last_dispatched_household_)<first_household_id ||
          static_cast<unsigned>(*last_dispatched_household_)>=next_household_id_)) ||
        (profile_!=RulesProfile::SettlementV4 && last_dispatched_household_) ||
        delivery.enabled!=(next_household_id_>first_household_id) ||
        household_reservations!=delivery.reserved ||
        clay.reserved!=(clay.phase==CourierPhase::ToWarehouse ? clay.cargo:0) ||
        pots.reserved!=(pots.phase==CourierPhase::ToWarehouse ? pots.cargo:0) ||
        delivery.reserved!=(delivery.phase==CourierPhase::ToWarehouse ? delivery.cargo:0) ||
        pottery.reserved_incoming!=clay.reserved ||
        warehouse.reserved_incoming!=pots.reserved || source.reserved_incoming!=0 ||
        pottery.input_clay+pottery.reserved_incoming>Rules::pottery_input_capacity ||
        warehouse.pottery_stock+warehouse.reserved_incoming>Rules::warehouse_capacity)
        return false;
    const auto clay_sum=static_cast<std::uint64_t>(source.output+clay.cargo+pottery.input_clay+
        pottery.active_recipe_clay)+2*pottery_completed_total_;
    const auto pottery_sum=static_cast<std::uint64_t>(pottery.output+pots.cargo+
        warehouse.pottery_stock+delivery.cargo)+home_stock+consumed;
    return source.clay_extracted==clay_extracted_total_ &&
        clay_extracted_total_==clay_sum && pottery_completed_total_==pottery_sum &&
        pottery.recipes_completed==pottery_completed_total_;
}

bool World::industry_balance_valid() const {
    if (next_production_id_<8 || next_production_id_>10 ||
        next_courier_id_<4 || next_courier_id_>6 ||
        next_production_id_-8!=next_courier_id_-4 ||
        next_household_id_<4 || next_household_id_>8 ||
        clay_extracted_total_>2*(ticks_/Rules::clay_ticks) ||
        pottery_completed_total_>2*(ticks_/Rules::pottery_recipe_ticks)) return false;
    const auto add=[](std::uint64_t& total,std::uint64_t value) {
        if (total>UINT64_MAX-value) return false;
        total+=value; return true;
    };
    std::uint64_t clay=0,pottery=0,recipes=0,consumed=0,extracted=0;
    unsigned clay_count=0,pot_count=0;
    std::array<int,9> expected_reservations{};
    for (unsigned id=1;id<=9;++id) {
        const auto& b=building(static_cast<BuildingId>(id));
        if (b.id!=static_cast<BuildingId>(id)) return false;
        if (!b.placed) {
            if (b.kind!=Object::Empty || b.cell!=Cell{} || b.input_clay || b.output ||
                b.pottery_stock || b.reserved_incoming || b.progress || b.active_recipe_clay ||
                b.recipes_completed || b.placed_tick || b.demand_progress ||
                b.fulfilled_demand || b.missed_demand || b.consumed_total ||
                b.last_demand_status || b.clay_extracted) return false;
            continue;
        }
        if (!in_bounds(b.cell) || object_at(b.cell)!=b.kind ||
            building_owner_at(b.cell)!=b.id) return false;
        if (b.kind==Object::ClaySource) {
            ++clay_count;
            if ((id!=1 && id<8) || b.output<0 || b.output>Rules::clay_output_capacity ||
                b.progress<0 || b.progress>=Rules::clay_ticks ||
                (b.output==Rules::clay_output_capacity && b.progress!=0) ||
                b.input_clay || b.pottery_stock || b.reserved_incoming ||
                b.active_recipe_clay || b.recipes_completed || b.placed_tick>ticks_ ||
                b.clay_extracted>(ticks_-b.placed_tick)/Rules::clay_ticks ||
                b.demand_progress || b.fulfilled_demand || b.missed_demand ||
                b.consumed_total || b.last_demand_status ||
                !add(clay,static_cast<std::uint64_t>(b.output)) ||
                !add(extracted,b.clay_extracted)) return false;
        } else if (b.kind==Object::Pottery) {
            ++pot_count;
            if ((id!=2 && id<8) || b.input_clay<0 ||
                b.input_clay>Rules::pottery_input_capacity ||
                b.output<0 || b.output>Rules::pottery_output_capacity ||
                b.reserved_incoming<0 || b.reserved_incoming>Rules::pottery_input_capacity ||
                b.input_clay+b.reserved_incoming>Rules::pottery_input_capacity ||
                (b.active_recipe_clay!=0 && b.active_recipe_clay!=Rules::pottery_recipe_clay) ||
                b.progress<0 || b.progress>=Rules::pottery_recipe_ticks ||
                (b.active_recipe_clay==0 && b.progress!=0) ||
                b.pottery_stock || b.placed_tick>ticks_ ||
                b.recipes_completed>(ticks_-b.placed_tick)/Rules::pottery_recipe_ticks ||
                b.demand_progress ||
                b.fulfilled_demand || b.missed_demand || b.consumed_total ||
                b.last_demand_status || b.clay_extracted ||
                !add(clay,static_cast<std::uint64_t>(b.input_clay+b.active_recipe_clay)) ||
                !add(pottery,static_cast<std::uint64_t>(b.output)) || !add(recipes,b.recipes_completed)) return false;
        } else if (b.kind==Object::Warehouse) {
            if (id!=3 || b.pottery_stock<0 || b.pottery_stock>Rules::warehouse_capacity ||
                b.reserved_incoming<0 ||
                b.pottery_stock+b.reserved_incoming>Rules::warehouse_capacity ||
                b.input_clay || b.output || b.progress || b.active_recipe_clay ||
                b.recipes_completed || b.placed_tick || b.demand_progress ||
                b.fulfilled_demand || b.missed_demand || b.consumed_total ||
                b.last_demand_status || b.clay_extracted ||
                !add(pottery,static_cast<std::uint64_t>(b.pottery_stock))) return false;
        } else if (b.kind==Object::Household) {
            if (id<4 || id>=next_household_id_ ||
                b.pottery_stock<0 || b.reserved_incoming<0 ||
                b.pottery_stock+b.reserved_incoming>Rules::household_capacity ||
                b.input_clay || b.output || b.progress || b.active_recipe_clay ||
                b.recipes_completed || b.clay_extracted || b.placed_tick>ticks_ ||
                b.demand_progress<0 ||
                b.demand_progress>=Rules::household_demand_ticks ||
                b.fulfilled_demand>UINT64_MAX-b.missed_demand ||
                b.fulfilled_demand+b.missed_demand!=
                    (ticks_-b.placed_tick)/Rules::household_demand_ticks ||
                b.demand_progress!=static_cast<int>((ticks_-b.placed_tick)%
                    Rules::household_demand_ticks) ||
                b.consumed_total!=b.fulfilled_demand ||
                (b.fulfilled_demand+b.missed_demand==0 ? b.last_demand_status!=0 :
                    b.last_demand_status==1 ? b.fulfilled_demand==0 :
                    b.last_demand_status!=2 || b.missed_demand==0) ||
                !add(pottery,static_cast<std::uint64_t>(b.pottery_stock)) || !add(consumed,b.consumed_total)) return false;
        } else return false;
    }
    if (clay_count>2 || pot_count>2 ||
        (buildings_[0].placed!=static_cast<bool>(clay_count)) ||
        (buildings_[1].placed!=static_cast<bool>(pot_count))) return false;
    for (unsigned id=4;id<8;++id)
        if (building(static_cast<BuildingId>(id)).placed!=(id<next_household_id_)) return false;
    for (unsigned id=8;id<10;++id)
        if (building(static_cast<BuildingId>(id)).placed!=(id<next_production_id_)) return false;
    if (last_dispatched_household_ &&
        (static_cast<unsigned>(*last_dispatched_household_)<4 ||
         static_cast<unsigned>(*last_dispatched_household_)>=next_household_id_)) return false;
    for (unsigned id=1;id<=5;++id) {
        const auto& c=courier(static_cast<CourierId>(id));
        if (c.id!=static_cast<CourierId>(id)) return false;
        const auto expected_owner=id==1 ? 1U:id==2 ? 2U:id==3 ? 3U:id+4U;
        const auto& owner=building(static_cast<BuildingId>(expected_owner));
        const auto role=id==3 ? CourierRole::Household:
            owner.kind==Object::ClaySource ? CourierRole::Clay:
            owner.kind==Object::Pottery ? CourierRole::Pottery:CourierRole::None;
        if (c.owner!=static_cast<BuildingId>(expected_owner) ||
            c.enabled!=(id==3 ? next_household_id_>4:owner.placed) ||
            (c.enabled && c.role!=role) ||
            (c.enabled && c.good!=(role==CourierRole::Clay ? Good::Clay:Good::Pottery)) ||
            c.cargo<0 || c.cargo>Rules::courier_capacity ||
            c.reserved<0 || c.reserved>Rules::courier_capacity ||
            c.reserved!=(c.phase==CourierPhase::ToWarehouse ? c.cargo:0) ||
            (c.phase==CourierPhase::ToWarehouse && c.cargo==0) ||
            (c.phase!=CourierPhase::IdleAtWorkshop && !c.enabled)) return false;
        if (!c.enabled) {
            if (c.role!=(id==1 ? CourierRole::Clay:id==2 ? CourierRole::Pottery:
                id==3 ? CourierRole::Household:CourierRole::None) ||
                c.phase!=CourierPhase::IdleAtWorkshop ||
                c.cargo || c.reserved || c.last_dispatched_pottery ||
                (id>=4 && (c.good!=Good::Clay || c.target!=BuildingId::Pottery ||
                           c.reroute_attempts))) return false;
            continue;
        }
        const auto& target=building(c.target);
        if (role==CourierRole::Clay) {
            if (target.kind!=Object::Pottery && (c.phase!=CourierPhase::IdleAtWorkshop ||
                c.target!=BuildingId::Pottery)) return false;
            if (c.last_dispatched_pottery &&
                building(*c.last_dispatched_pottery).kind!=Object::Pottery) return false;
        } else if (role==CourierRole::Pottery) {
            if (c.target!=BuildingId::Warehouse) return false;
            if (c.last_dispatched_pottery) return false;
        } else if (role==CourierRole::Household) {
            if (static_cast<unsigned>(c.target)<4 || static_cast<unsigned>(c.target)>=8 ||
                (c.phase!=CourierPhase::IdleAtWorkshop && target.kind!=Object::Household) ||
                c.last_dispatched_pottery) return false;
        } else return false;
        if (c.phase==CourierPhase::ToWarehouse) {
            auto& reservation=expected_reservations[static_cast<unsigned>(c.target)-1];
            reservation+=c.reserved;
        }
        if (role==CourierRole::Clay) {
            if (!add(clay,static_cast<std::uint64_t>(c.cargo))) return false;
        } else if (!add(pottery,static_cast<std::uint64_t>(c.cargo))) return false;
    }
    for (unsigned id=1;id<=9;++id)
        if (building(static_cast<BuildingId>(id)).reserved_incoming!=
            expected_reservations[id-1]) return false;
    if (extracted!=clay_extracted_total_ || recipes!=pottery_completed_total_ ||
        !add(pottery,consumed) ||
        pottery!=pottery_completed_total_ ||
        pottery_completed_total_>(UINT64_MAX-clay)/Rules::pottery_recipe_clay ||
        !add(clay,Rules::pottery_recipe_clay*pottery_completed_total_) ||
        clay!=clay_extracted_total_) return false;
    return true;
}

bool World::navigation_valid() const {
    if (!production_profile(profile_)) return true;
    for (const auto& c:couriers_) {
        if (c.phase==CourierPhase::IdleAtWorkshop) {
            if (!c.path.empty() || c.path_vertex || c.edge_progress || c.route_pending ||
                c.route_checked_revision) return false;
            continue;
        }
        const auto& source=building(c.phase==CourierPhase::ToWarehouse ? c.owner:c.target);
        const auto& destination=building(c.phase==CourierPhase::ToWarehouse ? c.target:c.owner);
        if (!c.enabled || !source.placed || !destination.placed ||
            c.path.empty() || c.path.size()>objects_.size() ||
            c.edge_progress<0 || c.edge_progress>=Rules::edge_ticks ||
            c.path_vertex>=c.path.size() ||
            (c.route_checked_revision && *c.route_checked_revision>road_revision_)) return false;
        if (c.route_pending) {
            if (c.path_vertex!=0 || c.path.size()!=(c.edge_progress>0 ? 2U:1U) ||
                (c.edge_progress>0 && c.route_checked_revision)) return false;
        } else if (c.path.size()<2 || c.path_vertex+1>=c.path.size() ||
                   c.path.back()!=destination.cell || c.route_checked_revision) return false;
        std::vector<std::uint8_t> visited(objects_.size(),0);
        for (std::size_t i=0;i<c.path.size();++i) {
            const auto point=c.path[i];
            if (!in_bounds(point) || visited[index(point)]++) return false;
            if (i>0 && std::abs(point.x-c.path[i-1].x)+
                           std::abs(point.y-c.path[i-1].y)!=1) return false;
            if (i<c.path_vertex) continue; // Historical road may have been removed.
            const auto kind=object_at(point);
            if (i==c.path_vertex) {
                if (kind!=Object::Road && point!=source.cell) return false;
            } else if (c.route_pending) {
                if (kind!=Object::Road && point!=destination.cell) return false;
            } else if (i+1==c.path.size()) {
                if (point!=destination.cell) return false;
            } else if (kind!=Object::Road) return false;
        }
    }
    return true;
}

std::string World::canonical_state() const {
    std::ostringstream out;
    out<<static_cast<int>(profile_)<<':'<<ticks_<<':'<<command_sequence_<<':'<<road_revision_
       <<':'<<roads_placed_total_<<':'<<roads_removed_total_
       <<':'<<static_cast<unsigned>(next_household_id_)<<':'
       <<static_cast<unsigned>(next_production_id_)<<':'
       <<static_cast<unsigned>(next_courier_id_)<<':'
       <<static_cast<unsigned>(last_dispatched_household_.value_or(static_cast<BuildingId>(0)))
       <<':'<<clay_extracted_total_<<':'<<pottery_completed_total_
       <<':'<<treasury_<<':'<<taxes_collected_total_<<':'<<construction_spent_total_
       <<':'<<total_produced_<<':'<<workshop_stock_<<':'<<production_progress_
       <<':'<<courier_cargo_<<':'<<warehouse_stock_<<':'<<static_cast<int>(phase_)
       <<':'<<path_vertex_<<':'<<edge_progress_;
    for (const auto& b:buildings_)
        out<<'|'<<static_cast<int>(b.id)<<','<<static_cast<int>(b.kind)<<','<<b.placed
           <<','<<b.cell.x<<','<<b.cell.y
           <<','<<b.input_clay<<','<<b.output<<','<<b.pottery_stock<<','<<b.reserved_incoming
           <<','<<b.progress<<','<<b.active_recipe_clay<<','<<b.recipes_completed
           <<','<<b.placed_tick<<','<<b.demand_progress<<','<<b.fulfilled_demand
           <<','<<b.missed_demand<<','<<b.consumed_total<<','<<b.last_demand_status
           <<','<<b.clay_extracted;
    for (const auto& c:couriers_) {
        out<<'|'<<static_cast<int>(c.id)<<','<<static_cast<int>(c.role)<<','
           <<static_cast<int>(c.last_dispatched_pottery.value_or(static_cast<BuildingId>(0)))<<','
           <<static_cast<int>(c.owner)<<','
           <<static_cast<int>(c.target)<<','<<static_cast<int>(c.good)<<','
           <<c.enabled<<','<<static_cast<int>(c.phase)
           <<','<<c.cargo<<','<<c.reserved<<','<<c.path_vertex<<','<<c.edge_progress
           <<','<<c.route_pending<<','<<c.route_checked_revision.value_or(UINT64_MAX)
           <<','<<c.reroute_attempts
           <<','<<c.cached_revision<<','<<c.path.size();
        for (const auto cell:c.path) out<<','<<cell.x<<','<<cell.y;
        out<<','<<static_cast<bool>(c.cached_route);
        if (c.cached_route) out<<','<<c.cached_route->size();
        if (c.cached_route) for (const auto cell:*c.cached_route) out<<','<<cell.x<<','<<cell.y;
    }
    for (const auto cell:path_) out<<','<<cell.x<<','<<cell.y;
    for (const auto object:objects_) out<<static_cast<int>(object);
    out<<'|';
    for (const auto owner:owners_) out<<static_cast<int>(owner);
    out<<'|';
    for (const auto allowed:buildable_) out<<static_cast<int>(allowed!=0);
    return out.str();
}

std::optional<Position> World::courier_position() const {
    if (!workshop_) return std::nullopt;
    if (phase_==CourierPhase::IdleAtWorkshop || path_.size()<2)
        return Position{static_cast<double>(workshop_->x),static_cast<double>(workshop_->y)};
    const auto from=path_[path_vertex_],to=path_[path_vertex_+1];
    const double t=static_cast<double>(edge_progress_)/Rules::edge_ticks;
    return Position{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
}
const char* World::blockage() {
    if (profile_!=RulesProfile::LogisticsV1) return "Use production status";
    if (!workshop_) return "Place a workshop";
    if (!warehouse_) return "Place a warehouse";
    if (warehouse_stock_==Rules::warehouse_capacity) return "Warehouse full";
    if (!route_for_revision()) return "No road connection";
    if (workshop_stock_==0 && phase_==CourierPhase::IdleAtWorkshop) return "Producing goods";
    return "Operating";
}
bool World::goods_balance_valid() const {
    return profile_==RulesProfile::LogisticsV1 &&
        workshop_stock_>=0 && workshop_stock_<=Rules::workshop_capacity &&
        courier_cargo_>=0 && courier_cargo_<=Rules::courier_capacity &&
        warehouse_stock_>=0 && warehouse_stock_<=Rules::warehouse_capacity &&
        total_produced_==static_cast<std::uint64_t>(workshop_stock_+courier_cargo_+warehouse_stock_);
}

WorldSnapshot World::snapshot() const {
    WorldSnapshot s;
    s.width=width_; s.height=height_; s.profile=profile_;
    s.rule_version=profile_==RulesProfile::ProductionV2 ? 2U:1U;
    s.ticks=ticks_; s.command_sequence=command_sequence_; s.road_revision=road_revision_;
    s.roads_placed_total=roads_placed_total_; s.roads_removed_total=roads_removed_total_;
    s.next_household_id=next_household_id_;
    s.next_production_id=next_production_id_;
    s.next_courier_id=next_courier_id_;
    s.last_dispatched_household=last_dispatched_household_;
    s.treasury=treasury_;
    s.taxes_collected_total=taxes_collected_total_;
    s.construction_spent_total=construction_spent_total_;
    for (int y=0;y<height_;++y) for (int x=0;x<width_;++x)
        if (object_at({x,y})==Object::Road) s.roads.push_back({x,y});
    s.workshop=workshop_; s.warehouse=warehouse_;
    s.total_produced=total_produced_; s.clay_extracted_total=clay_extracted_total_;
    s.pottery_completed_total=pottery_completed_total_;
    s.workshop_stock=workshop_stock_; s.production_progress=production_progress_;
    s.courier_cargo=courier_cargo_; s.warehouse_stock=warehouse_stock_;
    s.phase=phase_; s.path=path_; s.path_vertex=path_vertex_; s.edge_progress=edge_progress_;
    for (std::size_t i=0;i<buildings_.size();++i) {
        const auto& b=buildings_[i];
        s.buildings[i]={b.id,b.kind,b.cell,b.placed,b.input_clay,b.output,b.pottery_stock,
                        b.reserved_incoming,b.progress,b.active_recipe_clay,b.recipes_completed,
                        b.placed_tick,b.demand_progress,b.fulfilled_demand,b.missed_demand,
                        b.consumed_total,b.last_demand_status,b.clay_extracted};
    }
    for (std::size_t i=0;i<couriers_.size();++i) {
        const auto& c=couriers_[i];
        s.couriers[i]={c.id,c.role,c.owner,c.target,c.good,c.enabled,c.phase,c.cargo,c.reserved,
                       c.path,c.path_vertex,c.edge_progress,c.route_pending,
                       c.route_checked_revision,c.reroute_attempts,c.last_dispatched_pottery};
    }
    return s;
}

World World::restore(const WorldSnapshot& s,std::vector<std::uint8_t> mask) {
    if (s.rule_version!=(s.profile==RulesProfile::ProductionV2 ? 2U:1U))
        throw std::invalid_argument("unsupported sandbox rule version");
    World w(s.width,s.height,std::move(mask),s.profile);
    const auto fail=[](const char* message)->void { throw std::invalid_argument(message); };
    if (s.ticks==UINT64_MAX || s.command_sequence==UINT64_MAX || s.road_revision>s.command_sequence ||
        s.roads.size()>w.objects_.size() || s.command_sequence<s.roads.size())
        fail("invalid sandbox counters or road count");
    const auto put=[&](Cell cell,Object object,BuildingId owner) {
        if (!w.buildable(cell)) fail("saved object outside buildable mask");
        const auto i=w.index(cell);
        if (w.objects_[i]!=Object::Empty) fail("overlapping or duplicate saved objects");
        w.objects_[i]=object;
        if (object!=Object::Road) w.owners_[i]=static_cast<std::uint8_t>(owner);
    };
    for (const auto cell:s.roads) put(cell,Object::Road,BuildingId::ClaySource);
    w.ticks_=s.ticks; w.command_sequence_=s.command_sequence; w.road_revision_=s.road_revision;
    w.roads_placed_total_=s.roads_placed_total;
    w.roads_removed_total_=s.roads_removed_total;
    w.next_household_id_=s.next_household_id;
    w.next_production_id_=s.next_production_id;
    w.next_courier_id_=s.next_courier_id;
    w.last_dispatched_household_=s.last_dispatched_household;
    const auto route=[&](const std::vector<Cell>& path,std::size_t vertex,int edge,
                         CourierPhase phase,Cell source,Cell target) {
        if (phase==CourierPhase::IdleAtWorkshop) {
            if (!path.empty() || vertex!=0 || edge!=0) fail("idle courier has active route");
            return;
        }
        if (path.size()<3 || path.size()>w.objects_.size() || vertex>=path.size()-1 ||
            edge<0 || edge>=Rules::edge_ticks ||
            path.front()!=(phase==CourierPhase::ToWarehouse ? source:target) ||
            path.back()!=(phase==CourierPhase::ToWarehouse ? target:source))
            fail("invalid active courier route endpoints or progress");
        std::vector<std::uint8_t> visited(w.objects_.size(),0);
        for (std::size_t i=0;i<path.size();++i) {
            if (!w.in_bounds(path[i])) fail("route waypoint outside grid");
            if (visited[w.index(path[i])]++) fail("route repeats a waypoint");
            if (i>0) {
                const auto dx=std::abs(path[i].x-path[i-1].x);
                const auto dy=std::abs(path[i].y-path[i-1].y);
                if (dx+dy!=1) fail("route has nonorthogonal edge");
            }
            if (i>0 && i+1<path.size() && w.object_at(path[i])!=Object::Road)
                fail("route interior is not a placed road");
        }
    };
    if (industry_profile(s.profile)) {
        if (s.workshop || s.warehouse || s.total_produced || s.workshop_stock ||
            s.production_progress || s.courier_cargo || s.warehouse_stock ||
            s.phase!=CourierPhase::IdleAtWorkshop || !s.path.empty() ||
            s.path_vertex || s.edge_progress) fail("industry contains logistics state");
        unsigned placed=0;
        for (std::size_t i=0;i<s.buildings.size();++i) {
            const auto& b=s.buildings[i];
            if (b.id!=static_cast<BuildingId>(i+1)) fail("invalid industry building ID");
            if (b.placed) { put(b.cell,b.kind,b.id); ++placed; }
            w.buildings_[i]={b.id,b.kind,b.cell,b.placed,b.input_clay,b.output,
                b.pottery_stock,b.reserved_incoming,b.progress,b.active_recipe_clay,
                b.recipes_completed,b.placed_tick,b.demand_progress,b.fulfilled_demand,
                b.missed_demand,b.consumed_total,b.last_demand_status,b.clay_extracted};
        }
        if (s.roads_placed_total<s.roads_removed_total ||
            s.roads_placed_total-s.roads_removed_total!=s.roads.size() ||
            s.road_revision<s.roads_placed_total ||
            s.road_revision-s.roads_placed_total<s.roads_removed_total ||
            s.road_revision-s.roads_placed_total-s.roads_removed_total!=placed)
            fail("industry placement revision mismatch");
        for (std::size_t i=0;i<s.couriers.size();++i) {
            const auto& c=s.couriers[i];
            if (c.id!=static_cast<CourierId>(i+1) ||
                static_cast<unsigned>(c.owner)<1 || static_cast<unsigned>(c.owner)>9 ||
                static_cast<unsigned>(c.target)<1 || static_cast<unsigned>(c.target)>9)
                fail("invalid industry courier ID or reference");
            auto& out=w.couriers_[i];
            out.id=c.id; out.role=c.role; out.owner=c.owner; out.target=c.target;
            out.good=c.good; out.enabled=c.enabled; out.phase=c.phase;
            out.cargo=c.cargo; out.reserved=c.reserved; out.path=c.path;
            out.path_vertex=c.path_vertex; out.edge_progress=c.edge_progress;
            out.route_pending=c.route_pending; out.route_checked_revision=c.route_checked_revision;
            out.reroute_attempts=c.reroute_attempts;
            out.last_dispatched_pottery=c.last_dispatched_pottery;
        }
        w.clay_extracted_total_=s.clay_extracted_total;
        w.pottery_completed_total_=s.pottery_completed_total;
        w.treasury_=s.treasury;
        w.taxes_collected_total_=s.taxes_collected_total;
        w.construction_spent_total_=s.construction_spent_total;
        if (!w.industry_balance_valid() || !w.navigation_valid() || !w.city_economy_valid())
            fail("invalid industry totals, targets, reservations or navigation");
        w.refresh_routes();
        return w;
    }
    if (s.treasury || s.taxes_collected_total || s.construction_spent_total)
        fail("older profile contains City economy state");
    if (s.next_production_id!=8 || s.next_courier_id!=4)
        fail("older profile contains industry IDs");
    for (std::size_t i=7;i<s.buildings.size();++i)
        if (s.buildings[i]!=BuildingSnapshot{static_cast<BuildingId>(i+1)})
            fail("older profile contains industry building");
    for (std::size_t i=3;i<s.couriers.size();++i) {
        CourierSnapshot empty;
        empty.id=static_cast<CourierId>(i+1);
        empty.owner=static_cast<BuildingId>(i+5);
        if (s.couriers[i]!=empty) fail("older profile contains industry courier");
    }
    if (s.profile==RulesProfile::LogisticsV1) {
        if (s.next_household_id!=first_household_id || s.last_dispatched_household)
            fail("logistics contains household dispatch state");
        if (s.workshop) put(*s.workshop,Object::Workshop,BuildingId::ClaySource);
        if (s.warehouse) put(*s.warehouse,Object::Warehouse,BuildingId::Warehouse);
        if (s.roads_removed_total!=0 || s.roads_placed_total!=s.roads.size() ||
            s.road_revision!=s.roads.size()+static_cast<unsigned>(s.workshop.has_value())+
            static_cast<unsigned>(s.warehouse.has_value()) ||
            (!s.warehouse && s.warehouse_stock!=0))
            fail("logistics placement revision or warehouse state mismatch");
        if (s.total_produced>s.ticks/static_cast<std::uint64_t>(Rules::production_ticks) ||
            s.workshop_stock<0 || s.workshop_stock>Rules::workshop_capacity ||
            s.warehouse_stock<0 || s.warehouse_stock>Rules::warehouse_capacity ||
            s.courier_cargo<0 || s.courier_cargo>Rules::courier_capacity ||
            s.production_progress<0 || s.production_progress>=Rules::production_ticks ||
            (!s.workshop && (s.total_produced || s.workshop_stock || s.production_progress)) ||
            s.total_produced!=static_cast<std::uint64_t>(s.workshop_stock+s.courier_cargo+s.warehouse_stock) ||
            s.clay_extracted_total || s.pottery_completed_total)
            fail("invalid logistics stocks, progress or goods balance");
        for (std::size_t i=0;i<s.buildings.size();++i) {
            const BuildingSnapshot zero{static_cast<BuildingId>(i+1)};
            if (s.buildings[i]!=zero) fail("v1 contains production building state");
        }
        for (std::size_t i=0;i<3;++i) {
            CourierSnapshot zero;
            zero.id=static_cast<CourierId>(i+1);
            zero.role=i==0 ? CourierRole::Clay:i==1 ? CourierRole::Pottery:CourierRole::Household;
            zero.owner=i==0 ? BuildingId::ClaySource:i==1 ? BuildingId::Pottery:BuildingId::Warehouse;
            zero.target=i==0 ? BuildingId::Pottery:i==1 ? BuildingId::Warehouse:BuildingId::Household;
            zero.good=i==0 ? Good::Clay:Good::Pottery;
            if (s.couriers[i]!=zero) fail("v1 contains production courier state");
        }
        if (s.phase!=CourierPhase::IdleAtWorkshop && s.phase!=CourierPhase::ToWarehouse &&
            s.phase!=CourierPhase::Returning) fail("invalid logistics courier phase");
        if (s.phase!=CourierPhase::IdleAtWorkshop && (!s.workshop || !s.warehouse))
            fail("active logistics courier has missing building");
        if (s.phase==CourierPhase::ToWarehouse && (s.courier_cargo==0 ||
            s.warehouse_stock+s.courier_cargo>Rules::warehouse_capacity))
            fail("invalid outbound logistics cargo");
        if (s.phase!=CourierPhase::ToWarehouse && s.courier_cargo!=0)
            fail("idle or returning logistics courier has cargo");
        route(s.path,s.path_vertex,s.edge_progress,s.phase,s.workshop.value_or(Cell{}),
              s.warehouse.value_or(Cell{}));
        w.workshop_=s.workshop; w.warehouse_=s.warehouse;
        w.total_produced_=s.total_produced; w.workshop_stock_=s.workshop_stock;
        w.production_progress_=s.production_progress; w.courier_cargo_=s.courier_cargo;
        w.warehouse_stock_=s.warehouse_stock; w.phase_=s.phase;
        w.path_=s.path; w.path_vertex_=s.path_vertex; w.edge_progress_=s.edge_progress;
    } else {
        if (s.workshop || s.warehouse || s.total_produced || s.workshop_stock ||
            s.production_progress || s.courier_cargo || s.warehouse_stock ||
            s.phase!=CourierPhase::IdleAtWorkshop || !s.path.empty() || s.path_vertex || s.edge_progress)
            fail("production contains logistics state");
        const std::array<Object,7> kinds{Object::ClaySource,Object::Pottery,Object::Warehouse,
            Object::Household,Object::Household,Object::Household,Object::Household};
        for (std::size_t i=0;i<7;++i) {
            const auto& b=s.buildings[i];
            if (b.id!=static_cast<BuildingId>(i+1)) fail("duplicate or invalid building identity");
            if (i>=3 && s.profile!=RulesProfile::SettlementV4 &&
                (i>3 || s.profile!=RulesProfile::HouseholdV3) &&
                b!=BuildingSnapshot{static_cast<BuildingId>(i+1)})
                fail("older profile contains extra household");
            if (i<3 && (b.placed_tick || b.demand_progress || b.fulfilled_demand ||
                         b.missed_demand || b.consumed_total || b.last_demand_status))
                fail("non-household demand state");
            if (b.placed) {
                if (b.kind!=kinds[i]) fail("building kind does not match identity");
                put(b.cell,b.kind,b.id);
            } else if (b.kind!=Object::Empty || b.cell!=Cell{} || b.input_clay || b.output ||
                       b.pottery_stock || b.reserved_incoming || b.progress ||
                       b.active_recipe_clay || b.recipes_completed || b.placed_tick ||
                       b.demand_progress || b.fulfilled_demand || b.missed_demand ||
                       b.consumed_total || b.last_demand_status)
                fail("unplaced building has state");
            auto& out=w.buildings_[i];
            out={b.id,b.kind,b.cell,b.placed,b.input_clay,b.output,b.pottery_stock,
                 b.reserved_incoming,b.progress,b.active_recipe_clay,b.recipes_completed,
                 b.placed_tick,b.demand_progress,b.fulfilled_demand,b.missed_demand,
                 b.consumed_total,b.last_demand_status,b.clay_extracted};
        }
        const auto placed=static_cast<unsigned>(s.buildings[0].placed)+
            static_cast<unsigned>(s.buildings[1].placed)+
            static_cast<unsigned>(s.buildings[2].placed)+
            static_cast<unsigned>(s.buildings[3].placed)+
            static_cast<unsigned>(s.buildings[4].placed)+
            static_cast<unsigned>(s.buildings[5].placed)+
            static_cast<unsigned>(s.buildings[6].placed);
        if (s.roads_placed_total<s.roads_removed_total ||
            s.roads_placed_total-s.roads_removed_total!=s.roads.size() ||
            s.road_revision<s.roads_placed_total ||
            s.road_revision-s.roads_placed_total<s.roads_removed_total ||
            s.road_revision-s.roads_placed_total-s.roads_removed_total!=placed)
            fail("production placement revision mismatch");
        const auto& source=s.buildings[0];
        const auto& pots=s.buildings[1];
        const auto& warehouse=s.buildings[2];
        if (source.input_clay || source.pottery_stock || source.reserved_incoming ||
            source.active_recipe_clay || source.recipes_completed ||
            source.output<0 || source.output>Rules::clay_output_capacity ||
            source.progress<0 || source.progress>=Rules::clay_ticks ||
            (source.output==Rules::clay_output_capacity && source.progress!=0) ||
            pots.pottery_stock || pots.input_clay<0 || pots.input_clay>Rules::pottery_input_capacity ||
            pots.output<0 || pots.output>Rules::pottery_output_capacity ||
            pots.reserved_incoming<0 || pots.reserved_incoming>Rules::pottery_input_capacity ||
            pots.progress<0 || pots.progress>=Rules::pottery_recipe_ticks ||
            (pots.active_recipe_clay!=0 && pots.active_recipe_clay!=Rules::pottery_recipe_clay) ||
            (pots.active_recipe_clay==0 && pots.progress!=0) ||
            (pots.active_recipe_clay!=0 && pots.output>=Rules::pottery_output_capacity) ||
            warehouse.input_clay || warehouse.output || warehouse.progress ||
            warehouse.active_recipe_clay || warehouse.recipes_completed ||
            warehouse.pottery_stock<0 || warehouse.pottery_stock>Rules::warehouse_capacity ||
            warehouse.reserved_incoming<0 || warehouse.reserved_incoming>Rules::warehouse_capacity ||
            s.buildings[3].input_clay || s.buildings[3].output || s.buildings[3].progress ||
            s.buildings[3].active_recipe_clay || s.buildings[3].recipes_completed ||
            s.buildings[3].pottery_stock<0 ||
            s.buildings[3].pottery_stock>Rules::household_capacity ||
            s.buildings[3].reserved_incoming<0 ||
            s.buildings[3].reserved_incoming>Rules::household_capacity)
            fail("invalid production building state");
        for (std::size_t i=0;i<3;++i) {
            const auto& c=s.couriers[i];
            const auto owner=i==0 ? BuildingId::ClaySource:i==1 ? BuildingId::Pottery:BuildingId::Warehouse;
            const auto target=i==0 ? BuildingId::Pottery:i==1 ? BuildingId::Warehouse:
                s.profile==RulesProfile::SettlementV4 ? c.target:BuildingId::Household;
            const auto good=i==0 ? Good::Clay:Good::Pottery;
            CourierSnapshot inactive_household;
            inactive_household.id=CourierId::Household;
            inactive_household.role=CourierRole::Household;
            inactive_household.owner=BuildingId::Warehouse;
            inactive_household.target=BuildingId::Household;
            inactive_household.good=Good::Pottery;
            if (c.id!=static_cast<CourierId>(i+1) || c.owner!=owner || c.target!=target ||
                c.role!=(i==0 ? CourierRole::Clay:i==1 ? CourierRole::Pottery:
                          CourierRole::Household) || c.last_dispatched_pottery ||
                c.good!=good || c.enabled!=(i==2 ? s.next_household_id>first_household_id:
                                            s.buildings[i].placed) ||
                (i==2 && s.profile==RulesProfile::ProductionV2 && c!=inactive_household) ||
                (i==2 && s.profile==RulesProfile::SettlementV4 &&
                 (static_cast<unsigned>(c.target)<first_household_id ||
                  static_cast<unsigned>(c.target)>=household_id_end ||
                  (s.next_household_id==first_household_id ?
                      c.target!=BuildingId::Household :
                      static_cast<unsigned>(c.target)>=s.next_household_id))) ||
                c.cargo<0 || c.cargo>Rules::courier_capacity ||
                c.reserved<0 || c.reserved>Rules::courier_capacity ||
                (c.phase!=CourierPhase::IdleAtWorkshop && c.phase!=CourierPhase::ToWarehouse &&
                 c.phase!=CourierPhase::Returning)) fail("invalid courier identity or cargo");
            if (c.phase!=CourierPhase::IdleAtWorkshop && (!s.buildings[i].placed ||
                !s.buildings[static_cast<std::size_t>(target)-1].placed))
                fail("active courier has missing source or target");
            if ((c.phase==CourierPhase::ToWarehouse && (c.cargo==0 || c.reserved!=c.cargo)) ||
                (c.phase!=CourierPhase::ToWarehouse && (c.cargo || c.reserved)))
                fail("invalid courier cargo reservation");
            auto& out=w.couriers_[i];
            out.id=c.id; out.owner=c.owner; out.target=c.target; out.good=c.good;
            out.role=c.role;
            out.enabled=c.enabled; out.phase=c.phase; out.cargo=c.cargo; out.reserved=c.reserved;
            out.path=c.path; out.path_vertex=c.path_vertex; out.edge_progress=c.edge_progress;
            out.route_pending=c.route_pending;
            out.route_checked_revision=c.route_checked_revision;
            out.reroute_attempts=c.reroute_attempts;
        }
        w.clay_extracted_total_=s.clay_extracted_total;
        w.pottery_completed_total_=s.pottery_completed_total;
        if (s.clay_extracted_total>s.ticks/static_cast<std::uint64_t>(Rules::clay_ticks) ||
            s.pottery_completed_total>s.ticks/static_cast<std::uint64_t>(Rules::pottery_recipe_ticks) ||
            s.pottery_completed_total>UINT64_MAX/Rules::pottery_recipe_clay ||
            !w.production_balance_valid() || !w.navigation_valid())
            fail("invalid production totals, reservations, navigation or balances");
        w.refresh_routes(); // Future dispatch only; active paths remain untouched.
    }
    return w;
}

void World::import_snapshot(const WorldSnapshot& s) {
    World replacement=restore(s,buildable_);
    *this=std::move(replacement);
}

void TickDriver::update(double frame_seconds,World& world) {
    if (paused_ || !std::isfinite(frame_seconds) || frame_seconds<=0) return;
    accumulator_=std::min(0.4,accumulator_+std::min(frame_seconds,0.25)*speed_);
    constexpr double step=1.0/Rules::ticks_per_second;
    for (int i=0;i<8 && accumulator_+1e-12>=step;++i) {
        accumulator_-=step;
        world.tick();
    }
    accumulator_=std::max(0.0,accumulator_);
}
void TickDriver::toggle_pause() { paused_=!paused_; accumulator_=0; }
void TickDriver::set_speed(int speed) {
    if (speed!=1 && speed!=2 && speed!=4) throw std::invalid_argument("sandbox speed must be 1, 2 or 4");
    speed_=speed;
}
void TickDriver::step_once(World& world) { if (paused_) world.tick(); }

} // namespace openemperor::simulation
