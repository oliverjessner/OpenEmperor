#include "simulation/World.h"
#include "simulation/OriginalIndustryGoodsContext.h"
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace openemperor::simulation {
namespace {
[[noreturn]] void invalid(const char* s){throw std::invalid_argument(s);}
void add(std::uint64_t& n){if(n==UINT64_MAX)invalid("Seasonal Hemp history exhausted");++n;}
template<class Rows,class Id,class Key>decltype(auto) row(Rows& rows,Id id,Key key){
    const auto at=std::lower_bound(rows.begin(),rows.end(),id,[&](const auto& v,Id target){return key(v)<target;});
    if(at==rows.end()||key(*at)!=id)invalid("Seasonal Hemp lost paid identity");return *at;
}
const auto farm_key=[](const auto& v){return v.farm_id;};
const auto id_key=[](const auto& v){return v.id;};
template<class Bodies,class Couriers>
void validate_hemp(const SeasonalAgricultureState& s,const OriginalProductionState& p,const OriginalGoodsState& goods,
    const Bodies& bodies,const Couriers& couriers,std::uint64_t tick){
    if(!s.next_hemp_grower_id||s.hemp_farms.size()!=p.hemp_farms.size()||s.hemp_farms.size()>2||
       std::count_if(couriers.begin(),couriers.end(),[](const auto& c){return c.role==CourierRole::OriginalSeasonalHempGrower;})!=static_cast<std::ptrdiff_t>(s.hemp_farms.size()))
        invalid("Seasonal Hemp requires one exact paid worker channel per source");
    struct Observation{std::uint64_t id;BuildingId farm;std::uint64_t dispatch,ready,pick,returned;bool operator==(const Observation&)const=default;};
    std::vector<Observation> observations;observations.reserve(4);
    std::uint64_t total=0,maximum=0;
    for(std::size_t i=0;i<s.hemp_farms.size();++i){
        const auto& f=s.hemp_farms[i];const auto& base=p.hemp_farms[i];const auto& b=row(bodies,f.farm_id,id_key);const auto& c=row(couriers,f.courier_id,id_key);
        const bool live=f.latest_trip&&!f.latest_trip->returned_tick;const auto carried=f.latest_trip?seasonal_hemp_cargo(*f.latest_trip):0;
        if(f.farm_id!=base.farm_id||b.kind!=Object::OriginalHempFarm||c.owner!=b.id||c.role!=CourierRole::OriginalSeasonalHempGrower||
           c.good!=Good::Goods||!c.enabled||c.cargo||c.reserved||f.spawned>tick-b.placed_tick||f.retired>f.spawned||
           f.spawned-f.retired!=static_cast<unsigned>(live)||f.retired>(tick-b.placed_tick)/10||
           base.harvested>f.picked||f.picked-base.harvested!=static_cast<unsigned>(carried)||f.picked>f.spawned||base.harvested>f.retired)
            invalid("Seasonal Hemp source/channel/lifetime/cargo equation invalid");
        if(f.spawned>UINT64_MAX-total)invalid("Seasonal Hemp highwater overflow");total+=f.spawned;
        if(!base.field_id){
            if(f.productive_ticks||f.wasted_work||f.grown||f.picked||f.expired_ready||f.ready_tick||f.latest_trip||f.spawned||f.retired||f.last_receipt||b.progress)
                invalid("Hemp cannot work before its paid Field");
            continue;
        }
        const auto& field=row(bodies,*base.field_id,id_key);
        if(b.progress<0||b.progress>80||f.wasted_work>f.productive_ticks||f.picked>(f.productive_ticks-f.wasted_work)/80||
           f.productive_ticks-f.wasted_work-f.picked*80!=static_cast<unsigned>(b.progress)||f.expired_ready>f.wasted_work/80||
           f.grown<f.picked||f.grown-f.picked<f.expired_ready||f.grown-f.picked-f.expired_ready!=static_cast<unsigned>(b.progress==80)||
           f.ready_tick.has_value()!=(b.progress==80)||f.productive_ticks>seasonal_work_opportunities(original::ModelBuilding::HempField,field.placed_tick,tick)||
           (!seasonal_growing(original::ModelBuilding::HempField,tick)&&b.progress>0&&b.progress<80))
            invalid("Seasonal Hemp work/readiness/expiry equation invalid");
        if(f.ready_tick&&(*f.ready_tick<=field.placed_tick||f.productive_ticks>seasonal_work_opportunities(original::ModelBuilding::HempField,field.placed_tick,*f.ready_tick)||
           !seasonal_harvest_allowed(original::ModelBuilding::HempField,tick,*f.ready_tick)))
            invalid("Hemp readiness is outside its paid live seasonal cycle");
        int held=0;for(const auto& a:goods.stocks)if(a.building_id==f.farm_id)held=a.amounts[0];
        std::int64_t reserved=live?1:0;
        for(const auto& a:goods.cargo)if(a.source==f.farm_id&&a.good==original::Goods::Hemp)reserved+=a.amount;
        if(held<0||held>8||reserved<0||held+reserved>8)invalid("Hemp stock and physical return reservations exceed8");
        if(!f.latest_trip){
            if(f.spawned||f.retired||c.phase!=CourierPhase::IdleAtWorkshop||c.target!=b.id||c.last_dispatched_target||!c.path.empty())
                invalid("Unused Hemp channel owns a phantom actor");
            continue;
        }
        const auto& t=*f.latest_trip;
        const auto gap=static_cast<std::uint64_t>(std::max({std::int64_t{0},std::int64_t(b.cell.x)-field.cell.x,std::int64_t(field.cell.x)-b.cell.x-1})+
            std::max({std::int64_t{0},std::int64_t(b.cell.y)-field.cell.y,std::int64_t(field.cell.y)-b.cell.y-1}))*5;
        if(!f.spawned||!t.grower_id.value||t.grower_id.value>=s.next_hemp_grower_id||t.grower_id.value<f.spawned||t.field_id!=field.id||
           t.dispatched_tick<=std::max(field.placed_tick,b.placed_tick)||t.dispatched_tick>tick||t.ready_tick<=field.placed_tick||
           t.ready_tick>t.dispatched_tick||seasonal_work_opportunities(original::ModelBuilding::HempField,field.placed_tick,t.ready_tick)<80||
           !seasonal_harvest_allowed(original::ModelBuilding::HempField,t.dispatched_tick,t.ready_tick)||
           c.target!=field.id||c.last_dispatched_target!=field.id||t.delivered>1||!gap)
            invalid("Seasonal Hemp assignment or ready-cycle identity invalid");
        maximum=std::max(maximum,t.grower_id.value);
        observations.push_back({t.grower_id.value,b.id,t.dispatched_tick,t.ready_tick,t.picked_tick.value_or(0),t.returned_tick.value_or(0)});
        if(t.picked_tick&&(*t.picked_tick>tick||*t.picked_tick<t.dispatched_tick||*t.picked_tick-t.dispatched_tick+1<gap||
           !seasonal_harvest_allowed(original::ModelBuilding::HempField,*t.picked_tick,t.ready_tick)))
            invalid("Hemp pickup skipped real movement or its live harvest window");
        if(live&&!t.picked_tick&&seasonal_harvest_allowed(original::ModelBuilding::HempField,tick,t.ready_tick)&&f.ready_tick!=t.ready_tick)
            invalid("Unexpired Hemp assignment contradicts its actual ready crop");
        if(live){if(t.delivered||c.path.empty()||c.path_vertex>=c.path.size()||
                (c.phase!=CourierPhase::ToWarehouse&&c.phase!=CourierPhase::Returning)||
                (t.picked_tick&&c.phase!=CourierPhase::Returning))invalid("Live Hemp cargo lost shared physical movement");
            const auto here=c.path.at(c.path_vertex);
            const auto walked=t.picked_tick?static_cast<std::uint64_t>(std::abs(std::int64_t(here.x)-field.cell.x)+std::abs(std::int64_t(here.y)-field.cell.y))*5:
                static_cast<std::uint64_t>(std::max({std::int64_t{0},std::int64_t(b.cell.x)-here.x,std::int64_t(here.x)-b.cell.x-1})+
                    std::max({std::int64_t{0},std::int64_t(b.cell.y)-here.y,std::int64_t(here.y)-b.cell.y-1}))*5;
            const auto began=t.picked_tick.value_or(t.dispatched_tick);
            if(tick-began+1<walked+static_cast<std::uint64_t>(c.edge_progress))invalid("Hemp position exceeds actual elapsed land movement");
        }else if(c.phase!=CourierPhase::IdleAtWorkshop||!c.path.empty()||c.path_vertex||c.edge_progress||c.route_pending||c.route_checked_revision||
            *t.returned_tick<=t.dispatched_tick||*t.returned_tick-t.dispatched_tick+1<10||*t.returned_tick>tick||
            t.delivered!=static_cast<unsigned>(t.picked_tick.has_value())||
            (t.picked_tick&&(*t.returned_tick<=*t.picked_tick||*t.returned_tick-*t.picked_tick+1<gap)))
            invalid("Hemp retirement lacks actual source return");
        if(f.last_receipt.has_value()!=(base.harvested>0))invalid("Hemp return count lost its actual receipt");
        if(f.last_receipt){const auto& r=*f.last_receipt;
            observations.push_back({r.grower_id.value,b.id,r.dispatched_tick,r.ready_tick,r.picked_tick,r.returned_tick});
            if(!r.grower_id.value||r.grower_id.value>=s.next_hemp_grower_id||r.grower_id.value>t.grower_id.value||r.courier_id!=c.id||r.field_id!=field.id||
               r.ready_tick<=field.placed_tick||seasonal_work_opportunities(original::ModelBuilding::HempField,field.placed_tick,r.ready_tick)<80||
               r.dispatched_tick<=field.placed_tick||r.dispatched_tick<r.ready_tick||
               r.picked_tick<r.dispatched_tick||r.picked_tick-r.dispatched_tick+1<gap||r.returned_tick<=r.picked_tick||
               r.returned_tick-r.picked_tick+1<gap||r.returned_tick>tick||!seasonal_harvest_allowed(original::ModelBuilding::HempField,r.picked_tick,r.ready_tick))
                invalid("Hemp receipt lacks exact actual seasonal pickup/return");
            if(r.grower_id==t.grower_id&&(t.ready_tick!=r.ready_tick||t.dispatched_tick!=r.dispatched_tick||t.picked_tick!=r.picked_tick||t.returned_tick!=r.returned_tick||!t.delivered))
                invalid("Hemp receipt contradicts its retained lifetime");
            if(r.grower_id!=t.grower_id&&r.returned_tick>=t.dispatched_tick)invalid("Hemp source lifetimes overlap");
        }
    }
    std::sort(observations.begin(),observations.end(),[](const auto& a,const auto& b){return a.id<b.id;});
    for(std::size_t i=1;i<observations.size();++i){
        const auto& a=observations[i-1];const auto& b=observations[i];
        if(a.id==b.id&&a!=b)invalid("Hemp lifetime identity reused across sources");
        if(a.id!=b.id&&b.dispatch<a.dispatch)invalid("Hemp lifetime allocation reverses dispatch chronology");
    }
    if(total==UINT64_MAX||s.next_hemp_grower_id!=total+1||maximum!=total)invalid("Seasonal Hemp monotone lifetime highwater invalid");
}
}
void World::validate_seasonal_hemp_snapshot(const WorldSnapshot& s){validate_hemp(*s.seasonal_agriculture,*s.original_production,*s.original_goods,s.buildings,s.couriers,s.ticks);}
void World::validate_seasonal_hemp_state()const{validate_hemp(*seasonal_agriculture_,*original_production_,*original_goods_,buildings_,couriers_,ticks_);}
int World::seasonal_hemp_grower_cargo(CourierId id)const{
    if(!seasonal_agriculture_enabled())return 0;const auto& c=courier(id);if(c.role!=CourierRole::OriginalSeasonalHempGrower)return 0;
    const auto& f=row(seasonal_agriculture_->hemp_farms,c.owner,farm_key);return f.latest_trip?seasonal_hemp_cargo(*f.latest_trip):0;
}
bool World::seasonal_hemp_cell_allowed(Cell cell,BuildingId source,BuildingId field)const{
    if(!seasonal_agriculture_enabled()||!in_bounds(cell)||!plantation_cell_allowed(cell,source,field)||
       building(source).kind!=Object::OriginalHempFarm||building(field).kind!=Object::OriginalHempField||!original_terrain_facts_||
       !original_terrain_facts_->irrigation_boundary_allowed(cell)||!permissions_||permissions_->protected_original(cell)||
       permissions_->fixed_passage(cell)||!permissions_->road_allowed(cell))return false;
    const auto& f=row(original_production_->hemp_farms,source,farm_key);if(f.field_id!=field)return false;
    const auto raw=original_terrain_facts_->raw_terrain(cell);if(raw!=0x80U&&raw!=0xc0U)return false;
    return !irrigation_||object_at(cell)==Object::Road||std::none_of(irrigation_->ditches.begin(),irrigation_->ditches.end(),[&](const auto& d){return d.cell==cell;});
}
bool World::seasonal_hemp_edge_allowed(Cell a,Cell b,BuildingId source,BuildingId field)const{
    return std::abs(a.x-b.x)+std::abs(a.y-b.y)==1&&seasonal_hemp_cell_allowed(a,source,field)&&seasonal_hemp_cell_allowed(b,source,field)&&permission_edge_allowed(a,b);
}
std::optional<std::vector<Cell>> World::seasonal_hemp_route(Cell start,BuildingId source,BuildingId field,bool returning)const{
    std::optional<std::vector<Cell>> best;const auto consider=[&](Cell a,Cell b){auto p=find_road_route(a,b,source,field);if(p&&p->size()>1&&(!best||p->size()<best->size()))best=std::move(p);};
    if(returning)for(const auto to:building_cells(source))consider(start,to);
    else if(start.x<0)for(const auto from:building_cells(source))consider(from,building(field).cell);
    else consider(start,building(field).cell);return best;
}
void World::advance_seasonal_hemp(){
    for(auto& f:seasonal_agriculture_->hemp_farms){
        auto& source=mutable_building(f.farm_id);const auto& owned=row(original_production_->hemp_farms,source.id,farm_key);
        if(!owned.field_id||source.progress==80||!seasonal_growing(original::ModelBuilding::HempField,ticks_)||
           !source.operating_enabled||building_on_fire(source.id)||!building(*owned.field_id).operating_enabled||building_on_fire(*owned.field_id)||
           !building_staffed_for_tick(source.id)||building_entrances(source.id).empty())continue;
        add(f.productive_ticks);++source.progress;
        if(source.progress==80){add(f.grown);f.ready_tick=ticks_;}
    }
}
CourierDispatchDecision World::seasonal_hemp_dispatch_status(const CourierState& c)const{
    if(!seasonal_agriculture_enabled()||!c.enabled)return {CourierDispatchStatus::Disabled,{}};
    if(c.phase!=CourierPhase::IdleAtWorkshop)return {c.route_pending?CourierDispatchStatus::NoRoad:CourierDispatchStatus::AlreadyMoving,c.target};
    const auto& b=building(c.owner);const auto& f=row(seasonal_agriculture_->hemp_farms,b.id,farm_key);const auto& owned=row(original_production_->hemp_farms,b.id,farm_key);
    if(!b.operating_enabled)return {CourierDispatchStatus::OperationPaused,{}};
    if(building_on_fire(b.id))return {CourierDispatchStatus::OnFire,{}};
    if(!building_staffed_for_tick(b.id))return {CourierDispatchStatus::Unstaffed,{}};
    if(c.cached_revision!=road_revision_||building_entrances(b.id).empty())return {CourierDispatchStatus::NoRoad,{}};
    if(!owned.field_id||!f.ready_tick||b.progress!=80||!building(*owned.field_id).operating_enabled||building_on_fire(*owned.field_id)||
       !seasonal_harvest_allowed(original::ModelBuilding::HempField,ticks_,*f.ready_tick))return {CourierDispatchStatus::NoStock,{}};
    if(original_market_headroom(b.id,original::Goods::Hemp)<=0)return {CourierDispatchStatus::TargetFull,{}};
    if(f.spawned==UINT64_MAX||seasonal_agriculture_->next_hemp_grower_id==UINT64_MAX)return {CourierDispatchStatus::Disabled,{}};
    for(const auto& [target,path]:c.dynamic_target_routes)if(target==*owned.field_id&&path)return {CourierDispatchStatus::Ready,target};
    return {CourierDispatchStatus::NoRoad,{}};
}
void World::dispatch_seasonal_hemp(CourierState& c){
    const auto d=seasonal_hemp_dispatch_status(c);if(d.status!=CourierDispatchStatus::Ready||!d.selected_target)return;
    auto& f=row(seasonal_agriculture_->hemp_farms,c.owner,farm_key);
    const auto path=std::find_if(c.dynamic_target_routes.begin(),c.dynamic_target_routes.end(),[&](const auto& r){return r.first==*d.selected_target;});
    if(path==c.dynamic_target_routes.end()||!path->second)invalid("Hemp dispatch lost prepared route");
    f.latest_trip=SeasonalHempTrip{{seasonal_agriculture_->next_hemp_grower_id++},*d.selected_target,ticks_,*f.ready_tick,{},{},0};add(f.spawned);
    c.target=*d.selected_target;c.last_dispatched_target=c.target;c.path=*path->second;c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();c.phase=CourierPhase::ToWarehouse;
}
void World::arrive_seasonal_hemp(CourierState& c){
    auto& f=row(seasonal_agriculture_->hemp_farms,c.owner,farm_key);if(!f.latest_trip||f.latest_trip->returned_tick)invalid("Hemp duplicate arrival");
    auto& t=*f.latest_trip;auto& source=mutable_building(c.owner);auto& record=row(original_production_->hemp_farms,c.owner,farm_key);
    if(c.phase==CourierPhase::ToWarehouse){
        if(c.path.at(c.path_vertex)!=building(t.field_id).cell)invalid("Hemp false Field arrival");
        if(source.operating_enabled&&!building_on_fire(source.id)&&building_staffed_for_tick(source.id)&&
           building(t.field_id).operating_enabled&&!building_on_fire(t.field_id)&&!building_entrances(source.id).empty()&&source.progress==80&&f.ready_tick==t.ready_tick&&
           seasonal_harvest_allowed(original::ModelBuilding::HempField,ticks_,t.ready_tick)){
            source.progress=0;f.ready_tick.reset();add(f.picked);t.picked_tick=ticks_;
        }
        c.phase=CourierPhase::Returning;mark_route_pending(c);reroute_v2(c);return;
    }
    if(c.phase!=CourierPhase::Returning||!building_contains(c.owner,c.path.at(c.path_vertex)))invalid("Hemp false Farm return");
    t.returned_tick=ticks_;add(f.retired);
    if(t.picked_tick){
        if(original_market_headroom(source.id,original::Goods::Hemp)<=0)invalid("Hemp lost reserved return headroom");
        const original_industry_goods::Context context(buildings_,couriers_);
        auto result=original_goods_produce(context.view(*original_goods_,original_economy_->houses),source.id,original::Goods::Hemp,1);
        original_goods_=std::move(result.goods);add(record.harvested);t.delivered=1;
        f.last_receipt=SeasonalHempReceipt{t.grower_id,c.id,t.field_id,t.dispatched_tick,t.ready_tick,*t.picked_tick,ticks_};
    }
    c.phase=CourierPhase::IdleAtWorkshop;c.path.clear();c.path_vertex=0;c.edge_progress=0;c.route_pending=false;c.route_checked_revision.reset();
}
}
