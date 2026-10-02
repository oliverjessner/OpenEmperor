#include "app/AutosaveController.h"
#include "simulation/CityStartGuidance.h"
#include "core/PerformanceDiagnostics.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace {
constexpr int width=110,height=16;
auto mask() { return std::vector<std::uint8_t>(width*height,1); }
void check(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> void rejects(F action,const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
sim::World fresh() { return {width,height,mask(),sim::RulesProfile::CityV12}; }
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
void priority(sim::World& w,sim::BuildingId id,sim::WorkforcePriority value) {
    check(w.execute(sim::set_building_workforce_priority(id,value)).accepted,"priority failed");
}
sim::BuildingId kind(const sim::World& w,sim::Object wanted) {
    for (const auto& b:w.buildings()) if (b.kind==wanted) return b.id;
    throw std::runtime_error("missing building kind");
}
sim::CourierId owner_courier(const sim::World& w,sim::BuildingId owner) {
    for (const auto& c:w.couriers()) if (c.owner==owner) return c.id;
    throw std::runtime_error("missing owner courier");
}
void invariants(const sim::World& w) {
    check(w.production_balance_valid() && w.food_balance_valid() && w.city_economy_valid() &&
        w.service_state_valid() && w.population_valid() && w.navigation_valid() &&
        w.fire_state_valid(),"City-v12 invariant failed at tick "+std::to_string(w.ticks()));
    check(w.workforce_used()<=w.workforce_supply(),"workers allocated twice");
    check(w.buildings().size()<=40 && w.couriers().size()<=24,"entity budget exceeded");
    check(w.route_cache_entries()<=24*38,"route cache budget exceeded");
    for (const auto& c:w.couriers()) if (c.role==sim::CourierRole::FireInspector)
        check(c.cargo==0 && c.reserved==0 && c.dynamic_target_routes.size()<=38,
            "Inspector cargo/cache mismatch");
}
void advance(sim::World& w,std::uint64_t tick) {
    while (w.ticks()<tick) { w.tick(); invariants(w); }
}
template<class P> void until(sim::World& w,P done,int budget=3000) {
    while (budget-- && !done()) { w.tick(); invariants(w); }
    check(done(),"arrival/state wait timed out at "+std::to_string(w.ticks()));
}
sim::World starter(bool watch=true) {
    auto w=fresh();
    put(w,sim::CommandType::PlaceClaySource,{0,2});
    put(w,sim::CommandType::PlacePottery,{0,5});
    put(w,sim::CommandType::PlaceWarehouse,{3,2});
    put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceMarket,{3,5});
    put(w,sim::CommandType::PlaceServicePost,{5,5});
    road(w,0,14);
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(w,sim::CommandType::PlaceHousehold,cell);
    if (watch) put(w,sim::CommandType::PlaceFireWatch,{14,5});
    return w;
}

// Isolated incident fixtures are constructed from valid command-built Worlds.
// Only authored fire deadlines are changed; no goods, funds, routes or people
// are fabricated. Natural incidents are tested separately, including endurance.
sim::World incident(const sim::World& w,sim::BuildingId target) {
    auto s=w.snapshot();
    for (auto& b:s.buildings) if (sim::fire_eligible(b.kind)) {
        b.fire_risk=0;
        b.fire_until_tick=b.id==target ? s.ticks+600:s.ticks;
        b.fire_protection_until_tick=b.id==target ? 0:s.ticks+2400;
    }
    return sim::World::restore(s,mask());
}

void natural_boundaries() {
    auto w=fresh();
    const auto clay=put(w,sim::CommandType::PlaceClaySource,{0,2});
    for (int x=5;x<17;x+=3) put(w,sim::CommandType::PlaceHousehold,{x,2});
    advance(w,19);check(w.building(clay).fire_risk==0,"early risk");
    w.tick();check(w.building(clay).fire_risk==1,"risk at tick 20");
    advance(w,1999);check(w.building(clay).fire_risk==99 && !w.building_on_fire(clay),"early fire");
    const auto stock=w.building(clay).output;
    w.tick();check(w.building_on_fire(clay) && w.fire_remaining(clay)==600 &&
        w.building(clay).fire_risk==0 && w.building(clay).operating_enabled,"threshold fire mismatch");
    check(w.workforce_required()==4 && w.active_workforce_required()==0 &&
        w.workforce_supply()==w.total_population(),"burning workforce demand mismatch");
    const auto before=w.snapshot();
    const auto rejected=w.execute(sim::demolish_building(clay));
    check(!rejected.accepted && rejected.reason=="Cannot demolish: building is on fire." &&
        w.snapshot()==before,"burning demolition changed state or wrong reason");
    advance(w,2599);check(w.building_on_fire(clay) && w.fire_remaining(clay)==1 &&
        w.building(clay).output==stock,"fire lost stock or expired early");
    w.tick();check(!w.building_on_fire(clay) && w.building(clay).fire_risk==1,"expiry risk clock");
    check(w.active_workforce_required()==4,"workforce did not return after expiry");
    invariants(w);
    auto paused=fresh();const auto post=put(paused,sim::CommandType::PlaceServicePost,{1,2});
    operation(paused,post,false);advance(paused,2000);
    check(paused.building_on_fire(post) && !paused.building(post).operating_enabled,"paused risk stopped");
    advance(paused,2601);
    check(!paused.building(post).operating_enabled && paused.demolition_status(post).allowed,
        "fire changed player pause or demolition safety");
    check(paused.execute(sim::demolish_building(post)).accepted,"expired building demolition failed");
    std::cout<<"natural fire: risk tick 20, ignition 2000, natural expiry 2600\n";
}

void patrol_and_protection() {
    auto w=fresh();
    const auto house=put(w,sim::CommandType::PlaceHousehold,{0,2});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{8,5});
    priority(w,watch,sim::WorkforcePriority::High);road(w,0,8);
    const auto inspector=owner_courier(w,watch);
    const auto refresh=w.route_refresh_count();
    w.tick();check(w.courier(inspector).phase==sim::CourierPhase::ToWarehouse &&
        !w.building_fire_protected(house),"protection granted on dispatch");
    operation(w,watch,false); // The already active visit must still arrive.
    until(w,[&]{return w.building_fire_protected(house);});
    const auto arrival=w.ticks();
    check(w.fire_protection_remaining(house)==2400 && w.building(house).fire_risk==0,
        "protection TTL mismatch");
    auto refreshed=sim::World::restore(w.snapshot(),mask());
    operation(refreshed,watch,true);
    until(refreshed,[&]{return refreshed.building(house).fire_protection_until_tick>arrival+2400;});
    check(refreshed.building(house).fire_protection_until_tick==refreshed.ticks()+2400,
        "protection stacked instead of refreshed");
    advance(w,arrival+2399);check(w.building(house).fire_risk==0,"protected risk grew");
    w.tick();check(!w.building_fire_protected(house),"protection did not expire exactly");
    const auto next=((w.ticks()/20)+1)*20;
    advance(w,next);check(w.building(house).fire_risk>=1,"expired protection stopped risk");
    check(w.route_refresh_count()==refresh,"fire update refreshed topology");
    check(w.building(watch).fire_risk==0 && w.building(watch).fire_until_tick==0,
        "FireWatch became vulnerable");
    std::cout<<"one-target protection arrival "<<arrival<<", expiry "<<arrival+2400<<"\n";
}

