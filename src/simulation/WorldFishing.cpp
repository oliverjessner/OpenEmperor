#include "simulation/World.h"
#include "simulation/original/FishingRules.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) { throw std::invalid_argument(reason); }
bool cell_less(Cell a,Cell b) { return a.y!=b.y ? a.y<b.y:a.x<b.x; }
template<class Rows> auto quay_at(Rows& rows,BuildingId id) {
    return std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.quay_id<key;});
}
bool source_water(const World& world,Cell cell) {
    const auto permissions=world.map_permissions();const auto terrain=world.original_terrain_facts();
    if(!permissions || !terrain || !terrain->irrigation_complete() || !world.in_bounds(cell) ||
        !terrain->irrigation_boundary_allowed(cell)) return false;
    const auto* permission=permissions->cell_permission(cell);
    return permission && permission->bridge_water && !permission->protected_original && permission->height==0 &&
        terrain->raw_terrain(cell)==4U && !permissions->fixed_passage(cell);
}
bool site_water(const World& world,Cell cell) {
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)
        if(!source_water(world,{cell.x+dx,cell.y+dy})) return false;
    return true;
}
std::vector<FishingSite> derive_sites(const World& world) {
    std::vector<FishingSite> result;
    const auto terrain=world.original_terrain_facts();
    if(!terrain || !terrain->irrigation_complete()) return result;
    // A marker supplies geography only. Its protected source cell is never
    // reclassified as passable; choose the nearest qualifying water centre.
    for(const auto marker:terrain->fishing_points()) {
        std::optional<Cell> selected;int distance=std::numeric_limits<int>::max();
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx) {
            const Cell cell{marker.x+dx,marker.y+dy};const int d=std::abs(dx)+std::abs(dy);
            if(d>distance || !site_water(world,cell)) continue;
            if(d<distance || !selected || cell_less(cell,*selected)) {selected=cell;distance=d;}
        }
        if(selected && std::none_of(result.begin(),result.end(),[&](const auto& row){return row.cell==*selected;}))
            result.push_back(FishingSite{*selected,fishing_site_capacity,0,0,0,true});
        if(result.size()==fishing_site_limit) break;
    }
    if(result.empty()) {
        // AUTHORED fallback: evenly sample the complete canonical set of
        // source-qualified3x3 water centres. This is one creation/restore
        // preparation, never a frame/tick scan or an inferred native flood.
        std::vector<Cell> candidates;
        for(int y=1;y+1<world.height();++y)for(int x=1;x+1<world.width();++x)
            if(site_water(world,{x,y})) candidates.push_back({x,y});
        const auto count=std::min(fishing_site_limit,candidates.size());
        for(std::size_t i=0;i<count;++i) {
            const auto index=count==1 ? std::size_t{0}:i*(candidates.size()-1)/(count-1);
            result.push_back(FishingSite{candidates[index]});
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return cell_less(a.cell,b.cell);});
    return result;
}
bool infrastructure(const World& world,Cell cell) {
    return world.bridge_at(cell) || world.bridge_bank_at(cell) || world.ramp_endpoint(cell) ||
        world.source_cell_ramp_claim(cell) || world.fixed_passage(cell);
}
bool quay_context(const World& world,const original::FishingQuayGeometry& shape,
                  std::optional<BuildingId> owner={}) {
    const auto permissions=world.map_permissions();const auto terrain=world.original_terrain_facts();
    if(!permissions || !terrain || !terrain->irrigation_complete()) return false;
    for(const auto cell:shape.water) if(!source_water(world,cell)) return false;
    for(const auto cell:shape.land) {
        if(!world.in_bounds(cell) || !permissions->building_allowed(cell) || permissions->protected_original(cell) ||
            permissions->cell_height(cell)!=0 || (terrain->raw_terrain(cell)&0x87375a6fU)!=0) return false;
    }
    for(const auto cell:shape.occupied) {
        if(!world.in_bounds(cell) || infrastructure(world,cell)) return false;
        if(owner) {
            if(world.building_owner_at(cell)!=owner || world.object_at(cell)!=Object::OriginalFishingQuay) return false;
        } else if(world.building_owner_at(cell) || world.object_at(cell)!=Object::Empty) return false;
        if(world.irrigation_state() && std::any_of(world.irrigation_state()->ditches.begin(),
            world.irrigation_state()->ditches.end(),[&](const auto& ditch){return ditch.cell==cell;})) return false;
    }
    return true;
}
bool valid_phase(FishingBoatPhase phase) {
    return phase==FishingBoatPhase::Outbound || phase==FishingBoatPhase::Fishing ||
        phase==FishingBoatPhase::Returning || phase==FishingBoatPhase::Unloading;
}
bool valid_movement(FishingMovementResult result) {
    return result==FishingMovementResult::Moving || result==FishingMovementResult::Arrived ||
        result==FishingMovementResult::Retry || result==FishingMovementResult::Failed;
}
} // namespace

