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
sim::World fresh(sim::RulesProfile profile=sim::RulesProfile::CityV15) {
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
        w.fire_state_valid() && w.health_state_valid(),"City-v15 invariant at tick "+std::to_string(w.ticks()));
    std::uint64_t taxes=w.snapshot().demolition_history.taxes;
    for (const auto& b:w.buildings()) {
        if (b.kind==sim::Object::Household) taxes+=b.taxes_paid_total;
        else check(b.taxes_paid_total==0,"non-House tax authority");
    }
    if (sim::desirability_profile(w.profile()))
        check(taxes==w.taxes_collected_total(),"tax history conservation");
    check(w.buildings().size()<=46 && w.couriers().size()<=26 &&
        w.route_cache_entries()<=26*40,"collection/cache budget");
}
void advance(sim::World& w,std::uint64_t tick) { while (w.ticks()<tick) w.tick();invariants(w); }
template<class P> void until(sim::World& w,P done,int budget=3000) {
    while (budget-- && !done()) w.tick();
    check(done(),"state wait timed out at "+std::to_string(w.ticks()));invariants(w);
}
sim::World starter(sim::RulesProfile profile=sim::RulesProfile::CityV15) {
    auto w=fresh(profile);
    put(w,sim::CommandType::PlaceClaySource,{0,2});
    put(w,sim::CommandType::PlacePottery,{0,5});
    put(w,sim::CommandType::PlaceWarehouse,{3,2});
    put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceMarket,{3,5});
    put(w,sim::CommandType::PlaceServicePost,{5,5});road(w,0,14);
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(w,sim::CommandType::PlaceHousehold,cell);
    if(sim::fire_profile(profile)) put(w,sim::CommandType::PlaceFireWatch,{14,5});
    return w;
}
struct Temp {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-city15-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() {fs::create_directories(root/"data/Cities");
        std::ofstream(root/"data/Cities/Synthetic.map")<<"independently authored synthetic identity";}
    ~Temp() {std::error_code ec;fs::remove_all(root,ec);}
};
auto document(const Temp& t,const sim::World& w) {
    return save::make_document(t.root/"data","Cities/Synthetic.map",mask(),w);
}
void roundtrip(const Temp& t,const sim::World& w) {
    const auto before=w.snapshot();
    save::write_save(t.root/"save.json",document(t,w),t.root/"data",mask());
    const auto doc=save::read_save(t.root/"save.json");
    check(doc.source_schema_version==17,"schema17 identity");
    auto restored=save::restore_save(doc,t.root/"data",mask());
    check(restored.snapshot()==before && w.snapshot()==before,"save/load mutation");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(restored.household_has_water(b.id)==w.household_has_water(b.id) &&
            restored.nearest_water_source(b.id)==w.nearest_water_source(b.id) &&
            restored.household_level(b.id)==w.household_level(b.id),"derived water changed on load");
    auto control=sim::World::restore(before,mask());
    for (int i=0;i<500;++i) {restored.tick();control.tick();check(restored.snapshot()==control.snapshot(),"resume diverged");}
}
sim::CourierId worker(const sim::World& w,sim::BuildingId owner) {
    for (const auto& c:w.couriers()) if (c.owner==owner && c.role==sim::CourierRole::HealthWorker) return c.id;
    throw std::runtime_error("missing HealthWorker");
}
template<class F> void seed(sim::World& w,F edit) {
    auto s=w.snapshot();edit(s);w=sim::World::restore(s,mask());invariants(w);
}
sim::BuildingSnapshot& house(sim::WorldSnapshot& s,sim::BuildingId id) {
    return *std::ranges::find_if(s.buildings,[&](const auto& b){return b.id==id;});
}
void risk_boundaries() {
    auto dry=fresh();auto h=put(dry,sim::CommandType::PlaceHousehold,{10,10});
    advance(dry,99);check(dry.household_health_risk(h)==0,"early dry risk");
    dry.tick();check(dry.household_health_risk(h)==3,"dry tick100 risk");
    advance(dry,3299);check(dry.household_health_risk(h)==96,"dry tick3299 risk");
    dry.tick();check(dry.household_health_risk(h)==99 && !dry.household_sick(h),"dry tick3300 risk");
    advance(dry,3399);dry.tick();check(dry.household_sick(h) && dry.household_health_risk(h)==0 &&
        dry.household_sickness_remaining(h)==1200,"illness threshold tick3400");
    advance(dry,4599);check(dry.household_sickness_remaining(h)==1,"natural recovery early");
    dry.tick();check(!dry.household_sick(h) && dry.household_health_risk(h)==3,"exact natural recovery risk step");
    auto wet=fresh();auto wh=put(wet,sim::CommandType::PlaceHousehold,{10,10});
    put(wet,sim::CommandType::PlaceWell,{16,10});
    advance(wet,3300);check(wet.household_health_risk(wh)==33,"water risk not 3x slower");
    advance(wet,9900);check(wet.household_health_risk(wh)==99,"water risk99");
    advance(wet,9999);wet.tick();check(wet.household_sick(wh),"wet illness tick10000");
    auto protected_world=fresh();h=put(protected_world,sim::CommandType::PlaceHousehold,{10,10});
    advance(protected_world,99);
    seed(protected_world,[&](auto& s){house(s,h).health_protection_until_tick=200;});
    protected_world.tick();check(protected_world.household_health_risk(h)==0,"protected risk grew");
    advance(protected_world,199);protected_world.tick();check(protected_world.household_health_risk(h)==3 &&
        !protected_world.household_health_protected(h),"exact protection expiry");
    // A late placement has its own clock, even across save/restore.
    auto late=fresh();advance(late,37);h=put(late,sim::CommandType::PlaceHousehold,{10,10});
    advance(late,136);check(late.household_health_risk(h)==0,"global health clock");
    late.tick();check(late.household_health_risk(h)==3,"placement-relative boundary");
    auto s=late.snapshot();s.ticks=UINT64_MAX-2399;
    rejects([&]{sim::World::restore(s,mask());},"health headroom overflow accepted");
    std::cout<<"risk: dry+3/wet+1 each100, illness3400/10000, natural recovery4600, protection exact expiry\n";
}
void progression() {
    auto w=starter();check(w.treasury()==20 && w.construction_spent_total()==1280 && w.ticks()==0 &&
        w.workforce_supply()==24 && w.workforce_used()==24,"starter changed");
    for (const auto& b:w.buildings()) check(b.kind!=sim::Object::Well && b.kind!=sim::Object::HealthPost &&
        !b.health_risk && !b.sick_until_tick && !b.health_protection_until_tick,"free starter health/water");
    until(w,[&]{return w.taxes_collected_total()>0;});const auto tax=w.ticks();
    until(w,[&]{return w.treasury()>=60;});const auto well_tick=w.ticks();
    put(w,sim::CommandType::PlaceWell,{8,1});
    until(w,[&]{return w.treasury()>=120 && w.workforce_supply()>=26;});
    const auto purchase=w.ticks();auto p=put(w,sim::CommandType::PlaceHealthPost,{12,5});auto c=worker(w,p);
    const auto before=w.ticks();check(!w.household_health_protected(kind(w,sim::Object::Household)),"dispatch protected");
    until(w,[&]{return std::ranges::any_of(w.buildings(),[&](const auto& b){return w.household_health_protected(b.id);});});
    check(w.ticks()>before && !w.household_sick(kind(w,sim::Object::Household)),"first visit/early illness");
    check(w.courier(c).cargo==0 && w.courier(c).reserved==0 && w.courier(c).good==sim::Good::Goods &&
        sim::fire_eligible(sim::Object::HealthPost) && sim::desirability_impact(sim::Object::HealthPost)==0 &&
        w.workforce_required(p)==2 && sim::building_footprint(w.profile(),sim::Object::HealthPost)==sim::BuildingFootprint{1,1},"Health Post authority");
    std::cout<<"paid progression: first_tax="<<tax<<" well="<<well_tick<<" health_post="<<purchase<<" first_visit="<<w.ticks()<<'\n';
}
void visits_targets_and_roads() {
    auto w=fresh();road(w,0,18);
    std::vector<sim::BuildingId> homes;
    for (int x:{2,6,10,14}) homes.push_back(put(w,sim::CommandType::PlaceHousehold,{x,2}));
    auto p=put(w,sim::CommandType::PlaceHealthPost,{0,5});auto c=worker(w,p);
    seed(w,[&](auto& s){house(s,homes[0]).health_protection_until_tick=2400;
        house(s,homes[1]).health_risk=70;house(s,homes[2]).health_risk=40;
        house(s,homes[3]).sick_until_tick=1200;});
    check(w.courier_dispatch_status(c).selected_target==homes[3],"sick priority");
    w.tick();check(w.household_sick(homes[3]) && !w.household_health_protected(homes[3]),"cure at dispatch");
    operation(w,p,false);
    check(!w.demolition_status(p).allowed,"active worker demolition");
    until(w,[&]{return !w.household_sick(homes[3]);});const auto cure=w.ticks();
    check(w.household_health_protection_remaining(homes[3])==2400,"arrival protection duration");
    until(w,[&]{return w.courier(c).phase==sim::CourierPhase::IdleAtWorkshop;});
    operation(w,p,true);check(w.courier_dispatch_status(c).selected_target==homes[1],"risk70 priority");
    seed(w,[&](auto& s){for (auto id:homes){auto& b=house(s,id);b.health_risk=0;b.health_protection_until_tick=s.ticks+2400;}
        house(s,p).operating_enabled=false;});
    operation(w,p,true);check(w.courier_dispatch_status(c).selected_target==homes[0],"normal cyclic patrol");
    // Disconnect the highest-priority sick target; lower reachable targets still receive visits.
    seed(w,[&](auto& s){house(s,homes[3]).health_protection_until_tick=0;house(s,homes[3]).sick_until_tick=s.ticks+1200;});
    check(w.execute({sim::CommandType::RemoveRoad,{12,4}}).accepted,"disconnect road");
    check(w.courier_dispatch_status(c).selected_target!=homes[3],"unreachable sick target blocked visits");
    check(w.execute({sim::CommandType::PlaceRoad,{12,4}}).accepted,"repair road");
    check(w.courier_dispatch_status(c).selected_target==homes[3],"repaired sick target priority");
    w.tick();operation(w,p,false);
    check(w.execute({sim::CommandType::RemoveRoad,{12,4}}).accepted,"remove future worker road");
    until(w,[&]{return w.courier(c).route_pending && w.courier(c).edge_progress==0;});
    const auto protected_before=w.building(homes[3]).health_protection_until_tick;
    advance(w,w.ticks()+100);check(w.household_sick(homes[3]) &&
        w.building(homes[3]).health_protection_until_tick==protected_before,"waiting worker treated remotely");
    check(w.execute({sim::CommandType::PlaceRoad,{12,4}}).accepted,"worker repair");
    until(w,[&]{return !w.household_sick(homes[3]);});
    until(w,[&]{return w.courier(c).phase==sim::CourierPhase::IdleAtWorkshop;});
    const auto expires=w.building(homes[3]).health_protection_until_tick;const auto funds=w.treasury();
    check(w.execute(sim::demolish_building(p)).accepted && w.treasury()==funds &&
        w.building(homes[3]).health_protection_until_tick==expires,"demolition removed protection/refunded");
    check(w.couriers().empty(),"demolition retained idle worker");
    std::cout<<"cure_tick="<<cure<<" sick/risk/cyclic priority, pause-in-flight, road break/wait/repair and safe demolition\n";
}
void demand_fire_and_staffing() {
    auto w=starter();advance(w,1199);
    auto h=kind(w,sim::Object::Household);
    check(w.building(h).pottery_stock>0 && w.building(h).food_stock>0 && w.household_service_active(h),"demand fixture not supplied");
    seed(w,[&](auto& s){auto& b=house(s,h);b.sick_until_tick=s.ticks+1200;b.health_risk=0;
        b.fire_until_tick=s.ticks+600;b.fire_protection_until_tick=0;b.fire_risk=0;});
    const auto b=w.building(h);const auto taxes=w.building(h).taxes_paid_total;w.tick();
    check(w.building(h).missed_demand==b.missed_demand+1 && w.building(h).population==b.population-1 &&
        w.building(h).pottery_stock==b.pottery_stock && w.building(h).food_stock==b.food_stock &&
        w.building(h).taxes_paid_total==taxes && w.household_sick(h),"sick+fire demand doubled penalty/consumed/taxed");
    // A Health arrival cures sickness independently from fire.
    auto p=put(w,sim::CommandType::PlaceHealthPost,{12,5});auto c=worker(w,p);
    check(w.execute(sim::set_building_workforce_priority(p,sim::WorkforcePriority::High)).accepted,"High health priority");
    until(w,[&]{return !w.household_sick(h);});check(w.building_on_fire(h),"health cure extinguished fire");
    // Burning owner suppresses new dispatch while an active worker completes.
    seed(w,[&](auto& s){auto& post=house(s,p);post.fire_until_tick=s.ticks+600;
        post.fire_risk=0;post.fire_protection_until_tick=0;});
    check(w.workers_assigned(p)==0 && w.courier(c).phase!=sim::CourierPhase::IdleAtWorkshop,"burning staffing");
    until(w,[&]{return w.courier(c).phase==sim::CourierPhase::IdleAtWorkshop;});
    check(w.courier_dispatch_status(c).status==sim::CourierDispatchStatus::OnFire,"burning dispatch");
    until(w,[&]{return !w.building_on_fire(p);});w.tick();
    check(w.courier(c).phase!=sim::CourierPhase::IdleAtWorkshop,"post-fire dispatch did not resume");
    // Complete requirements, priority order and pause status; sick residents supply workforce.
    auto scarce=fresh();auto sh=put(scarce,sim::CommandType::PlaceHousehold,{10,10});
    put(scarce,sim::CommandType::PlaceClaySource,{0,0});put(scarce,sim::CommandType::PlaceWarehouse,{4,0});
    auto sp=put(scarce,sim::CommandType::PlaceHealthPost,{7,0});auto sc=worker(scarce,sp);
    check(scarce.courier_dispatch_status(sc).status==sim::CourierDispatchStatus::Unstaffed,"Health unstaffed diagnostic");
    scarce.execute(sim::set_building_workforce_priority(sp,sim::WorkforcePriority::High));
    check(scarce.workers_assigned(sp)==2 && scarce.courier_dispatch_status(sc).status==sim::CourierDispatchStatus::NoRoad,"High does not acquire complete workers");
    operation(scarce,sp,false);check(scarce.courier_dispatch_status(sc).status==sim::CourierDispatchStatus::OperationPaused,"Paused health diagnostic");
    seed(scarce,[&](auto& s){house(s,sh).sick_until_tick=1200;});
    check(scarce.workforce_supply()==6,"sick workforce reduced directly");
    auto empty=fresh();auto ep=put(empty,sim::CommandType::PlaceHealthPost,{0,0});
    auto eh=put(empty,sim::CommandType::PlaceHousehold,{10,10});
    check(empty.courier_dispatch_status(worker(empty,ep)).status==sim::CourierDispatchStatus::NoRoad,"No road health");
    empty.execute(sim::demolish_building(eh)); // No stock/payload is ever needed by a Health Worker.
    std::cout<<"sick/fire single demand miss, retained goods/tax, independent cure, burning/paused/unstaffed service\n";
}
void persistence() {
    Temp t;auto w=fresh();auto h=put(w,sim::CommandType::PlaceHousehold,{10,10});advance(w,99);
    for (int risk:{98,99}) {seed(w,[&](auto& s){house(s,h).health_risk=risk;});roundtrip(t,w);}
    w.tick();roundtrip(t,w);advance(w,700);roundtrip(t,w);advance(w,1299);roundtrip(t,w);
    w.tick();roundtrip(t,w);
    seed(w,[&](auto& s){auto& b=house(s,h);b.health_risk=0;b.health_protection_until_tick=s.ticks+1;});
    roundtrip(t,w);w.tick();roundtrip(t,w);
    auto v=starter();advance(v,1200);auto p=put(v,sim::CommandType::PlaceHealthPost,{12,5});
    v.execute(sim::set_building_workforce_priority(p,sim::WorkforcePriority::High));auto c=worker(v,p);
    seed(v,[&](auto& s){auto& b=house(s,kind(v,sim::Object::Household));b.health_risk=0;b.sick_until_tick=s.ticks+1200;});
    v.tick();roundtrip(t,v);until(v,[&]{return v.courier(c).phase==sim::CourierPhase::Returning;});roundtrip(t,v);
    const auto original=v.snapshot();
    const auto invalid=[&](auto edit) {auto bad=original;edit(bad);rejects([&]{sim::World::restore(bad,mask());},"invalid health authority accepted");};
    auto vh=kind(v,sim::Object::Household);
    invalid([&](auto& s){house(s,vh).health_risk=100;});
    invalid([&](auto& s){house(s,vh).health_risk=-1;});
    invalid([&](auto& s){house(s,p).health_risk=1;});
    invalid([&](auto& s){house(s,vh).sick_until_tick=s.ticks+1201;});
    invalid([&](auto& s){house(s,vh).health_protection_until_tick=s.ticks+2401;});
    invalid([&](auto& s){house(s,vh).sick_until_tick=s.ticks+1;house(s,vh).health_protection_until_tick=s.ticks+1;});
    invalid([](auto& s){s.profile=sim::RulesProfile::CityV14;});
    invalid([&](auto& s){for(auto& x:s.couriers)if(x.id==c)x.cargo=1;});
    invalid([&](auto& s){for(auto& x:s.couriers)if(x.id==c)x.owner=vh;});
    invalid([&](auto& s){for(auto& x:s.couriers)if(x.id==c)x.target=p;});
    save::write_save(t.root/"save.json",document(t,v),t.root/"data",mask());
    nlohmann::json json;{std::ifstream in(t.root/"save.json");in>>json;}
    const auto bad_json=[&](auto edit){auto x=json;edit(x);{std::ofstream out(t.root/"bad.json");out<<x;}
        rejects([&]{save::restore_save(save::read_save(t.root/"bad.json"),t.root/"data",mask());},"bad schema17 JSON accepted");};
    bad_json([](auto& x){x["schema_version"]=16;});bad_json([](auto& x){x["rules"]["id"]="sandbox-city-v14";});
    bad_json([](auto& x){x["world"]["couriers"][0]["role"]=12;});
    bad_json([](auto& x){x["world"]["buildings"][0]["kind"]=13;});
    seed(v,[&](auto& s){auto& b=house(s,vh);b.health_protection_until_tick=0;b.sick_until_tick=s.ticks+600;});
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    const auto before=v.snapshot();check(autosave.begin(document(t,v),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"sick checkpoint");
    const auto catalog=autosave.store().catalog();const auto entry=catalog.histories[0].entries.front();
    check(save::restore_save(save::read_save(entry.path),t.root/"data",mask()).snapshot()==before,"recovery cured sickness");
    for (auto profile:{sim::RulesProfile::CityV14,sim::RulesProfile::CityV13,sim::RulesProfile::CityV12,sim::RulesProfile::CityV11}) {
        auto old=starter(profile);advance(old,2000);const auto snapshot=old.snapshot();
        check(!old.execute({sim::CommandType::PlaceHealthPost,{12,5}}).accepted && old.snapshot()==snapshot,"old profile HealthPost accepted");
        save::write_save(t.root/"old.json",document(t,old),t.root/"data",mask());
        nlohmann::json x;{std::ifstream in(t.root/"old.json");in>>x;}
        for(const auto& b:x["world"]["buildings"])check(!b.contains("health_risk") && !b.contains("sick_until_tick"),"old schema gained health fields");
        check(save::restore_save(save::read_save(t.root/"old.json"),t.root/"data",mask()).snapshot()==snapshot,"old schema changed");
        auto bad=snapshot;bad.buildings.front().health_risk=1;rejects([&]{sim::World::restore(bad,mask());},"old snapshot gained health");
    }
    std::cout<<"schema17 exact boundaries/continuation, invalid authority fail-closed, sick recovery, old schemas unchanged\n";
}
void districts() {
    auto served=starter();advance(served,1200);put(served,sim::CommandType::PlaceWell,{8,1});
    until(served,[&]{return served.treasury()>=120 && served.workforce_supply()>=26;});
    auto unserved=sim::World::restore(served.snapshot(),mask());
    auto post=put(served,sim::CommandType::PlaceHealthPost,{12,5});
    served.execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));
    auto dry=starter();advance(dry,3300);
    auto wet=fresh();auto h=put(wet,sim::CommandType::PlaceHousehold,{80,10});put(wet,sim::CommandType::PlaceWell,{83,10});advance(wet,3300);
    check(wet.household_health_risk(h)==33 && !wet.household_health_protected(h),"disconnected Well created health visit");
    bool sick_without=false;int gaps=0;std::uint64_t longest_route=0;
    for(int i=0;i<20000;++i) {
        served.tick();unserved.tick();
        for(const auto& b:served.buildings()) if(b.kind==sim::Object::Household) {
            check(!served.household_sick(b.id),"compact district sick despite reachable health");
            if(i>1000 && !served.household_health_protected(b.id))++gaps;
        }
        for(const auto& b:unserved.buildings()) sick_without=sick_without || unserved.household_sick(b.id);
        for(const auto& c:served.couriers()) if(c.role==sim::CourierRole::HealthWorker)
            longest_route=std::max(longest_route,static_cast<std::uint64_t>(c.path.size()));
    }
    check(sick_without && gaps==0,"Health Post lacked compact long-run value");
    // A separate road-connected district has its own post and private cursor.
    advance(served,40000);
    road(served,60,76);auto remote=put(served,sim::CommandType::PlaceHousehold,{74,2});
    put(served,sim::CommandType::PlaceWell,{77,2});
    advance(served,served.ticks()+500);check(!served.household_health_protected(remote),"cross-district health teleport");
    auto second=put(served,sim::CommandType::PlaceHealthPost,{60,5});
    served.execute(sim::set_building_workforce_priority(second,sim::WorkforcePriority::High));
    until(served,[&]{return served.household_health_protected(remote);});
    check(served.courier(worker(served,second)).last_dispatched_target==remote &&
        served.courier(worker(served,post)).last_dispatched_target!=remote,"shared Health cursor/district leak");
    check(!served.execute({sim::CommandType::PlaceHealthPost,{80,5}}).accepted,"third HealthPost allowed");
    // Measure a deliberately long patrol: 20 Houses spread over >130 road edges.
    auto long_city=starter();advance(long_city,1200);put(long_city,sim::CommandType::PlaceWell,{8,1});advance(long_city,40000);
    // Relocate the starter houses only when safely empty would disturb the economy;
    // keep them and add 16 remote houses on one long connected street.
    road(long_city,15,145);
    for(int i=0;i<16;++i)put(long_city,sim::CommandType::PlaceHousehold,{32+7*i,2});
    auto lp=put(long_city,sim::CommandType::PlaceHealthPost,{20,5});
    long_city.execute(sim::set_building_workforce_priority(lp,sim::WorkforcePriority::High));
    check(std::ranges::count_if(long_city.courier(worker(long_city,lp)).dynamic_target_routes,[](const auto& r){return r.second.has_value();})==20,"scattered houses not reachable");
    int protected_min=20,unprotected_ticks=0;std::size_t max_path=0;
    for(int i=0;i<20000;++i) {
        long_city.tick();int protected_count=0;
        for(const auto& b:long_city.buildings())if(b.kind==sim::Object::Household)protected_count+=long_city.household_health_protected(b.id);
        if(i>5000){protected_min=std::min(protected_min,protected_count);unprotected_ticks+=protected_count<20;}
        max_path=std::max(max_path,long_city.courier(worker(long_city,lp)).path.size());
    }
    std::cout<<"scattered raw max_path="<<max_path<<" protected_min="<<protected_min<<" gaps="<<unprotected_ticks<<std::endl;
    check(max_path>0 && unprotected_ticks>0,"scattered Health patrol not exercised or guaranteed all Houses");
    std::cout<<"compact max_path_vertices="<<longest_route<<" protection_gap_house_ticks="<<gaps
        <<"; scattered20 max_path_vertices="<<max_path<<" min_protected="<<protected_min
        <<" incomplete_protection_ticks="<<unprotected_ticks<<" (no balance changes)\n";
}
void determinism() {
    auto a=starter(),b=starter();sim::BuildingId well{},industry{},remote{},post{},sick_house{};bool relocated=false,burned=false,disease=false;
    for (int i=0;i<20000;++i) {
        if (i==1200) {well=put(a,sim::CommandType::PlaceWell,{8,1});check(put(b,sim::CommandType::PlaceWell,{8,1})==well,"ID mismatch");}
        if (i==1800) for(auto* w:{&a,&b}) {
            post=put(*w,sim::CommandType::PlaceHealthPost,{13,2});
            w->execute({sim::CommandType::PlaceRoad,{13,3}});
            w->execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));
            sick_house=put(*w,sim::CommandType::PlaceHousehold,{70,4});
        }
        if (i==4000) for(auto* w:{&a,&b}) operation(*w,post,false);
        if (i==5000) for(auto* w:{&a,&b}) operation(*w,post,true);
        if (i==6000) {
            operation(a,kind(a,sim::Object::ClaySource),false);
            operation(b,kind(b,sim::Object::ClaySource),false);
            industry=put(a,sim::CommandType::PlacePottery,{12,5});put(b,sim::CommandType::PlacePottery,{12,5});
            operation(a,industry,false);operation(b,industry,false);
            remote=put(a,sim::CommandType::PlaceServicePost,{60,10});put(b,sim::CommandType::PlaceServicePost,{60,10});
        }
        if (i==6500) for (auto* w:{&a,&b}) {
            check(w->execute(sim::demolish_building(well)).accepted,"Well removal");
            put(*w,sim::CommandType::PlaceWell,{15,5});
        }
        if (i>=7000 && !relocated && a.demolition_status(industry).allowed) {
            for (auto* w:{&a,&b}) {check(w->execute(sim::demolish_building(industry)).accepted,"industry relocation");put(*w,sim::CommandType::PlacePottery,{65,10});}
            relocated=true;
        }
        if (i==8000) for(auto* w:{&a,&b}) {
            const auto r=w->execute({sim::CommandType::RemoveRoad,{13,3}});check(r.accepted,"20k road break");
        }
        if (i==8500) for(auto* w:{&a,&b}) check(w->execute({sim::CommandType::PlaceRoad,{13,3}}).accepted,"20k road repair");
        a.tick();b.tick();disease=disease || (sick_house!=sim::BuildingId{} && a.household_sick(sick_house));burned=burned || (remote!=sim::BuildingId{} && a.building_on_fire(remote));
        check(a.snapshot()==b.snapshot(),"20k snapshots diverged");
    }
    check(relocated && burned && disease,"20k missing fire/relocation");invariants(a);
    std::cout<<"20,000 deterministic ticks, water, Health visits/disease, road break/repair, operation priorities, fire, demolition/rebuild\n";
}
void endurance() {
    Temp t;auto w=starter();advance(w,1200);auto well=put(w,sim::CommandType::PlaceWell,{8,1});advance(w,40000);
    auto post=put(w,sim::CommandType::PlaceHealthPost,{12,7});
    w.execute({sim::CommandType::PlaceRoad,{12,6}});w.execute({sim::CommandType::PlaceRoad,{12,5}});
    w.execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));
    const std::array<std::tuple<sim::CommandType,sim::Object,unsigned>,10> types{{
        {sim::CommandType::PlaceClaySource,sim::Object::ClaySource,4},{sim::CommandType::PlacePottery,sim::Object::Pottery,4},
        {sim::CommandType::PlaceWarehouse,sim::Object::Warehouse,2},{sim::CommandType::PlaceFarm,sim::Object::Farm,2},
        {sim::CommandType::PlaceServicePost,sim::Object::ServicePost,2},{sim::CommandType::PlaceMarket,sim::Object::Market,4},
        {sim::CommandType::PlaceHousehold,sim::Object::Household,20},{sim::CommandType::PlaceFireWatch,sim::Object::FireWatch,2},
        {sim::CommandType::PlaceWell,sim::Object::Well,4},
        {sim::CommandType::PlaceHealthPost,sim::Object::HealthPost,2}}};
    int row=0;sim::BuildingId remote{};
    for (const auto [type,object,limit]:types) {
        const auto count=static_cast<unsigned>(std::ranges::count_if(w.buildings(),[&](const auto& b){return b.kind==object;}));
        for (unsigned i=count;i<limit;++i) {
            const auto id=put(w,type,{35+3*static_cast<int>(i),row});
            if (object==sim::Object::Pottery && remote==sim::BuildingId{}) {remote=id;operation(w,id,false);}
        }
        row+=2;
    }
    check(w.buildings().size()==46 && w.couriers().size()==26,"full46/26 limits");
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    check(autosave.begin(document(t,w),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"endurance start");
    bool fire=false,replaced=false,disease=false,treated=false;unsigned trips=0,rebuilds=0;
    for (int i=0;i<100000;++i) {
        if(i%10000==0) operation(w,post,false);
        if(i%10000==3000) operation(w,post,true);
        if(i==500)check(w.execute({sim::CommandType::RemoveRoad,{12,6}}).accepted,"endurance disconnect");
        if(i==1500)check(w.execute({sim::CommandType::PlaceRoad,{12,6}}).accepted,"endurance repair");
        w.tick();
        for(const auto& b:w.buildings()) {disease=disease || w.household_sick(b.id);treated=treated || w.household_health_protected(b.id);}
        if (i%100==0) invariants(w);
        if (!replaced) fire=fire || w.building_on_fire(remote);
        if (fire && !replaced && w.demolition_status(remote).allowed) {
            check(w.execute(sim::demolish_building(remote)).accepted,"industry demolition");
            put(w,sim::CommandType::PlacePottery,{95,10});replaced=true;
        }
        if (i%5000==4999) {
            check(w.execute(sim::demolish_building(well)).accepted,"repeated Well demolition");
            well=put(w,sim::CommandType::PlaceWell,{8,(rebuilds%2 ? 1:0)});++rebuilds;
            roundtrip(t,w);const auto before=w.snapshot();
            check(autosave.poll(document(t,w),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved &&
                w.snapshot()==before,"endurance autosave mutation");
            const auto catalog=autosave.store().catalog();const auto& entry=catalog.histories.at(0).entries.front();
            check(save::restore_save(save::read_save(entry.path),t.root/"data",mask()).snapshot()==before,"endurance recovery");++trips;
        }
    }
    check(fire && replaced && disease && treated && trips==20 && rebuilds==20 && w.taxes_collected_total()>0,"endurance coverage incomplete");
    invariants(w);std::cout<<"100,000 ticks,46 buildings/26 couriers,20 paid Well relocations,disease,treatment,disconnect,fire,industry,20 save/autosave/recovery roundtrips\n";
}
}
int main(int argc,char* argv[]) {
    try {
        const std::string mode=argc>1 ? argv[1]:"base";
        if(mode=="base"){risk_boundaries();progression();visits_targets_and_roads();demand_fire_and_staffing();}
        else if(mode=="persistence")persistence();else if(mode=="districts")districts();
        else if(mode=="determinism")determinism();else if(mode=="endurance")endurance();
        else throw std::invalid_argument("unknown mode");return 0;
    }catch(const std::exception& e){std::cerr<<"City-v15: "<<e.what()<<'\n';return 1;}
}