void tick_order_and_staffing() {
    auto w=fresh();
    const auto house=put(w,sim::CommandType::PlaceHousehold,{2,2});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{0,5});road(w,0,2);
    advance(w,19);
    auto s=w.snapshot();s.buildings.front().fire_risk=99;
    w=sim::World::restore(s,mask());w.tick();
    check(w.building_fire_protected(house) && !w.building_on_fire(house) &&
        w.building(house).fire_risk==0 && w.fire_protection_remaining(house)==2400,
        "arrival did not protect before the risk phase at tick 20");
    check(w.courier(owner_courier(w,watch)).phase==sim::CourierPhase::Returning,
        "tick-order fixture did not physically arrive");

    w=fresh();const auto clay=put(w,sim::CommandType::PlaceClaySource,{0,2});
    const auto farm=put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceHousehold,{6,2});advance(w,19);
    s=w.snapshot();s.buildings.front().fire_risk=99;
    for (auto& b:s.buildings) if (b.id!=clay) {
        b.fire_risk=0;b.fire_protection_until_tick=s.ticks+2400;
    }
    w=sim::World::restore(s,mask());const auto progress=w.building(clay).progress;
    w.tick();check(w.building_on_fire(clay) && w.building(clay).progress==progress+1 &&
        w.building(farm).progress==0,"new fire changed tick-start production allocation");
    w.tick();check(w.building(farm).progress==1 && !w.building_staffed(clay) &&
        w.workforce_used()==4 && w.workforce_required()==8 && w.active_workforce_required()==4,
        "released workforce did not apply on the following tick");
    advance(w,620);check(!w.building_on_fire(clay),"staffing incident failed to expire");
    const auto farm_progress=w.building(farm).progress;
    w.tick();check(w.building(clay).progress==progress+2 && w.building(farm).progress==farm_progress,
        "normal stable-ID allocation did not return next tick");
}