const char* fishing_boat_phase_name(FishingBoatPhase phase) {
    switch(phase) {
    case FishingBoatPhase::Outbound:return "Outbound";
    case FishingBoatPhase::Fishing:return "Fishing";
    case FishingBoatPhase::Returning:return "Returning";
    case FishingBoatPhase::Unloading:return "Unloading";
    }
    return "Invalid";
}
const char* fishing_movement_result_name(FishingMovementResult result) {
    switch(result) {
    case FishingMovementResult::Moving:return "Moving";
    case FishingMovementResult::Arrived:return "Arrived";
    case FishingMovementResult::Retry:return "Waiting for water route";
    case FishingMovementResult::Failed:return "Invalid movement context";
    }
    return "Invalid";
}

void World::initialize_fishing_sites() {
    if(!playable_fishing_enabled() || !fishing_) invalid("Fishing sites require their explicit fresh World generation");
    if(!fishing_->sites.empty() || !fishing_->quays.empty() || !fishing_->boats.empty() || fishing_->launched || fishing_->retired)
        invalid("Fishing site initialization cannot replenish an existing ecology");
    fishing_->sites=derive_sites(*this);
}
std::vector<Cell> World::fishing_quay_cells(BuildingId id) const {
    if(!fishing_) return {};
    const auto row=quay_at(fishing_->quays,id);
    if(row==fishing_->quays.end() || row->quay_id!=id) return {};
    const auto shape=original::fishing_quay_geometry(building(id).cell,row->orientation);
    return {shape.occupied.begin(),shape.occupied.end()};
}
bool World::fishing_boat_cell_allowed(Cell cell,BuildingId quay_id) const {
    if(!playable_fishing_enabled() || !fishing_ || !source_water(*this,cell) || infrastructure(*this,cell)) return false;
    const auto row=quay_at(fishing_->quays,quay_id);
    if(row==fishing_->quays.end() || row->quay_id!=quay_id) return false;
    const auto dock=original::fishing_quay_geometry(building(quay_id).cell,row->orientation).dock;
    if(cell==dock) return building_owner_at(cell)==quay_id && object_at(cell)==Object::OriginalFishingQuay;
    return !building_owner_at(cell) && object_at(cell)==Object::Empty;
}
bool World::fishing_boat_edge_allowed(Cell from,Cell to,BuildingId quay_id) const {
    return std::abs(from.x-to.x)+std::abs(from.y-to.y)==1 &&
        fishing_boat_cell_allowed(from,quay_id) && fishing_boat_cell_allowed(to,quay_id);
}
bool World::fishing_boat_cell_claimed(Cell cell) const {
    if(!fishing_) return false;
    for(const auto& boat:fishing_->boats) {
        if(boat.current==cell) return true;
        if(boat.edge_progress && boat.path_vertex+1<boat.path.size() && boat.path[boat.path_vertex+1]==cell) return true;
    }
    return false;
}
std::optional<std::vector<Cell>> World::find_fishing_route(Cell from,Cell to,BuildingId quay_id) const {
    return find_road_route(from,to,{},{},false,nullptr,nullptr,false,quay_id);
}
Position World::fishing_boat_position(FishingBoatId id) const {
    if(!fishing_) throw std::out_of_range("No Fishing state");
    const auto boat=std::find_if(fishing_->boats.begin(),fishing_->boats.end(),[&](const auto& b){return b.id==id;});
    if(boat==fishing_->boats.end()) throw std::out_of_range("Unknown active Fishing boat identity");
    Position result{static_cast<double>(boat->current.x),static_cast<double>(boat->current.y)};
    if(boat->edge_progress && boat->path_vertex+1<boat->path.size()) {
        const auto next=boat->path[boat->path_vertex+1];const double t=static_cast<double>(boat->edge_progress)/fishing_edge_ticks;
        result.x+=static_cast<double>(next.x-boat->current.x)*t;
        result.y+=static_cast<double>(next.y-boat->current.y)*t;
    }
    return result;
}

