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
    CommandResult result{false,false,"",command_sequence_+1,ticks_};
    if (command.type!=CommandType::PlaceRoad && command.type!=CommandType::PlaceWorkshop &&
        command.type!=CommandType::PlaceWarehouse && command.type!=CommandType::PlaceClaySource &&
        command.type!=CommandType::PlacePottery) {
        result.reason="Unknown placement command"; return result;
    }
    if ((profile_==RulesProfile::LogisticsV1 &&
         (command.type==CommandType::PlaceClaySource || command.type==CommandType::PlacePottery)) ||
        (profile_==RulesProfile::ProductionV2 && command.type==CommandType::PlaceWorkshop)) {
        result.reason="Command unavailable in selected rules profile"; return result;
    }
    if (!in_bounds(command.cell)) { result.reason="Outside sandbox grid"; return result; }
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
    if (start_kind==Object::Empty || start_kind==Object::Road ||
        goal_kind==Object::Empty || goal_kind==Object::Road) return std::nullopt;
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
                if (current==start) continue; // At least one placed road is required.
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
    if (!production_balance_valid()) throw std::logic_error("sandbox production balance violated");
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
    courier.phase=CourierPhase::ToWarehouse;
}

void World::move_v2(CourierState& courier) {
    if (!courier.enabled || courier.phase==CourierPhase::IdleAtWorkshop) return;
    if (courier.path.size()<3) throw std::logic_error("invalid courier route");
    if (++courier.edge_progress<Rules::edge_ticks) return;
    courier.edge_progress=0;
    ++courier.path_vertex;
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
        std::reverse(courier.path.begin(),courier.path.end());
        courier.path_vertex=0;
        courier.phase=CourierPhase::Returning;
    } else {
        courier.path.clear();
        courier.path_vertex=0;
        courier.phase=CourierPhase::IdleAtWorkshop;
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
    if (c.phase==CourierPhase::IdleAtWorkshop || c.path.size()<2)
        return Position{static_cast<double>(source.cell.x),static_cast<double>(source.cell.y)};
    const auto from=c.path[c.path_vertex],to=c.path[c.path_vertex+1];
    const double t=static_cast<double>(c.edge_progress)/Rules::edge_ticks;
    return Position{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
}
const char* World::courier_blockage(CourierId id) const {
    const auto& c=courier(id);
    if (!c.enabled) return "No source building";
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

std::string World::canonical_state() const {
    std::ostringstream out;
    out<<static_cast<int>(profile_)<<':'<<ticks_<<':'<<command_sequence_<<':'<<road_revision_
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
