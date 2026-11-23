#include "simulation/World.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* why){throw std::invalid_argument(why);}
void add(std::uint64_t& n,std::uint64_t delta=1){if(delta>UINT64_MAX-n)invalid("Physical Grower history overflow");n+=delta;}
template<class Rows,class Id,class Key> decltype(auto) row(Rows& rows,Id id,Key key){
    auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& value,Id target){return key(value)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Physical Grower lost paid identity");return *at;
}
const auto body_key=[](const auto& b){return b.id;};
const auto farm_key=[](const auto& b){return b.farm_id;};
const auto field_key=[](const auto& b){return b.field_id;};
const auto stock_key=[](const auto& b){return b.building_id;};
bool work_area(Cell farm,Cell cell){return std::int64_t(cell.x)>=std::int64_t(farm.x)-3&&std::int64_t(cell.x)<=std::int64_t(farm.x)+5&&
    std::int64_t(cell.y)>=std::int64_t(farm.y)-3&&std::int64_t(cell.y)<=std::int64_t(farm.y)+5;}
std::uint64_t gap(Cell farm,Cell field){return static_cast<std::uint64_t>(
    std::max({std::int64_t{0},std::int64_t(farm.x)-field.x,std::int64_t(field.x)-farm.x-2})+
    std::max({std::int64_t{0},std::int64_t(farm.y)-field.y,std::int64_t(field.y)-farm.y-2}));}
struct Observation {std::uint64_t id;BuildingId farm,field;CourierId courier;AgricultureCrop crop;std::uint64_t dispatch,arrival,harvest,returned;
    bool operator==(const Observation&)const=default;};