CommandResult World::validate_fishing_command(Command command) const {
    CommandResult result{false,false,"Fishing Quay requires explicit Playable Fishing",command_sequence_,ticks_};
    if(!playable_fishing_enabled() || !fishing_ || (!fishing_food_ && !hunting_food_) || command.type!=CommandType::PlaceOriginalFishingQuay) return result;
    if(command.orientation>3) {result.reason="Fishing Quay orientation must be0..3";return result;}
    original::FishingQuayGeometry shape;
    try {shape=original::fishing_quay_geometry(command.cell,command.orientation);} catch(const std::exception&) {
        result.reason="Fishing Quay geometry exceeds coordinates";return result;
    }
    if(!quay_context(*this,shape)) {result.reason="Fishing Quay requires its complete empty shoreline body, dock, eight water and two land cells";return result;}
    for(const auto cell:shape.occupied) {
        if(fishing_boat_cell_claimed(cell) || hunting_actor_cell_claimed(cell) || grower_navigation_cell_claimed(cell)) {
            result.reason="Fishing Quay overlaps a physical actor or its begun edge";return result;
        }
        for(const auto& c:couriers_) if((c.role==CourierRole::OriginalPlantationGrower || c.role==CourierRole::OriginalFoodGrower || c.role==CourierRole::OriginalSeasonalHempGrower) && c.phase!=CourierPhase::IdleAtWorkshop && !c.path.empty() &&
            (c.path.at(c.path_vertex)==cell || (c.edge_progress && c.path.at(c.path_vertex+1)==cell))) {
            result.reason="Fishing Quay overlaps a physical plantation worker";return result;
        }
    }
    if(fishing_->quays.size()>=fishing_quay_limit || buildings_.size()>=Rules::original_production_building_limit ||
        couriers_.size()+fishing_->quays.size()+grower_navigation_tracks_.size()+
            (hunting_ ? hunting_->tents.size()+hunting_habitat_capacity*hunting_->habitats.size():0)>Rules::original_production_courier_limit-2 ||
        !next_building_id_ || next_building_id_==UINT32_MAX || !next_courier_id_ || next_courier_id_==UINT32_MAX ||
        command_sequence_>=UINT64_MAX-1 || road_revision_==UINT64_MAX) {
        result.reason="Fishing paid building, movement channel or identity budget exhausted";return result;
    }
    const auto cost=construction_cost(command.type);
    if(cost<=0 || static_cast<std::uint64_t>(cost)>UINT64_MAX-construction_spent_total_) {
        result.reason="Fishing construction spending exhausted";return result;
    }
    if(cost>treasury_) {result.reason="Insufficient Funds for Fishing Quay";result.diagnostic={BuildBlocker::InsufficientFunds,command.cell,{}};return result;}
    return {true,true,"Paid oriented Fishing Quay ready",command_sequence_,ticks_};
}
CommandResult World::execute_fishing_command(Command command) {
    auto result=validate_fishing_command(command);if(!result.accepted) return result;
    auto candidate=*this;const auto id=static_cast<BuildingId>(candidate.next_building_id_++);
    candidate.buildings_.push_back(BuildingState{id,Object::OriginalFishingQuay,command.cell,true});
    candidate.buildings_.back().placed_tick=ticks_;
    const auto shape=original::fishing_quay_geometry(command.cell,command.orientation);
    for(const auto cell:shape.occupied) {
        candidate.objects_[candidate.index(cell)]=Object::OriginalFishingQuay;
        candidate.owners_[candidate.index(cell)]=static_cast<std::uint32_t>(id);
    }
    candidate.fishing_->quays.push_back(FishingQuay{id,command.orientation});
    candidate.with_agriculture_food([&](auto& food) {
        using Stock=typename std::remove_cvref_t<decltype(food.stocks)>::value_type;
        Stock stock;stock.building_id=id;food.stocks.push_back(stock);
    });
    CourierState courier;courier.id=static_cast<CourierId>(candidate.next_courier_id_++);
    courier.role=CourierRole::OriginalFoodToMarket;courier.owner=id;courier.target=id;courier.good=Good::Food;courier.enabled=true;
    candidate.couriers_.push_back(std::move(courier));
    const auto cost=construction_cost(command.type);candidate.treasury_-=cost;
    candidate.construction_spent_total_+=static_cast<std::uint64_t>(cost);++candidate.command_sequence_;++candidate.road_revision_;
    candidate.tick_staffed_.clear();candidate.tick_staffing_active_=false;candidate.cached_route_.reset();
    candidate.refresh_routes();candidate.refresh_irrigation();*this=std::move(candidate);
    result.sequence=command_sequence_;result.reason="Paid Fishing Quay and dock installed";return result;
}

