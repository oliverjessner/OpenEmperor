#include "simulation/World.h"
#include "simulation/original/OriginalRulesModel.h"
#include "simulation/original/OriginalScheduler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* reason) {throw std::invalid_argument(reason);}
constexpr std::array<Cell,4> directions{{{0,-1},{1,0},{0,1},{-1,0}}};
bool cell_less(Cell a,Cell b) {return a.y!=b.y ? a.y<b.y:a.x<b.x;}
int distance(Cell a,Cell b) {return std::abs(a.x-b.x)+std::abs(a.y-b.y);}
bool species_valid(HuntingSpecies species) {
    return species==HuntingSpecies::WildPig75 || species==HuntingSpecies::Pheasant76 || species==HuntingSpecies::Antelope77;
}
template<class Rows> auto tent_at(Rows& rows,BuildingId id) {
    return std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.tent_id<key;});
}
template<class Rows,class Id> auto actor_at(Rows& rows,Id id) {
    return std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,Id key){return row.id<key;});
}
bool source_land(const World& world,Cell cell,bool habitat=false) {
    const auto permissions=world.map_permissions();const auto terrain=world.original_terrain_facts();
    if(!permissions || !terrain || !terrain->irrigation_complete() || !world.in_bounds(cell) ||
        !terrain->irrigation_boundary_allowed(cell) || permissions->protected_original(cell) ||
        permissions->fixed_passage(cell) || !permissions->road_allowed(cell))return false;
    const auto raw=terrain->raw_terrain(cell);
    return raw==0x80U || (!habitat && raw==0xc0U);
}
bool habitat_centre(const World& world,Cell centre) {
    const auto height=world.map_permissions()->cell_height(centre);
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        const Cell cell{centre.x+dx,centre.y+dy};
        if(!source_land(world,cell,true) || world.map_permissions()->cell_height(cell)!=height)return false;
    }
    return true;
}
bool infrastructure(const World& world,Cell cell) {
    if(world.bridge_at(cell) || world.bridge_bank_at(cell) || world.ramp_endpoint(cell) || world.source_cell_ramp_claim(cell))return true;
    if(world.object_at(cell)==Object::Road)return false;
    const auto& irrigation=world.irrigation_state();
    return irrigation && std::any_of(irrigation->ditches.begin(),irrigation->ditches.end(),[&](const auto& row){return row.cell==cell;});
}
bool animal_cell(const World& world,Cell cell,const HuntingHabitat& habitat) {
    return distance(cell,habitat.centre)<=hunting_habitat_radius && source_land(world,cell) &&
        !infrastructure(world,cell) && !world.building_owner_at(cell) &&
        (world.object_at(cell)==Object::Empty || world.object_at(cell)==Object::Road);
}
bool animal_spawn_cell(const World& world,Cell cell,const HuntingHabitat& habitat) {
    // A shared transport floor is usable for roaming, but it is an installed
    // claim rather than an empty spawn site. Birth never reinterprets a paid
    // or imported road as free habitat stock.
    return animal_cell(world,cell,habitat) && world.object_at(cell)==Object::Empty &&
        world.original_terrain_facts()->raw_terrain(cell)==0x80U;
}
std::vector<Cell> habitat_cells(Cell centre) {
    std::vector<Cell> cells;
    for(int dy=-hunting_habitat_radius;dy<=hunting_habitat_radius;++dy)
        for(int dx=-hunting_habitat_radius;dx<=hunting_habitat_radius;++dx)
            if(std::abs(dx)+std::abs(dy)<=hunting_habitat_radius)cells.push_back({centre.x+dx,centre.y+dy});
    std::stable_sort(cells.begin(),cells.end(),[&](Cell a,Cell b) {
        const int ad=distance(a,centre),bd=distance(b,centre);return ad!=bd ? ad<bd:cell_less(a,b);
    });
    return cells;
}
std::vector<HuntingHabitat> derive_habitats(const World& world) {
    std::vector<HuntingHabitat> result;const auto terrain=world.original_terrain_facts();
    if(!terrain || !terrain->irrigation_complete())return result;
    const auto append=[&](Cell centre,bool marker) {
        if(result.size()>=hunting_habitat_limit ||
            std::any_of(result.begin(),result.end(),[&](const auto& row){return distance(row.centre,centre)<=2*hunting_habitat_radius;}))return;
        HuntingHabitat habitat;habitat.centre=centre;habitat.source_marker=marker;result.push_back(habitat);
    };
    for(const auto marker:terrain->prey_points()) {
        std::optional<Cell> selected;int best=std::numeric_limits<int>::max();
        for(int dy=-6;dy<=6;++dy)for(int dx=-6;dx<=6;++dx) {
            const Cell cell{marker.x+dx,marker.y+dy};const int d=std::abs(dx)+std::abs(dy);
            if(d>best || !habitat_centre(world,cell))continue;
            if(!selected || d<best || cell_less(cell,*selected)){selected=cell;best=d;}
        }
        if(selected)append(*selected,true);
    }
    if(result.empty()) {
        // Explicit AUTHORED geographic fallback, prepared once at creation or
        // exact restore. No map name, source ID or appearance grants authority.
        std::vector<Cell> candidates;
        for(int y=1;y+1<world.height();++y)for(int x=1;x+1<world.width();++x)
            if(habitat_centre(world,{x,y}))candidates.push_back({x,y});
        const auto count=std::min(hunting_habitat_limit,candidates.size());
        for(std::size_t i=0;i<count;++i) {
            const auto at=count==1 ? std::size_t{0}:i*(candidates.size()-1)/(count-1);append(candidates[at],false);
        }
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return cell_less(a.centre,b.centre);});
    constexpr std::array species{HuntingSpecies::WildPig75,HuntingSpecies::Pheasant76,HuntingSpecies::Antelope77};
    for(std::size_t i=0;i<result.size();++i)result[i].species=species[i%species.size()];
    return result;
}
bool animal_occupied(const HuntingState& state,Cell cell,std::optional<HuntingAnimalId> except={}) {
    for(const auto& animal:state.animals)if(!except || animal.id!=*except)
        if(animal.current==cell || (animal.next && *animal.next==cell))return true;
    return false;
}
bool phase_valid(HuntingHunterPhase phase) {
    return phase==HuntingHunterPhase::Pursuing || phase==HuntingHunterPhase::Hunting ||
        phase==HuntingHunterPhase::Returning || phase==HuntingHunterPhase::Unloading;
}
bool movement_valid(HuntingMovementResult result) {
    return result==HuntingMovementResult::Moving || result==HuntingMovementResult::Arrived ||
        result==HuntingMovementResult::Retry || result==HuntingMovementResult::Failed;
}
} // namespace

