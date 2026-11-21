#pragma once

// Independent authored source/model inputs. All stock, work, residents and
// receipts below come from paid commands and ordinary World movement/ticks.
#include "FoodProcessingFixture.h"
#include "simulation/FoodWarehousesState.h"
#include <sstream>

namespace oe_food_warehouse_fixture {
namespace p=oe_food_processing_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace f=oe_influence_fixture;
using p::check;
using p::stock;
using p::mill;
using p::volume;
using p::crop;
using p::purchase;

struct Fixture:p::Fixture {
    using p::Fixture::Fixture;
    sim::World world()const{return sim::World::create_food_warehouses(policy,model,terrain);}
    sim::World restore(const sim::WorldSnapshot& s)const{return sim::World::restore_food_warehouses(s,policy,model,terrain);}
    sim::World restore(const save::SaveDocument& s)const{
        check(s.source_schema_version==51,"Warehouse fixture requires exact schema51");
        return restore(s.world);
    }
};
struct Scene:p::Scene {sim::BuildingId warehouse{},second_warehouse{},second_mill{},second_market{};};
inline void operate(sim::World& w,sim::BuildingId id,bool enabled){f::buy(w,sim::set_building_operation(id,enabled));}
inline Scene build(sim::World& w,bool watches=true,std::span<const std::size_t> kinds={}){
    constexpr std::array<std::size_t,5> all{0,1,2,3,4};
    Scene s;static_cast<p::Scene&>(s)=p::build(w,kinds.empty()?std::span<const std::size_t>(all):kinds,watches);
    for(int y=110;y<=118;++y)f::buy(w,{sim::CommandType::PlaceRoad,{132,y}});
    for(int x=125;x<132;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,118}});
    s.warehouse=purchase(w,sim::CommandType::PlaceWarehouse,{128,116});
    return s;
}
inline sim::BuildingId add_second_mill(sim::World& w){return purchase(w,sim::CommandType::PlaceOriginalMill,{127,110});}
inline sim::BuildingId add_second_market(sim::World& w){return purchase(w,sim::CommandType::PlaceMarket,{125,110});}
inline sim::BuildingId single_crop_farm(sim::World& w,std::size_t kind,sim::Cell at){
    const auto road=[&](sim::Cell cell){if(w.object_at(cell)!=sim::Object::Road)f::buy(w,{sim::CommandType::PlaceRoad,cell});};
    for(int y=110;y<=124;++y)road({89,y});for(int x=89;x<=at.x+3;++x)road({x,124});
    const auto farm=purchase(w,sim::CommandType::PlaceOriginalFoodFarm,at);crop(w,farm,kind,{at.x+3,at.y});return farm;
}
inline sim::CourierId channel(const sim::World& w,sim::BuildingId owner,sim::CourierRole role=sim::CourierRole::OriginalWarehouseFood){
    for(const auto& c:w.couriers())if(c.owner==owner&&c.role==role)return c.id;
    throw std::runtime_error("Paid Warehouse lacks its counted Food courier");
}
inline const sim::ProcessedFoodTrip& trip(const sim::World& w,sim::CourierId id){
    for(const auto& t:w.processed_food_state()->trips)if(t.transfer.courier_id==id)return t;
    throw std::runtime_error("Real Food courier lost its actual transaction");
}
inline int cargo(const sim::ProcessedFoodTrip& t){int n=0;for(std::size_t g=0;g<12;++g)n+=t.loaded[g]-t.delivered[g]-t.returned[g];return n;}
inline const sim::FoodWarehouseHistory& history(const sim::World& w,sim::BuildingId id){
    for(const auto& r:w.food_warehouses_state()->warehouses)if(r.warehouse_id==id)return r;
    throw std::runtime_error("Paid Warehouse lacks immutable arrival history");
}
inline std::size_t kinds_held(const sim::World& w,sim::BuildingId id){const auto& s=stock(w,id);return static_cast<std::size_t>(std::count_if(s.amounts.begin(),s.amounts.end(),[](int n){return n>0;}));}
inline void valid(const sim::World& w){
    check(w.rule_version()==35&&w.food_warehouses_enabled()&&w.food_processing_enabled()&&w.processed_food_state()&&w.food_warehouses_state()&&
        !w.hunting_food_state()&&!w.fishing_food_state()&&!w.original_agriculture_state()&&w.original_production_state()->food==sim::OriginalFoodState{},
        "Warehouse Food acquired another inventory or historical generation");
    check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&w.navigation_valid()&&w.fire_state_valid(),"Warehouse broke shared World invariants");
    check(w.buildings().size()<=82&&w.couriers().size()+w.fishing_state()->quays.size()+w.hunting_state()->tents.size()+3*w.hunting_state()->habitats.size()<=58,
        "Warehouse raised historical body/channel budgets");
    sim::ProcessedFoodCounters origin{},held{},carried{},used{},departed{},received{},returned{};
    for(const auto& f:w.physical_food_growers_state()->fields){const auto at=std::find_if(w.agriculture_fields().begin(),w.agriculture_fields().end(),[&](const auto& a){return a.field_id==f.field_id;});check(at!=w.agriculture_fields().end(),"Crop return lost its field");origin[static_cast<std::size_t>(at->crop)]+=f.returned;}
    for(const auto& q:w.fishing_state()->quays)origin[5]+=q.unloaded;
    for(const auto& t:w.hunting_state()->tents)origin[6]+=t.unloaded;
    for(const auto& m:w.food_processing_state()->mills)for(std::size_t g=0;g<4;++g)origin[7+g]+=m.completed_by_grade[g]*(g+1);
    for(const auto& s:w.processed_food_state()->stocks)for(std::size_t g=0;g<12;++g){check(s.amounts[g]>=0,"Negative Warehouse Food");held[g]+=static_cast<unsigned>(s.amounts[g]);used[g]+=s.consumed[g];departed[g]+=s.departed[g];received[g]+=s.received[g];returned[g]+=s.returned[g];}
    for(const auto& t:w.processed_food_state()->trips)for(std::size_t g=0;g<12;++g)carried[g]+=static_cast<std::uint64_t>(t.loaded[g]-t.delivered[g]-t.returned[g]);
    for(std::size_t g=0;g<12;++g){check(origin[g]==held[g]+carried[g]+used[g],"Earned Food differs from physical stock/cargo/use");check(departed[g]==received[g]+returned[g]+carried[g],"Warehouse delivery or return booked twice");}
    check(!origin[11]&&!held[11]&&!used[11]&&!carried[11],"Unavailable grade5 acquired an origin");
    for(const auto& h:w.food_warehouses_state()->warehouses){const auto& s=stock(w,h.warehouse_id);const auto status=w.food_warehouse_status(h.warehouse_id);
        check(volume(s.amounts)+status.incoming+status.return_reserved<=32&&!s.quality_witness&&!s.missed_demands,"Warehouse capacity or House authority leaked");
        for(std::size_t g=0;g<12;++g){int incoming=0,back=0;for(const auto& t:w.processed_food_state()->trips){const auto n=t.loaded[g]-t.delivered[g]-t.returned[g];if(t.target==h.warehouse_id&&t.phase==sim::OriginalGoodsTransferPhase::Loaded)incoming+=n;if(t.source==h.warehouse_id)back+=n;}
            check(s.amounts[g]+incoming+back<=4&&!s.consumed[g],"Warehouse per-kind/return quota or no-consumption contract failed");}}
    for(std::size_t g=0;g<openemperor::original::goods_count;++g)check(w.original_goods_balance(static_cast<openemperor::original::Goods>(g)).conserved,"Food changed the Goods ledger");
    for(std::size_t g=0;g<sim::original_aux_material_count;++g)check(w.original_auxiliary_balance(static_cast<sim::OriginalAuxMaterial>(g)).conserved,"Food changed the Aux ledger");
    sim::World::validate_food_processing_snapshot(w.snapshot());sim::World::validate_food_warehouses_snapshot(w.snapshot());
}
template<class F>inline void until(sim::World& w,F predicate,const std::string& reason,int bound=30000){
    for(int n=0;n<bound&&!predicate();++n)w.tick();if(predicate())return;
    std::ostringstream details;details<<reason<<" tick="<<w.ticks()<<" population="<<w.total_population()<<" workforce="<<w.workforce_used()<<'/'<<w.workforce_supply();
    for(const auto& s:w.processed_food_state()->stocks){const auto& b=w.building(s.building_id);if(sim::original_house_kind(b.kind))continue;
        details<<" | owner"<<static_cast<unsigned>(b.id)<<" kind"<<static_cast<unsigned>(b.kind)<<" staff"<<w.workers_assigned(b.id)<<'/'<<w.workforce_required(b.id)<<" op"<<b.operating_enabled<<" fire"<<w.building_on_fire(b.id)<<" stock[";
        for(const auto n:s.amounts)details<<n<<',';details<<"]";
        if(b.kind==sim::Object::Warehouse){const auto status=w.food_warehouse_status(b.id);details<<" in"<<status.incoming<<" return"<<status.return_reserved;}}
    for(const auto& c:w.couriers())if(c.role==sim::CourierRole::OriginalFoodToMarket||c.role==sim::CourierRole::OriginalWarehouseFood){
        details<<" | actor"<<static_cast<unsigned>(c.id)<<" owner"<<static_cast<unsigned>(c.owner)<<" target"<<static_cast<unsigned>(c.target)<<" phase"<<static_cast<unsigned>(c.phase)
            <<" dispatch:"<<sim::courier_dispatch_status_name(w.courier_dispatch_status(c.id).status)<<" pending"<<c.route_pending<<" route"<<c.path_vertex<<'/'<<c.path.size()<<" edge"<<c.edge_progress;}
    throw std::runtime_error(details.str());
}
inline void ticks(sim::World& w,int n){for(int i=0;i<n;++i)w.tick();}
}
