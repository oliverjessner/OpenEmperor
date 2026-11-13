#include "simulation/World.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* why) { throw std::invalid_argument(why); }
void add(std::uint64_t& value,std::uint64_t amount) {
    if(amount>UINT64_MAX-value) invalid("plantation counter exhausted");
    value+=amount;
}
bool source_kind(Object kind) {
    return kind==Object::OriginalTeaCuringShed237 || kind==Object::OriginalLacquerRefinery238 || kind==Object::OriginalSilkwormShed239;
}
Object plant_kind(Object source) {
    if(source==Object::OriginalTeaCuringShed237) return Object::OriginalTeaBush26;
    if(source==Object::OriginalLacquerRefinery238) return Object::OriginalLacquerTree27;
    if(source==Object::OriginalSilkwormShed239) return Object::OriginalMulberryTree28;
    return Object::Empty;
}
bool work_area(Cell source,Cell plant) {
    return std::int64_t(plant.x)>=std::int64_t(source.x)-3 && std::int64_t(plant.x)<=std::int64_t(source.x)+4 &&
        std::int64_t(plant.y)>=std::int64_t(source.y)-3 && std::int64_t(plant.y)<=std::int64_t(source.y)+4;
}
template<class Rows,class Id> decltype(auto) body(Rows& rows,Id id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& b,Id key){return b.id<key;});
    if(at==rows.end() || at->id!=id) invalid("plantation references a missing paid entity");
    return *at;
}
template<class Rows> decltype(auto) account(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.building_id<key;});
    if(at==rows.end() || at->building_id!=id) invalid("plantation source account missing");
    return *at;
}
template<class Rows> decltype(auto) plant_row(Rows& rows,BuildingId id) {
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[](const auto& row,BuildingId key){return row.plant_id<key;});
    if(at==rows.end() || at->plant_id!=id) invalid("plantation plant authority missing");
    return *at;
}
int cargo(const PlantationTrip& trip) { return trip.picked_tick && !trip.returned_tick ? 1:0; }
const std::optional<std::vector<Cell>>* plantation_cached_route(const CourierState& c,BuildingId id) {
    const auto at=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),id,
        [](const auto& row,BuildingId key){return row.first<key;});
    return at!=c.dynamic_target_routes.end() && at->first==id ? &at->second:nullptr;
}