const char* hunting_species_name(HuntingSpecies species) {
    switch(species) {
    case HuntingSpecies::WildPig75:return "Wild Pig75";
    case HuntingSpecies::Pheasant76:return "Pheasant76";
    case HuntingSpecies::Antelope77:return "Antelope77";
    }
    return "Invalid species";
}
const char* hunting_hunter_phase_name(HuntingHunterPhase phase) {
    switch(phase) {
    case HuntingHunterPhase::Pursuing:return "Pursuing";
    case HuntingHunterPhase::Hunting:return "Hunting";
    case HuntingHunterPhase::Returning:return "Returning";
    case HuntingHunterPhase::Unloading:return "Unloading";
    }
    return "Invalid phase";
}
const char* hunting_movement_result_name(HuntingMovementResult result) {
    switch(result) {
    case HuntingMovementResult::Moving:return "Moving";
    case HuntingMovementResult::Arrived:return "Arrived";
    case HuntingMovementResult::Retry:return "Waiting for land route";
    case HuntingMovementResult::Failed:return "Invalid movement context";
    }
    return "Invalid movement";
}
void World::initialize_hunting_habitats() {
    if(!playable_hunting_enabled() || !hunting_ || (!hunting_food_ && !processed_food_))invalid("Hunting ecology requires its explicit fresh World generation");
    auto& state=*hunting_;
    if(!state.habitats.empty() || !state.animals.empty() || !state.tents.empty() || !state.hunters.empty() ||
        state.next_animal_id!=1 || state.next_hunter_id!=1 || state.launched || state.retired)
        invalid("Hunting initialization cannot replenish an existing ecology");
    state.habitats=derive_habitats(*this);
    for(std::size_t i=0;i<state.habitats.size();++i) {
        int count=0;for(const auto cell:habitat_cells(state.habitats[i].centre)) {
            if(!animal_spawn_cell(*this,cell,state.habitats[i]) || animal_occupied(state,cell))continue;
            HuntingAnimal animal;animal.id={state.next_animal_id++};animal.habitat_index=i;
            animal.species=state.habitats[i].species;animal.current=cell;state.animals.push_back(animal);
            if(++count==hunting_habitat_capacity)break;
        }
        if(count!=hunting_habitat_capacity)invalid("Fresh source-qualified habitat lacks complete finite initial animal placement");
    }
}
bool World::hunting_hunter_cell_allowed(Cell cell,BuildingId tent_id) const {
    if(!playable_hunting_enabled() || !hunting_ || !source_land(*this,cell) || infrastructure(*this,cell))return false;
    const auto tent=tent_at(hunting_->tents,tent_id);
    if(tent==hunting_->tents.end() || tent->tent_id!=tent_id)return false;
    const auto owner=building_owner_at(cell);
    if(owner)return *owner==tent_id && object_at(cell)==Object::OriginalHuntersTent && building_contains(tent_id,cell);
    return object_at(cell)==Object::Empty || object_at(cell)==Object::Road;
}
bool World::hunting_hunter_edge_allowed(Cell from,Cell to,BuildingId tent_id) const {
    return distance(from,to)==1 && hunting_hunter_cell_allowed(from,tent_id) && hunting_hunter_cell_allowed(to,tent_id) &&
        transport_edge_blocker(from,to)==BuildBlocker::None;
}
bool World::hunting_actor_cell_claimed(Cell cell) const {
    if(!hunting_)return false;
    for(const auto& animal:hunting_->animals)
        if(animal.current==cell || (animal.next && *animal.next==cell))return true;
    for(const auto& hunter:hunting_->hunters)
        if(hunter.current==cell || (hunter.edge_progress && hunter.path_vertex+1<hunter.path.size() &&
            hunter.path[hunter.path_vertex+1]==cell))return true;
    return false;
}
std::optional<std::vector<Cell>> World::find_hunting_route(Cell from,Cell to,BuildingId tent_id) const {
    return find_road_route(from,to,{},{},false,nullptr,nullptr,false,{},tent_id);
}
Position World::hunting_hunter_position(HuntingHunterId id) const {
    if(!hunting_)throw std::out_of_range("No Hunting state");
    const auto hunter=actor_at(hunting_->hunters,id);
    if(hunter==hunting_->hunters.end() || hunter->id!=id)throw std::out_of_range("Unknown active Hunter identity");
    Position result{static_cast<double>(hunter->current.x),static_cast<double>(hunter->current.y)};
    if(hunter->edge_progress && hunter->path_vertex+1<hunter->path.size()) {
        const auto next=hunter->path[hunter->path_vertex+1];const double t=static_cast<double>(hunter->edge_progress)/hunting_edge_ticks;
        result.x+=static_cast<double>(next.x-hunter->current.x)*t;result.y+=static_cast<double>(next.y-hunter->current.y)*t;
    }
    return result;
}
Position World::hunting_animal_position(HuntingAnimalId id) const {
    if(!hunting_)throw std::out_of_range("No Hunting state");
    const auto animal=actor_at(hunting_->animals,id);
    if(animal==hunting_->animals.end() || animal->id!=id)throw std::out_of_range("Unknown live Animal identity");
    Position result{static_cast<double>(animal->current.x),static_cast<double>(animal->current.y)};
    if(animal->next) {
        const double t=static_cast<double>(animal->edge_progress)/hunting_edge_ticks;
        result.x+=static_cast<double>(animal->next->x-animal->current.x)*t;
        result.y+=static_cast<double>(animal->next->y-animal->current.y)*t;
    }
    return result;
}

