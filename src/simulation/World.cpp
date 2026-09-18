#include "simulation/World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
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

World::World(int width,int height,std::vector<std::uint8_t> buildable)
    : width_(width),height_(height),buildable_(std::move(buildable)) {
    if (width<=0 || height<=0 || width>512 || height>512 ||
        buildable_.size()!=static_cast<std::size_t>(width)*static_cast<std::size_t>(height))
        throw std::invalid_argument("invalid bounded sandbox grid");
    objects_.assign(buildable_.size(),Object::Empty);
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
CommandResult World::validate(Command command) const {
    CommandResult result{false,false,"",command_sequence_+1,ticks_};
    if (command.type!=CommandType::PlaceRoad && command.type!=CommandType::PlaceWorkshop &&
        command.type!=CommandType::PlaceWarehouse) {
        result.reason="Unknown placement command"; return result;
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
        warehouse_=command.cell;
        break;
    }
    ++road_revision_;
    cached_route_.reset();
    return result;
}

std::optional<std::vector<Cell>> World::find_route() const {
    if (!workshop_ || !warehouse_) return std::nullopt;
    const auto start=*workshop_, goal=*warehouse_;
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

std::optional<Position> World::courier_position() const {
    if (!workshop_) return std::nullopt;
    if (phase_==CourierPhase::IdleAtWorkshop || path_.size()<2)
        return Position{static_cast<double>(workshop_->x),static_cast<double>(workshop_->y)};
    const auto from=path_[path_vertex_],to=path_[path_vertex_+1];
    const double t=static_cast<double>(edge_progress_)/Rules::edge_ticks;
    return Position{from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t};
}
const char* World::blockage() {
    if (!workshop_) return "Place a workshop";
    if (!warehouse_) return "Place a warehouse";
    if (warehouse_stock_==Rules::warehouse_capacity) return "Warehouse full";
    if (!route_for_revision()) return "No road connection";
    if (workshop_stock_==0 && phase_==CourierPhase::IdleAtWorkshop) return "Producing goods";
    return "Operating";
}
bool World::goods_balance_valid() const {
    return workshop_stock_>=0 && workshop_stock_<=Rules::workshop_capacity &&
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