void World::advance_fishing() {
    if(!playable_fishing_enabled() || !fishing_) return;
    auto& state=*fishing_;
    for(auto& site:state.sites) {
        if(site.stock==fishing_site_capacity) {site.regeneration_progress=0;continue;}
        if(++site.regeneration_progress==fishing_regeneration_ticks) {
            if(site.regenerated==UINT64_MAX) invalid("Fishing regeneration history exhausted");
            site.regeneration_progress=0;++site.stock;++site.regenerated;
        }
    }
    const auto eligible=[&](const FishingQuay& quay) {
        const auto& source=building(quay.quay_id);
        return source.placed && source.operating_enabled && !building_on_fire(source.id) &&
            building_staffed_for_tick(source.id) && !building_entrances(source.id).empty();
    };
    const auto plan=[&](FishingBoat& boat,Cell goal) {
        auto route=find_fishing_route(boat.current,goal,boat.quay_id);
        boat.path=route ? std::move(*route):std::vector<Cell>{};boat.path_vertex=0;boat.edge_progress=0;
        boat.route_revision=road_revision_;boat.movement=route ? FishingMovementResult::Moving:FishingMovementResult::Retry;
    };
    for(std::size_t i=0;i<state.boats.size();) {
        auto& boat=state.boats[i];auto quay=quay_at(state.quays,boat.quay_id);
        if(quay==state.quays.end() || quay->quay_id!=boat.quay_id || boat.site_index>=state.sites.size())
            invalid("Fishing actor lost its paid source or geographic target");
        const auto dock=original::fishing_quay_geometry(building(quay->quay_id).cell,quay->orientation).dock;
        const bool source_eligible=eligible(*quay);
        if(!source_eligible && boat.phase!=FishingBoatPhase::Unloading) {boat.return_requested=true;boat.work_progress=0;}
        if(boat.phase==FishingBoatPhase::Outbound || boat.phase==FishingBoatPhase::Returning) {
            if(boat.edge_progress) {
                if(++boat.edge_progress==fishing_edge_ticks) {
                    ++boat.path_vertex;boat.current=boat.path.at(boat.path_vertex);boat.edge_progress=0;
                }
                ++i;continue;
            }
            if(boat.return_requested && boat.phase==FishingBoatPhase::Outbound) {
                boat.phase=FishingBoatPhase::Returning;plan(boat,dock);
            }
            const auto goal=boat.phase==FishingBoatPhase::Outbound ? state.sites[boat.site_index].cell:dock;
            if(boat.current==goal) {
                boat.phase=boat.phase==FishingBoatPhase::Outbound ? FishingBoatPhase::Fishing:FishingBoatPhase::Unloading;
                boat.path.clear();boat.path_vertex=0;boat.work_progress=0;boat.movement=FishingMovementResult::Arrived;
                ++i;continue;
            }
            if(boat.route_revision!=road_revision_) plan(boat,goal);
            if(boat.path.empty()) {boat.movement=FishingMovementResult::Retry;++i;continue;}
            if(boat.path_vertex+1>=boat.path.size() || boat.path[boat.path_vertex]!=boat.current) {
                boat.movement=FishingMovementResult::Failed;boat.path.clear();boat.path_vertex=0;boat.edge_progress=0;++i;continue;
            }
            if(!fishing_boat_edge_allowed(boat.current,boat.path[boat.path_vertex+1],boat.quay_id)) {
                plan(boat,goal);++i;continue;
            }
            boat.edge_progress=1;boat.movement=FishingMovementResult::Moving;++i;continue;
        }
        if(boat.phase==FishingBoatPhase::Fishing) {
            auto& site=state.sites[boat.site_index];
            if(boat.current!=site.cell) invalid("Fishing work has no actual target arrival");
            if(boat.return_requested || !source_eligible || !site.stock || boat.cargo>=boat.reserved) {
                boat.phase=FishingBoatPhase::Returning;boat.work_progress=0;plan(boat,dock);++i;continue;
            }
            if(++boat.work_progress==fishing_catch_ticks) {
                if(site.caught==UINT64_MAX) invalid("Fishing catch history exhausted");
                boat.work_progress=0;--site.stock;++site.caught;++boat.cargo;
                if(!site.stock || boat.cargo==boat.reserved) {boat.phase=FishingBoatPhase::Returning;plan(boat,dock);}
            }
            ++i;continue;
        }
        if(boat.phase!=FishingBoatPhase::Unloading || boat.current!=dock) invalid("Fishing unload has no physical dock return");
        if(boat.cargo) {
            if(!source_eligible) {++i;continue;}
            if(++boat.work_progress<fishing_unload_ticks) {++i;continue;}
            const auto load=boat.cargo;
            if(load>original_food_headroom(quay->quay_id)+boat.reserved || quay->unloaded>UINT64_MAX-static_cast<std::uint64_t>(load))
                invalid("Fishing return lost its finite reserved dock capacity");
            boat.reserved=0;boat.cargo=0;quay->unloaded+=static_cast<std::uint64_t>(load);
            credit_fishing_return(quay->quay_id,load);
        }
        quay->boat.reset();++state.retired;state.boats.erase(state.boats.begin()+static_cast<std::ptrdiff_t>(i));
    }
    // Use the already shared51-phase World clock. A due attempt only allocates
    // a real actor with a complete physical outward route and reserved return.
    if(!ticks_ || original::scheduler_position(ticks_-1).phase!=31) return;
    for(auto& quay:state.quays) {
        if(quay.boat || !eligible(quay) || original_food_headroom(quay.quay_id)<=0 ||
            !state.next_boat_id || state.next_boat_id==UINT64_MAX || state.launched==UINT64_MAX) continue;
        const auto dock=original::fishing_quay_geometry(building(quay.quay_id).cell,quay.orientation).dock;
        std::optional<std::size_t> chosen;std::optional<std::vector<Cell>> selected;
        for(std::size_t j=0;j<state.sites.size();++j) {
            if(!state.sites[j].stock || state.sites[j].cell==dock) continue;
            auto path=find_fishing_route(dock,state.sites[j].cell,quay.quay_id);
            if(!path || path->size()<2) continue;
            if(!selected || path->size()<selected->size()) {chosen=j;selected=std::move(path);}
        }
        if(!chosen) continue;
        FishingBoat boat;boat.id={state.next_boat_id++};boat.quay_id=quay.quay_id;boat.site_index=*chosen;
        boat.current=dock;boat.path=std::move(*selected);boat.reserved=std::min(fishing_boat_capacity,original_food_headroom(quay.quay_id));
        boat.route_revision=road_revision_;boat.launched_tick=ticks_;quay.boat=boat.id;++state.launched;state.boats.push_back(std::move(boat));
    }
}

