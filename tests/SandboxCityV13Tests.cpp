#include "app/AutosaveController.h"
#include "core/PerformanceDiagnostics.h"
#include "simulation/World.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>

namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace {
constexpr int width=160,height=32;
auto mask() { return std::vector<std::uint8_t>(width*height,1); }
void check(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action,const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
sim::World fresh(sim::RulesProfile profile=sim::RulesProfile::CityV13) {
    return {width,height,mask(),profile};
}
sim::BuildingId put(sim::World& w,sim::CommandType type,sim::Cell cell) {
    const auto r=w.execute({type,cell});
    check(r.accepted && r.changed,"placement failed: "+r.reason);
    return *w.building_owner_at(cell);
}
void road(sim::World& w,int first,int last,int y=4) {
    for (int x=first;x<=last;++x)
        check(w.execute({sim::CommandType::PlaceRoad,{x,y}}).accepted,"road failed");
}
void operation(sim::World& w,sim::BuildingId id,bool enabled) {
    check(w.execute(sim::set_building_operation(id,enabled)).accepted,"operation failed");
}
sim::BuildingId kind(const sim::World& w,sim::Object wanted) {
    for (const auto& b:w.buildings()) if (b.kind==wanted) return b.id;
    throw std::runtime_error("missing building kind");
}
void invariants(const sim::World& w) {
    check(w.production_balance_valid() && w.food_balance_valid() && w.city_economy_valid() &&
        w.service_state_valid() && w.population_valid() && w.navigation_valid() &&
        w.fire_state_valid(),"City-v13 invariant at tick "+std::to_string(w.ticks()));
    std::uint64_t taxes=w.snapshot().demolition_history.taxes;
    for (const auto& b:w.buildings()) {
        if (b.kind==sim::Object::Household) taxes+=b.taxes_paid_total;
        else check(b.taxes_paid_total==0,"non-House tax authority");
    }
    if (sim::desirability_profile(w.profile()))
        check(taxes==w.taxes_collected_total(),"tax history conservation");
    check(w.buildings().size()<=40 && w.couriers().size()<=24 &&
        w.route_cache_entries()<=24*38,"collection/cache budget");
}
void advance(sim::World& w,std::uint64_t tick) { while (w.ticks()<tick) w.tick();invariants(w); }
template<class P> void until(sim::World& w,P done,int budget=3000) {
    while (budget-- && !done()) w.tick();
    check(done(),"state wait timed out at "+std::to_string(w.ticks()));invariants(w);
}
sim::World starter(sim::RulesProfile profile=sim::RulesProfile::CityV13) {
    auto w=fresh(profile);
    put(w,sim::CommandType::PlaceClaySource,{0,2});
    put(w,sim::CommandType::PlacePottery,{0,5});
    put(w,sim::CommandType::PlaceWarehouse,{3,2});
    put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceMarket,{3,5});
    put(w,sim::CommandType::PlaceServicePost,{5,5});road(w,0,14);
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(w,sim::CommandType::PlaceHousehold,cell);
    put(w,sim::CommandType::PlaceFireWatch,{14,5});
    return w;
}
void geometry_and_score() {
    check(sim::footprint_distance({0,0},{2,2},{3,3},{2,2})==4,"origin distance used");
    check(sim::footprint_distance({0,0},{2,2},{2,1},{1,1})==1,"one-cell Market distance");
    for (int x=-10;x<11;++x) for (int y=-10;y<11;++y) {
        int expected=INT_MAX;
        for (int a=0;a<2;++a) for (int b=0;b<2;++b)
            for (int c=0;c<2;++c) for (int d=0;d<2;++d)
                expected=std::min(expected,std::abs(x+c-a)+std::abs(y+d-b));
        check(sim::footprint_distance({0,0},{2,2},{x,y},{2,2})==expected,"footprint minimum wrong");
    }
    check(sim::footprint_distance({INT_MIN,0},{2,2},{INT_MAX,0},{2,2})==INT_MAX,
        "distance arithmetic overflow");
    const int pottery[]={-22,-22,-22,-16,-16,-11,-11,-5,-5,0};
    const int market[]={12,12,12,9,9,6,6,3,3,0};
    for (int d=0;d<10;++d) {
        check(sim::desirability_contribution(sim::Object::Pottery,d)==pottery[d],"negative rounding");
        check(sim::desirability_contribution(sim::Object::Market,d)==market[d],"positive rounding");
    }
    check(sim::desirability_impact(sim::Object::ClaySource)==-18 &&
        sim::desirability_impact(sim::Object::Warehouse)==-10 &&
        sim::desirability_impact(sim::Object::Farm)==-4 &&
        sim::desirability_impact(sim::Object::ServicePost)==10 &&
        sim::desirability_impact(sim::Object::FireWatch)==8 &&
        sim::desirability_impact(sim::Object::Household)==0,"authored table changed");
    check(sim::clamp_desirability(-101)==-100 && sim::clamp_desirability(101)==100 &&
        sim::clamp_desirability(20)==20,"clamp");
    check(sim::desirability_level_cap(-21)==0 && sim::desirability_level_cap(-20)==1 &&
        sim::desirability_level_cap(9)==1 && sim::desirability_level_cap(10)==2,"thresholds");
    auto w=fresh();const auto h=put(w,sim::CommandType::PlaceHousehold,{10,10});
    check(w.household_desirability(h)==0,"House alone");
    const auto p=put(w,sim::CommandType::PlacePottery,{12,10});
    check(w.building_distance(h,p)==1 && w.household_desirability(h)==-22,"adjacent Pottery");
    operation(w,p,false);check(w.household_desirability(h)==-22,"pause changed score");
    const auto m=put(w,sim::CommandType::PlaceMarket,{15,10});
    check(w.building_distance(h,m)==4 && w.household_desirability(h)==-13,"nearby Market");
    put(w,sim::CommandType::PlaceWarehouse,{21,10});
    put(w,sim::CommandType::PlaceHousehold,{9,12});
    check(w.household_desirability(h)==-13,"outside radius / neighbor House changed score");
    check(w.household_desirability_at({10,10})==-13,"preview differs from placed House");
    const auto before=w.snapshot();const auto refresh=w.route_refresh_count();
    openemperor::performance::set_enabled(true);openemperor::performance::reset();
    for (int i=0;i<1000;++i) {
        check(w.household_desirability(h)==-13,"read query unstable");
        (void)w.household_desirability_sources(h);(void)w.household_desirability_at({10,10});
    }
    for (const auto counter:{openemperor::performance::Counter::WorldCopies,
        openemperor::performance::Counter::WorldRestores,openemperor::performance::Counter::WorldExecutes,
        openemperor::performance::Counter::BfsCalls,openemperor::performance::Counter::RouteRefreshes})
        check(openemperor::performance::counter(counter)==0,"score query did work or mutation");
    openemperor::performance::set_enabled(false);
    check(w.snapshot()==before && w.route_refresh_count()==refresh,"query changed authority");
    rejects([&]{w.household_desirability(p);},"non-House query accepted");
    auto old=fresh(sim::RulesProfile::CityV12);const auto old_h=put(old,sim::CommandType::PlaceHousehold,{0,0});
    rejects([&]{old.household_desirability(old_h);},"old profile score accepted");
}
void paid_starter_and_quality() {
    auto w=starter();check(w.ticks()==0 && w.treasury()==20 && w.total_population()==24 &&
        w.workforce_used()==24 && w.construction_spent_total()==1280 &&
        w.taxes_collected_total()==0,"paid starter changed");
    const int expected[]={-14,-5,2,10};int i=0;
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(w.household_desirability(b.id)==expected[i++],"starter score changed");
    advance(w,10000);
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household) {
        check(w.historical_household_level(b.id)==2,"starter did not develop through actual supply");
        const bool good=w.household_desirability(b.id)>=10;
        check(w.household_level(b.id)==(good ? 2:1) &&
            b.population==(good ? 16:10) && b.taxes_paid_total>0,"starter quality/capacity did not matter");
    }
    check(w.burning_buildings()==0 && w.covered_households()==4,"starter lost protection/supply");
    check(w.settlement_goal_households_ready()==1,"goal counted historical instead of effective L2");
    const auto neutral=*w.building_owner_at({6,5});
    const auto good=*w.building_owner_at({9,5});
    put(w,sim::CommandType::PlaceServicePost,{11,8});
    check(w.household_desirability(neutral)==0 && w.household_level(neutral)==1 &&
        w.historical_household_level(neutral)==2,"historical L2 at exact neutral score 0");
    // Same supply histories, different spatial caps; no extra demand is necessary.
    check(w.household_desirability(good)>=10 && w.household_level(good)==2,
        "good residential district lost level");
    const auto before_ready=w.settlement_goal_households_ready();
    operation(w,kind(w,sim::Object::ClaySource),false);
    const auto near=put(w,sim::CommandType::PlacePottery,{12,5});operation(w,near,false);
    check(w.household_level(good)<2 && w.historical_household_level(good)==2 &&
        w.settlement_goal_households_ready()<before_ready,"goal readiness latched historical level");
    check(w.execute(sim::demolish_building(near)).accepted &&
        w.settlement_goal_households_ready()==before_ready,"relocation did not restore derived readiness");
    std::cout<<"paid starter: 1280, scores [-14,-5,2,10], capacities [10,10,10,16]\n";
}
// The quality/rate/over-cap fixture earns all development, people, goods and funds
// through the command-built starter. Industry additions are ordinary paid commands.
void tax_overcap_relocation() {
    auto w=starter();advance(w,10000);
    const auto h=*w.building_owner_at({9,5});const auto watch=kind(w,sim::Object::FireWatch);
    operation(w,watch,false);
    until(w,[&]{return std::ranges::all_of(w.couriers(),[&](const auto& c){return c.owner!=watch ||
        c.phase==sim::CourierPhase::IdleAtWorkshop;});});
    operation(w,kind(w,sim::Object::ClaySource),false);
    const auto pottery=put(w,sim::CommandType::PlacePottery,{12,5});operation(w,pottery,false);
    const auto clay=put(w,sim::CommandType::PlaceClaySource,{12,2});operation(w,clay,false);
    check(w.household_desirability(h)==-25 && w.household_level(h)==0 &&
        w.historical_household_level(h)==2 && w.household_population_capacity(h)==6 &&
        w.building(h).population==16,"instant population truncation / historical loss");
    const auto before=w.building(h);const auto world_tax=w.taxes_collected_total();
    const auto deadline=w.ticks()+static_cast<unsigned>(400-before.demand_progress);
    advance(w,deadline);
    check(w.building(h).fulfilled_demand==before.fulfilled_demand+1 &&
        w.building(h).population==15 && w.building(h).taxes_paid_total==before.taxes_paid_total+25,
        "poor quality supplied rate / over-cap single decline");
    const auto paid=w.building(h).taxes_paid_total;
    check(w.demolition_status(pottery).allowed && w.demolition_status(clay).allowed,
        "paused empty industry unexpectedly blocked");
    check(w.execute(sim::demolish_building(pottery)).accepted &&
        w.execute(sim::demolish_building(clay)).accepted,"industry removal");
    check(w.household_desirability(h)==10 && w.household_level(h)==2 &&
        w.building(h).population==15,"derived recovery changed population/history");
    put(w,sim::CommandType::PlacePottery,{60,10});
    advance(w,deadline+400);
    check(w.building(h).taxes_paid_total==paid+60 &&
        w.building(h).taxes_paid_total-before.taxes_paid_total==85 &&
        w.building(h).population==16 && w.taxes_collected_total()>world_tax,"recovered tax/growth");
    // A supplied burning House still misses without consuming goods or paying tax.
    // Only valid fire deadlines are isolated; no stock/population/economy is invented.
    const auto second_pot=put(w,sim::CommandType::PlacePottery,{12,5});operation(w,second_pot,false);
    const auto second_clay=put(w,sim::CommandType::PlaceClaySource,{12,2});operation(w,second_clay,false);
    check(w.household_desirability(h)==-25 && w.building(h).population==16,"miss over-cap setup");
    auto state=w.snapshot();for (auto& b:state.buildings) if (b.id==h) {
        b.fire_protection_until_tick=0;b.fire_risk=0;b.fire_until_tick=state.ticks+600;
    }
    w=sim::World::restore(state,mask());
    check(w.household_desirability(h)==-25,"fire changed score");
    const auto before_fire=w.building(h);
    advance(w,w.ticks()+400);
    check(w.building(h).missed_demand==before_fire.missed_demand+1 &&
        w.building(h).consumed_total==before_fire.consumed_total &&
        w.building(h).taxes_paid_total==before_fire.taxes_paid_total &&
        w.building(h).population==before_fire.population-1,"fire demand not orthogonal");
    std::cout<<"historical L2: poor L0 +25, recover L2 +60; over-cap 16->15, recovery 15->16\n";
}
void negative_clamp_and_sources() {
    auto w=starter();advance(w,20000);
    const auto h=put(w,sim::CommandType::PlaceHousehold,{50,10});
    for (const auto cell:{sim::Cell{48,8},sim::Cell{50,8},sim::Cell{52,8}})
        put(w,sim::CommandType::PlacePottery,cell);
    for (const auto cell:{sim::Cell{48,12},sim::Cell{50,12},sim::Cell{52,12}})
        put(w,sim::CommandType::PlaceClaySource,cell);
    put(w,sim::CommandType::PlaceWarehouse,{48,10});
    check(w.household_desirability(h)==-100,"World did not clamp negative sum");
    const auto sources=w.household_desirability_sources(h);int raw=0;
    for (const auto& source:sources) raw+=source.contribution;
    check(raw== -130 && sources.size()==7,"source list lost unclamped signed contributions");
    for (std::size_t i=1;i<sources.size();++i)
        check(std::abs(sources[i-1].contribution)>std::abs(sources[i].contribution) ||
            (std::abs(sources[i-1].contribution)==std::abs(sources[i].contribution) &&
             sources[i-1].id<sources[i].id),"source order is not magnitude then ID");
    const auto start=std::chrono::steady_clock::now();int checksum=0;
    for (int i=0;i<100000;++i) checksum+=w.household_desirability(h);
    const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now()-start).count();
    check(checksum== -10000000,"score benchmark changed result");
    std::cout<<"score scan: "<<w.buildings().size()<<" buildings, "<<ns/100000
        <<" ns/query (100k queries), no cache/BFS\n";
}
// Isolated goal query fixture: normal paid placement creates all geometry.
// Independently constructed, validated accounting then represents eight historical
// supplied demands per House. It is not a claimed simulated supply playthrough.
void derived_goal_can_be_lost() {
    auto w=starter();advance(w,20000);
    for (const int y:{5,8}) for (int x=40;x<=52;x+=3)
        put(w,sim::CommandType::PlaceHousehold,{x,y});
    put(w,sim::CommandType::PlaceMarket,{44,7});
    put(w,sim::CommandType::PlaceMarket,{50,7});
    put(w,sim::CommandType::PlaceServicePost,{45,7});
    put(w,sim::CommandType::PlaceFireWatch,{47,7});
    auto s=w.snapshot();std::uint64_t consumed=0,taxes=0;
    for (auto& b:s.buildings) {
        b.input_clay=0;b.output=0;b.pottery_stock=0;b.reserved_incoming=0;
        b.food_stock=0;b.reserved_food_incoming=0;b.progress=0;b.active_recipe_clay=0;
        b.clay_extracted=0;b.recipes_completed=0;b.food_produced=0;
        if (b.kind==sim::Object::Household) {
            b.placed_tick=s.ticks-3200;b.demand_progress=0;b.fulfilled_demand=8;b.missed_demand=0;
            b.consumed_total=8;b.food_consumed_total=8;b.last_demand_status=1;
            b.population=10;b.taxes_paid_total=200;consumed+=8;taxes+=200;
        }
    }
    for (auto& b:s.buildings) {
        if (b.id==kind(w,sim::Object::ClaySource)) b.clay_extracted=consumed*2;
        if (b.id==kind(w,sim::Object::Pottery)) b.recipes_completed=consumed;
        if (b.id==kind(w,sim::Object::Farm)) b.food_produced=consumed;
    }
    for (auto& c:s.couriers) {
        c.phase=sim::CourierPhase::IdleAtWorkshop;c.target=c.owner;c.cargo=0;c.reserved=0;
        c.path.clear();c.path_vertex=0;c.edge_progress=0;c.route_pending=false;
        c.route_checked_revision.reset();c.last_dispatched_target.reset();c.last_dispatched_pottery.reset();
    }
    s.clay_extracted_total=consumed*2;s.pottery_completed_total=consumed;s.food_produced_total=consumed;
    s.taxes_collected_total=taxes;s.treasury=1300+static_cast<std::int64_t>(taxes)-
        static_cast<std::int64_t>(s.construction_spent_total);
    w=sim::World::restore(s,mask());invariants(w);
    check(w.settlement_goal_households_ready()>=10 && w.settlement_goal_reached(),"good district goal fixture");
    const auto a=put(w,sim::CommandType::PlacePottery,{40,3});
    const auto b=put(w,sim::CommandType::PlacePottery,{43,3});
    check(w.settlement_goal_households_ready()<8 && !w.settlement_goal_reached(),"goal latched after zoning deteriorated");
    check(w.execute(sim::demolish_building(a)).accepted && w.execute(sim::demolish_building(b)).accepted &&
        w.settlement_goal_reached(),"goal did not recover immediately after safe relocation");
}
void spatial_supply_independent() {
    auto w=fresh();const auto h=put(w,sim::CommandType::PlaceHousehold,{10,10});
    put(w,sim::CommandType::PlaceMarket,{12,10});
    put(w,sim::CommandType::PlaceServicePost,{12,11});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{12,12});
    check(w.household_desirability(h)==30 && !w.household_service_active(h) &&
        !w.building_fire_protected(h),"spatial infrastructure gave actual coverage");
    advance(w,400);
    check(w.building(h).missed_demand==1 && w.building(h).fulfilled_demand==0 &&
        w.taxes_collected_total()==0,"quality fabricated supply");
    operation(w,watch,false);advance(w,2000);
    check(w.building_on_fire(h) && w.household_desirability(h)==30,"fire/pause spatial score changed");
}
struct Temp {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-city13-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {fs::create_directories(root/"data/Cities");
        std::ofstream(root/"data/Cities/Synthetic.map")<<"independently authored synthetic identity";}
    ~Temp() {std::error_code ec;fs::remove_all(root,ec);}
};
void roundtrip(const Temp& t,const sim::World& w) {
    const auto before=w.snapshot();
    save::write_save(t.root/"save.json",save::make_document(t.root/"data","Cities/Synthetic.map",mask(),w),
        t.root/"data",mask());
    const auto doc=save::read_save(t.root/"save.json");
    check(doc.source_schema_version==15,"schema15 identity");
    auto restored=save::restore_save(doc,t.root/"data",mask());
    check(restored.snapshot()==before && w.snapshot()==before,"save/load mutation");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(restored.household_desirability(b.id)==w.household_desirability(b.id),"derived score changed on load");
    auto control=sim::World::restore(before,mask());
    for (int i=0;i<500;++i) {
        restored.tick();control.tick();check(restored.snapshot()==control.snapshot(),"resume diverged");
    }
}
void persistence_and_demolition() {
    Temp t;auto w=starter();advance(w,6000);roundtrip(t,w);
    auto malformed=w.snapshot();malformed.buildings[0].taxes_paid_total=25;
    rejects([&]{sim::World::restore(malformed,mask());},"non-House tax accepted");
    const auto h=*w.building_owner_at({9,5});
    malformed=w.snapshot();for (auto& b:malformed.buildings) if (b.id==h) ++b.taxes_paid_total;
    rejects([&]{sim::World::restore(malformed,mask());},"invalid tax accounting accepted");
    nlohmann::json j;{std::ifstream in(t.root/"save.json");in>>j;}
    check(j.dump().find("desirability")==std::string::npos,"derived score serialized");
    for (const auto& b:j["world"]["buildings"]) check(b.contains("taxes_paid_total"),"missing actual tax authority");
    const auto bad=[&](auto edit) {
        auto copy=j;edit(copy);{std::ofstream out(t.root/"bad.json");out<<copy;}
        rejects([&]{save::restore_save(save::read_save(t.root/"bad.json"),t.root/"data",mask());},"invalid schema/profile/tax accepted");
    };
    bad([](auto& x){x["schema_version"]=14;});
    bad([](auto& x){x["rules"]["id"]=sim::city_v12_profile_name;});
    bad([](auto& x){x["world"]["buildings"][0]["taxes_paid_total"]=25;});
    bad([](auto& x){x["world"]["buildings"][6].erase("taxes_paid_total");});
    bad([](auto& x){x["world"]["buildings"][6]["taxes_paid_total"]=-1;});
    bad([](auto& x){x["world"]["buildings"][6]["taxes_paid_total"]=UINT64_MAX;});
    auto old=starter(sim::RulesProfile::CityV12);advance(old,6000);
    save::write_save(t.root/"old.json",save::make_document(t.root/"data","Cities/Synthetic.map",mask(),old),t.root/"data",mask());
    nlohmann::json old_json;{std::ifstream in(t.root/"old.json");in>>old_json;}
    check(old_json["schema_version"]==14 && old_json.dump().find("taxes_paid_total")==std::string::npos,
        "schema14 gained field");
    for (const auto& b:old.buildings()) if (b.kind==sim::Object::Household)
        check(old.household_level(b.id)==old.historical_household_level(b.id) &&
            b.taxes_paid_total==0,"City12 level/tax changed");
    check(save::restore_save(save::read_save(t.root/"old.json"),t.root/"data",mask()).snapshot()==old.snapshot(),"schema14 roundtrip");
    // Isolate exactly one genuine paid demand using normal production controls.
    w=fresh();
    const auto clay=put(w,sim::CommandType::PlaceClaySource,{0,2});
    const auto pot=put(w,sim::CommandType::PlacePottery,{0,5});
    put(w,sim::CommandType::PlaceWarehouse,{3,2});
    const auto farm=put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceMarket,{3,5});
    const auto service=put(w,sim::CommandType::PlaceServicePost,{5,5});
    road(w,0,14);
    const auto empty=put(w,sim::CommandType::PlaceHousehold,{6,2});
    for (int x=80;x<89;x+=3) put(w,sim::CommandType::PlaceHousehold,{x,2});
    operation(w,farm,false);
    until(w,[&]{return w.clay_extracted_total()==2;});operation(w,clay,false);
    until(w,[&]{return w.pottery_completed_total()==1;});operation(w,pot,false);
    operation(w,farm,true);
    until(w,[&]{return w.food_produced_total()==1;});operation(w,farm,false);
    until(w,[&]{return w.building(empty).fulfilled_demand==1;});operation(w,service,false);
    until(w,[&]{return w.demolition_status(empty).allowed;});
    check(w.building(empty).pottery_stock==0 && w.building(empty).food_stock==0,
        "controlled production did not drain House");
    const auto tax=w.building(empty).taxes_paid_total,world_tax=w.taxes_collected_total();
    const auto history=w.snapshot().demolition_history.taxes;
    check(tax>0 && w.demolition_status(empty).allowed,"safe tax-paying House demolition");
    check(w.execute(sim::demolish_building(empty)).accepted,"House demolition");
    check(w.taxes_collected_total()==world_tax && w.snapshot().demolition_history.taxes==history+tax,
        "demolition lost or doubled actual tax history");invariants(w);roundtrip(t,w);
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    auto document=[&]{return save::make_document(t.root/"data","Cities/Synthetic.map",mask(),w);};
    check(autosave.begin(document(),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"recovery start");
    advance(w,w.ticks()+1200);const auto before=w.snapshot();
    check(autosave.poll(document(),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved &&
        w.snapshot()==before,"checkpoint mutation");
    const auto catalog=autosave.store().catalog();const auto& entry=catalog.histories.at(0).entries.front();
    check(entry.schema==15 && entry.profile==sim::city_v13_profile_name,"recovery schema identity");
    auto restored=save::restore_save(save::read_save(entry.path),t.root/"data",mask());
    check(restored.snapshot()==before,"recovery changed tax/history");
    std::cout<<"schema15: actual tax authority, no score; safe House demolition and recovery exact\n";
}
void two_supplied_districts() {
    auto w=starter();advance(w,20000);
    const auto mixed=*w.building_owner_at({6,2});
    const auto clay=put(w,sim::CommandType::PlaceClaySource,{30,2});
    put(w,sim::CommandType::PlacePottery,{30,5});
    put(w,sim::CommandType::PlaceWarehouse,{33,2});
    put(w,sim::CommandType::PlaceFarm,{34,5});
    put(w,sim::CommandType::PlaceMarket,{43,5});
    put(w,sim::CommandType::PlaceServicePost,{44,5});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{46,5});
    const auto quiet=put(w,sim::CommandType::PlaceHousehold,{44,2});
    put(w,sim::CommandType::PlaceHousehold,{48,2});road(w,30,50);
    check(w.household_desirability(quiet)==25 && w.household_desirability(mixed)==-14,
        "separated residential / mixed district scores");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::ClaySource ||
        b.kind==sim::Object::Pottery || b.kind==sim::Object::Warehouse || b.kind==sim::Object::Farm)
        check(w.building_distance(quiet,b.id)>8,"quiet House still inside industrial radius");
    advance(w,26000);
    check(w.historical_household_level(quiet)==2 && w.historical_household_level(mixed)==2 &&
        w.household_level(quiet)==2 && w.household_level(mixed)==1 &&
        w.building(quiet).population==16 && w.building(mixed).population==10,
        "two supplied districts did not develop differently");
    const auto a=w.building(quiet),b=w.building(mixed);advance(w,28000);
    check(w.building(quiet).fulfilled_demand-a.fulfilled_demand==5 &&
        w.building(mixed).fulfilled_demand-b.fulfilled_demand==5 &&
        w.building(quiet).taxes_paid_total-a.taxes_paid_total==300 &&
        w.building(mixed).taxes_paid_total-b.taxes_paid_total==200,
        "equal five actual supplied demands did not yield 60 vs 40 rates");
    operation(w,watch,false);operation(w,clay,false);
    until(w,[&]{return std::ranges::all_of(w.couriers(),[&](const auto& c){return c.owner!=watch ||
        c.phase==sim::CourierPhase::IdleAtWorkshop;});});
    const auto p1=put(w,sim::CommandType::PlacePottery,{46,2});operation(w,p1,false);
    const auto p2=put(w,sim::CommandType::PlacePottery,{48,0});operation(w,p2,false);
    const auto c=put(w,sim::CommandType::PlaceClaySource,{46,0});operation(w,c,false);
    check(w.household_desirability(quiet)==-31 && w.household_level(quiet)==0 &&
        w.building(quiet).population==16,"industrial intrusion did not cap quality / instant truncation");
    const auto before=w.building(quiet);
    advance(w,w.ticks()+static_cast<unsigned>(400-before.demand_progress));
    check(w.building(quiet).fulfilled_demand==before.fulfilled_demand+1 &&
        w.building(quiet).population==15 && w.building(quiet).taxes_paid_total==before.taxes_paid_total+25,
        "perfect buffered supply did not coexist with poor spatial quality");
    for (const auto id:{p1,p2,c}) check(w.execute(sim::demolish_building(id)).accepted,"empty industrial relocation blocked");
    put(w,sim::CommandType::PlacePottery,{90,18});
    check(w.household_desirability(quiet)==25 && w.household_level(quiet)==2,"quiet district did not recover");
    operation(w,clay,true);operation(w,watch,true);advance(w,w.ticks()+800);
    check(w.building(quiet).population==16,"quiet district growth did not recover");
    std::cout<<"two paid, supplied districts: scores +25/-14, residents 16/10; five demands each, taxes 300/200; "
        "industry intrusion -31, real 25 tax, safe relocation +25 and population recovery\n";
}
void determinism() {
    auto a=starter(),b=starter();sim::BuildingId near{},far{};bool removed=false,burned=false;
    for (int i=0;i<20000;++i) {
        if (i==6000) {
            operation(a,kind(a,sim::Object::ClaySource),false);
            operation(b,kind(b,sim::Object::ClaySource),false);
            near=put(a,sim::CommandType::PlacePottery,{12,5});
            check(put(b,sim::CommandType::PlacePottery,{12,5})==near,"ID differed");
            operation(a,near,false);operation(b,near,false);
            const auto watch=kind(a,sim::Object::FireWatch);
            for (auto* w:{&a,&b}) check(w->execute(sim::set_building_workforce_priority(watch,
                sim::WorkforcePriority::High)).accepted,"priority");
            far=put(a,sim::CommandType::PlaceServicePost,{60,10});
            put(b,sim::CommandType::PlaceServicePost,{60,10});
            operation(a,far,false);operation(b,far,false);
        }
        if (i>=6500 && near!=sim::BuildingId{} && !removed && a.demolition_status(near).allowed) {
            check(a.execute(sim::demolish_building(near)).accepted &&
                b.execute(sim::demolish_building(near)).accepted,"deterministic demolition");
            put(a,sim::CommandType::PlacePottery,{65,10});put(b,sim::CommandType::PlacePottery,{65,10});removed=true;
        }
        a.tick();b.tick();
        if (far!=sim::BuildingId{}) burned=burned || a.building_on_fire(far);
        check(a.snapshot()==b.snapshot(),"20k identical commands diverged");
    }
    invariants(a);check(removed && burned,"20k missing relocation/fire");
    std::cout<<"20,000 deterministic ticks: industry, demolition, distant rebuild, fire, priority\n";
}
void endurance() {
    Temp t;auto w=starter();advance(w,10000);
    const auto developed=*w.building_owner_at({9,5});
    const auto watch=kind(w,sim::Object::FireWatch);
    operation(w,watch,false);operation(w,kind(w,sim::Object::ClaySource),false);
    until(w,[&]{return std::ranges::all_of(w.couriers(),[&](const auto& c){return c.owner!=watch ||
        c.phase==sim::CourierPhase::IdleAtWorkshop;});});
    const auto mixed_pot=put(w,sim::CommandType::PlacePottery,{12,5});operation(w,mixed_pot,false);
    const auto mixed_clay=put(w,sim::CommandType::PlaceClaySource,{12,2});operation(w,mixed_clay,false);
    check(w.household_level(developed)==0 && w.building(developed).population==16,"endurance over-cap setup");
    const auto decline_start=w.ticks();advance(w,decline_start+1600);
    check(w.building(developed).population==12,"over-cap decline not exactly one per demand");
    until(w,[&]{return w.demolition_status(mixed_pot).allowed && w.demolition_status(mixed_clay).allowed;},1000);
    check(w.execute(sim::demolish_building(mixed_pot)).accepted &&
        w.execute(sim::demolish_building(mixed_clay)).accepted,"endurance relocation failed");
    check(w.household_level(developed)==2,"endurance historical recovery failed");
    operation(w,kind(w,sim::Object::ClaySource),true);operation(w,watch,true);
    advance(w,20000);check(w.building(developed).population==16,"endurance recovery growth failed");
    const std::array<std::tuple<sim::CommandType,sim::Object,unsigned>,8> types{{
        {sim::CommandType::PlaceClaySource,sim::Object::ClaySource,4},
        {sim::CommandType::PlacePottery,sim::Object::Pottery,4},
        {sim::CommandType::PlaceWarehouse,sim::Object::Warehouse,2},
        {sim::CommandType::PlaceFarm,sim::Object::Farm,2},
        {sim::CommandType::PlaceServicePost,sim::Object::ServicePost,2},
        {sim::CommandType::PlaceMarket,sim::Object::Market,4},
        {sim::CommandType::PlaceHousehold,sim::Object::Household,20},
        {sim::CommandType::PlaceFireWatch,sim::Object::FireWatch,2}}};
    int row=0;sim::BuildingId remote{};
    for (const auto [type,object,limit]:types) {
        const auto count=static_cast<unsigned>(std::ranges::count_if(w.buildings(),[&](const auto& b){return b.kind==object;}));
        for (unsigned i=count;i<limit;++i) {
            const auto id=put(w,type,{35+3*static_cast<int>(i),row});
            if (object==sim::Object::Pottery && remote==sim::BuildingId{}) {remote=id;operation(w,id,false);}
        }
        row+=2;
    }
    check(w.buildings().size()==40 && w.couriers().size()==24,"full city limits");
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    auto document=[&]{return save::make_document(t.root/"data","Cities/Synthetic.map",mask(),w);};
    check(autosave.begin(document(),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"endurance autosave start");
    bool fire=false,replaced=false;std::uint64_t roundtrips=0;
    for (int i=0;i<100000;++i) {
        w.tick();if (i%100==0) invariants(w);
        if (!replaced) fire=fire || w.building_on_fire(remote);
        if (fire && !replaced && w.demolition_status(remote).allowed) {
            check(w.execute(sim::demolish_building(remote)).accepted,"endurance demolition");
            put(w,sim::CommandType::PlacePottery,{95,10});replaced=true;
        }
        if (i%5000==4999) {
            roundtrip(t,w);const auto before=w.snapshot();
            check(autosave.poll(document(),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved &&
                w.snapshot()==before,"endurance checkpoint mutation");
            const auto catalog=autosave.store().catalog();const auto& entry=catalog.histories.at(0).entries.front();
            check(save::restore_save(save::read_save(entry.path),t.root/"data",mask()).snapshot()==before,
                "endurance recovery mismatch");++roundtrips;
        }
    }
    check(fire && replaced && roundtrips==20 && w.taxes_collected_total()>0,"endurance coverage incomplete");
    invariants(w);std::cout<<"100,000 ticks, 40 buildings / 24 couriers, fire/relocation, 20 save/autosave/recovery roundtrips\n";
}
}
int main(int argc,char* argv[]) {
    try {
        const std::string mode=argc>1 ? argv[1]:"base";
        if (mode=="base") {geometry_and_score();paid_starter_and_quality();tax_overcap_relocation();negative_clamp_and_sources();derived_goal_can_be_lost();spatial_supply_independent();}
        else if (mode=="persistence") persistence_and_demolition();
        else if (mode=="districts") two_supplied_districts();
        else if (mode=="determinism") determinism();
        else if (mode=="endurance") endurance();
        else throw std::invalid_argument("unknown test mode");
        return 0;
    } catch (const std::exception& e) {std::cerr<<"City-v13: "<<e.what()<<'\n';return 1;}
}
