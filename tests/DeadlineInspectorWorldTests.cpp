// Authored Rule16 checks. Physical proofs use paid commands and actual ticks;
// synthetic selector-only inputs are separately labelled and never persisted.
#include "OriginalFinanceFixture.h"
#include "simulation/OriginalTerrainFacts.h"

#include <algorithm>
#include <iostream>

namespace {
namespace f=oe_influence_fixture;
namespace q=oe_finance_fixture;
namespace sim=openemperor::simulation;
using f::check;
struct Fixture {
    f::Root root;
    std::shared_ptr<const sim::MapPermissions> policy=std::make_shared<const sim::MapPermissions>(228,228,3,
        std::vector<sim::MapCellPermission>(228*228,
            {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}),
        std::vector<sim::FixedGatePassage>{});
    std::shared_ptr<const openemperor::original::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain=std::make_shared<const sim::OriginalTerrainFacts>(
        std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count,0x80U),std::string(64,'a'));
    Fixture() {
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",q::authored_model());
        model=f::save::prepare_original_production_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_original_deadline_fire_dispatch(policy,model,terrain);}
    sim::World old() const {return sim::World::create_original_inspector_coordination(policy,model,terrain);}
    sim::World restore(const sim::World& w) const {
        return sim::World::restore_original_deadline_fire_dispatch(w.snapshot(),policy,model,terrain);
    }
};
template<class F> void refuses(F action,const char* why) {
    try {action();}catch(const std::exception&) {return;}
    throw std::runtime_error(why);
}
void ticks(sim::World& w,int count) {for(int i=0;i<count;++i)w.tick();}
sim::BuildingId buy(sim::World& w,sim::CommandType type,sim::Cell at) {
    f::buy(w,{type,at});return *w.building_owner_at(at);
}
sim::CourierId inspector(const sim::World& w,sim::BuildingId watch) {
    const auto it=std::find_if(w.couriers().begin(),w.couriers().end(),[&](const auto& c) {
        return c.owner==watch&&c.role==sim::CourierRole::FireInspector;
    });
    check(it!=w.couriers().end(),"Paid Watch lost its Inspector");return it->id;
}
void road(sim::World& w,int last=22) {
    for(int x=10;x<=last;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,12}});
}
void invariant(const sim::World& w) {
    check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&
        w.navigation_valid()&&w.fire_state_valid(),"Paid Rule16 invariant failed");
}
void exact_identity_and_historical_generations() {
    Fixture fxt;auto w=fxt.world();
    check(w.rule_version()==16&&w.active_rules().version==16&&w.original_deadline_fire_dispatch_enabled()&&
        !w.original_inspector_coordination_enabled()&&w.original_cardinal_water_enabled()&&
        w.original_market_selection_enabled()&&w.original_sustainable_economy_enabled()&&
        w.original_fire_protection_enabled()&&w.original_production_enabled()&&
        w.original_worker_upkeep_interval()==816&&sim::original_food_farm_limit(w.profile(),16)==3&&
        w.treasury()==12000&&w.original_production_state()->development_revision==27,
        "Rule16 changed inherited paid authority");
    check(!sim::original_deadline_fire_dispatch_enabled(w.profile(),15)&&
        !sim::original_inspector_coordination_enabled(w.profile(),16)&&
        sim::current_rule_version(sim::RulesProfile::CityV16)==4&&
        sim::current_rule_version(sim::RulesProfile::CityV11)==3&&
        sim::source_cell_ramps_enabled(w.profile(),20)&&
        !sim::original_economy_enabled(w.profile(),20)&&
        sim::rule_version_supported(w.profile(),21)&&!sim::original_economy_enabled(w.profile(),21)&&
        !sim::rule_version_supported(w.profile(),24),"Rule16 identity/defaults or separate authored Rule20 changed");
    refuses([&]{sim::World ordinary(fxt.policy,sim::RulesProfile::CityV16,16);},"Ordinary constructor enabled Rule16");
    refuses([&]{(void)sim::World::create_original_deadline_fire_dispatch(fxt.policy,fxt.model,nullptr);},"Missing terrain accepted");
    auto old=fxt.old();
    refuses([&]{(void)sim::World::restore_original_deadline_fire_dispatch(old.snapshot(),fxt.policy,fxt.model,fxt.terrain);},
        "Rule15 silently migrated to16");
    refuses([&]{(void)sim::World::restore_original_inspector_coordination(w.snapshot(),fxt.policy,fxt.model,fxt.terrain);},
        "Rule16 restored through15");
    check(fxt.restore(w).snapshot()==w.snapshot(),"Empty exact-generation restore differs");
}
void paid_arrival_at_ignition_boundary(bool one_tick_late) {
    Fixture fxt;auto w=fxt.world();road(w);
    const auto home=buy(w,sim::CommandType::PlaceHousehold,{12,10});
    ticks(w,1980+(one_tick_late?1:0));
    check(!w.building_on_fire(home),"Paid boundary House ignited too early");
    const auto watch=buy(w,sim::CommandType::PlaceFireWatch,{10,11});const auto actor=inspector(w,watch);
    w.tick();
    check(w.courier(actor).phase==sim::CourierPhase::ToWarehouse&&w.courier(actor).target==home&&
        w.courier(actor).path.size()==5&&!w.building_fire_protected(home),
        "Paid four-edge departure granted early coverage or changed route");
    auto restored=fxt.restore(w);check(restored.snapshot()==w.snapshot(),"Paid boundary trip restore differs");
    while(w.ticks()<2000) {w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Boundary continuation differs");}
    if(one_tick_late) {
        check(w.building_on_fire(home)&&w.fire_remaining(home)==600&&!w.building_fire_protected(home),
            "A physically late trip suppressed the real ignition");
        w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Late arrival continuation differs");
    }
    const auto& arrivals=w.original_production_state()->fire_protection->arrivals;
    check(!w.building_on_fire(home)&&w.fire_protection_remaining(home)==2400&&
        !arrivals.empty()&&arrivals.back().tick==std::uint64_t(one_tick_late?2001:2000)&&
        arrivals.back().extinguished==one_tick_late&&w.courier(actor).phase==sim::CourierPhase::Returning,
        "Actual physical arrival did not exclusively own the unchanged fire effects");
    invariant(w);
}
struct Scene {sim::BuildingId home,first,second;sim::CourierId a,b;};
Scene scene(sim::World& w) {
    road(w);const auto home=buy(w,sim::CommandType::PlaceHousehold,{12,10});ticks(w,240);
    check(w.total_population()>=4,"Paid House did not admit both Watches' workers");
    const auto first=buy(w,sim::CommandType::PlaceFireWatch,{10,11});
    const auto second=buy(w,sim::CommandType::PlaceFireWatch,{18,11});
    f::buy(w,sim::set_building_operation(second,false));
    return {home,first,second,inspector(w,first),inspector(w,second)};
}
void paid_pause_and_cut_repair() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w);w.tick();
    check(w.courier(s.a).target==s.home&&!w.building_fire_protected(s.home),"Paid first trip absent");
    f::buy(w,sim::set_building_operation(s.first,false));const auto progress=w.courier(s.a).edge_progress;w.tick();
    check(w.courier(s.a).edge_progress==progress+1&&!w.building_fire_protected(s.home),
        "Paused source stopped begun movement or fabricated coverage");
    f::buy(w,{sim::CommandType::RemoveRoad,{11,12}});ticks(w,3);
    check(w.courier(s.a).route_pending&&w.courier(s.a).edge_progress==0&&!w.building_fire_protected(s.home),
        "Real cut failed begun-edge safety or fabricated an arrival");
    f::buy(w,sim::set_building_operation(s.second,true));w.tick();
    check(w.courier(s.b).phase==sim::CourierPhase::ToWarehouse&&w.courier(s.b).target==s.home,
        "Road-waiting actor falsely reserved an actual reachable recipient");
    f::buy(w,{sim::CommandType::PlaceRoad,{11,12}});w.tick();
    check(!w.courier(s.a).route_pending&&w.courier(s.a).target==s.home&&w.courier(s.b).target==s.home&&
        w.original_production_state()->fire_protection->trips.size()==2,
        "Ordinary repair did not preserve legitimate duplicate physical trips");
    auto restored=fxt.restore(w);check(restored.snapshot()==w.snapshot(),"Repaired duplicate restore differs");
    for(int i=0;i<100;++i){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Repair continuation differs");}
    check(w.building_fire_protected(s.home),"Repaired actual arrival never protected the House");invariant(w);
}
void paid_burning_source_keeps_incoming_trip() {
    Fixture fxt;auto w=fxt.world();road(w);
    const auto home=buy(w,sim::CommandType::PlaceHousehold,{12,10});
    const auto first=buy(w,sim::CommandType::PlaceFireWatch,{10,11});
    f::buy(w,sim::set_building_operation(first,false));ticks(w,1981);
    f::buy(w,sim::set_building_operation(first,true));w.tick();
    const auto a=inspector(w,first);
    check(w.courier(a).target==home&&w.courier(a).phase==sim::CourierPhase::ToWarehouse,
        "Paid late-only target did not start a real trip");
    while(w.ticks()<1999)w.tick();
    const auto second=buy(w,sim::CommandType::PlaceFireWatch,{18,11});
    f::buy(w,sim::set_building_operation(second,false));w.tick();
    check(w.building_on_fire(first)&&w.building_on_fire(home)&&w.courier(a).phase==sim::CourierPhase::ToWarehouse,
        "Real source/recipient ignition interrupted begun travel");
    f::buy(w,sim::set_building_operation(second,true));
    check(w.courier_dispatch_status(inspector(w,second)).selected_target==first,
        "Burning source erased its still-real incoming commitment");
    auto restored=fxt.restore(w);w.tick();restored.tick();
    check(w.snapshot()==restored.snapshot()&&!w.building_on_fire(home)&&w.building_fire_protected(home),
        "Actual arrival from burning source failed or replay diverged");invariant(w);
}
void paid_return_blocking_exchange() {
    Fixture fxt;auto w=fxt.world();auto old=fxt.old();
    struct PaidChoices {sim::BuildingId near,far,watch;sim::CourierId actor;};
    const auto prepare=[](sim::World& city) {
        road(city,110);
        const auto far=buy(city,sim::CommandType::PlaceHousehold,{108,10});ticks(city,200);
        const auto near=buy(city,sim::CommandType::PlaceHousehold,{12,10});ticks(city,1849);
        const auto watch=buy(city,sim::CommandType::PlaceFireWatch,{10,11});
        check(city.building_on_fire(far)&&!city.building_on_fire(near),"Paid exchange input fires differ");
        return PaidChoices{near,far,watch,inspector(city,watch)};
    };
    const auto s=prepare(w),legacy=prepare(old);
    // At actual dispatch2050, near has ignition2200 and four-edge arrival2069;
    // far burns until2600 and has100-edge response2549. Initial outbound-only
    // slack51 favors far over131, but its1000tick return trip blocks near.
    // Near first occupies40ticks, so far can still actually arrive2589<2600.
    w.tick();old.tick();
    check(w.ticks()==2050&&w.courier(s.actor).target==s.near&&w.courier(s.actor).path.size()==5&&
        old.courier(legacy.actor).target==legacy.far&&old.courier(legacy.actor).path.size()==101,
        "Generic return exchange did not choose the feasible earlier-deadline ordering");
    auto restored=fxt.restore(w);
    while(w.ticks()<2589) {
        w.tick();restored.tick();old.tick();
        check(w.snapshot()==restored.snapshot(),"Paid exchange exact continuation differs");
        if(w.ticks()==2069)check(w.building_fire_protected(s.near),"Short real visit did not arrive at2069");
        if(w.ticks()==2200)check(!w.building_on_fire(s.near)&&old.building_on_fire(legacy.near),
            "The blocked original ordering did not distinguish actual ignition");
    }
    const auto& arrivals=w.original_production_state()->fire_protection->arrivals;
    check(!w.building_on_fire(s.near)&&!w.building_on_fire(s.far)&&
        w.fire_protection_remaining(s.far)==2400&&arrivals.back().tick==2589&&arrivals.back().extinguished,
        "Short-first ordering did not physically complete both real visits in time");invariant(w);
}

