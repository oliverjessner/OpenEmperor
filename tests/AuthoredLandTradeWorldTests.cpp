// Authored geometry/model input and ordinary paid World commands only.
// No original data, live stock/Funds/population grants or parallel transport.
#include "OriginalInfluenceFixture.h"
#include "maps/MapGeometry.h"
#include "simulation/AuthoredLandTrade.h"
#include "simulation/original/OriginalRulesModel.h"

#include <algorithm>
#include <array>
#include <climits>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
namespace sim=openemperor::simulation;
namespace original=openemperor::original;
void check(bool ok,const std::string& why) {if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F action,const char* why) {
    try {action();}catch(const std::exception&){return;}throw std::runtime_error(why);
}
struct Fixture {
    static constexpr int side=228;
    oe_influence_fixture::Root root;
    openemperor::maps::MapGeometry geometry{226};
    std::shared_ptr<const sim::MapPermissions> policy;
    std::shared_ptr<const original::OriginalRulesModel> model;
    std::shared_ptr<const sim::LandTradeBoundaryFacts> boundary;
    Fixture() {
        check(geometry.supported,"Shared authored Diamond geometry unavailable");
        std::vector<sim::MapCellPermission> cells(geometry.candidate.size());
        for(std::size_t i=0;i<cells.size();++i) {
            const bool land=geometry.candidate[i]!=0;
            cells[i]={land,land,false,0,land?sim::BuildBlocker::None:sim::BuildBlocker::OutsideMap,
                land?sim::BuildBlocker::None:sim::BuildBlocker::OutsideMap};
        }
        policy=std::make_shared<const sim::MapPermissions>(side,side,3,std::move(cells),std::vector<sim::FixedGatePassage>{});
        model=openemperor::persistence::prepare_original_rules_model(root.data,original::Difficulty::Normal);
        // A World-only authored identity, not a claim to have parsed an Original
        // map. Save/View tests independently bind an encoded authored source.
        boundary=std::make_shared<const sim::LandTradeBoundaryFacts>(side,side,geometry.candidate,std::string(64,'a'));
    }
    sim::World fresh() const {return sim::World::create_authored_land_trade(policy,model,boundary);}
    sim::World restore(const sim::WorldSnapshot& s) const {return sim::World::restore_authored_land_trade(s,policy,model,boundary);}
};
void balance(const sim::World& w) {
    const auto trade=w.land_trade_balance();
    std::uint64_t physical=0,consumed=0;
    for(const auto& b:w.buildings()) {
        if(b.kind==sim::Object::Pottery)physical+=static_cast<std::uint64_t>(b.output);
        physical+=static_cast<std::uint64_t>(b.pottery_stock);
        consumed+=b.consumed_total;
    }
    for(const auto& c:w.couriers())if(c.good==sim::Good::Pottery)physical+=static_cast<std::uint64_t>(c.cargo);
    check(w.pottery_completed_total()+trade.imported==physical+consumed+trade.exported,
        "Paid production/import/export/consumption/stock/cargo do not conserve Pottery");
    const auto expected=sim::starting_treasury_for(w.profile(),w.rule_version())+
        static_cast<std::int64_t>(w.taxes_collected_total())+static_cast<std::int64_t>(trade.exports_received)-
        static_cast<std::int64_t>(w.construction_spent_total())-static_cast<std::int64_t>(w.maintenance_spent_total())-
        static_cast<std::int64_t>(trade.imports_paid);
    check(w.treasury()==expected,"Exact city+trade Funds equation disagrees");
}
void buy(sim::World& w,sim::Command c) {
    const auto result=w.execute(c);
    check(result.accepted&&result.changed,"Paid trade fixture command refused: "+result.reason);
    balance(w);
}
void refused(sim::World& w,sim::Command c,const std::string& reason) {
    const auto before=w.snapshot();const auto canonical=w.canonical_state();
    const auto preview=w.validate(c),result=w.execute(c);
    check(!preview.accepted&&!result.accepted&&!result.changed,"Expected atomic trade refusal: "+reason);
    check(result.reason.find(reason)!=std::string::npos,"Unexpected trade refusal: "+result.reason+"; expected "+reason);
    check(w.snapshot()==before&&w.canonical_state()==canonical,"Refusal changed state/Funds/IDs/quotas/cargo");
    balance(w);
}
template<class F> void until(sim::World& w,F condition,unsigned bound,const char* why) {
    for(unsigned i=0;i<bound&&!condition();++i){w.tick();balance(w);}
    check(condition(),why);
}
struct Scene {sim::BuildingId city{},gateway{},clay{},pottery{};};
Scene setup(sim::World& w,bool production=true,unsigned houses=5) {
    check(houses<=5,"Unbounded authored House fixture");
    for(unsigned i=0;i<houses;++i)buy(w,{sim::CommandType::PlaceHousehold,{107+4*static_cast<int>(i),18}});
    Scene s;
    const auto place=[&](sim::CommandType type,sim::Cell p) {
        buy(w,{type,p});const auto id=w.building_owner_at(p);check(bool(id),"Paid footprint lost owner");return *id;
    };
    if(production) {s.clay=place(sim::CommandType::PlaceClaySource,{108,15});s.pottery=place(sim::CommandType::PlacePottery,{108,11});}
    s.city=place(sim::CommandType::PlaceWarehouse,{113,12});
    s.gateway=place(sim::CommandType::PlaceWarehouse,{113,1});
    for(int y=3;y<=11;++y)buy(w,{sim::CommandType::PlaceRoad,{114,y}});
    for(int x=110;x<=113;++x)buy(w,{sim::CommandType::PlaceRoad,{x,11}});
    for(int y=12;y<=16;++y)buy(w,{sim::CommandType::PlaceRoad,{110,y}});
    if(production&&houses)check(w.workforce_supply()==static_cast<int>(houses)*sim::Rules::household_initial_population&&
        w.workforce_required()==14&&w.workforce_used()==14,
        "Actual paid residents must staff Clay/Pottery/two Warehouses");
    return s;
}
void earn_city_stock(sim::World& w,const Scene& s,int quantity,unsigned bound,const char* stage,bool exclusive_city_route=false) {
    // At least two real Clay units are needed per native Pottery recipe.
    // With the other Warehouse physically disconnected, stop once enough
    // material has left the source. Otherwise both Warehouses can receive
    // output, so actual city stock (not an aggregate recipe count) ends earning.
    // Paused installed upkeep stays payable and moving cargo still arrives.
    const auto required_clay=static_cast<std::uint64_t>(quantity*sim::Rules::pottery_recipe_clay);
    for(unsigned i=0;i<bound&&w.building(s.city).pottery_stock<quantity;++i) {
        const auto& clay=w.building(s.clay);
        if(exclusive_city_route&&clay.operating_enabled&&clay.clay_extracted>=required_clay&&clay.output==0)
            buy(w,sim::set_building_operation(s.clay,false));
        w.tick();balance(w);
    }
    if(w.building(s.city).pottery_stock<quantity) {
        const auto& clay=w.building(s.clay);const auto& pottery=w.building(s.pottery);
        std::string diagnostic=std::string(stage)+": paid stock target="+std::to_string(quantity)+
            " tick="+std::to_string(w.ticks())+" Funds="+std::to_string(w.treasury())+
            " population="+std::to_string(w.workforce_supply())+" used="+std::to_string(w.workforce_used())+
            " cityStock="+std::to_string(w.building(s.city).pottery_stock)+
            " gatewayStock="+std::to_string(w.building(s.gateway).pottery_stock)+
            " clayExtracted="+std::to_string(clay.clay_extracted)+" clayOutput="+std::to_string(clay.output)+
            " clayOperating="+std::to_string(clay.operating_enabled)+
            " potteryInput="+std::to_string(pottery.input_clay)+" activeRecipe="+std::to_string(pottery.active_recipe_clay)+
            " potteryProgress="+std::to_string(pottery.progress)+" potteryOutput="+std::to_string(pottery.output)+
            " completed="+std::to_string(w.pottery_completed_total());
        for(const auto& c:w.couriers())if(c.owner==s.clay||c.owner==s.pottery)
            diagnostic+=" courier="+std::to_string(static_cast<unsigned>(c.id))+
                ",phase="+std::to_string(static_cast<unsigned>(c.phase))+",cargo="+std::to_string(c.cargo)+
                ",target="+std::to_string(static_cast<unsigned>(c.target))+
                ",dispatch="+std::to_string(static_cast<unsigned>(w.courier_dispatch_status(c.id).status));
        throw std::runtime_error(diagnostic);
    }
    if(w.building(s.clay).operating_enabled)buy(w,sim::set_building_operation(s.clay,false));
    check(w.building_staffed(s.city),"Actual city Warehouse lost its paid workforce after production");
}
sim::CourierId trader(const sim::World& w) {
    check(w.land_trade_state()->binding.has_value(),"No paid trade binding");return w.land_trade_state()->binding->courier;
}
void completed(sim::World& w) {
    until(w,[&]{return !w.land_trade_balance().busy;},500,"Real caravan did not complete its physical round trip");
    const auto& c=w.courier(trader(w));
    check(c.phase==sim::CourierPhase::IdleAtWorkshop&&c.cargo==0&&c.reserved==0,"Finished caravan retains active payload");
}
void restore_refused(const Fixture& f,sim::World& live,const sim::WorldSnapshot& bad) {
    const auto state=live.snapshot();const auto canonical=live.canonical_state();
    rejects([&]{(void)f.restore(bad);},"Hostile trade snapshot restored");
    rejects([&]{live.import_snapshot(bad);},"Hostile trade snapshot published into live World");
    check(live.snapshot()==state&&live.canonical_state()==canonical,"Failed restore/import changed live authority");
}
void geometry_identity_and_configuration() {
    Fixture f;
    check(f.boundary->boundary_footprint({113,1},2,2)&&!f.boundary->boundary_footprint({113,12},2,2),
        "Common Diamond boundary confused with interior geography");
    for(const auto p:{sim::Cell{INT_MAX,1},sim::Cell{1,INT_MAX},sim::Cell{INT_MIN,1},sim::Cell{1,INT_MIN},sim::Cell{-1,1}})
        check(!f.boundary->boundary_footprint(p,2,2),"Extreme/outside boundary footprint accepted");
    check(!f.boundary->boundary_footprint({113,1},0,2)&&!f.boundary->boundary_footprint({113,1},5,2),"Unbounded boundary dimensions admitted");
    rejects([&]{(void)sim::World::create_authored_land_trade({},f.model,f.boundary);},"Missing policy admitted");
    rejects([&]{(void)sim::World::create_authored_land_trade(f.policy,{},f.boundary);},"Missing model admitted");
    rejects([&]{(void)sim::World::create_authored_land_trade(f.policy,f.model,{});},"Missing boundary admitted");
    check(sim::rule_version_supported(sim::RulesProfile::CityV16,21)&&!sim::rule_version_supported(sim::RulesProfile::CityV16,24)&&
        !sim::original_economy_enabled(sim::RulesProfile::CityV16,21)&&!sim::source_cell_ramps_enabled(sim::RulesProfile::CityV16,21)&&
        sim::current_rule_version(sim::RulesProfile::CityV16)==4&&sim::current_rule_version(sim::RulesProfile::CityV11)==3,
        "New authored trade inherited Original/source-cell authority or changed defaults");
    auto w=f.fresh();const auto s=setup(w);
    refused(w,sim::configure_land_trade(s.gateway,s.city),"boundary");
    refused(w,sim::configure_land_trade(s.city,s.city),"distinct");
    refused(w,sim::configure_land_trade(s.clay,s.gateway),"Warehouses");
    refused(w,sim::configure_land_trade(static_cast<sim::BuildingId>(999),s.gateway),"missing");
    buy(w,{sim::CommandType::RemoveRoad,{114,5}});
    refused(w,sim::configure_land_trade(s.city,s.gateway),"road route");
    buy(w,{sim::CommandType::PlaceRoad,{114,5}});
    const auto funds=w.treasury();const auto spent=w.construction_spent_total();const auto actors=w.couriers().size();
    buy(w,sim::configure_land_trade(s.city,s.gateway));
    check(w.treasury()==funds-40&&w.construction_spent_total()==spent+40&&w.couriers().size()==actors+1,
        "License did not pay40 and allocate exactly one actual caravan");
    refused(w,sim::configure_land_trade(s.city,s.gateway),"already bound");
    refused(w,sim::export_pottery(),"physical export stock");
    for(const auto id:{s.city,s.gateway}) {
        const auto before=w.snapshot();const auto result=w.execute(sim::demolish_building(id));
        check(!result.accepted&&!result.changed&&w.snapshot()==before,"Configured Warehouse demolition changed authority");
    }
    for(const auto id:{s.city,s.gateway}) {
        buy(w,sim::set_building_operation(id,false));refused(w,sim::import_pottery(),"paused");buy(w,sim::set_building_operation(id,true));
    }
    auto unstaffed=f.fresh();const auto no_workers=setup(unstaffed,false,0);
    refused(unstaffed,sim::configure_land_trade(no_workers.city,no_workers.gateway),"unstaffed");
}
void real_paid_export_import_and_hostility() {
    Fixture f;auto w=f.fresh();const auto s=setup(w);buy(w,sim::configure_land_trade(s.city,s.gateway));
    earn_city_stock(w,s,4,1800,"First earned export");
    check(w.pottery_completed_total()>=4&&w.land_trade_balance().imported==0,"First export would be imported/injected goods");
    const auto funds=w.treasury();const auto stock=w.building(s.city).pottery_stock;
    buy(w,sim::export_pottery());
    check(w.building(s.city).pottery_stock==stock-4&&w.courier(trader(w)).cargo==4&&w.treasury()==funds&&
        w.land_trade_balance().exported==0,"Order granted early export revenue or lost actual load");
    refused(w,sim::import_pottery(),"busy");
    w.tick();balance(w);
    const auto begun=w.courier(trader(w));check(begun.edge_progress>0&&begun.path.size()>3,"Actual caravan edge never began");
    refused(w,{sim::CommandType::RemoveRoad,begun.path[1]},"courier");
    check(std::find(begun.path.begin(),begun.path.end(),sim::Cell{114,5})!=begun.path.end(),"Control road is not on real outbound route");
    buy(w,{sim::CommandType::RemoveRoad,{114,5}});
    until(w,[&]{const auto& c=w.courier(trader(w));return c.route_pending&&c.edge_progress==0;},15,"Caravan did not finish begun edge safely");
    const auto waiting=w.courier(trader(w));
    for(int i=0;i<10;++i){w.tick();balance(w);}
    const auto& still=w.courier(trader(w));
    check(still.path==waiting.path&&still.path_vertex==waiting.path_vertex&&still.cargo==4&&
        !w.land_trade_state()->orders.back().boundary_tick&&w.land_trade_balance().exports_received==0,
        "Disconnected caravan teleported, discarded cargo or sold before boundary");
    buy(w,{sim::CommandType::PlaceRoad,{114,5}});
    for(const auto id:{s.city,s.gateway})buy(w,sim::set_building_operation(id,false));
    until(w,[&]{return w.land_trade_state()->orders.back().boundary_tick.has_value();},250,"Paid repaired route never reached boundary sale");
    check(w.land_trade_balance().exported==4&&w.land_trade_balance().exports_received==12&&w.courier(trader(w)).cargo==0,
        "Actual sale did not book exactly4 physical goods/12Funds");
    completed(w);
    auto forged=w.snapshot();auto& closed=forged.land_trade->orders.front();closed.boundary_tick=closed.ordered_tick+10;closed.completed_tick=closed.ordered_tick+20;
    restore_refused(f,w,forged);
    for(const auto id:{s.city,s.gateway})buy(w,sim::set_building_operation(id,true));
    const auto before=w.snapshot();const auto city=w.building(s.city).pottery_stock;const auto reserved=w.building(s.city).reserved_incoming;
    buy(w,sim::import_pottery());
    check(w.treasury()==before.treasury-24&&w.land_trade_balance().imports_paid==24&&w.land_trade_balance().imported==0&&
        w.building(s.city).pottery_stock==city&&w.building(s.city).reserved_incoming==reserved+4&&w.courier(trader(w)).cargo==0,
        "Prepaid empty outbound import granted stock/cargo or lost reserved headroom");
    refused(w,sim::export_pottery(),"busy");
    until(w,[&]{return w.land_trade_state()->orders.back().boundary_tick.has_value();},250,"Empty import caravan never reached external load");
    check(w.land_trade_balance().imported==4&&w.courier(trader(w)).cargo==4&&w.courier(trader(w)).reserved==4&&
        w.courier(trader(w)).phase==sim::CourierPhase::Returning,"Actual external load lost loaded return/reservation");
    const auto active=w.snapshot();
    for(int variant=0;variant<18;++variant) {
        auto bad=active;auto& state=*bad.land_trade;auto& order=state.orders.back();
        auto actor=std::find_if(bad.couriers.begin(),bad.couriers.end(),[&](const auto& c){return c.id==state.binding->courier;});
        switch(variant) {
        case 0:state.source_map_sha256=std::string(64,'b');break;
        case 1:state.model_source.sha256=std::string(64,'c');break;
        case 2:state.model_source.difficulty=original::Difficulty::Hard;break;
        case 3:++state.next_order_id;break;
        case 4:order.direction=static_cast<sim::LandTradeDirection>(255);break;
        case 5:order.boundary_tick=order.ordered_tick+1;break;
        case 6:state.binding->courier=static_cast<sim::CourierId>(999);break;
        case 7:std::swap(state.binding->warehouse,state.binding->gateway);break;
        case 8:actor->good=sim::Good::Clay;break;
        case 9:actor->cargo=5;break;
        case 10:actor->reserved=0;break;
        case 11:actor->phase=sim::CourierPhase::IdleAtWorkshop;break;
        case 12:order.boundary_tick.reset();break;
        case 13:order.completed_tick=bad.ticks+1;break;
        case 14:order.id=static_cast<sim::LandTradeOrderId>(99);break;
        case 15:state.orders.push_back(order);++state.next_order_id;break;
        case 16:++state.model_source.byte_count;break;
        case 17:bad.rule_version=6;break;
        }
        restore_refused(f,w,bad);
    }
    auto fork=f.restore(active);
    for(int i=0;i<103;++i){w.tick();fork.tick();balance(w);balance(fork);check(w.snapshot()==fork.snapshot(),"Complete active loaded-return continuation diverged");}
    completed(w);
    check(w.building(s.city).pottery_stock>=4&&w.land_trade_state()->orders.back().completed_tick&&
        w.land_trade_balance().imported==4,"Loaded import failed to become actual Warehouse stock");
    buy(w,sim::import_pottery());completed(w);refused(w,sim::import_pottery(),"offer exhausted");
    until(w,[&]{return w.building(s.city).pottery_stock>=4;},3000,"Real stock unavailable for second finite export");
    buy(w,sim::export_pottery());completed(w);refused(w,sim::export_pottery(),"offer or budget exhausted");
    check(w.land_trade_state()->orders.size()==4&&w.land_trade_balance().imports_paid==48&&
        w.land_trade_balance().exports_received==24&&w.land_trade_balance().imported==8&&w.land_trade_balance().exported==8,
        "Finite partner source/sink/money did not close exactly");
    auto exhausted=w.snapshot();exhausted.command_sequence=UINT64_MAX;restore_refused(f,w,exhausted);
    exhausted.command_sequence=UINT64_MAX-1;auto counter=f.restore(exhausted);
    buy(counter,sim::set_building_operation(s.city,false));
    check(counter.snapshot().command_sequence==UINT64_MAX,"Last representable ordinary command did not advance exactly once");
    refused(counter,sim::import_pottery(),"counters exhausted");
}
void paid_low_funds_full_headroom_and_fire() {
    Fixture f;
    auto low=f.fresh();const auto s=setup(low);buy(low,sim::configure_land_trade(s.city,s.gateway));
    for(int y=0;y<Fixture::side&&low.treasury()>=24;++y)for(int x=0;x<Fixture::side&&low.treasury()>=24;++x) {
        const sim::Command road{sim::CommandType::PlaceRoad,{x,y}};
        if(low.object_at({x,y})==sim::Object::Empty&&low.validate(road).accepted)buy(low,road);
    }
    check(low.treasury()>=0&&low.treasury()<24,"Ordinary paid purchases did not establish insufficient Funds");
    refused(low,sim::import_pottery(),"Not enough money");
    auto full=f.fresh();const auto p=setup(full,true,4);buy(full,sim::configure_land_trade(p.city,p.gateway));
    // Cut before any producer dispatch: ordinary Pottery can reach only the
    // city Warehouse, so full stock is earned without copying/injecting goods.
    // Four paid Houses retain at least eight actual residents. The unused
    // gateway pauses while production runs; after the Clay drain, Pottery6 +
    // city Warehouse2 remain staffed through ordinary population loss/fire.
    buy(full,sim::set_building_operation(p.gateway,false));
    buy(full,{sim::CommandType::RemoveRoad,{114,5}});
    earn_city_stock(full,p,sim::Rules::warehouse_capacity-4+1,4000,"Full import headroom",true);
    buy(full,sim::set_building_operation(p.pottery,false));
    buy(full,{sim::CommandType::PlaceRoad,{114,5}});
    buy(full,sim::set_building_operation(p.gateway,true));
    check(full.building_staffed(p.city)&&full.building_staffed(p.gateway)&&
        !full.building_on_fire(p.city)&&!full.building_on_fire(p.gateway),
        "Full-stock control must have two actual staffed, usable Warehouses");
    check(full.treasury()>=24,"Full-stock control lost purchase affordability");
    refused(full,sim::import_pottery(),"headroom");
    auto fire=f.fresh();const auto b=setup(fire,false);buy(fire,sim::configure_land_trade(b.city,b.gateway));
    until(fire,[&]{return fire.building_on_fire(b.city)||fire.building_on_fire(b.gateway);},2100,"Natural paid Warehouse fire did not occur");
    refused(fire,sim::import_pottery(),"on fire");
}
}
int main() {
    try {
        geometry_identity_and_configuration();real_paid_export_import_and_hostility();paid_low_funds_full_headroom_and_fire();
        std::cout<<"Authored Rule21 paid export/import, physical boundary/cargo, atomic negatives and hostile restore PASS\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