void burning_priority_and_road_break() {
    auto w=fresh();
    const auto a=put(w,sim::CommandType::PlaceServicePost,{2,5});
    const auto b=put(w,sim::CommandType::PlaceServicePost,{6,5});
    put(w,sim::CommandType::PlaceHousehold,{9,2});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{0,5});
    operation(w,a,false);operation(w,b,false);operation(w,watch,false);road(w,0,10);
    advance(w,2000);check(w.building_on_fire(a) && w.building_on_fire(b),"natural district fire absent");
    operation(w,watch,true);priority(w,watch,sim::WorkforcePriority::High);
    const auto inspector=owner_courier(w,watch);
    w.tick();check(w.courier(inspector).target==a && w.building_on_fire(a),"burning cyclic priority failed");
    // The future road is not either endpoint of the inspector's begun edge.
    check(w.execute({sim::CommandType::RemoveRoad,{2,4}}).accepted,"future road removal failed");
    until(w,[&]{return w.courier(inspector).route_pending;},30);
    check(w.building_on_fire(a) && w.fire_protection_remaining(a)==0,"waiting extinguished target");
    until(w,[&]{const auto& c=w.courier(inspector);return c.edge_progress==0 &&
        c.route_checked_revision==w.road_revision();},30);
    const auto attempts=w.courier(inspector).reroute_attempts;
    advance(w,w.ticks()+25);
    check(w.courier(inspector).reroute_attempts==attempts,"failed route retried every tick");
    road(w,2,2);
    until(w,[&]{return !w.building_on_fire(a);},100);
    const auto extinguished=w.ticks();
    check(w.building_fire_protected(a),"arrival did not extinguish and protect");
    until(w,[&]{return w.courier(inspector).phase==sim::CourierPhase::IdleAtWorkshop;},100);
    check(w.courier_dispatch_status(inspector).selected_target==b,"next burning target not preferred");
    until(w,[&]{return w.building_fire_protected(b);},100);
    operation(w,watch,false);
    until(w,[&]{return w.courier(inspector).phase==sim::CourierPhase::IdleAtWorkshop;},100);
    check(w.demolition_status(a).allowed && w.execute(sim::demolish_building(a)).accepted,
        "extinguished target cannot be demolished after active visit finishes");
    invariants(w);
    std::cout<<"road-break fire: ignition 2000, extinguish "<<extinguished<<"\n";
}