template<class Buildings,class Couriers>
void validate_plantations(const PlantationsState& state,const Buildings& buildings,const Couriers& couriers,
    const OriginalIndustryState& industry,std::uint64_t tick) {
    if(!state.next_grower_id || state.plants.size()>plantation_plant_limit || state.sources.size()>plantation_source_limit ||
        std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return source_kind(b.kind);})!=static_cast<std::ptrdiff_t>(state.sources.size()) ||
        std::count_if(buildings.begin(),buildings.end(),[](const auto& b){return original_industry_plant(b.kind);})!=static_cast<std::ptrdiff_t>(state.plants.size()) ||
        std::count_if(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalPlantationGrower;})!=static_cast<std::ptrdiff_t>(state.sources.size()))
        invalid("plantation paid bodies, plants and bounded worker channels disagree");
    struct Observation {
        std::uint64_t id;BuildingId source,plant;CourierId courier;std::uint64_t dispatch;
        std::optional<std::uint64_t> pick,returned;
        bool operator==(const Observation&) const = default;
    };
    std::array<Observation,plantation_plant_limit+plantation_source_limit> observations{};
    std::size_t observation_count=0;
    std::uint64_t spawned=0,latest=0;
    for(std::size_t i=0;i<state.sources.size();++i) {
        const auto& row=state.sources[i];const auto& source=body(buildings,row.building_id);const auto& c=body(couriers,row.courier_id);
        if((i && !(state.sources[i-1].building_id<row.building_id)) || !source_kind(source.kind) || !source.placed || source.placed_tick>tick ||
            c.owner!=source.id || c.role!=CourierRole::OriginalPlantationGrower || !c.enabled || c.good!=Good::Goods || c.cargo || c.reserved || c.last_dispatched_pottery)
            invalid("plantation worker has no unique paid source or exclusive channel");
        if(c.phase!=CourierPhase::IdleAtWorkshop && c.phase!=CourierPhase::ToWarehouse && c.phase!=CourierPhase::Returning)
            invalid("plantation worker movement phase is outside its explicit contract");
        add(spawned,row.spawned);
        std::uint64_t returned=0,harvested=0;
        for(const auto& p:state.plants) if(p.source_id==source.id) {add(returned,p.returned);add(harvested,p.harvested);}
        const auto& recipe=account(industry.sources,source.id);
        if(recipe.plant_id || recipe.progress || returned!=recipe.completed || harvested>row.spawned || row.spawned>tick-source.placed_tick)
            invalid("plantation output lacks actual plant pickup/return history");
        if(!row.latest_trip) {
            if(row.spawned || c.target!=source.id || c.last_dispatched_target || c.phase!=CourierPhase::IdleAtWorkshop ||
                !c.path.empty() || c.path_vertex || c.edge_progress || c.route_pending || c.route_checked_revision || c.reroute_attempts)
                invalid("unused plantation channel owns a phantom worker");
            continue;
        }
        const auto& trip=*row.latest_trip;const auto id=static_cast<std::uint64_t>(trip.grower_id);
        const auto& p=plant_row(state.plants,trip.plant_id);const auto& plant=body(buildings,p.plant_id);
        if(!row.spawned || !id || id>=state.next_grower_id || id<row.spawned || p.source_id!=source.id ||
            c.target!=plant.id || c.last_dispatched_target!=plant.id || trip.dispatched_tick<=plant.placed_tick ||
            trip.dispatched_tick<=source.placed_tick || trip.dispatched_tick>tick || trip.delivered>1 ||
            (trip.delivered && !trip.picked_tick)) invalid("plantation trip lost its monotone lifetime or paid target");
        latest=std::max(latest,id);
        observations[observation_count++]={id,source.id,plant.id,c.id,trip.dispatched_tick,trip.picked_tick,trip.returned_tick};
        // Optimistic physical lower bounds, not invented historical paths.
        const auto dx=std::max({std::int64_t{0},std::int64_t(source.cell.x)-plant.cell.x,std::int64_t(plant.cell.x)-source.cell.x-1});
        const auto dy=std::max({std::int64_t{0},std::int64_t(source.cell.y)-plant.cell.y,std::int64_t(plant.cell.y)-source.cell.y-1});
        const auto minimum=static_cast<std::uint64_t>(dx+dy)*5;
        if(!minimum) invalid("plant overlaps its production building");
        if(trip.picked_tick && (*trip.picked_tick<=trip.dispatched_tick || *trip.picked_tick>tick ||
            *trip.picked_tick-trip.dispatched_tick+1<minimum || !p.harvested)) invalid("plantation pickup predates physical arrival or readiness");
        if(trip.returned_tick) {
            if(*trip.returned_tick<=trip.dispatched_tick || *trip.returned_tick>tick ||
                *trip.returned_tick-trip.dispatched_tick+1<2*minimum || c.phase!=CourierPhase::IdleAtWorkshop ||
                trip.delivered!=static_cast<unsigned>(trip.picked_tick.has_value()) ||
                !c.path.empty() || c.path_vertex || c.edge_progress || c.route_pending || c.route_checked_revision ||
                (trip.picked_tick && (*trip.returned_tick<=*trip.picked_tick || *trip.returned_tick-*trip.picked_tick<minimum)))
                invalid("plantation return lacks a possible physical completed trip");
        } else {
            if(trip.delivered || c.phase==CourierPhase::IdleAtWorkshop ||
                (trip.picked_tick && c.phase!=CourierPhase::Returning) || c.path.empty() || c.path_vertex>=c.path.size())
                invalid("active plantation worker has phantom output or impossible movement phase");
            const auto at=c.path[c.path_vertex];
            const bool returning=c.phase==CourierPhase::Returning;
            const auto gap=returning ? std::abs(std::int64_t(at.x)-plant.cell.x)+std::abs(std::int64_t(at.y)-plant.cell.y):
                std::max({std::int64_t{0},std::int64_t(source.cell.x)-at.x,std::int64_t(at.x)-source.cell.x-1})+
                std::max({std::int64_t{0},std::int64_t(source.cell.y)-at.y,std::int64_t(at.y)-source.cell.y-1});
            const auto start=trip.picked_tick.value_or(trip.dispatched_tick);
            const auto outward=returning && !trip.picked_tick ? minimum:0;
            if(tick-start+1<outward+static_cast<std::uint64_t>(gap)*5+static_cast<std::uint64_t>(c.edge_progress))
                invalid("plantation worker moved faster than its shared physical channel");
        }
    }
    if(spawned==UINT64_MAX || state.next_grower_id!=spawned+1 || latest!=spawned)
        invalid("plantation successful-spawn highwater or lifetime IDs changed");
    for(std::size_t i=0;i<state.plants.size();++i) {
        const auto& p=state.plants[i];const auto& plant=body(buildings,p.plant_id);const auto& source=body(buildings,p.source_id);
        const auto& worker=account(state.sources,source.id);
        if((i && !(state.plants[i-1].plant_id<p.plant_id)) || plant.kind!=plant_kind(source.kind) || plant.kind==Object::Empty ||
            !plant.placed || !(source.id<plant.id) || plant.placed_tick<source.placed_tick || plant.placed_tick>tick ||
            !work_area(source.cell,plant.cell) || p.growth>plantation_growth_ticks || p.productive_ticks>tick-plant.placed_tick ||
            p.grown!=p.productive_ticks/plantation_growth_ticks || p.harvested>p.grown || p.returned>p.harvested ||
            p.grown-p.harvested!=static_cast<std::uint64_t>(p.growth==plantation_growth_ticks) ||
            (p.growth==plantation_growth_ticks ? p.productive_ticks%plantation_growth_ticks!=0:
                p.growth!=p.productive_ticks%plantation_growth_ticks))
            invalid("plantation growth/readiness has no paid matching plant and bounded productive work");
        const bool carried=worker.latest_trip && worker.latest_trip->plant_id==plant.id && cargo(*worker.latest_trip);
        if(p.harvested-p.returned!=static_cast<std::uint64_t>(carried) || static_cast<bool>(p.last_receipt)!=(p.returned>0))
            invalid("plantation picked/returned/cargo conservation or receipt missing");
        if(p.last_receipt) {
            const auto& w=*p.last_receipt;const auto id=static_cast<std::uint64_t>(w.grower_id);
            const auto dx=std::max({std::int64_t{0},std::int64_t(source.cell.x)-plant.cell.x,std::int64_t(plant.cell.x)-source.cell.x-1});
            const auto dy=std::max({std::int64_t{0},std::int64_t(source.cell.y)-plant.cell.y,std::int64_t(plant.cell.y)-source.cell.y-1});
            const auto minimum=static_cast<std::uint64_t>(dx+dy)*5;
            if(!id || id<p.returned || id>=state.next_grower_id || !worker.latest_trip || w.courier_id!=worker.courier_id ||
                w.dispatched_tick<=plant.placed_tick || w.picked_tick<=w.dispatched_tick ||
                w.returned_tick<=w.picked_tick || w.returned_tick>tick ||
                !minimum || w.picked_tick-w.dispatched_tick+1<minimum || w.returned_tick-w.picked_tick<minimum ||
                static_cast<std::uint64_t>(worker.latest_trip->grower_id)<id)
                invalid("plantation receipt lost its actual worker lifetime or chronology");
            if(worker.latest_trip->grower_id==w.grower_id) {
                const auto& t=*worker.latest_trip;
                if(t.plant_id!=plant.id || t.dispatched_tick!=w.dispatched_tick || t.picked_tick!=w.picked_tick ||
                    t.returned_tick!=w.returned_tick || t.delivered!=1) invalid("plantation receipt contradicts actual latest return");
            } else if(worker.latest_trip->dispatched_tick<=w.returned_tick)
                invalid("plantation retained receipt postdates its worker's next spawn");
            observations[observation_count++]={id,source.id,plant.id,w.courier_id,w.dispatched_tick,w.picked_tick,w.returned_tick};
        }
    }
    std::sort(observations.begin(),observations.begin()+static_cast<std::ptrdiff_t>(observation_count),
        [](const auto& a,const auto& b){return a.id<b.id;});
    std::array<std::uint64_t,plantation_source_limit> retained{};
    for(std::size_t i=0;i<observation_count;++i) {
        const auto& o=observations[i];
        if(i && observations[i-1].id==o.id) {
            if(observations[i-1]!=o) invalid("one plantation Grower lifetime changed source, plant or actual events");
            continue;
        }
        if(i && (observations[i-1].dispatch>o.dispatch ||
            (observations[i-1].dispatch==o.dispatch && !(observations[i-1].courier<o.courier))))
            invalid("plantation Grower IDs contradict physical monotone spawn order");
        const auto at=std::lower_bound(state.sources.begin(),state.sources.end(),o.source,
            [](const auto& row,BuildingId key){return row.building_id<key;});
        if(at==state.sources.end() || at->building_id!=o.source) invalid("retained Grower source missing");
        auto& n=retained[static_cast<std::size_t>(at-state.sources.begin())];
        if(++n>at->spawned) invalid("retained Grower lifetimes exceed actual source spawns");
    }
}
} // namespace