void World::advance_hunting() {
    if(!playable_hunting_enabled() || !hunting_)return;
    auto& state=*hunting_;
    for(auto& animal:state.animals) {
        if(animal.next) {
            if(++animal.edge_progress==hunting_edge_ticks) {animal.current=*animal.next;animal.next.reset();animal.edge_progress=0;}
            continue;
        }
        if(animal.reserved_by) {
            const auto hunter=actor_at(state.hunters,*animal.reserved_by);
            if(hunter!=state.hunters.end() && hunter->id==*animal.reserved_by &&
                hunter->phase==HuntingHunterPhase::Hunting && hunter->current==animal.current)continue;
        }
        if((ticks_%hunting_roam_ticks+animal.id.value%hunting_roam_ticks)%hunting_roam_ticks)continue;
        const auto first=(animal.id.value+ticks_/hunting_roam_ticks)%directions.size();
        for(std::size_t i=0;i<directions.size();++i) {
            const auto delta=directions[(first+i)%directions.size()];const Cell next{animal.current.x+delta.x,animal.current.y+delta.y};
            if(!animal_cell(*this,next,state.habitats.at(animal.habitat_index)) || animal_occupied(state,next,animal.id) ||
                transport_edge_blocker(animal.current,next)!=BuildBlocker::None)continue;
            animal.next=next;animal.edge_progress=1;break;
        }
    }
    for(std::size_t i=0;i<state.habitats.size();++i) {
        auto& habitat=state.habitats[i];
        const auto live=std::count_if(state.animals.begin(),state.animals.end(),[&](const auto& animal){return animal.habitat_index==i;});
        if(live==hunting_habitat_capacity){habitat.regeneration_progress=0;continue;}
        if(++habitat.regeneration_progress<hunting_regeneration_ticks)continue;
        habitat.regeneration_progress=hunting_regeneration_ticks-1;
        if(state.next_animal_id==UINT64_MAX || habitat.births==UINT64_MAX)continue;
        for(const auto cell:habitat_cells(habitat.centre)) {
            if(!animal_spawn_cell(*this,cell,habitat) || hunting_actor_cell_claimed(cell))continue;
            const bool courier_claim=std::any_of(couriers_.begin(),couriers_.end(),[&](const auto& courier) {
                return courier.phase!=CourierPhase::IdleAtWorkshop && !courier.path.empty() &&
                    (courier.path.at(courier.path_vertex)==cell || (courier.edge_progress &&
                     courier.path_vertex+1<courier.path.size() && courier.path[courier.path_vertex+1]==cell));
            });
            if(courier_claim)continue;
            HuntingAnimal animal;animal.id={state.next_animal_id++};animal.habitat_index=i;
            animal.species=habitat.species;animal.current=cell;animal.born_tick=ticks_;
            state.animals.push_back(animal);++habitat.births;habitat.regeneration_progress=0;break;
        }
    }
    const auto eligible=[&](const HuntingTent& tent) {
        const auto& source=building(tent.tent_id);
        return source.placed && source.operating_enabled && !building_on_fire(source.id) &&
            building_staffed_for_tick(source.id) && !building_entrances(source.id).empty();
    };
    const auto plan=[&](HuntingHunter& hunter,Cell goal) {
        auto route=find_hunting_route(hunter.current,goal,hunter.tent_id);
        hunter.path=route ? std::move(*route):std::vector<Cell>{};hunter.path_vertex=0;hunter.edge_progress=0;
        hunter.route_revision=road_revision_;hunter.movement=route ? HuntingMovementResult::Moving:HuntingMovementResult::Retry;
    };
    const auto release=[&](HuntingHunter& hunter) {
        if(hunter.target) {
            const auto animal=actor_at(state.animals,*hunter.target);
            if(animal!=state.animals.end() && animal->id==*hunter.target && animal->reserved_by==hunter.id)animal->reserved_by.reset();
            hunter.target.reset();
        }
    };
    for(std::size_t i=0;i<state.hunters.size();) {
        auto& hunter=state.hunters[i];auto tent=tent_at(state.tents,hunter.tent_id);
        if(tent==state.tents.end() || tent->tent_id!=hunter.tent_id || hunter.habitat_index>=state.habitats.size())
            invalid("Hunter lost its paid Tent or actual habitat");
        const auto& body=building(tent->tent_id);const auto home=building_front_cell(profile_,rule_version_,body.kind,body.cell);
        const bool active=eligible(*tent);
        if(!active && hunter.phase!=HuntingHunterPhase::Unloading){hunter.return_requested=true;hunter.work_progress=0;}
        if(hunter.phase==HuntingHunterPhase::Pursuing || hunter.phase==HuntingHunterPhase::Returning) {
            if(hunter.edge_progress) {
                if(++hunter.edge_progress==hunting_edge_ticks) {
                    ++hunter.path_vertex;hunter.current=hunter.path.at(hunter.path_vertex);hunter.edge_progress=0;
                }
                ++i;continue;
            }
            if(hunter.return_requested && hunter.phase==HuntingHunterPhase::Pursuing) {
                release(hunter);hunter.phase=HuntingHunterPhase::Returning;plan(hunter,home);
            }
            if(hunter.phase==HuntingHunterPhase::Pursuing) {
                if(!hunter.target)invalid("Pursuing Hunter has no real reserved animal");
                const auto animal=actor_at(state.animals,*hunter.target);
                if(animal==state.animals.end() || animal->id!=*hunter.target || animal->reserved_by!=hunter.id)
                    invalid("Pursuing Hunter lost its unique live prey reservation");
                if(hunter.current==hunter.target_cell) {
                    if(animal->current==hunter.current && !animal->next) {
                        hunter.phase=HuntingHunterPhase::Hunting;hunter.path.clear();hunter.path_vertex=0;
                        hunter.work_progress=0;hunter.movement=HuntingMovementResult::Arrived;++i;continue;
                    }
                    if(animal->current==hunter.current && animal->next){++i;continue;}
                    hunter.target_cell=animal->current;plan(hunter,hunter.target_cell);
                } else if(hunter.path.empty() && animal->current!=hunter.target_cell) {
                    hunter.target_cell=animal->current;plan(hunter,hunter.target_cell);
                }
            }
            const auto goal=hunter.phase==HuntingHunterPhase::Pursuing ? hunter.target_cell:home;
            if(hunter.phase==HuntingHunterPhase::Returning && hunter.current==home) {
                hunter.phase=HuntingHunterPhase::Unloading;hunter.path.clear();hunter.path_vertex=0;
                hunter.work_progress=0;hunter.movement=HuntingMovementResult::Arrived;++i;continue;
            }
            if(hunter.route_revision!=road_revision_)plan(hunter,goal);
            if(hunter.path.empty()){hunter.movement=HuntingMovementResult::Retry;++i;continue;}
            if(hunter.path_vertex+1>=hunter.path.size() || hunter.path[hunter.path_vertex]!=hunter.current) {
                hunter.movement=HuntingMovementResult::Failed;hunter.path.clear();hunter.path_vertex=0;hunter.edge_progress=0;++i;continue;
            }
            if(!hunting_hunter_edge_allowed(hunter.current,hunter.path[hunter.path_vertex+1],hunter.tent_id)) {
                plan(hunter,goal);++i;continue;
            }
            hunter.edge_progress=1;hunter.movement=HuntingMovementResult::Moving;++i;continue;
        }
        if(hunter.phase==HuntingHunterPhase::Hunting) {
            if(hunter.return_requested || !active) {
                release(hunter);hunter.phase=HuntingHunterPhase::Returning;hunter.work_progress=0;plan(hunter,home);++i;continue;
            }
            if(!hunter.target)invalid("Hunting work has no real animal");
            const auto animal=actor_at(state.animals,*hunter.target);
            if(animal==state.animals.end() || animal->id!=*hunter.target || animal->reserved_by!=hunter.id ||
                animal->current!=hunter.current || animal->next)invalid("Hunting work lacks physical contact with its unique live animal");
            if(++hunter.work_progress<hunting_work_ticks){++i;continue;}
            auto& habitat=state.habitats[hunter.habitat_index];
            if(habitat.killed==UINT64_MAX)invalid("Hunting kill history exhausted");
            ++habitat.killed;hunter.cargo=1;hunter.kill_cell=hunter.current;hunter.killed_tick=ticks_;
            state.animals.erase(animal);hunter.target.reset();hunter.phase=HuntingHunterPhase::Returning;
            hunter.work_progress=0;plan(hunter,home);++i;continue;
        }
        if(hunter.phase!=HuntingHunterPhase::Unloading || hunter.current!=home)invalid("Meat unload lacks physical owning Tent return");
        if(hunter.cargo) {
            if(!active){++i;continue;}
            if(++hunter.work_progress<hunting_unload_ticks){++i;continue;}
            const auto load=hunter.cargo;
            if(load>original_food_headroom(tent->tent_id)+hunter.reserved || tent->unloaded==UINT64_MAX)
                invalid("Hunter return lost reserved finite Tent capacity");
            hunter.reserved=0;hunter.cargo=0;++tent->unloaded;credit_hunting_return(tent->tent_id,load);
        }
        release(hunter);tent->hunter.reset();++state.retired;state.hunters.erase(state.hunters.begin()+static_cast<std::ptrdiff_t>(i));
    }
    if(!ticks_ || original::scheduler_position(ticks_-1).phase!=31)return;
    for(auto& tent:state.tents) {
        if(tent.hunter || !eligible(tent) || original_food_headroom(tent.tent_id)<=0 ||
            !state.next_hunter_id || state.next_hunter_id==UINT64_MAX || state.launched==UINT64_MAX)continue;
        const auto& body=building(tent.tent_id);const auto home=building_front_cell(profile_,rule_version_,body.kind,body.cell);
        std::optional<std::size_t> selected;std::optional<std::vector<Cell>> route;
        for(std::size_t i=0;i<state.animals.size();++i) {
            const auto& animal=state.animals[i];if(animal.reserved_by)continue;
            auto path=find_hunting_route(home,animal.current,tent.tent_id);
            if(!path || path->size()<2)continue;
            if(!route || path->size()<route->size()){selected=i;route=std::move(path);}
        }
        if(!selected)continue;
        auto& animal=state.animals[*selected];HuntingHunter hunter;hunter.id={state.next_hunter_id++};hunter.tent_id=tent.tent_id;
        hunter.target=animal.id;hunter.target_cell=animal.current;hunter.habitat_index=animal.habitat_index;
        hunter.current=home;hunter.path=std::move(*route);hunter.reserved=1;
        hunter.route_revision=road_revision_;hunter.launched_tick=ticks_;animal.reserved_by=hunter.id;
        tent.hunter=hunter.id;++state.launched;state.hunters.push_back(std::move(hunter));
    }
}