void shortage_and_fairness() {
    auto w=fresh();
    const auto clay=put(w,sim::CommandType::PlaceClaySource,{0,2});
    const auto farm=put(w,sim::CommandType::PlaceFarm,{4,5});
    const auto post=put(w,sim::CommandType::PlaceServicePost,{5,5});
    put(w,sim::CommandType::PlaceHousehold,{6,2});
    const auto watch=put(w,sim::CommandType::PlaceFireWatch,{8,5});road(w,0,8);
    const auto inspector=owner_courier(w,watch);
    check(!w.building_staffed(watch),"shortage Watch unexpectedly staffed");
    advance(w,40);check(w.courier(inspector).phase==sim::CourierPhase::IdleAtWorkshop &&
        w.building(clay).fire_risk==2,"shortage did not stop patrol only");
    priority(w,watch,sim::WorkforcePriority::High);w.tick();
    check(w.building_staffed(watch) && w.courier(inspector).phase==sim::CourierPhase::ToWarehouse,
        "High priority did not restore patrol");
    operation(w,clay,false);operation(w,farm,false);operation(w,post,false);
    std::set<sim::BuildingId> visited;
    for (int tick=0;tick<500;++tick) {
        w.tick();const auto& c=w.courier(inspector);
        if (c.phase==sim::CourierPhase::Returning) visited.insert(c.target);
    }
    check(visited.size()==4,"normal cyclic patrol starved targets");
    const auto watch2=put(w,sim::CommandType::PlaceFireWatch,{10,5});road(w,9,10);
    priority(w,watch2,sim::WorkforcePriority::High);
    // A fresh second House supplies enough workforce for both independent Watches.
    put(w,sim::CommandType::PlaceHousehold,{12,2});road(w,11,12);
    std::set<sim::BuildingId> visited2;
    for (int tick=0;tick<800;++tick) {
        w.tick();const auto& c=w.courier(owner_courier(w,watch2));
        if (c.phase==sim::CourierPhase::Returning) visited2.insert(c.target);
    }
    check(visited2.size()==5 && w.fire_state_valid(),"second Watch cyclic fairness");
    check(!w.validate({sim::CommandType::PlaceFireWatch,{20,5}}).accepted,"third Watch permitted");
}

void paid_starter_and_district() {
    auto w=starter();
    check(w.ticks()==0 && w.treasury()==20 && w.construction_spent_total()==1280 &&
        w.total_population()==24 && w.workforce_required()==24 && w.workforce_used()==24 &&
        w.protected_buildings()==0 && w.taxes_collected_total()==0,"paid tick-0 starter mismatch");
    const auto watch=kind(w,sim::Object::FireWatch);
    const auto inspector=owner_courier(w,watch);
    std::uint64_t first_protection=0,first_tax=0;
    for (int i=0;i<4000;++i) {
        w.tick();invariants(w);
        if (!first_protection && w.protected_buildings()) first_protection=w.ticks();
        if (!first_tax && w.taxes_collected_total()) first_tax=w.ticks();
        check(w.burning_buildings()==0,"connected starter burned");
    }
    check(first_protection>0 && first_protection<2000 && first_tax>0 &&
        w.protected_buildings()==10 && w.courier(inspector).last_dispatched_target,
        "starter patrol or real tax absent");
    const auto distant=put(w,sim::CommandType::PlaceServicePost,{35,5});
    operation(w,distant,false);road(w,35,40);
    advance(w,6000);
    check(w.building_on_fire(distant) && !w.building_fire_protected(distant),
        "disconnected district got protection from first Watch");
    const auto second=put(w,sim::CommandType::PlaceFireWatch,{40,5});
    priority(w,second,sim::WorkforcePriority::High);
    until(w,[&]{return w.building_fire_protected(distant);},100);
    check(!w.building_on_fire(distant),"second district Watch did not extinguish");
    std::cout<<"paid starter: first patrol 1, protection "<<first_protection<<", tax "<<first_tax
        <<"; disconnected ignition 6000, second-Watch extinguish "<<w.ticks()<<"\n";
}

