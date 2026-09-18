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
const char* rules_profile_name(RulesProfile profile) {
    switch (profile) {
    case RulesProfile::LogisticsV1: return profile_name;
    case RulesProfile::ProductionV2: return production_profile_name;
    }
    return "unknown";
}

World::World(int width,int height,std::vector<std::uint8_t> buildable,RulesProfile profile)
    : width_(width),height_(height),profile_(profile),buildable_(std::move(buildable)) {
    if (width<=0 || height<=0 || width>512 || height>512 ||
        buildable_.size()!=static_cast<std::size_t>(width)*static_cast<std::size_t>(height))
        throw std::invalid_argument("invalid bounded sandbox grid");
    if (profile!=RulesProfile::LogisticsV1 && profile!=RulesProfile::ProductionV2)
        throw std::invalid_argument("unknown sandbox rules profile");
    objects_.assign(buildable_.size(),Object::Empty);
    owners_.assign(buildable_.size(),0);
    buildings_[0].id=BuildingId::ClaySource;
    buildings_[1].id=BuildingId::Pottery;
    buildings_[2].id=BuildingId::Warehouse;
    couriers_[0].id=CourierId::Clay;
    couriers_[0].owner=BuildingId::ClaySource;
    couriers_[0].target=BuildingId::Pottery;
    couriers_[0].good=Good::Clay;
    couriers_[1].id=CourierId::Pottery;
    couriers_[1].owner=BuildingId::Pottery;
    couriers_[1].target=BuildingId::Warehouse;
    couriers_[1].good=Good::Pottery;
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
    if (value<1 || value>3) throw std::out_of_range("invalid building ID");
    return buildings_[value-1];
}
BuildingState& World::mutable_building(BuildingId id) {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>3) throw std::out_of_range("invalid building ID");
    return buildings_[value-1];
}
const CourierState& World::courier(CourierId id) const {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>2) throw std::out_of_range("invalid courier ID");
    return couriers_[value-1];
}
CourierState& World::mutable_courier(CourierId id) {
    const auto value=static_cast<unsigned>(id);
    if (value<1 || value>2) throw std::out_of_range("invalid courier ID");
    return couriers_[value-1];
}
CommandResult World::validate(Command command) const {
    if (command_sequence_==UINT64_MAX) throw std::overflow_error("sandbox command counter exhausted");
    CommandResult result{false,false,"",command_sequence_+1,ticks_};
    if (command.type!=CommandType::PlaceRoad && command.type!=CommandType::PlaceWorkshop &&
        command.type!=CommandType::PlaceWarehouse && command.type!=CommandType::PlaceClaySource &&
        command.type!=CommandType::PlacePottery && command.type!=CommandType::RemoveRoad) {
        result.reason="Unknown placement command"; return result;
    }
    if ((profile_==RulesProfile::LogisticsV1 &&
        (command.type==CommandType::PlaceClaySource || command.type==CommandType::PlacePottery ||
         command.type==CommandType::RemoveRoad)) ||
        (profile_==RulesProfile::ProductionV2 && command.type==CommandType::PlaceWorkshop)) {
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
    if (command.type==CommandType::PlaceWarehouse && profile_==RulesProfile::ProductionV2 &&
        building(BuildingId::Warehouse).placed) {
        result.reason="Warehouse already exists"; return result;
    }
    if (command.type==CommandType::PlaceClaySource && building(BuildingId::ClaySource).placed) {
        result.reason="Clay source already exists"; return result;
    }
    if (command.type==CommandType::PlacePottery && building(BuildingId::Pottery).placed) {
        result.reason="Pottery already exists"; return result;
    }
    if (road_revision_==UINT64_MAX ||
        (command.type==CommandType::PlaceRoad && roads_placed_total_==UINT64_MAX)) {
        result.reason="Road counter exhausted"; return result;
    }
    return {true,true,"Placed",result.sequence,ticks_};
}
CommandResult World::execute(Command command) {
    auto result=validate(command);
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
        if (profile_==RulesProfile::ProductionV2) {
            auto& b=mutable_building(BuildingId::Warehouse);
            b.placed=true; b.kind=Object::Warehouse; b.cell=command.cell;
            owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        } else warehouse_=command.cell;
        break;
    case CommandType::PlaceClaySource: {
        objects_[index(command.cell)]=Object::ClaySource;
        auto& b=mutable_building(BuildingId::ClaySource);
        b.placed=true; b.kind=Object::ClaySource; b.cell=command.cell;
        owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        mutable_courier(CourierId::Clay).enabled=true;
        break;
    }
    case CommandType::PlacePottery: {
        objects_[index(command.cell)]=Object::Pottery;
        auto& b=mutable_building(BuildingId::Pottery);
        b.placed=true; b.kind=Object::Pottery; b.cell=command.cell;
        owners_[index(command.cell)]=static_cast<std::uint8_t>(b.id);
        mutable_courier(CourierId::Pottery).enabled=true;
        break;
    }
    }
    ++road_revision_;
    cached_route_.reset();
    if (profile_==RulesProfile::ProductionV2) refresh_routes();
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
        courier.cached_route=(source.placed && target.placed) ?
            find_route(source.cell,target.cell) : std::nullopt;
        courier.cached_revision=road_revision_;
    }
}