World World::create_original_plantations(std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    auto world=create_original_agriculture(std::move(permissions),std::move(model),std::move(terrain));
    world.rule_version_=kOriginalPlantationsRuleVersion;world.plantations_.emplace();return world;
}
World World::restore_original_plantations(const WorldSnapshot& s,std::shared_ptr<const MapPermissions> permissions,
    std::shared_ptr<const original::OriginalRulesModel> model,std::shared_ptr<const OriginalTerrainFacts> terrain) {
    if(!simulation::original_plantations_enabled(s.profile,s.rule_version) || !permissions || !model || !terrain ||
        permissions->policy_version()!=kBridgesMapPermissionsPolicyVersion || !terrain->fertility_complete())
        invalid("plantation restore requires exact Rule25, policy3 and source-bound model/terrain");
    validate_original_production_snapshot(s);
    for(const auto& field:s.agriculture->fields) if(field.crop==AgricultureCrop::Rice && terrain->fertility(body(s.buildings,field.field_id).cell)!=100)
        invalid("plantation generation retains Rice fertility100 admission");
    auto world=restore_impl(s,{},std::move(permissions),true,std::move(model),std::move(terrain));
    world.validate_original_hemp_model();world.validate_agriculture_model();world.validate_original_consumption_model();
    world.validate_original_services_model();world.validate_original_water_model();world.validate_original_housing_model();
    world.validate_original_industry_model();world.validate_original_finance_model();return world;
}
void World::validate_original_plantations_snapshot(const WorldSnapshot& s) {
    if(!simulation::original_plantations_enabled(s.profile,s.rule_version)) {
        if(s.plantations || std::any_of(s.couriers.begin(),s.couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalPlantationGrower;}))
            invalid("plantation authority requires exact Agriculture Rule25");
        return;
    }
    if(!s.plantations || !s.original_production) invalid("plantation generation lacks explicit plant/worker authority");
    validate_plantations(*s.plantations,s.buildings,s.couriers,s.original_production->industry,s.ticks);
}
void World::validate_plantations_state() const {
    if(original_plantations_enabled()) validate_plantations(*plantations_,buildings_,couriers_,original_production_->industry,ticks_);
}
std::optional<BuildingId> World::plantation_source_for_plant(Object kind,Cell cell,std::optional<BuildingId> preferred) const {
    if(!original_plantations_enabled() || !original_industry_plant(kind) || !in_bounds(cell) || plantations_->plants.size()>=plantation_plant_limit)
        return {};
    std::optional<BuildingId> result;
    for(const auto& row:plantations_->sources) {
        const auto& b=building(row.building_id);
        if(plant_kind(b.kind)!=kind || !work_area(b.cell,cell) || (preferred && static_cast<std::uint32_t>(*preferred) && b.id!=*preferred)) continue;
        if(result) return {}; // Ambiguous ownership needs an explicit selection.
        result=b.id;
    }
    return result;
}
int World::plantation_grower_cargo(CourierId id) const {
    if(!original_plantations_enabled()) return 0;
    const auto& c=courier(id);if(c.role!=CourierRole::OriginalPlantationGrower) return 0;
    const auto& row=account(plantations_->sources,c.owner);return row.latest_trip ? cargo(*row.latest_trip):0;
}
bool World::plantation_cell_allowed(Cell cell,BuildingId source,BuildingId plant) const {
    if(!original_plantations_enabled() || !in_bounds(cell) || !buildable_[index(cell)] ||
        !work_area(building(source).cell,cell)) return false;
    const auto owner=building_owner_at(cell);
    if(!owner) return object_at(cell)==Object::Empty || object_at(cell)==Object::Road;
    if(*owner==source || *owner==plant) return true;
    const auto at=std::lower_bound(plantations_->plants.begin(),plantations_->plants.end(),*owner,[](const auto& p,BuildingId key){return p.plant_id<key;});
    return at!=plantations_->plants.end() && at->plant_id==*owner && at->source_id==source;
}
bool World::plantation_edge_allowed(Cell from,Cell to,BuildingId source,BuildingId plant) const {
    return std::abs(from.x-to.x)+std::abs(from.y-to.y)==1 && plantation_cell_allowed(from,source,plant) &&
        plantation_cell_allowed(to,source,plant) && permission_edge_allowed(from,to);
}
std::optional<std::vector<Cell>> World::plantation_route(Cell start,BuildingId source,BuildingId plant,bool returning) const {
    std::optional<std::vector<Cell>> best;
    const auto source_cells=building_footprint_cells(profile_,rule_version_,building(source).kind,building(source).cell);
    const auto consider=[&](Cell from,Cell to) {
        auto path=find_road_route(from,to,source,plant);
        if(path && path->size()>1 && (!best || path->size()<best->size())) best=std::move(path);
    };
    if(returning) {for(const auto to:source_cells) consider(start,to);}
    else if(start.x<0) {for(const auto from:source_cells) consider(from,building(plant).cell);}
    else consider(start,building(plant).cell);
    return best;
}
int World::plantation_output_room(BuildingId id) const {
    if(building(id).kind==Object::OriginalTeaCuringShed237) return original_market_headroom(id,original::Goods::Tea);
    const auto material=building(id).kind==Object::OriginalLacquerRefinery238 ? OriginalAuxMaterial::Lacquer:OriginalAuxMaterial::RawSilk;
    auto room=8-account(original_production_->industry.auxiliary,id).amounts[static_cast<std::size_t>(material)];
    for(const auto& t:original_production_->industry.aux_trips) if(t.source==id || (t.target==id && t.phase==OriginalGoodsTransferPhase::Loaded))
        room-=t.loaded-t.delivered-t.returned;
    return room;
}
void World::update_plantations() {
    for(auto& p:plantations_->plants) {
        const auto& source=building(p.source_id);
        if(p.growth==plantation_growth_ticks || !source.operating_enabled || building_on_fire(source.id) ||
            !building(p.plant_id).operating_enabled || !building_staffed_for_tick(source.id) || building_entrances(source.id).empty()) continue;
        add(p.productive_ticks,1);++p.growth;
        if(p.growth==plantation_growth_ticks) add(p.grown,1);
    }
}
CourierDispatchDecision World::plantation_dispatch_status(const CourierState& c) const {
    if(!original_plantations_enabled() || !c.enabled) return {CourierDispatchStatus::Disabled,{}};
    if(c.phase!=CourierPhase::IdleAtWorkshop) return {c.route_pending ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::AlreadyMoving,c.target};
    const auto& source=building(c.owner);
    if(!source.operating_enabled) return {CourierDispatchStatus::OperationPaused,{}};
    if(building_on_fire(source.id)) return {CourierDispatchStatus::OnFire,{}};
    if(!building_staffed_for_tick(source.id)) return {CourierDispatchStatus::Unstaffed,{}};
    if(c.cached_revision!=road_revision_ || building_entrances(source.id).empty()) return {CourierDispatchStatus::NoRoad,{}};
    if(plantation_output_room(source.id)<=0) return {CourierDispatchStatus::TargetFull,{}};
    const auto& worker=account(plantations_->sources,source.id);
    if(worker.spawned==UINT64_MAX || plantations_->next_grower_id==UINT64_MAX) return {CourierDispatchStatus::Disabled,{}};
    bool ready=false;
    // Complete canonical plant cursor, so regrowth cannot starve later plants.
    for(int pass=0;pass<2;++pass) for(const auto& p:plantations_->plants) {
        if(p.source_id!=source.id || p.growth!=plantation_growth_ticks || !building(p.plant_id).operating_enabled ||
            (c.last_dispatched_target && (pass==0 ? p.plant_id<=*c.last_dispatched_target:p.plant_id>*c.last_dispatched_target)) ||
            (!c.last_dispatched_target && pass==1)) continue;
        ready=true;const auto* path=plantation_cached_route(c,p.plant_id);
        if(path && *path) return {CourierDispatchStatus::Ready,p.plant_id};
    }
    return {ready ? CourierDispatchStatus::NoRoad:CourierDispatchStatus::NoStock,{}};
}
void World::dispatch_plantation_grower(CourierState& c) {
    const auto decision=plantation_dispatch_status(c);
    if(decision.status!=CourierDispatchStatus::Ready || !decision.selected_target) return;
    const auto* route=plantation_cached_route(c,*decision.selected_target);
    if(!route || !*route) throw std::logic_error("plantation dispatch lost its prepared physical route");
    auto& worker=account(plantations_->sources,c.owner);
    worker.latest_trip=PlantationTrip{static_cast<PlantationGrowerId>(plantations_->next_grower_id++),*decision.selected_target,ticks_,{},{},0};
    ++worker.spawned;c.target=*decision.selected_target;c.last_dispatched_target=c.target;
    c.path=**route;c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::arrive_plantation_grower(CourierState& c) {
    auto& worker=account(plantations_->sources,c.owner);
    if(!worker.latest_trip || worker.latest_trip->returned_tick || worker.latest_trip->plant_id!=c.target)
        throw std::logic_error("plantation arrival is duplicated or orphaned");
    auto& trip=*worker.latest_trip;auto& p=plant_row(plantations_->plants,c.target);const auto& source=building(c.owner);
    if(c.phase==CourierPhase::ToWarehouse) {
        if(trip.picked_tick) throw std::logic_error("plantation worker harvested twice");
        if(source.operating_enabled && !building_on_fire(source.id) && building_staffed_for_tick(source.id) &&
            building(p.plant_id).operating_enabled && !building_entrances(source.id).empty() && p.growth==plantation_growth_ticks) {
            p.growth=0;add(p.harvested,1);trip.picked_tick=ticks_;
        }
        c.phase=CourierPhase::Returning;
        if(valid_return_path(c)) {std::reverse(c.path.begin(),c.path.end());c.path_vertex=0;}
        else {c.path={building(p.plant_id).cell};c.path_vertex=0;c.route_pending=true;c.route_checked_revision.reset();reroute_v2(c);}
        return;
    }
    // Physical return settles retained cargo even if work was paused or burned.
    // The single source-owned trip reserved one output slot at dispatch.
    if(trip.picked_tick) {
        if(plantation_output_room(source.id)<=0) throw std::logic_error("plantation return lost its reserved source capacity");
        auto& recipe=account(original_production_->industry.sources,source.id);
        if(source.kind==Object::OriginalTeaCuringShed237) {
            const original_industry_goods::Context context(buildings_,couriers_);
            OriginalGoodsLedgerSnapshot current{*original_goods_,original_economy_->houses};
            auto next=original_goods_produce(context.view(current.goods,current.houses),source.id,original::Goods::Tea,1);
            original_goods_=std::move(next.goods);
        } else {
            const auto material=source.kind==Object::OriginalLacquerRefinery238 ? OriginalAuxMaterial::Lacquer:OriginalAuxMaterial::RawSilk;
            ++account(original_production_->industry.auxiliary,source.id).amounts[static_cast<std::size_t>(material)];
        }
        add(recipe.completed,1);add(p.returned,1);trip.delivered=1;
        p.last_receipt=PlantationReceipt{trip.grower_id,c.id,trip.dispatched_tick,*trip.picked_tick,ticks_};
    }
    trip.returned_tick=ticks_;c.phase=CourierPhase::IdleAtWorkshop;c.path.clear();c.path_vertex=0;c.edge_progress=0;
    c.route_pending=false;c.route_checked_revision.reset();
}

} // namespace openemperor::simulation