void isolated_operations() {
    auto baseline=starter(false);
    until(baseline,[&]{
        const auto& pot=baseline.building(kind(baseline,sim::Object::Pottery));
        return pot.active_recipe_clay==2 && pot.progress>0;
    });
    const auto pot=kind(baseline,sim::Object::Pottery);
    auto w=incident(baseline,pot);const auto recipe=w.building(pot);
    const auto demand=w.workforce_required();
    check(w.active_workforce_required()==demand-6,"burning Pottery did not free workers");
    advance(w,w.ticks()+599);
    check(w.building(pot).progress==recipe.progress && w.building(pot).active_recipe_clay==2 &&
        w.building(pot).recipes_completed==recipe.recipes_completed,"recipe progressed or Clay lost in fire");
    w.tick();check(w.building(pot).progress==recipe.progress,"production resumed inside expiry tick");
    w.tick();check(w.building(pot).progress==recipe.progress+1,"recipe did not resume next tick");
    for (const auto object:{sim::Object::ClaySource,sim::Object::Farm}) {
        auto t=incident(baseline,kind(baseline,object));const auto id=kind(t,object);
        const auto before=t.building(id);advance(t,t.ticks()+600);
        check(t.building(id).progress==before.progress && t.building(id).output<=before.output &&
            t.building(id).clay_extracted==before.clay_extracted &&
            t.building(id).food_produced==before.food_produced,"producer progressed during fire");
        t.tick();check(t.building(id).progress==before.progress+1,"producer did not resume");
    }
    // Let the valid supply chain accumulate genuine stock, coverage and taxes.
    advance(baseline,1600);
    const auto market=kind(baseline,sim::Object::Market);
    w=incident(baseline,market);const auto stock=w.building(market);
    bool inbound=false;
    for (int i=0;i<100;++i) {
        const auto before=w.building(market);w.tick();invariants(w);
        const auto after=w.building(market);
        check(after.pottery_stock>=before.pottery_stock && after.food_stock>=before.food_stock,
            "burning Market dispatched new cargo");
        inbound=inbound || after.pottery_stock>before.pottery_stock || after.food_stock>before.food_stock;
    }
    check(inbound && w.building(market).pottery_stock>=stock.pottery_stock &&
        w.building(market).food_stock>=stock.food_stock,"burning Market rejected inbound/lost goods");
    advance(w,2201);bool outbound=false;
    for (const auto& c:w.couriers()) if (c.owner==market)
        outbound=outbound || c.phase==sim::CourierPhase::ToWarehouse;
    check(outbound,"Market did not resume output");
    const auto service=kind(baseline,sim::Object::ServicePost);
    w=incident(baseline,service);
    for (int i=0;i<600;++i) {
        const auto c=w.courier(owner_courier(w,service));w.tick();
        if (c.phase==sim::CourierPhase::IdleAtWorkshop)
            check(w.courier(c.id).phase==sim::CourierPhase::IdleAtWorkshop,"burning Service dispatched");
    }
    w.tick();check(w.courier_dispatch_status(owner_courier(w,service)).status!=sim::CourierDispatchStatus::OnFire,
        "Service remained suspended");
    sim::BuildingId house{};
    for (const auto& b:baseline.buildings()) if (b.kind==sim::Object::Household && b.pottery_stock>0 &&
        b.food_stock>0 && baseline.household_service_active(b.id)) {house=b.id;break;}
    check(static_cast<unsigned>(house)>0,"no genuinely supplied House fixture");
    w=incident(baseline,house);
    // Stop future deliveries, but allow every active trip to finish before comparing stock.
    operation(w,market,false);operation(w,service,false);
    until(w,[&]{return std::ranges::all_of(w.couriers(),[&](const auto& c){
        return (c.owner!=market && c.owner!=service) || c.phase==sim::CourierPhase::IdleAtWorkshop;
    });},200);
    const auto b=w.building(house);const auto tax=w.household_tax_contributed(house);
    check(b.pottery_stock>0 && b.food_stock>0 && w.household_service_active(house),"supply lost before fire demand");
    const auto deadline=w.ticks()+static_cast<unsigned>(400-b.demand_progress);
    advance(w,deadline);
    check(w.building(house).missed_demand==b.missed_demand+1 &&
        w.building(house).pottery_stock==b.pottery_stock && w.building(house).food_stock==b.food_stock &&
        w.building(house).population==b.population-1 && w.household_tax_contributed(house)==tax,
        "burning supplied House consumed goods/paid tax/failed ordinary decline");
    check(w.workforce_supply()==w.total_population(),"burning residents withdrawn from workforce");
    std::cout<<"isolated incidents: recipe/producer freeze, Market inbound, Service pause, House miss passed\n";
}