template<class Buildings,class Couriers,class Food>
void validate_physical_state(const PhysicalFoodGrowersState& state,const Food& food,const MultiFieldAgricultureState& multi,
    const Buildings& buildings,const Couriers& couriers,std::uint64_t tick,const SeasonalAgricultureState* season=nullptr,const IrrigationProductivityState* irrigation=nullptr,const AgricultureReconfigurationState* reconfiguration=nullptr){
    if(!irrigation){
        for(const auto& f:state.farms)if(f.latest_trip&&f.latest_trip->irrigation_bonus_before)invalid("Historical Food trip has irrigation work authority");
        for(const auto& f:state.fields)if(f.last_receipt&&f.last_receipt->irrigation_bonus_at_harvest)invalid("Historical Food receipt has irrigation work authority");
    }
    if(!state.next_grower_id||state.farms.size()!=food.farms.size()||state.fields.size()!=food.fields.size()||
       state.farms.size()>5||state.fields.size()>agriculture_field_limit)invalid("Physical Grower collection or ID bounds invalid");
    if(std::count_if(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalFoodGrower;})!=static_cast<std::ptrdiff_t>(state.farms.size()))
        invalid("Physical Grower requires exactly one paid Farm channel");
    std::uint64_t spawned=0,latest=0;std::vector<Observation> observations;observations.reserve(37);
    for(std::size_t i=0;i<state.farms.size();++i){
        const auto& source=state.farms[i];const auto& farm=row(buildings,source.farm_id,body_key);const auto& c=row(couriers,source.courier_id,body_key);
        if(source.farm_id!=food.farms[i].farm_id||farm.kind!=Object::OriginalFoodFarm||c.owner!=farm.id||c.role!=CourierRole::OriginalFoodGrower||
           c.good!=Good::Food||c.cargo||c.reserved||!c.enabled||source.spawned>tick-farm.placed_tick)
            invalid("Physical Grower source/channel/legacy mirrors invalid");
        add(spawned,source.spawned);
        const bool live=source.latest_trip&&!source.latest_trip->returned_tick;
        // Even an empty cancellation must traverse one five-tick edge out
        // and one home. A source has only one live movement channel.
        if(source.retired>source.spawned||source.spawned-source.retired!=static_cast<std::uint64_t>(live)||
           source.retired>(tick-farm.placed_tick)/10)invalid("Physical Grower retirement count invalid");
        std::uint64_t harvested=0,returned=0,earned_work=0;
        for(const auto& field:food.fields)if(field.farm_id==farm.id)add(harvested,field.harvested);
        for(const auto& field:state.fields)if(field.farm_id==farm.id)add(returned,field.returned);
        for(const auto& field:multi.fields)if(field.farm_id==farm.id)add(earned_work,field.productive_ticks);
        if(season)for(const auto& field:season->fields)if(field.farm_id==farm.id)add(earned_work,field.harvest_work);
        if(harvested>source.spawned||returned>source.retired)invalid("Physical Grower harvest/receipt exceeds allocated lifetimes");
        // Harvest work, its five waiting ticks and completed physical trips
        // share one source timeline. The first return microstep may coincide
        // with the last work tick, hence nine additional ticks per retirement.
        const auto age=tick-farm.placed_tick;
        if(earned_work>age||harvested>(age-earned_work)/physical_food_grower_wait_ticks||
           source.retired>(age-earned_work-harvested*physical_food_grower_wait_ticks)/9)
            invalid("Physical Grower work/wait/return histories exceed source lifetime");
        if(!source.latest_trip){
            if(source.spawned||source.retired||c.phase!=CourierPhase::IdleAtWorkshop||c.target!=farm.id||c.last_dispatched_target||
               !c.path.empty()||c.path_vertex||c.edge_progress||c.route_pending||c.route_checked_revision||c.reroute_attempts)
                invalid("Unused Food Grower channel owns a phantom actor");
            continue;
        }
        const auto& t=*source.latest_trip;const auto& f=row(food.fields,t.field_id,field_key);const auto& paid=row(buildings,t.field_id,body_key);
        const auto through=agriculture_retirement_tick(reconfiguration,t.field_id,tick);
        const auto* retired=agriculture_reconfiguration_entry(reconfiguration,t.field_id);
        if(!paid.placed&&(!retired||!retired->removed_tick))invalid("Unplaced Food Field lacks closed reconfiguration history");
        if(through>tick||through<paid.placed_tick||(!paid.placed&&(!t.returned_tick||*t.returned_tick>through)))invalid("Retired Food Field owns a live or later Grower assignment");
        const auto& work=row(multi.fields,t.field_id,field_key);const auto* seasonal=season?&row(season->fields,t.field_id,field_key):nullptr;
        if((!season&&(t.wasted_work_before||t.ready_tick))||(seasonal&&(t.wasted_work_before>seasonal->wasted_work||t.wasted_work_before>t.work_before+t.irrigation_bonus_before)))invalid("Food trip lost seasonal work provenance");
        const auto g=static_cast<std::size_t>(t.crop);const auto minimum=gap(farm.cell,paid.cell)*5;
        if(!source.spawned||!t.grower_id.value||t.grower_id.value>=state.next_grower_id||t.grower_id.value<source.spawned||
           f.farm_id!=farm.id||g>=agriculture_crop_count||f.crop!=t.crop||!minimum||
           t.dispatched_tick<=std::max(farm.placed_tick,paid.placed_tick)||t.dispatched_tick>tick||
           source.spawned-1>(t.dispatched_tick-farm.placed_tick-1)/10||
           c.target!=f.field_id||c.last_dispatched_target!=f.field_id||t.work_ticks>80-(t.work_before+t.irrigation_bonus_before-t.wasted_work_before)%80||
           t.work_before>UINT64_MAX-t.work_ticks||work.productive_ticks!=t.work_before+t.work_ticks||t.delivered>1||
           t.wait_left>physical_food_grower_wait_ticks)
            invalid("Physical Grower trip identity/crop/work bounds invalid");
        latest=std::max(latest,t.grower_id.value);
        if(t.phase!=PhysicalFoodGrowerPhase::Outward&&t.phase!=PhysicalFoodGrowerPhase::Waiting&&t.phase!=PhysicalFoodGrowerPhase::Working&&t.phase!=PhysicalFoodGrowerPhase::Returning)
            invalid("Unknown physical Grower phase");
        if(t.arrived_tick&&(*t.arrived_tick<t.dispatched_tick||*t.arrived_tick>tick||*t.arrived_tick-t.dispatched_tick+1<minimum))
            invalid("Physical Grower arrived before traversing real land");
        if(t.harvested_tick&&(!t.arrived_tick||*t.harvested_tick<=*t.arrived_tick||*t.harvested_tick>tick||
            *t.harvested_tick-*t.arrived_tick<physical_food_grower_wait_ticks+t.work_ticks||(!season&&!t.work_ticks)||
            (!season&&(t.work_before+t.work_ticks)%80)||
            (season&&(!t.ready_tick||*t.ready_tick>=*t.harvested_tick||*t.harvested_tick-*t.ready_tick<seasonal_harvest_work_ticks||!seasonal_harvest_allowed(static_cast<original::ModelBuilding>(195+g),*t.harvested_tick,*t.ready_tick)))))invalid("Physical Grower harvest lacks actual waiting/work");
        if(t.ready_tick&&(!season||*t.ready_tick>tick||!seasonal_growing(static_cast<original::ModelBuilding>(195+g),*t.ready_tick)))invalid("Food trip has no actual growing ready epoch");
        if(t.ready_tick&&t.work_ticks&&(!t.arrived_tick||*t.ready_tick<*t.arrived_tick||
            *t.ready_tick-*t.arrived_tick<physical_food_grower_wait_ticks+t.work_ticks))
            invalid("Food readiness predates the worker's actual field work");
        if(t.work_ticks&&(!t.arrived_tick||tick-*t.arrived_tick<physical_food_grower_wait_ticks+t.work_ticks))
            invalid("Physical Grower work predates arrival/wait");
        if(t.phase==PhysicalFoodGrowerPhase::Working&&t.arrived_tick&&tick-*t.arrived_tick<physical_food_grower_wait_ticks)invalid("Food Grower skipped its real waiting interval");
        if(t.phase==PhysicalFoodGrowerPhase::Waiting&&t.arrived_tick&&tick-*t.arrived_tick<physical_food_grower_wait_ticks-t.wait_left)invalid("Food Grower waiting clock advanced without real time");
        if(t.phase==PhysicalFoodGrowerPhase::Outward ? (t.arrived_tick||t.harvested_tick||t.work_ticks||t.wait_left):
           t.phase==PhysicalFoodGrowerPhase::Waiting ? (!t.arrived_tick||!t.wait_left||t.work_ticks||t.harvested_tick):
           t.phase==PhysicalFoodGrowerPhase::Working ? (!t.arrived_tick||t.wait_left||t.harvested_tick):t.wait_left!=0)
            invalid("Physical Grower phase and waiting/work stamps disagree");
        if(t.phase==PhysicalFoodGrowerPhase::Returning ? c.phase!= (live?CourierPhase::Returning:CourierPhase::IdleAtWorkshop):
            (!live||c.phase!=CourierPhase::ToWarehouse))invalid("Physical Grower phase and shared movement disagree");
        if(!live){
            if(*t.returned_tick<=t.dispatched_tick||*t.returned_tick-t.dispatched_tick+1<10||*t.returned_tick>tick||t.delivered!=static_cast<unsigned>(t.harvested_tick.has_value())||
               !c.path.empty()||c.path_vertex||c.edge_progress||c.route_pending||c.route_checked_revision||
               (t.harvested_tick&&(*t.returned_tick<=*t.harvested_tick||*t.returned_tick-*t.harvested_tick+1<minimum)))
                invalid("Physical Grower retirement lacks actual source return");
        }else if(t.delivered||c.path.empty()||c.path_vertex>=c.path.size())invalid("Live Food Grower has no retained physical position");
        if(seasonal&&live&&t.phase!=PhysicalFoodGrowerPhase::Returning&&seasonal->ready_tick&&t.ready_tick!=seasonal->ready_tick)
            invalid("Ready physical Food worker lost its bound crop cycle");
        if(live&&(t.phase==PhysicalFoodGrowerPhase::Working||t.phase==PhysicalFoodGrowerPhase::Waiting)&&
           (c.path.size()!=1||c.path.front()!=paid.cell||c.path_vertex||c.edge_progress||c.route_pending))
            invalid("Working Food Grower is not physically at its assigned field");
        if(live){const auto here=c.path.at(c.path_vertex);
            const auto walked=t.harvested_tick ? static_cast<std::uint64_t>(std::abs(std::int64_t(here.x)-paid.cell.x)+std::abs(std::int64_t(here.y)-paid.cell.y))*5:gap(farm.cell,here)*5;
            const auto began=t.harvested_tick.value_or(t.dispatched_tick);
            if(tick-began+1<walked+static_cast<std::uint64_t>(c.edge_progress))invalid("Food Grower position exceeds actual elapsed land movement");
        }
        observations.push_back({t.grower_id.value,farm.id,paid.id,c.id,t.crop,t.dispatched_tick,t.arrived_tick.value_or(0),t.harvested_tick.value_or(0),t.returned_tick.value_or(0)});
        int held=0;for(const auto n:row(food.stocks,farm.id,stock_key).amounts){if(n<0||n>8)invalid("Invalid Farm Food stock");held+=n;}
        int reserved=live?1:0;
        for(const auto& cart:food.trips)if(cart.source==farm.id)for(std::size_t k=0;k<7;++k){const auto n=std::int64_t(cart.loaded[k])-cart.delivered[k]-cart.returned[k];if(n<0||n>4)invalid("Invalid cart return reservation");reserved+=static_cast<int>(n);}
        if(held+reserved>8)invalid("Food Grower return reservation exceeds actual Farm capacity");
    }
    if(spawned==UINT64_MAX||state.next_grower_id!=spawned+1||latest!=spawned)invalid("Physical Grower monotone global highwater invalid");
    for(std::size_t i=0;i<state.fields.size();++i){
        const auto& p=state.fields[i];const auto& f=food.fields[i];const auto& paid=row(buildings,p.field_id,body_key);const auto& farm=row(buildings,p.farm_id,body_key);
        const auto through=agriculture_retirement_tick(reconfiguration,p.field_id,tick);
        const auto* retired=agriculture_reconfiguration_entry(reconfiguration,p.field_id);
        if(!paid.placed&&(!retired||!retired->removed_tick))invalid("Unplaced Food Field lacks closed reconfiguration history");
        if(through>tick||through<paid.placed_tick)invalid("Food Field retirement chronology invalid");
        const auto& worker=row(state.farms,p.farm_id,farm_key);const auto& c=row(couriers,worker.courier_id,body_key);
        const bool carried=worker.latest_trip&&worker.latest_trip->field_id==p.field_id&&physical_food_trip_cargo(*worker.latest_trip);
        if(p.field_id!=f.field_id||p.farm_id!=f.farm_id||p.returned>f.harvested||f.harvested-p.returned!=static_cast<unsigned>(carried)||
           p.last_receipt.has_value()!=(p.returned!=0))invalid("Physical Field harvest/cargo/receipt equation invalid");
        if(!p.last_receipt)continue;
        const auto& r=*p.last_receipt;const auto minimum=gap(farm.cell,paid.cell)*5;
        if(!r.grower_id.value||r.grower_id.value>=state.next_grower_id||r.courier_id!=c.id||r.crop!=f.crop||
           r.dispatched_tick<=std::max(farm.placed_tick,paid.placed_tick)||r.arrived_tick<r.dispatched_tick||r.arrived_tick-r.dispatched_tick+1<minimum||
           r.harvested_tick<=r.arrived_tick||r.harvested_tick-r.arrived_tick<physical_food_grower_wait_ticks+1||
           r.returned_tick<=r.harvested_tick||r.returned_tick-r.harvested_tick+1<minimum||r.returned_tick>through||
           r.wasted_work_at_harvest>r.work_at_harvest+r.irrigation_bonus_at_harvest||
           (r.work_at_harvest+r.irrigation_bonus_at_harvest-r.wasted_work_at_harvest)/80!=p.returned||(r.work_at_harvest+r.irrigation_bonus_at_harvest-r.wasted_work_at_harvest)%80||!worker.latest_trip||
           (!season&&(r.wasted_work_at_harvest||r.ready_tick))||
           (season&&(!r.ready_tick||r.ready_tick>=r.harvested_tick||r.harvested_tick-r.ready_tick<seasonal_harvest_work_ticks||
            r.work_at_harvest>seasonal_work_opportunities(static_cast<original::ModelBuilding>(195+static_cast<unsigned>(f.crop)),std::max(farm.placed_tick,paid.placed_tick),r.ready_tick)||
            !seasonal_harvest_allowed(static_cast<original::ModelBuilding>(195+static_cast<unsigned>(f.crop)),r.harvested_tick,r.ready_tick)||
            r.wasted_work_at_harvest>row(season->fields,p.field_id,field_key).wasted_work)))
            invalid("Food Grower receipt lacks bound crop/lifetime/physical chronology");
        if(worker.latest_trip->grower_id==r.grower_id){const auto& t=*worker.latest_trip;
            if(t.field_id!=p.field_id||t.dispatched_tick!=r.dispatched_tick||t.arrived_tick!=r.arrived_tick||t.harvested_tick!=r.harvested_tick||t.returned_tick!=r.returned_tick||t.delivered!=1||
               (season&&(t.ready_tick!=r.ready_tick||t.wasted_work_before!=r.wasted_work_at_harvest)))
                invalid("Food receipt contradicts latest actual return");
        }else if(worker.latest_trip->grower_id.value<r.grower_id.value||worker.latest_trip->dispatched_tick<=r.returned_tick)
            invalid("Food receipt postdates source's later lifetime");
        observations.push_back({r.grower_id.value,farm.id,paid.id,c.id,r.crop,r.dispatched_tick,r.arrived_tick,r.harvested_tick,r.returned_tick});
    }
    std::sort(observations.begin(),observations.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    for(std::size_t i=1;i<observations.size();++i){
        const auto& before=observations[i-1];const auto& after=observations[i];
        if(before.id==after.id){
            if(before!=after)invalid("Food Grower lifetime identity reused across physical contributors");
        }else if(after.dispatch<before.dispatch)invalid("Food Grower monotone lifetime allocation reverses dispatch chronology");
    }
    for(const auto& farm:state.farms){
        const Observation* previous=nullptr;
        for(const auto& observation:observations)if(observation.farm==farm.farm_id){
            if(previous&&previous->id!=observation.id&&(!previous->returned||observation.dispatch<=previous->returned))
                invalid("One Farm allocated overlapping physical Grower lifetimes");
            previous=&observation;
        }
    }
}
}
void World::validate_physical_food_growers_snapshot(const WorldSnapshot& s){
    if(!simulation::physical_food_growers_enabled(s.profile,s.rule_version)){
        if(s.physical_food_growers||std::any_of(s.couriers.begin(),s.couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalFoodGrower;}))
            invalid("Physical Food Growers require exact Rule31/schema47");
        return;
    }
    if(!s.physical_food_growers||!s.multi_field_agriculture||
       (simulation::food_processing_enabled(s.profile,s.rule_version)?(!s.processed_food||s.hunting_food):(!s.hunting_food||s.processed_food)))
        invalid("Physical Food Growers lost sole exact-generation Food/work authority");
    const auto validate=[&](const auto& food){validate_physical_state(*s.physical_food_growers,food,*s.multi_field_agriculture,s.buildings,s.couriers,s.ticks,s.seasonal_agriculture?&*s.seasonal_agriculture:nullptr,s.irrigation_productivity?&*s.irrigation_productivity:nullptr,simulation::agriculture_reconfiguration_enabled(s.profile,s.rule_version)&&s.agriculture_reconfiguration?&*s.agriculture_reconfiguration:nullptr);};
    if(s.processed_food)validate(*s.processed_food);else validate(*s.hunting_food);
}
void World::validate_physical_food_growers_state()const{
    if(physical_food_growers_enabled())with_agriculture_food([&](const auto& food){validate_physical_state(*physical_food_growers_,food,*multi_field_agriculture_,buildings_,couriers_,ticks_,seasonal_agriculture_?&*seasonal_agriculture_:nullptr,irrigation_productivity_?&*irrigation_productivity_:nullptr,agriculture_reconfiguration_enabled()?&*agriculture_reconfiguration_:nullptr);});
}
int World::physical_food_grower_cargo(CourierId id)const{
    if(!physical_food_growers_)return 0;const auto& c=courier(id);if(c.role!=CourierRole::OriginalFoodGrower)return 0;
    const auto& f=row(physical_food_growers_->farms,c.owner,farm_key);return f.latest_trip?physical_food_trip_cargo(*f.latest_trip):0;
}
bool World::physical_food_grower_cell_allowed(Cell cell,BuildingId source,BuildingId field)const{
    if(!physical_food_growers_enabled()||!in_bounds(cell)||!buildable_[index(cell)]||!work_area(building(source).cell,cell)||
       !original_terrain_facts_||!original_terrain_facts_->irrigation_boundary_allowed(cell)||!permissions_||
       permissions_->protected_original(cell)||permissions_->fixed_passage(cell)||!permissions_->road_allowed(cell))return false;
    const auto raw=original_terrain_facts_->raw_terrain(cell);
    if(raw!=0x80U&&raw!=0xc0U)return false;
    if(irrigation_&&object_at(cell)!=Object::Road&&std::any_of(irrigation_->ditches.begin(),irrigation_->ditches.end(),[&](const auto& d){return d.cell==cell;}))return false;
    const auto fields=agriculture_fields();const auto& f=row(fields,field,field_key);if(f.farm_id!=source)return false;
    const auto owner=building_owner_at(cell);if(!owner)return object_at(cell)==Object::Empty||object_at(cell)==Object::Road;
    if(*owner==source)return true;
    const auto at=std::lower_bound(fields.begin(),fields.end(),*owner,[](const auto& p,BuildingId id){return p.field_id<id;});
    return at!=fields.end()&&at->field_id==*owner&&at->farm_id==source;
}
bool World::physical_food_grower_edge_allowed(Cell a,Cell b,BuildingId source,BuildingId field)const{
    return std::abs(a.x-b.x)+std::abs(a.y-b.y)==1&&physical_food_grower_cell_allowed(a,source,field)&&
        physical_food_grower_cell_allowed(b,source,field)&&permission_edge_allowed(a,b);
}
std::optional<std::vector<Cell>> World::physical_food_grower_route(Cell start,BuildingId source,BuildingId field,bool returning)const{
    std::optional<std::vector<Cell>> best;const auto consider=[&](Cell a,Cell b){auto p=find_road_route(a,b,source,field);if(p&&p->size()>1&&(!best||p->size()<best->size()))best=std::move(p);};
    const auto cells=building_cells(source);
    if(returning){for(const auto to:cells)consider(start,to);}else if(start.x<0){for(const auto from:cells)consider(from,building(field).cell);}else consider(start,building(field).cell);
    return best;
}
bool World::physical_food_grower_stationary(const CourierState& c)const{
    if(c.role!=CourierRole::OriginalFoodGrower||!physical_food_growers_||c.phase!=CourierPhase::ToWarehouse||c.edge_progress||c.route_pending||c.path.size()!=1||c.path_vertex)return false;
    const auto& f=row(physical_food_growers_->farms,c.owner,farm_key);if(!f.latest_trip||f.latest_trip->returned_tick)return false;
    const auto& t=*f.latest_trip;return (t.phase==PhysicalFoodGrowerPhase::Waiting||t.phase==PhysicalFoodGrowerPhase::Working)&&c.target==t.field_id&&c.path.front()==building(t.field_id).cell;
}
CourierDispatchDecision World::physical_food_grower_dispatch_status(const CourierState& c)const{
    if(!physical_food_growers_enabled()||!c.enabled)return {CourierDispatchStatus::Disabled,{}};
    if(c.phase!=CourierPhase::IdleAtWorkshop)return {c.route_pending?CourierDispatchStatus::NoRoad:CourierDispatchStatus::AlreadyMoving,c.target};
    const auto& source=building(c.owner);if(!source.operating_enabled)return {CourierDispatchStatus::OperationPaused,{}};
    if(building_on_fire(source.id))return {CourierDispatchStatus::OnFire,{}};
    if(!building_staffed_for_tick(source.id))return {CourierDispatchStatus::Unstaffed,{}};
    if(c.cached_revision!=road_revision_||building_entrances(source.id).empty())return {CourierDispatchStatus::NoRoad,{}};
    if(original_food_headroom(source.id)<=0)return {CourierDispatchStatus::TargetFull,{}};
    const auto& worker=row(physical_food_growers_->farms,source.id,farm_key);
    if(worker.spawned==UINT64_MAX||physical_food_growers_->next_grower_id==UINT64_MAX)return {CourierDispatchStatus::Disabled,{}};
    bool eligible=false;
    for(int pass=0;pass<2;++pass)for(const auto& f:agriculture_fields()){
        if(f.farm_id!=source.id||!building(f.field_id).placed||!building(f.field_id).operating_enabled||building_on_fire(f.field_id)||
           (agriculture_draining(f.field_id)&&agriculture_field_progress(f.field_id)==0)||
           (c.last_dispatched_target&&(pass==0?f.field_id<=*c.last_dispatched_target:f.field_id>*c.last_dispatched_target))||(!c.last_dispatched_target&&pass==1))continue;
        if(seasonal_agriculture_enabled()){
            const auto& cycle=row(seasonal_agriculture_->fields,f.field_id,field_key);
            const auto model=static_cast<original::ModelBuilding>(195+static_cast<unsigned>(f.crop));
            if(!seasonal_growing(model,ticks_)&&(!cycle.ready_tick||!seasonal_harvest_allowed(model,ticks_,*cycle.ready_tick)))continue;
        }
        eligible=true;const auto at=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),f.field_id,[](const auto& p,BuildingId id){return p.first<id;});
        if(at!=c.dynamic_target_routes.end()&&at->first==f.field_id&&at->second)return {CourierDispatchStatus::Ready,f.field_id};
    }
    return {eligible?CourierDispatchStatus::NoRoad:CourierDispatchStatus::NoTarget,{}};
}
void World::dispatch_physical_food_grower(CourierState& c){
    const auto decision=physical_food_grower_dispatch_status(c);if(decision.status!=CourierDispatchStatus::Ready||!decision.selected_target)return;
    const auto id=*decision.selected_target;const auto at=std::lower_bound(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),id,[](const auto& p,BuildingId key){return p.first<key;});
    if(at==c.dynamic_target_routes.end()||!at->second)throw std::logic_error("Food Grower lost prepared route");
    auto& f=row(physical_food_growers_->farms,c.owner,farm_key);PhysicalFoodGrowerTrip t;
    const auto fields=agriculture_fields();
    t.grower_id={physical_food_growers_->next_grower_id++};t.field_id=id;t.crop=row(fields,id,field_key).crop;
    t.dispatched_tick=ticks_;t.work_before=row(multi_field_agriculture_->fields,id,field_key).productive_ticks;
    if(seasonal_agriculture_enabled()){const auto& cycle=row(seasonal_agriculture_->fields,id,field_key);t.wasted_work_before=cycle.wasted_work;t.ready_tick=cycle.ready_tick;}
    if(irrigation_productivity_enabled())t.irrigation_bonus_before=irrigation_bonus(&*irrigation_productivity_,id);
    f.latest_trip=t;add(f.spawned);c.target=id;c.last_dispatched_target=id;c.path=*at->second;c.path_vertex=0;c.edge_progress=0;
    c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::begin_physical_food_grower_return(CourierState& c){
    auto& t=*row(physical_food_growers_->farms,c.owner,farm_key).latest_trip;
    if(c.edge_progress){t.cancel_requested=true;return;}
    t.phase=PhysicalFoodGrowerPhase::Returning;t.wait_left=0;c.phase=CourierPhase::Returning;
    mark_route_pending(c);reroute_v2(c);
}
void World::arrive_physical_food_grower(CourierState& c){
    auto& source=row(physical_food_growers_->farms,c.owner,farm_key);if(!source.latest_trip||source.latest_trip->returned_tick)throw std::logic_error("Food Grower duplicated arrival");
    auto& t=*source.latest_trip;
    if(c.phase==CourierPhase::ToWarehouse){
        if(c.path.at(c.path_vertex)!=building(t.field_id).cell||c.target!=t.field_id||t.phase!=PhysicalFoodGrowerPhase::Outward)throw std::logic_error("Food Grower false field arrival");
        t.arrived_tick=ticks_;t.phase=PhysicalFoodGrowerPhase::Waiting;t.wait_left=physical_food_grower_wait_ticks;
        c.path={building(t.field_id).cell};c.path_vertex=0;c.edge_progress=0;return;
    }
    if(c.phase!=CourierPhase::Returning||t.phase!=PhysicalFoodGrowerPhase::Returning||!building_contains(c.owner,c.path.at(c.path_vertex)))
        throw std::logic_error("Food Grower false Farm return");
    const bool loaded=t.harvested_tick.has_value();if(loaded&&original_food_headroom(c.owner)+1<1)throw std::logic_error("Food Grower lost reserved return room");
    t.returned_tick=ticks_;add(source.retired);
    if(loaded){
        auto& field=row(physical_food_growers_->fields,t.field_id,field_key);add(field.returned);
        with_agriculture_food([&](auto& food){++row(food.stocks,c.owner,stock_key).amounts[static_cast<std::size_t>(t.crop)];});t.delivered=1;
        field.last_receipt=PhysicalFoodGrowerReceipt{t.grower_id,c.id,t.crop,t.dispatched_tick,*t.arrived_tick,*t.harvested_tick,ticks_,row(multi_field_agriculture_->fields,t.field_id,field_key).productive_ticks};
        if(seasonal_agriculture_enabled()){
            field.last_receipt->wasted_work_at_harvest=row(seasonal_agriculture_->fields,t.field_id,field_key).wasted_work;
            field.last_receipt->ready_tick=*t.ready_tick;
            if(irrigation_productivity_enabled())field.last_receipt->irrigation_bonus_at_harvest=irrigation_bonus(&*irrigation_productivity_,t.field_id);
        }
    }
    c.path.clear();c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::IdleAtWorkshop;
}
void World::advance_physical_food_growers(){
    for(auto& source:physical_food_growers_->farms){
        if(!source.latest_trip||source.latest_trip->returned_tick)continue;
        auto& t=*source.latest_trip;auto& c=mutable_courier(source.courier_id);
        if(t.phase==PhysicalFoodGrowerPhase::Returning)continue;
        const auto& farm=building(source.farm_id);const auto& field=building(t.field_id);
        const bool eligible=farm.placed&&field.placed&&
            (!agriculture_draining(field.id)||agriculture_field_progress(field.id)!=0)&&farm.operating_enabled&&!building_on_fire(farm.id)&&building_staffed_for_tick(farm.id)&&!building_entrances(farm.id).empty()&&field.operating_enabled&&!building_on_fire(field.id);
        bool crop_eligible=true;
        if(seasonal_agriculture_enabled()){
            const auto& cycle=row(seasonal_agriculture_->fields,t.field_id,field_key);
            const auto model=static_cast<original::ModelBuilding>(195+static_cast<unsigned>(t.crop));
            crop_eligible=seasonal_growing(model,ticks_)||(cycle.ready_tick&&seasonal_harvest_allowed(model,ticks_,*cycle.ready_tick));
            if(t.ready_tick&&(!cycle.ready_tick||t.ready_tick!=cycle.ready_tick))crop_eligible=false;
        }
        if(!eligible||!crop_eligible)t.cancel_requested=true;
        if(t.cancel_requested){if(!c.edge_progress)begin_physical_food_grower_return(c);continue;}
        if(t.phase==PhysicalFoodGrowerPhase::Outward)continue;
        if(!physical_food_grower_stationary(c))throw std::logic_error("Food Grower tending without physical field arrival");
        if(t.phase==PhysicalFoodGrowerPhase::Waiting){if(--t.wait_left==0)t.phase=PhysicalFoodGrowerPhase::Working;continue;}
        if(seasonal_agriculture_enabled()){
            auto& cycle=row(seasonal_agriculture_->fields,t.field_id,field_key);
            if(cycle.ready_tick){
                if(t.ready_tick!=cycle.ready_tick)throw std::logic_error("Food worker changed ready cycle");
                add(cycle.harvest_work);++cycle.harvest_progress;
                if(cycle.harvest_progress==seasonal_harvest_work_ticks){
                    add(row(mutable_agriculture_fields(),t.field_id,field_key).harvested);t.harvested_tick=ticks_;
                    cycle.ready_tick.reset();cycle.harvest_progress=0;begin_physical_food_grower_return(c);
                }
                continue;
            }
            // Keep labor/trip counters in real ticks; seasonal progress adds
            // the separately earned bonus recorded by this same work attempt.
            (void)irrigation_growth_increment(t.field_id,80-seasonal_food_progress(t.field_id));
            auto& work=row(multi_field_agriculture_->fields,t.field_id,field_key);add(work.productive_ticks);++t.work_ticks;
            row(multi_field_agriculture_->farms,source.farm_id,farm_key).last_field=t.field_id;
            if(seasonal_food_progress(t.field_id)==80){cycle.ready_tick=ticks_;t.ready_tick=ticks_;}
            continue;
        }
        auto& work=row(multi_field_agriculture_->fields,t.field_id,field_key);add(work.productive_ticks);++t.work_ticks;
        row(multi_field_agriculture_->farms,source.farm_id,farm_key).last_field=t.field_id;
        if(work.productive_ticks%80==0){add(row(mutable_agriculture_fields(),t.field_id,field_key).harvested);t.harvested_tick=ticks_;begin_physical_food_grower_return(c);}
    }
}
}