void World::validate_hunting_snapshot(const WorldSnapshot& s) {
    const auto bodies=std::count_if(s.buildings.begin(),s.buildings.end(),[](const auto& row){return row.kind==Object::OriginalHuntersTent;});
    if(!simulation::playable_hunting_enabled(s.profile,s.rule_version)) {
        if(s.hunting || s.hunting_food || s.processed_food || bodies)invalid("Historical generation rejects Hunting authority");return;
    }
    if(!s.hunting || s.fishing_food || s.agriculture || !s.fishing ||
       (simulation::food_processing_enabled(s.profile,s.rule_version)?(!s.processed_food||s.hunting_food):(!s.hunting_food||s.processed_food)))
        invalid("Hunting requires its sole seven-kind Food and inherited Fishing metadata");
    const auto& state=*s.hunting;
    const auto initial=state.habitats.size()*static_cast<std::size_t>(hunting_habitat_capacity);
    if(state.habitats.size()>hunting_habitat_limit || state.tents.size()>hunting_tent_limit || state.animals.size()>initial ||
        state.hunters.size()>state.tents.size() || bodies!=static_cast<std::ptrdiff_t>(state.tents.size()) ||
        !state.next_animal_id || !state.next_hunter_id || state.launched==UINT64_MAX || state.next_hunter_id!=state.launched+1 ||
        state.retired>state.launched || state.launched-state.retired!=state.hunters.size() ||
        s.couriers.size()+s.fishing->quays.size()+state.tents.size()+initial>Rules::original_production_courier_limit)
        invalid("Hunting body, finite ecology, shared channel or monotone identity bounds disagree");
    const auto attempts=s.ticks/51+1;
    if((state.tents.empty() && state.launched) || (!state.tents.empty() &&
        (state.launched/state.tents.size()>attempts || (state.launched/state.tents.size()==attempts && state.launched%state.tents.size()))))
        invalid("Hunter allocations exceed shared World attempt opportunities");
    const auto in_bounds=[&](Cell cell){return cell.x>=0 && cell.y>=0 && cell.x<s.width && cell.y<s.height;};
    std::uint64_t births=0,killed=0,unloaded=0,cargo=0;
    for(std::size_t i=0;i<state.habitats.size();++i) {
        const auto& habitat=state.habitats[i];const auto live=std::count_if(state.animals.begin(),state.animals.end(),[&](const auto& a){return a.habitat_index==i;});
        if(!in_bounds(habitat.centre) || !species_valid(habitat.species) || (i && !cell_less(state.habitats[i-1].centre,habitat.centre)) ||
            habitat.regeneration_progress>=hunting_regeneration_ticks || habitat.regeneration_progress>s.ticks || habitat.births>s.ticks/hunting_regeneration_ticks ||
            habitat.births>UINT64_MAX-hunting_habitat_capacity || live>hunting_habitat_capacity ||
            (live==hunting_habitat_capacity && habitat.regeneration_progress) ||
            hunting_habitat_capacity+habitat.births!=static_cast<std::uint64_t>(live)+habitat.killed ||
            habitat.births>UINT64_MAX-births || habitat.killed>UINT64_MAX-killed ||
            habitat.births*hunting_regeneration_ticks>s.ticks-habitat.regeneration_progress)
            invalid("Hunting habitat population, birth opportunities or finite regeneration invalid");
        births+=habitat.births;killed+=habitat.killed;
    }
    if(births>UINT64_MAX-initial-1 || state.next_animal_id!=initial+births+1 || killed>state.launched)
        invalid("Animal creation or successful kill history exceeds actual finite identities");
    for(std::size_t i=0;i<state.animals.size();++i) {
        const auto& animal=state.animals[i];
        if(!animal.id.value || animal.id.value>=state.next_animal_id || (i && !(state.animals[i-1].id<animal.id)) ||
            animal.habitat_index>=state.habitats.size() || !species_valid(animal.species) ||
            animal.species!=state.habitats[animal.habitat_index].species || !in_bounds(animal.current) ||
            distance(animal.current,state.habitats[animal.habitat_index].centre)>hunting_habitat_radius ||
            animal.born_tick>s.ticks || animal.edge_progress>=hunting_edge_ticks || animal.next.has_value()!=(animal.edge_progress>0) ||
            s.ticks-animal.born_tick<animal.edge_progress)
            invalid("Animal identity, species, habitat, chronology or begun motion invalid");
        if(animal.id.value<=initial) {
            if(animal.born_tick || (animal.id.value-1)/hunting_habitat_capacity!=animal.habitat_index)
                invalid("Initial animal identity differs from its finite fresh habitat");
        } else if(animal.born_tick<hunting_regeneration_ticks)invalid("Animal birth precedes its ecology clock");
        if(i && state.animals[i-1].born_tick>animal.born_tick)invalid("Animal creation chronology differs from monotone identity order");
        if(animal.next && (!in_bounds(*animal.next) || distance(animal.current,*animal.next)!=1 ||
            distance(*animal.next,state.habitats[animal.habitat_index].centre)>hunting_habitat_radius))
            invalid("Animal begun step leaves its actual habitat or cardinal land edge");
        for(std::size_t j=0;j<i;++j) {
            const auto& other=state.animals[j];
            if(other.current==animal.current || (other.next && *other.next==animal.current) ||
                (animal.next && (other.current==*animal.next || (other.next && *other.next==*animal.next))))
                invalid("Animals overlap an actual or begun animal cell");
        }
        if(animal.reserved_by) {
            const auto hunter=actor_at(state.hunters,*animal.reserved_by);
            if(hunter==state.hunters.end() || hunter->id!=*animal.reserved_by || hunter->target!=animal.id ||
                (hunter->phase!=HuntingHunterPhase::Pursuing && hunter->phase!=HuntingHunterPhase::Hunting))
                invalid("Animal reservation lacks its unique actual pursuing Hunter");
        }
    }
    for(std::size_t i=0;i<state.tents.size();++i) {
        const auto& tent=state.tents[i];const auto body=actor_at(s.buildings,tent.tent_id);
        if((i && !(state.tents[i-1].tent_id<tent.tent_id)) || body==s.buildings.end() || body->id!=tent.tent_id ||
            body->kind!=Object::OriginalHuntersTent || !body->placed || body->placed_tick>s.ticks || tent.unloaded>UINT64_MAX-unloaded ||
            std::count_if(s.couriers.begin(),s.couriers.end(),[&](const auto& c){return c.owner==tent.tent_id && c.role==CourierRole::OriginalFoodToMarket;})!=1 ||
            tent.unloaded>(s.ticks-body->placed_tick)/(hunting_work_ticks+hunting_unload_ticks))
            invalid("Hunter Tent lost exact paid body, output courier, receipt chronology or identity");
        unloaded+=tent.unloaded;
        if(std::count_if(state.hunters.begin(),state.hunters.end(),[&](const auto& h){return h.tent_id==tent.tent_id;})!=(tent.hunter ? 1:0))
            invalid("Tent active Hunter link differs from actual actor lifetime");
        if(tent.hunter) {
            const auto hunter=actor_at(state.hunters,*tent.hunter);
            if(hunter==state.hunters.end() || hunter->id!=*tent.hunter || hunter->tent_id!=tent.tent_id)
                invalid("Tent reused or lost its Hunter identity");
        }
    }
    for(std::size_t i=0;i<state.hunters.size();++i) {
        const auto& hunter=state.hunters[i];const auto tent=tent_at(state.tents,hunter.tent_id);
        if(!hunter.id.value || hunter.id.value>=state.next_hunter_id || (i && !(state.hunters[i-1].id<hunter.id)) ||
            tent==state.tents.end() || tent->tent_id!=hunter.tent_id || tent->hunter!=hunter.id || hunter.habitat_index>=state.habitats.size() ||
            !phase_valid(hunter.phase) || !movement_valid(hunter.movement) || !in_bounds(hunter.current) ||
            hunter.cargo<0 || hunter.cargo>hunting_hunter_capacity || hunter.reserved!=1 ||
            hunter.edge_progress>=hunting_edge_ticks || hunter.route_revision>s.road_revision ||
            hunter.path.size()>static_cast<std::size_t>(s.width)*static_cast<std::size_t>(s.height))
            invalid("Hunter identity, source, phase, actual cargo, reservation or route invalid");
        const auto body=actor_at(s.buildings,hunter.tent_id);
        const auto home=building_front_cell(s.profile,s.rule_version,body->kind,body->cell);
        const bool pursuing=hunter.phase==HuntingHunterPhase::Pursuing,hunting=hunter.phase==HuntingHunterPhase::Hunting;
        const bool unloading=hunter.phase==HuntingHunterPhase::Unloading;
        if(hunter.target.has_value()!=(pursuing || hunting) || hunter.kill_cell.has_value()!=(hunter.cargo!=0) ||
            hunter.killed_tick.has_value()!=(hunter.cargo!=0) || ((pursuing || hunting) && hunter.cargo))
            invalid("Hunter prey, kill provenance and physical cargo disagree");
        if(hunter.target) {
            const auto animal=actor_at(state.animals,*hunter.target);
            if(animal==state.animals.end() || animal->id!=*hunter.target || animal->reserved_by!=hunter.id || animal->habitat_index!=hunter.habitat_index ||
                !in_bounds(hunter.target_cell) || distance(hunter.target_cell,state.habitats[hunter.habitat_index].centre)>hunting_habitat_radius)
                invalid("Hunter does not own its unique live target and observed habitat cell");
            if(hunting && (hunter.current!=animal->current || animal->next || hunter.target_cell!=animal->current))
                invalid("Hunt work lacks actual stationary contact with its reserved animal");
        }
        const auto bound=hunting ? hunting_work_ticks:unloading ? hunting_unload_ticks:std::uint16_t{1};
        if(hunter.work_progress>=bound)invalid("Hunter work exceeds its actual lifecycle phase");
        if(hunter.path.empty()) {
            if(hunter.path_vertex || hunter.edge_progress || ((pursuing || hunter.phase==HuntingHunterPhase::Returning) &&
                hunter.movement!=HuntingMovementResult::Retry && hunter.movement!=HuntingMovementResult::Failed))
                invalid("Missing Hunter route cannot establish physical arrival");
        } else {
            if((!pursuing && hunter.phase!=HuntingHunterPhase::Returning) || hunter.path_vertex>=hunter.path.size() ||
                hunter.path[hunter.path_vertex]!=hunter.current || (hunter.edge_progress && hunter.path_vertex+1>=hunter.path.size()) ||
                hunter.path.back()!=(pursuing ? hunter.target_cell:home))invalid("Hunter route/current/goal differs from its actual phase");
            std::vector<bool> visited(static_cast<std::size_t>(s.width)*static_cast<std::size_t>(s.height),false);
            for(std::size_t j=0;j<hunter.path.size();++j) {
                const auto cell=hunter.path[j];
                if(!in_bounds(cell) || (j && distance(cell,hunter.path[j-1])!=1))invalid("Hunter route contains an invalid land step");
                const auto at=static_cast<std::size_t>(cell.y)*static_cast<std::size_t>(s.width)+static_cast<std::size_t>(cell.x);
                if(visited[at])invalid("Hunter route repeats a land cell");visited[at]=true;
            }
        }
        if((hunting || unloading) ? hunter.movement!=HuntingMovementResult::Arrived:hunter.movement==HuntingMovementResult::Arrived)
            invalid("Hunter movement result cannot replace a physical lifecycle arrival");
        if(unloading && hunter.current!=home)invalid("Hunter unloading has no physical owning Tent arrival");
        if(!hunter.launched_tick || hunter.launched_tick<body->placed_tick || hunter.launched_tick>s.ticks ||
            original::scheduler_position(hunter.launched_tick-1).phase!=31 ||
            (i && state.hunters[i-1].launched_tick>hunter.launched_tick))invalid("Hunter launch lacks paid-source chronology and shared phase31");
        std::uint64_t minimum=static_cast<std::uint64_t>(distance(home,hunter.current))*hunting_edge_ticks+hunter.edge_progress;
        if(hunting)minimum+=hunter.work_progress;
        if(s.ticks-hunter.launched_tick<minimum)invalid("Hunter moved or worked before its physical opportunities");
        if(hunter.cargo) {
            if(!in_bounds(*hunter.kill_cell) || distance(*hunter.kill_cell,state.habitats[hunter.habitat_index].centre)>hunting_habitat_radius ||
                *hunter.killed_tick<hunter.launched_tick || *hunter.killed_tick>s.ticks ||
                *hunter.killed_tick-hunter.launched_tick<static_cast<std::uint64_t>(distance(home,*hunter.kill_cell))*hunting_edge_ticks+hunting_work_ticks ||
                s.ticks-*hunter.killed_tick<static_cast<std::uint64_t>(distance(*hunter.kill_cell,hunter.current))*hunting_edge_ticks+
                    hunter.edge_progress+(unloading ? hunter.work_progress:0U))
                invalid("Hunter Meat lacks possible real pursuit, hunt and return chronology");
        }
        cargo+=static_cast<std::uint64_t>(hunter.cargo);
    }
    if(unloaded>UINT64_MAX-cargo || killed!=unloaded+cargo)invalid("Hunting kills=active Hunter Meat+actual Tent receipts balance failed");
}
void World::validate_hunting_context() const {
    if(!playable_hunting_enabled())return;
    if(!hunting_ || (!hunting_food_ && !processed_food_) || !original_terrain_facts_ || !original_terrain_facts_->irrigation_complete() || !original_rules_model_ ||
        original_rules_model_->building_value(original::ModelBuilding::HuntersTent,original::BuildingField::Cost)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::HuntersTent,original::BuildingField::Employees)<=0 ||
        original_rules_model_->building_value(original::ModelBuilding::HuntersTent,original::BuildingField::Employees)>INT16_MAX)
        invalid("Hunting requires complete source-bound terrain and prepared paid Model33 with positive staffing");
    const auto expected=derive_habitats(*this);
    if(expected.size()!=hunting_->habitats.size())invalid("Hunting habitat catalog differs from immutable source geography");
    for(std::size_t i=0;i<expected.size();++i) {
        const auto& actual=hunting_->habitats[i];
        if(expected[i].centre!=actual.centre || expected[i].species!=actual.species || expected[i].source_marker!=actual.source_marker)
            invalid("Hunting habitat geography, authored species catalog or marker provenance differs from source");
    }
    for(const auto& tent:hunting_->tents) {
        const auto& body=building(tent.tent_id);const auto level=permissions_->cell_height(body.cell);
        for(const auto cell:building_footprint_cells(profile_,rule_version_,body.kind,body.cell))
            if(!in_bounds(cell) || !permissions_->building_allowed(cell) || permissions_->protected_original(cell) ||
                permissions_->cell_height(cell)!=level || building_owner_at(cell)!=tent.tent_id ||
                object_at(cell)!=Object::OriginalHuntersTent || infrastructure(*this,cell))
                invalid("Hunter Tent lost its complete permitted level2x2 paid footprint");
    }
    for(const auto& animal:hunting_->animals) {
        if(!animal_cell(*this,animal.current,hunting_->habitats.at(animal.habitat_index)) || (animal.next &&
            (!animal_cell(*this,*animal.next,hunting_->habitats.at(animal.habitat_index)) ||
             transport_edge_blocker(animal.current,*animal.next)!=BuildBlocker::None)))
            invalid("Animal occupies a blocked source cell or begun land edge");
    }
    for(const auto& hunter:hunting_->hunters) {
        if(!hunting_hunter_cell_allowed(hunter.current,hunter.tent_id) || (hunter.edge_progress &&
            !hunting_hunter_edge_allowed(hunter.current,hunter.path.at(hunter.path_vertex+1),hunter.tent_id)))
            invalid("Hunter current/begun edge is not actual admissible land");
        if(hunter.route_revision==road_revision_)
            for(std::size_t i=hunter.path_vertex+1;i<hunter.path.size();++i)
                if(!hunting_hunter_edge_allowed(hunter.path[i-1],hunter.path[i],hunter.tent_id))invalid("Current Hunter route crosses blocked land");
    }
}

} // namespace openemperor::simulation