// These controlled synthetic state edits are selector queries only. They are
// never ticked, saved or restored and are not claimed as paid gameplay evidence.
sim::BuildingState& body(sim::World& w,sim::BuildingId id) {
    return const_cast<sim::BuildingState&>(w.building(id));
}
sim::CourierState& courier(sim::World& w,sim::CourierId id) {
    return const_cast<sim::CourierState&>(w.courier(id));
}
struct Choices {sim::BuildingId near,far,watch;sim::CourierId actor;};
Choices choices(sim::World& w,bool far_market=false) {
    road(w,35);const auto near=buy(w,sim::CommandType::PlaceHousehold,{12,10});
    const auto far=far_market ? buy(w,sim::CommandType::PlaceMarket,{30,11}):
        buy(w,sim::CommandType::PlaceHousehold,{30,10});ticks(w,240);
    const auto watch=buy(w,sim::CommandType::PlaceFireWatch,{10,11});
    f::buy(w,sim::set_building_workforce_priority(watch,sim::WorkforcePriority::High));
    return {near,far,watch,inspector(w,watch)};
}
void synthetic_generic_deadlines_and_pure_queries() {
    Fixture fxt;
    {
        auto w=fxt.world();const auto s=choices(w);
        body(w,s.near).fire_risk=93;body(w,s.far).fire_risk=92;
        const auto before=w.snapshot();
        for(int i=0;i<4;++i)check(w.courier_dispatch_status(s.actor).selected_target==s.far,
            "Longer physical journey lost urgency to a higher raw risk alone");
        check(w.snapshot()==before,"Deadline query mutated complete authority");
    }
    {
        auto w=fxt.world();const auto s=choices(w,true);
        body(w,s.near).fire_risk=93;body(w,s.far).fire_risk=92;
        check(w.building(s.far).kind==sim::Object::Market&&
            w.courier_dispatch_status(s.actor).selected_target==s.far,
            "The same physical deadline ranking treated a Market differently from a House");
    }
    {
        auto w=fxt.world();const auto s=choices(w);body(w,s.near).fire_risk=91;
        body(w,s.far).fire_risk=0;body(w,s.far).fire_until_tick=w.ticks()+600;
        check(w.courier_dispatch_status(s.actor).selected_target==s.near,
            "Unconditional burning-first starved a feasible near-ignition recipient");
        auto old=fxt.old();const auto legacy=choices(old);body(old,legacy.near).fire_risk=91;
        body(old,legacy.far).fire_risk=0;body(old,legacy.far).fire_until_tick=old.ticks()+600;
        check(old.courier_dispatch_status(legacy.actor).selected_target==legacy.far,
            "Historical Rule15 burning-first changed");
    }
    {
        auto w=fxt.world();const auto s=choices(w);
        body(w,s.near).fire_risk=0;body(w,s.far).fire_risk=0;
        body(w,s.near).fire_protection_until_tick=w.ticks()+2400;
        body(w,s.far).fire_protection_until_tick=w.ticks()+2400;
        const auto before=w.snapshot();
        check(w.courier_dispatch_status(s.actor).selected_target==s.far&&w.snapshot()==before,
            "Real protected recipient could not receive future physical scheduling");
    }
    {
        auto w=fxt.world();const auto s=choices(w);body(w,s.near).fire_risk=99;
        body(w,s.far).fire_risk=90;
        check(w.courier_dispatch_status(s.actor).selected_target==s.far,
            "An already impossible physical deadline blocked useful feasible work");
    }
    {
        auto w=fxt.world();const auto s=choices(w);
        body(w,s.near).fire_until_tick=w.ticks()+1;body(w,s.near).fire_risk=0;
        body(w,s.far).fire_risk=90;
        check(w.courier_dispatch_status(s.actor).selected_target==s.far,
            "Arrival after natural expiry pretended to extinguish the current incident");
    }
}
void synthetic_incoming_etas_and_exclusions() {
    Fixture fxt;auto w=fxt.world();road(w);
    const auto home=buy(w,sim::CommandType::PlaceHousehold,{12,10});ticks(w,240);
    const auto far=buy(w,sim::CommandType::PlaceFireWatch,{22,11});
    const auto near=buy(w,sim::CommandType::PlaceFireWatch,{10,11});
    f::buy(w,sim::set_building_operation(near,false));w.tick();
    const auto a=inspector(w,far),b=inspector(w,near);
    check(w.courier(a).target==home,"Paid precursor incoming trip absent");
    f::buy(w,sim::set_building_operation(near,true));
    check(w.courier_dispatch_status(b).selected_target==far,
        "Timely actual incoming trip did not coalesce despite a faster spare actor");
    body(w,home).fire_risk=99;
    check(w.courier_dispatch_status(b).selected_target==home,
        "An actually late incoming trip blocked a physically earlier replacement");
    for(int mode=0;mode<4;++mode) {
        const auto old=courier(w,a);
        if(mode==0)courier(w,a).enabled=false;
        if(mode==1)courier(w,a).phase=sim::CourierPhase::Returning;
        if(mode==2)courier(w,a).route_pending=true;
        if(mode==3)courier(w,a).target=near;
        check(w.courier_dispatch_status(b).selected_target==home,
            "Disabled/returning/waiting/mismatched actor supplied fictitious incoming coverage");
        courier(w,a)=old;
    }
}
}
int main() {
    try {
        exact_identity_and_historical_generations();
        paid_arrival_at_ignition_boundary(false);paid_arrival_at_ignition_boundary(true);
        paid_pause_and_cut_repair();paid_burning_source_keeps_incoming_trip();paid_return_blocking_exchange();
        synthetic_generic_deadlines_and_pure_queries();synthetic_incoming_etas_and_exclusions();
        std::cout<<"Deadline Inspector World checks passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