void World::validate_fishing_snapshot(const WorldSnapshot& s) {
    const auto bodies=std::count_if(s.buildings.begin(),s.buildings.end(),[](const auto& b){return b.kind==Object::OriginalFishingQuay;});
    if(!simulation::playable_fishing_enabled(s.profile,s.rule_version)) {
        if(s.fishing || s.fishing_food || bodies) invalid("Historical generation rejects Fishing authority");
        return;
    }
    const bool hunting=simulation::playable_hunting_enabled(s.profile,s.rule_version);
    if(!s.fishing || s.agriculture || (hunting ? (!s.hunting_food || s.fishing_food):(!s.fishing_food || s.hunting_food)))
        invalid("Fishing requires the sole exact-generation Food authority");
    const auto& f=*s.fishing;
    if(f.quays.size()>fishing_quay_limit || f.boats.size()>f.quays.size() || f.sites.size()>fishing_site_limit ||
        bodies!=static_cast<std::ptrdiff_t>(f.quays.size()) || !f.next_boat_id || f.launched==UINT64_MAX ||
        f.next_boat_id!=f.launched+1 || f.retired>f.launched || f.launched-f.retired!=f.boats.size() ||
        s.couriers.size()+f.quays.size()+
            (s.hunting ? s.hunting->tents.size()+hunting_habitat_capacity*s.hunting->habitats.size():0)>Rules::original_production_courier_limit)
        invalid("Fishing body, channel, actor lifetime or identity bounds disagree");
    // At most one actual allocation per paid Quay at each shared phase31.
    const auto attempts=s.ticks/51+1;
    if((f.quays.empty() && f.launched) || (!f.quays.empty() &&
        (f.launched/f.quays.size()>attempts ||
         (f.launched/f.quays.size()==attempts && f.launched%f.quays.size()))))
        invalid("Fishing actor allocation exceeds shared World attempt opportunities");
    std::uint64_t caught=0,unloaded=0,cargo=0;
    for(std::size_t i=0;i<f.sites.size();++i) {
        const auto& site=f.sites[i];
        if((i && !cell_less(f.sites[i-1].cell,site.cell)) || site.cell.x<0 || site.cell.y<0 || site.cell.x>=s.width || site.cell.y>=s.height ||
            site.stock<0 || site.stock>fishing_site_capacity || site.regeneration_progress>=fishing_regeneration_ticks ||
            (site.stock==fishing_site_capacity && site.regeneration_progress) || site.regenerated>s.ticks/fishing_regeneration_ticks ||
            site.regenerated>UINT64_MAX-fishing_site_capacity || site.caught>UINT64_MAX-static_cast<std::uint64_t>(site.stock) ||
            fishing_site_capacity+site.regenerated!=static_cast<std::uint64_t>(site.stock)+site.caught || site.caught>UINT64_MAX-caught)
            invalid("Fishing finite ecology, regeneration history or site identity invalid");
        caught+=site.caught;
    }
    for(std::size_t i=0;i<f.quays.size();++i) {
        const auto& q=f.quays[i];
        const auto b=std::lower_bound(s.buildings.begin(),s.buildings.end(),q.quay_id,[](const auto& row,BuildingId id){return row.id<id;});
        if((i && !(f.quays[i-1].quay_id<q.quay_id)) || q.orientation>3 || b==s.buildings.end() || b->id!=q.quay_id ||
            b->kind!=Object::OriginalFishingQuay || !b->placed || b->placed_tick>s.ticks || q.unloaded>UINT64_MAX-unloaded ||
            std::count_if(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.owner==q.quay_id && c.role==CourierRole::OriginalFoodToMarket;})!=1)
            invalid("Fishing Quay lost exact paid body, orientation, courier or identity");
        unloaded+=q.unloaded;
        const auto age=s.ticks-b->placed_tick;
        const auto earliest=static_cast<std::uint64_t>(fishing_unload_ticks)+2U*fishing_edge_ticks;
        if(q.unloaded>(age>=earliest ? (age-earliest)/fishing_catch_ticks:0))
            invalid("Fishing dock receipt precedes possible travel, catch and unloading work");
        const auto active=std::count_if(f.boats.begin(),f.boats.end(),[&](const auto& boat){return boat.quay_id==q.quay_id;});
        if(active!=(q.boat ? 1:0)) invalid("Fishing Quay active boat link disagrees");
        if(q.boat && std::none_of(f.boats.begin(),f.boats.end(),[&](const auto& boat){return boat.id==*q.boat && boat.quay_id==q.quay_id;}))
            invalid("Fishing Quay reused or lost its actual boat identity");
    }
    for(std::size_t i=0;i<f.boats.size();++i) {
        const auto& boat=f.boats[i];const auto q=quay_at(f.quays,boat.quay_id);
        if(!boat.id.value || boat.id.value>=f.next_boat_id || (i && !(f.boats[i-1].id<boat.id)) || q==f.quays.end() || q->quay_id!=boat.quay_id ||
            q->boat!=boat.id || boat.site_index>=f.sites.size() || !valid_phase(boat.phase) || !valid_movement(boat.movement) ||
            boat.current.x<0 || boat.current.y<0 || boat.current.x>=s.width || boat.current.y>=s.height ||
            boat.cargo<0 || boat.cargo>fishing_boat_capacity || boat.reserved<1 || boat.reserved>fishing_boat_capacity || boat.cargo>boat.reserved ||
            boat.edge_progress>=fishing_edge_ticks || boat.route_revision>s.road_revision ||
            boat.path.size()>static_cast<std::size_t>(s.width)*static_cast<std::size_t>(s.height))
            invalid("Fishing boat identity, source, phase, cargo, reservation or route invalid");
        const auto bound=boat.phase==FishingBoatPhase::Fishing ? fishing_catch_ticks:
            boat.phase==FishingBoatPhase::Unloading ? fishing_unload_ticks:std::uint16_t{1};
        if(boat.work_progress>=bound) invalid("Fishing work exceeds its current physical phase");
        if(boat.path.empty()) {
            if(boat.path_vertex || boat.edge_progress ||
                ((boat.phase==FishingBoatPhase::Outbound || boat.phase==FishingBoatPhase::Returning) &&
                 boat.movement!=FishingMovementResult::Retry && boat.movement!=FishingMovementResult::Failed))
                invalid("Missing Fishing route cannot establish arrival");
        } else {
            if(boat.path_vertex>=boat.path.size() || boat.path[boat.path_vertex]!=boat.current ||
                (boat.edge_progress && boat.path_vertex+1>=boat.path.size()) ||
                (boat.phase!=FishingBoatPhase::Outbound && boat.phase!=FishingBoatPhase::Returning))
                invalid("Fishing route/current/begun edge disagrees with physical phase");
            std::vector<bool> visited(static_cast<std::size_t>(s.width)*static_cast<std::size_t>(s.height),false);
            for(std::size_t j=0;j<boat.path.size();++j) {
                const auto cell=boat.path[j];
                if(cell.x<0 || cell.y<0 || cell.x>=s.width || cell.y>=s.height ||
                    (j && std::abs(cell.x-boat.path[j-1].x)+std::abs(cell.y-boat.path[j-1].y)!=1))
                    invalid("Fishing route contains an invalid water step");
                const auto index=static_cast<std::size_t>(cell.y)*static_cast<std::size_t>(s.width)+static_cast<std::size_t>(cell.x);
                if(visited[index]) invalid("Fishing route repeats a water cell");
                visited[index]=true;
            }
        }
        const auto body=std::lower_bound(s.buildings.begin(),s.buildings.end(),boat.quay_id,[](const auto& row,BuildingId id){return row.id<id;});
        const auto dock=original::fishing_quay_geometry(body->cell,q->orientation).dock;
        if(!boat.path.empty() && boat.path.back()!=(boat.phase==FishingBoatPhase::Outbound ? f.sites[boat.site_index].cell:dock))
            invalid("Fishing route does not terminate at its assigned actual site or dock");
        if((boat.phase==FishingBoatPhase::Fishing || boat.phase==FishingBoatPhase::Unloading) ?
            boat.movement!=FishingMovementResult::Arrived:boat.movement==FishingMovementResult::Arrived)
            invalid("Fishing movement outcome cannot substitute for actual lifecycle arrival");
        if(!boat.launched_tick || boat.launched_tick<body->placed_tick || boat.launched_tick>s.ticks ||
            original::scheduler_position(boat.launched_tick-1).phase!=31 ||
            (i && f.boats[i-1].launched_tick>boat.launched_tick))
            invalid("Fishing boat launch lacks paid-source chronology and shared phase31");
        const auto distance=[](Cell a,Cell b) {return static_cast<std::uint64_t>(std::abs(a.x-b.x)+std::abs(a.y-b.y));};
        const auto site=f.sites[boat.site_index].cell;
        const auto age=s.ticks-boat.launched_tick;
        std::uint64_t minimum=distance(dock,boat.current)*fishing_edge_ticks;
        if(boat.phase==FishingBoatPhase::Outbound && boat.cargo)
            invalid("Outbound Fishing boat cannot already own a catch");
        if(boat.phase==FishingBoatPhase::Fishing)
            minimum=distance(dock,site)*fishing_edge_ticks+static_cast<std::uint64_t>(boat.cargo)*fishing_catch_ticks+boat.work_progress;
        if(boat.cargo && (boat.phase==FishingBoatPhase::Returning || boat.phase==FishingBoatPhase::Unloading))
            minimum=(distance(dock,site)+distance(site,boat.current))*fishing_edge_ticks+
                static_cast<std::uint64_t>(boat.cargo)*fishing_catch_ticks+
                (boat.phase==FishingBoatPhase::Unloading ? boat.work_progress:0U);
        minimum+=boat.edge_progress;
        if(age<minimum) invalid("Fishing actor position/work/cargo precedes its actual travel and productive opportunities");
        if((boat.phase==FishingBoatPhase::Fishing && boat.current!=f.sites[boat.site_index].cell) ||
            (boat.phase==FishingBoatPhase::Unloading && boat.current!=dock))
            invalid("Fishing work/unload lacks actual site/dock arrival");
        cargo+=static_cast<std::uint64_t>(boat.cargo);
    }
    if(cargo>UINT64_MAX-unloaded || caught!=unloaded+cargo) invalid("Fishing catch=boat cargo+actual dock receipts balance failed");
    if(caught/fishing_boat_capacity>f.launched ||
        (caught/fishing_boat_capacity==f.launched && caught%fishing_boat_capacity))
        invalid("Fishing catch exceeds actual allocated finite-cargo voyages");
}
void World::validate_fishing_context() const {
    if(!playable_fishing_enabled()) return;
    if(!fishing_ || !original_terrain_facts_ || !original_terrain_facts_->irrigation_complete()) invalid("Fishing requires complete source-bound terrain");
    if(!original_rules_model_ || original_rules_model_->building_value(original::ModelBuilding::FishingQuay,original::BuildingField::Cost)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::FishingQuay,original::BuildingField::Employees)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::FishingQuay,original::BuildingField::Employees)>INT16_MAX)
        invalid("Fishing requires a prepared paid Quay31 model with positive staffing");
    const auto expected=derive_sites(*this);
    if(expected.size()!=fishing_->sites.size()) invalid("Fishing geographic catalog differs from immutable source");
    for(std::size_t i=0;i<expected.size();++i)
        if(expected[i].cell!=fishing_->sites[i].cell || expected[i].source_marker!=fishing_->sites[i].source_marker)
            invalid("Fishing geographic identity or marker provenance differs from immutable source");
    for(const auto& q:fishing_->quays)
        if(!quay_context(*this,original::fishing_quay_geometry(building(q.quay_id).cell,q.orientation),q.quay_id))
            invalid("Fishing Quay lost complete source shoreline or five occupied cells");
    for(const auto& boat:fishing_->boats) {
        if(!fishing_boat_cell_allowed(boat.current,boat.quay_id) ||
            (boat.edge_progress && !fishing_boat_edge_allowed(boat.current,boat.path.at(boat.path_vertex+1),boat.quay_id)))
            invalid("Fishing boat current/begun edge is not actual passable water");
        if(boat.route_revision==road_revision_)
            for(std::size_t i=boat.path_vertex+1;i<boat.path.size();++i)
                if(!fishing_boat_edge_allowed(boat.path[i-1],boat.path[i],boat.quay_id))
                    invalid("Current Fishing route crosses blocked water");
    }
}
} // namespace openemperor::simulation