struct Temp {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-city12-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { fs::create_directories(root/"data/Cities");
        std::ofstream(root/"data/Cities/Synthetic.map")<<"OpenEmperor synthetic map identity"; }
    ~Temp() { std::error_code ec;fs::remove_all(root,ec); }
};
void roundtrip(const Temp& temp,const sim::World& w) {
    const auto before=w.snapshot();
    save::write_save(temp.root/"save.json",save::make_document(temp.root/"data",
        "Cities/Synthetic.map",mask(),w),temp.root/"data",mask());
    const auto doc=save::read_save(temp.root/"save.json");
    check(doc.source_schema_version==14 && w.snapshot()==before,"save schema/state changed");
    auto restored=save::restore_save(doc,temp.root/"data",mask());
    auto control=sim::World::restore(before,mask());
    check(restored.snapshot()==before,"fire restore differs immediately");
    for (int i=0;i<1000;++i) {
        restored.tick();control.tick();
        check(restored.snapshot()==control.snapshot(),"continued fire restore diverged");
    }
}
void persistence() {
    Temp temp;auto w=fresh();const auto post=put(w,sim::CommandType::PlaceServicePost,{0,5});
    operation(w,post,false);
    for (const auto tick:{19U,20U,1999U,2000U,2599U,2600U}) { advance(w,tick);roundtrip(temp,w); }
    auto patrol=fresh();const auto house=put(patrol,sim::CommandType::PlaceHousehold,{0,2});
    const auto watch=put(patrol,sim::CommandType::PlaceFireWatch,{8,5});
    operation(patrol,watch,false);road(patrol,0,8);advance(patrol,2000);
    operation(patrol,watch,true);
    const auto inspector=owner_courier(patrol,watch);
    until(patrol,[&]{const auto& c=patrol.courier(inspector);return c.phase==sim::CourierPhase::ToWarehouse &&
        c.path_vertex+2==c.path.size() && c.edge_progress==4;},100);
    check(patrol.building_on_fire(house),"pre-arrival no fire");roundtrip(temp,patrol);
    patrol.tick();check(!patrol.building_on_fire(house),"arrival did not extinguish");roundtrip(temp,patrol);
    const auto arrival=patrol.ticks();operation(patrol,watch,false);
    advance(patrol,arrival+2399);roundtrip(temp,patrol);patrol.tick();roundtrip(temp,patrol);
    auto malformed=patrol.snapshot();malformed.buildings[0].fire_risk=100;
    rejects([&]{sim::World::restore(malformed,mask());},"risk100 accepted");
    malformed=patrol.snapshot();malformed.buildings.back().fire_risk=1;
    rejects([&]{sim::World::restore(malformed,mask());},"Watch self-risk accepted");
    malformed=patrol.snapshot();malformed.buildings[0].fire_protection_until_tick=UINT64_MAX;
    rejects([&]{sim::World::restore(malformed,mask());},"overflow deadline accepted");
    malformed=patrol.snapshot();malformed.couriers[0].cargo=1;
    rejects([&]{sim::World::restore(malformed,mask());},"Inspector cargo accepted");
    malformed=patrol.snapshot();malformed.couriers[0].target=watch;
    malformed.couriers[0].last_dispatched_target=watch;
    rejects([&]{sim::World::restore(malformed,mask());},"Inspector targeted immune Watch");
    auto empty=fresh().snapshot();empty.ticks=UINT64_MAX-2400;
    auto exhausted=sim::World::restore(empty,mask());const auto exhausted_before=exhausted.snapshot();
    rejects([&]{exhausted.tick();},"fire deadline arithmetic overflowed");
    check(exhausted.snapshot()==exhausted_before,"overflow rejection changed World");
    nlohmann::json valid;{std::ifstream in(temp.root/"save.json");in>>valid;}
    const auto bad_json=[&](auto change) {
        auto bad=valid;change(bad);{std::ofstream out(temp.root/"bad.json");out<<bad;}
        rejects([&]{save::restore_save(save::read_save(temp.root/"bad.json"),temp.root/"data",mask());},
            "malformed schema14 JSON accepted");
    };
    bad_json([](auto& j){j["world"]["buildings"][0]["fire_risk"]=-1;});
    bad_json([](auto& j){j["world"]["buildings"][0]["fire_risk"]=100;});
    bad_json([](auto& j){j["world"]["buildings"][1]["fire_until_tick"]=1;});
    bad_json([](auto& j){j["world"]["buildings"][0].erase("fire_until_tick");});
    bad_json([](auto& j){j["schema_version"]=13;});
    for (unsigned version=1;version<=4;++version) {
        sim::World old(width,height,mask(),sim::RulesProfile::CityV11,version);
        put(old,sim::CommandType::PlaceServicePost,{0,5});advance(old,2600);
        check(old.fire_state_valid() && old.fire_eligible_buildings()==0 && old.burning_buildings()==0,
            "old profile gained fire");
        auto bad=old.snapshot();bad.buildings[0].fire_risk=1;
        rejects([&]{sim::World::restore(bad,mask());},"old profile nonzero fire accepted");
        save::write_save(temp.root/"old.json",save::make_document(temp.root/"data",
            "Cities/Synthetic.map",mask(),old),temp.root/"data",mask());
        nlohmann::json json;{std::ifstream in(temp.root/"old.json");in>>json;}
        check(!json["world"]["buildings"][0].contains("fire_risk"),"old schema gained fire fields");
        json["world"]["buildings"][0]["fire_risk"]=1;
        {std::ofstream out(temp.root/"old.json");out<<json;}
        rejects([&]{save::read_save(temp.root/"old.json");},"old JSON silently ignored fire state");
    }
    // Checkpoint a live incident through the ordinary scheduler and RecoveryStore.
    auto recovery=fresh();const auto target=put(recovery,sim::CommandType::PlaceServicePost,{0,5});
    operation(recovery,target,false);
    openemperor::AutosaveController autosave(temp.root/"prefs",temp.root/"data",true);
    const auto document=[&]{return save::make_document(temp.root/"data","Cities/Synthetic.map",mask(),recovery);};
    check(autosave.begin(document(),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"recovery start failed");
    advance(recovery,2400);check(recovery.building_on_fire(target),"recovery fixture not burning");
    const auto before=recovery.snapshot();
    check(autosave.poll(document(),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved &&
        recovery.snapshot()==before,"autosave mutated fire state");
    const auto catalog=autosave.store().catalog();
    check(catalog.histories.size()==1 && catalog.histories[0].entries.size()==2,"unexpected fire recovery slots");
    const auto& entry=catalog.histories[0].entries.front();
    check(entry.schema==14 && entry.profile==sim::city_v12_profile_name && entry.tick==2400,
        "recovery list fire identity mismatch");
    auto restored=save::restore_save(save::read_save(entry.path),temp.root/"data",mask());
    check(restored.snapshot()==before && restored.building_on_fire(target),"recovery fire state changed");
    std::cout<<"schema14: risk/ignition/expiry/arrival/protection boundaries and burning autosave passed\n";
}

void determinism() {
    auto a=starter(),b=starter();
    const auto same=[&]{check(a.snapshot()==b.snapshot(),"20k deterministic states diverged");};
    const auto watch=kind(a,sim::Object::FireWatch);
    sim::BuildingId distant{};
    bool burned=false,extinguished=false,removed=false,road_broken=false;
    for (int i=0;i<20000;++i) {
        if (i==3000) {
            distant=put(a,sim::CommandType::PlaceServicePost,{35,5});
            check(put(b,sim::CommandType::PlaceServicePost,{35,5})==distant,"ID diverged");
            operation(a,distant,false);operation(b,distant,false);
            priority(a,watch,sim::WorkforcePriority::High);priority(b,watch,sim::WorkforcePriority::High);
        }
        if (i==5000) { road(a,15,35);road(b,15,35); }
        if (i>5000 && !road_broken) {
            const sim::Command cmd{sim::CommandType::RemoveRoad,{30,4}};
            if (a.validate(cmd).accepted) {
                check(a.execute(cmd).accepted && b.execute(cmd).accepted,"deterministic road break failed");
                road_broken=true;
            }
        }
        if (i==5100) {road(a,30,30);road(b,30,30);}
        a.tick();b.tick();invariants(a);same();
        if (distant!=sim::BuildingId{} && !removed) {
            burned=burned || a.building_on_fire(distant);
            extinguished=extinguished || (burned && a.building_fire_protected(distant));
            if (extinguished && a.demolition_status(distant).allowed) {
                check(a.execute(sim::demolish_building(distant)).accepted &&
                    b.execute(sim::demolish_building(distant)).accepted,"post-fire demolition failed");
                removed=true;same();
            }
        }
    }
    check(burned && extinguished && removed && road_broken,"20k scenario did not exercise required events");
    std::cout<<"20,000 ticks: deterministic patrol, fire, road break, extinguish, priority, demolition\n";
}

void endurance() {
    auto w=starter();advance(w,20000);
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
        const auto count=static_cast<unsigned>(std::ranges::count_if(w.buildings(),
            [&](const auto& b){return b.kind==object;}));
        for (unsigned i=count;i<limit;++i) {
            const auto id=put(w,type,{35+3*static_cast<int>(i),row});
            if (object==sim::Object::ServicePost) {remote=id;operation(w,id,false);}
        }
        row+=2;
    }
    check(w.buildings().size()==40 && w.couriers().size()==24,"full City-v12 entity limits mismatch");
    std::uint64_t natural_cycles=0;bool was_burning=false,reconnected=false,replaced=false;
    const auto nextid=w.next_building_id();
    for (int i=0;i<100000;++i) {
        w.tick(); // World checks every invariant after each tick itself.
        if (i%100==0) invariants(w);
        if (!replaced) {
            const bool burning=w.building_on_fire(remote);
            if (burning && !was_burning) ++natural_cycles;
            was_burning=burning;
        }
        if (i==10000) {
            // A road connects the remote Service Post to the original Watch.
            road(w,15,30);
            for (int y=5;y<=9;++y)
                check(w.execute({sim::CommandType::PlaceRoad,{30,y}}).accepted,"district road failed");
            road(w,31,38,9);reconnected=true;
        }
        if (reconnected && !replaced && w.building_fire_protected(remote) &&
            w.demolition_status(remote).allowed) {
            const auto cell=w.building(remote).cell;
            check(w.execute(sim::demolish_building(remote)).accepted,"endurance demolition failed");
            const auto replacement=put(w,sim::CommandType::PlaceServicePost,cell);
            check(static_cast<std::uint32_t>(replacement)>=nextid,"endurance recycled ID");
            operation(w,replacement,false);replaced=true;
        }
        if (i%5000==0) {
            auto restored=sim::World::restore(w.snapshot(),mask());
            check(restored.snapshot()==w.snapshot(),"endurance snapshot failed validation");
        }
    }
    check(natural_cycles>=3 && reconnected && replaced && w.protected_buildings()>0 &&
        w.taxes_collected_total()>0,"100k scenario missing protected/disconnected cycles/rebuild");
    std::cout<<"100,000-tick full 40-building/24-courier city: "<<natural_cycles
        <<" remote natural incidents, reconnection and demolition/rebuild passed\n";
}
}

int main(int argc,char* argv[]) {
    try {
        const std::string mode=argc>1 ? argv[1]:"base";
        if (mode=="base") {
            natural_boundaries();patrol_and_protection();tick_order_and_staffing();burning_priority_and_road_break();
            shortage_and_fairness();paid_starter_and_district();isolated_operations();
        } else if (mode=="persistence") persistence();
        else if (mode=="determinism") determinism();
        else if (mode=="endurance") endurance();
        else throw std::invalid_argument("unknown test mode");
        return 0;
    } catch (const std::exception& e) { std::cerr<<"City-v12: "<<e.what()<<'\n';return 1; }
}