void World::tick_production_v2() {
    if (ticks_==UINT64_MAX) throw std::overflow_error("sandbox tick counter exhausted");
    ++ticks_;
    auto& source=mutable_building(BuildingId::ClaySource);
    auto& pottery=mutable_building(BuildingId::Pottery);
    // Stable building ID order. A newly started recipe gains no progress this tick.
    if (source.placed) {
        if (source.output<Rules::clay_output_capacity) {
            if (++source.progress==Rules::clay_ticks) {
                source.progress=0;
                ++source.output;
                ++clay_extracted_total_;
            }
        } else source.progress=0;
    }
    if (pottery.placed) {
        if (pottery.active_recipe_clay>0) {
            if (++pottery.progress==Rules::pottery_recipe_ticks) {
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
    // Stable courier ID order. Arrival is after both production decisions.
    for (auto& courier:couriers_) dispatch_v2(courier);
    for (auto& courier:couriers_) move_v2(courier);
    if (!production_balance_valid() || !navigation_valid())
        throw std::logic_error("sandbox production or navigation invariant violated");
}

void World::dispatch_v2(CourierState& courier) {
    if (!courier.enabled || courier.phase!=CourierPhase::IdleAtWorkshop) return;
    auto& source=mutable_building(courier.owner);
    auto& target=mutable_building(courier.target);
    if (!source.placed || !target.placed || source.output<=0 ||
        courier.cached_revision!=road_revision_ || !courier.cached_route) return;
    const int destination_stock=courier.good==Good::Clay ? target.input_clay : target.pottery_stock;
    const int capacity=courier.good==Good::Clay ? Rules::pottery_input_capacity : Rules::warehouse_capacity;
    const int free=capacity-destination_stock-target.reserved_incoming;
    const int amount=std::min({source.output,Rules::courier_capacity,free});
    if (amount<=0) return;
    source.output-=amount;
    courier.cargo=amount;
    courier.reserved=amount;
    target.reserved_incoming+=amount;
    courier.path=*courier.cached_route;
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
        if (courier.good==Good::Clay) target.input_clay+=courier.cargo;
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
    if (profile_==RulesProfile::ProductionV2) { tick_production_v2(); return; }
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
    if (!c.enabled) return "No source building";
    if (c.route_pending) return "Waiting for road connection";
    if (c.phase==CourierPhase::ToWarehouse) return "Delivery underway";
    if (c.phase==CourierPhase::Returning) return "Returning";
    if (!building(c.target).placed) return "No target building";
    if (!c.cached_route || c.cached_revision!=road_revision_) return "No road connection";
    const auto& target=building(c.target);
    const int stock=c.good==Good::Clay ? target.input_clay : target.pottery_stock;
    const int capacity=c.good==Good::Clay ? Rules::pottery_input_capacity : Rules::warehouse_capacity;
    if (stock+target.reserved_incoming>=capacity) return "Target buffer full";
    if (building(c.owner).output==0) return c.good==Good::Clay ? "No Clay output" : "No Pottery output";
    return "Ready";
}
const char* World::pottery_blockage() const {
    const auto& b=building(BuildingId::Pottery);
    if (!b.placed) return "No pottery building";
    if (b.active_recipe_clay>0) return "Processing Clay";
    if (b.output>=Rules::pottery_output_capacity) return "Pottery output full";
    if (b.input_clay<Rules::pottery_recipe_clay) return "No Clay input";
    return "Ready to process";
}
bool World::production_balance_valid() const {
    if (profile_!=RulesProfile::ProductionV2) return false;
    const auto& source=building(BuildingId::ClaySource);
    const auto& pottery=building(BuildingId::Pottery);
    const auto& warehouse=building(BuildingId::Warehouse);
    const auto& clay=courier(CourierId::Clay);
    const auto& pots=courier(CourierId::Pottery);
    const auto clay_sum=static_cast<std::uint64_t>(source.output+clay.cargo+pottery.input_clay+
        pottery.active_recipe_clay)+2*pottery_completed_total_;
    const auto pottery_sum=static_cast<std::uint64_t>(pottery.output+pots.cargo+warehouse.pottery_stock);
    return source.output>=0 && source.output<=Rules::clay_output_capacity &&
        source.progress>=0 && source.progress<Rules::clay_ticks &&
        pottery.input_clay>=0 && pottery.input_clay<=Rules::pottery_input_capacity &&
        pottery.output>=0 && pottery.output<=Rules::pottery_output_capacity &&
        pottery.progress>=0 && pottery.progress<Rules::pottery_recipe_ticks &&
        (pottery.active_recipe_clay==0 || pottery.active_recipe_clay==Rules::pottery_recipe_clay) &&
        (pottery.active_recipe_clay!=0 || pottery.progress==0) &&
        warehouse.pottery_stock>=0 && warehouse.pottery_stock<=Rules::warehouse_capacity &&
        clay.cargo>=0 && clay.cargo<=Rules::courier_capacity &&
        pots.cargo>=0 && pots.cargo<=Rules::courier_capacity &&
        clay.reserved>=0 && clay.reserved<=Rules::courier_capacity &&
        pots.reserved>=0 && pots.reserved<=Rules::courier_capacity &&
        clay.reserved==(clay.phase==CourierPhase::ToWarehouse ? clay.cargo : 0) &&
        pots.reserved==(pots.phase==CourierPhase::ToWarehouse ? pots.cargo : 0) &&
        pottery.reserved_incoming==clay.reserved &&
        warehouse.reserved_incoming==pots.reserved &&
        source.reserved_incoming==0 &&
        pottery.input_clay+pottery.reserved_incoming<=Rules::pottery_input_capacity &&
        warehouse.pottery_stock+warehouse.reserved_incoming<=Rules::warehouse_capacity &&
        clay_extracted_total_==clay_sum && pottery_completed_total_==pottery_sum &&
        pottery.recipes_completed==pottery_completed_total_;
}

bool World::navigation_valid() const {
    if (profile_!=RulesProfile::ProductionV2) return true;
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
       <<':'<<clay_extracted_total_<<':'<<pottery_completed_total_
       <<':'<<total_produced_<<':'<<workshop_stock_<<':'<<production_progress_
       <<':'<<courier_cargo_<<':'<<warehouse_stock_<<':'<<static_cast<int>(phase_)
       <<':'<<path_vertex_<<':'<<edge_progress_;
    for (const auto& b:buildings_)
        out<<'|'<<static_cast<int>(b.id)<<','<<static_cast<int>(b.kind)<<','<<b.placed
           <<','<<b.cell.x<<','<<b.cell.y
           <<','<<b.input_clay<<','<<b.output<<','<<b.pottery_stock<<','<<b.reserved_incoming
           <<','<<b.progress<<','<<b.active_recipe_clay<<','<<b.recipes_completed;
    for (const auto& c:couriers_) {
        out<<'|'<<static_cast<int>(c.id)<<','<<static_cast<int>(c.owner)<<','
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
                        b.reserved_incoming,b.progress,b.active_recipe_clay,b.recipes_completed};
    }
    for (std::size_t i=0;i<couriers_.size();++i) {
        const auto& c=couriers_[i];
        s.couriers[i]={c.id,c.owner,c.target,c.good,c.enabled,c.phase,c.cargo,c.reserved,
                       c.path,c.path_vertex,c.edge_progress,c.route_pending,
                       c.route_checked_revision,c.reroute_attempts};
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
    if (s.profile==RulesProfile::LogisticsV1) {
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
        for (std::size_t i=0;i<s.couriers.size();++i) {
            CourierSnapshot zero;
            zero.id=static_cast<CourierId>(i+1);
            zero.owner=i==0 ? BuildingId::ClaySource:BuildingId::Pottery;
            zero.target=i==0 ? BuildingId::Pottery:BuildingId::Warehouse;
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
            fail("v2 contains logistics state");
        const std::array<Object,3> kinds{Object::ClaySource,Object::Pottery,Object::Warehouse};
        for (std::size_t i=0;i<s.buildings.size();++i) {
            const auto& b=s.buildings[i];
            if (b.id!=static_cast<BuildingId>(i+1)) fail("duplicate or invalid building identity");
            if (b.placed) {
                if (b.kind!=kinds[i]) fail("building kind does not match identity");
                put(b.cell,b.kind,b.id);
            } else if (b.kind!=Object::Empty || b.cell!=Cell{} || b.input_clay || b.output ||
                       b.pottery_stock || b.reserved_incoming || b.progress ||
                       b.active_recipe_clay || b.recipes_completed)
                fail("unplaced building has state");
            auto& out=w.buildings_[i];
            out={b.id,b.kind,b.cell,b.placed,b.input_clay,b.output,b.pottery_stock,
                 b.reserved_incoming,b.progress,b.active_recipe_clay,b.recipes_completed};
        }
        const auto placed=static_cast<unsigned>(s.buildings[0].placed)+
            static_cast<unsigned>(s.buildings[1].placed)+
            static_cast<unsigned>(s.buildings[2].placed);
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
            warehouse.reserved_incoming<0 || warehouse.reserved_incoming>Rules::warehouse_capacity)
            fail("invalid production building state");
        for (std::size_t i=0;i<s.couriers.size();++i) {
            const auto& c=s.couriers[i];
            const auto owner=i==0 ? BuildingId::ClaySource:BuildingId::Pottery;
            const auto target=i==0 ? BuildingId::Pottery:BuildingId::Warehouse;
            const auto good=i==0 ? Good::Clay:Good::Pottery;
            if (c.id!=static_cast<CourierId>(i+1) || c.owner!=owner || c.target!=target ||
                c.good!=good || c.enabled!=s.buildings[i].placed ||
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
