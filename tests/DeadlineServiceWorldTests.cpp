// Rule17 paid authored behavior. No live timer, population, trip or route injection.
#include "OriginalFinanceFixture.h"
#include "simulation/OriginalTerrainFacts.h"
#include "simulation/original/ServiceRules.h"

#include <algorithm>
#include <array>
#include <iostream>

namespace {
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using f::check;
struct Fixture {
    f::Root root;
    std::shared_ptr<const sim::MapPermissions> policy=std::make_shared<const sim::MapPermissions>(228,228,3,
        std::vector<sim::MapCellPermission>(228*228,{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}),
        std::vector<sim::FixedGatePassage>{});
    std::shared_ptr<const orig::OriginalRulesModel> model;
    std::shared_ptr<const sim::OriginalTerrainFacts> terrain=std::make_shared<const sim::OriginalTerrainFacts>(
        std::vector<std::uint32_t>(sim::OriginalTerrainFacts::cell_count,0x80U),std::string(64,'a'));
    Fixture() {
        f::write_text(root.data/"Model/EmperorBuildingModels.txt",oe_production_fixture::authored_model_text(37));
        model=f::save::prepare_original_production_rules_model(root.data);
    }
    sim::World world() const {return sim::World::create_original_deadline_service_dispatch(policy,model,terrain);}
    sim::World old() const {return sim::World::create_original_deadline_fire_dispatch(policy,model,terrain);}
    sim::World restore(const sim::World& w) const {
        return sim::World::restore_original_deadline_service_dispatch(w.snapshot(),policy,model,terrain);
    }
};
sim::BuildingId buy(sim::World& w,sim::CommandType type,sim::Cell cell) {
    f::buy(w,{type,cell});return *w.building_owner_at(cell);
}
sim::CourierId actor(const sim::World& w,sim::BuildingId owner) {
    for(const auto& c:w.couriers())if(c.owner==owner)return c.id;
    throw std::runtime_error("Paid service provider lost its actual actor");
}
const sim::OriginalServiceTrip* trip(const sim::World& w,sim::CourierId id) {
    for(const auto& t:w.original_production_state()->services.trips)if(t.transfer.courier_id==id)return &t;
    return nullptr;
}
const sim::OriginalServiceWitness* witness(const sim::World& w,sim::BuildingId id,orig::ServiceRole role=orig::ServiceRole::Ancestor) {
    for(const auto& v:w.original_production_state()->services.witnesses)if(v.building_id==id&&v.role==role)return &v;
    return nullptr;
}
const orig::HouseState& house(const sim::World& w,sim::BuildingId id) {return oe_finance_fixture::house(w,id);}
void invariant(const sim::World& w) {
    check(w.population_valid()&&w.city_economy_valid()&&w.production_balance_valid()&&w.navigation_valid()&&w.fire_state_valid(),
        "Paid deadline-service World invariant failed");
    oe_finance_fixture::accounting(w);
}
void through(sim::World& w,std::uint64_t tick) {while(w.ticks()<tick)w.tick();invariant(w);}
template<class P>void until(sim::World& w,P done,const char* reason,int limit=5000) {
    for(int i=0;i<limit&&!done();++i)w.tick();check(done(),reason);invariant(w);
}
void roads(sim::World& w) {for(int x=0;x<=36;++x)f::buy(w,{sim::CommandType::PlaceRoad,{x,4}});}
struct Scene {std::array<sim::BuildingId,3> homes{};sim::BuildingId first{},second{};sim::CourierId a{},b{};};
Scene scene(sim::World& w) {
    roads(w);Scene s;
    for(unsigned i=0;i<3;++i)s.homes[i]=buy(w,sim::CommandType::PlaceHousehold,{13+7*int(i),2});
    s.first=buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{10,5});
    s.second=buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{25,5});
    s.a=actor(w,s.first);s.b=actor(w,s.second);
    f::buy(w,sim::set_building_operation(s.first,false));f::buy(w,sim::set_building_operation(s.second,false));
    check(w.construction_spent_total()==394&&w.treasury()==11606&&!w.total_population(),
        "Deadline-service fixture did not pay37Roads/threeHouses/twoShrines");
    through(w,600);check(w.total_population()>=8&&w.original_production_state()->services.trips.empty(),
        "Paused paid providers dispatched or immigration failed to earn workers");return s;
}
void warm(sim::World& w,const Scene& s) {
    f::buy(w,sim::set_building_operation(s.first,true));
    until(w,[&]{return std::all_of(s.homes.begin(),s.homes.end(),[&](auto id){return witness(w,id)!=nullptr;});},
        "Paid first provider did not actually visit all three Houses");
    f::buy(w,sim::set_building_operation(s.first,false));
    until(w,[&]{return w.courier(s.a).phase==sim::CourierPhase::IdleAtWorkshop;},"Paused service actor failed its physical return");
}
void exact_generation_and_unchanged16() {
    Fixture fxt;auto w=fxt.world();check(w.rule_version()==17&&w.original_deadline_service_dispatch_enabled()&&
        !w.original_deadline_fire_dispatch_enabled()&&w.original_fire_protection_enabled()&&w.original_worker_upkeep_interval()==816,
        "Rule17 did not remain an exact independently selected generation");
    check(!sim::original_deadline_service_dispatch_enabled(w.profile(),16)&&
        sim::current_rule_version(sim::RulesProfile::CityV16)==4&&fxt.restore(w).snapshot()==w.snapshot(),
        "Rule17 widened defaults, historical predicates or exact restore");
    const auto s=scene(w);f::buy(w,sim::set_building_operation(s.first,true));w.tick();
    check(trip(w,s.a)&&trip(w,s.a)->target==s.homes[2]&&!witness(w,s.homes[2]),
        "Unvisited real expiry/travel ranking did not select the far due recipient or granted early service");
    auto old=fxt.old();const auto os=scene(old);f::buy(old,sim::set_building_operation(os.first,true));old.tick();
    check(trip(old,os.a)&&trip(old,os.a)->target==os.homes[0]&&!old.original_deadline_service_dispatch_enabled(),
        "Historical16 no longer uses its unchanged first cyclic eligible target");
    invariant(w);invariant(old);
}
void paid_actual_phase35_arrival_and_expiry() {
    Fixture fxt;auto w=fxt.world();roads(w);const auto home=buy(w,sim::CommandType::PlaceHousehold,{13,2});
    through(w,271);const auto provider=buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{10,5});const auto id=actor(w,provider);
    w.tick();check(trip(w,id)&&trip(w,id)->dispatched_tick==272&&w.courier(id).path.size()==5&&house(w,home).ancestor==0,
        "Paid four-edge departure granted service or changed physical timing");
    f::buy(w,sim::set_building_operation(provider,false));auto restored=fxt.restore(w);
    while(w.ticks()<291){w.tick();restored.tick();check(w.snapshot()==restored.snapshot(),"Paid actual arrival restore continuation differs");}
    check(trip(w,id)->arrival_tick==291&&trip(w,id)->visit_applied&&house(w,home).ancestor==39&&
        !house(w,home).herbalist&&!house(w,home).confucian,
        "Actual phase35 arrival failed to assign40 then decay exactly once, or granted another role");
    through(w,2279);check(house(w,home).ancestor==1,"Actual witnessed religion timer expired before its40th countdown");
    w.tick();check(house(w,home).ancestor==0&&witness(w,home)->arrival_tick==291,
        "Absent visits fabricated renewal or countdown boundary is off by one");invariant(w);
}
void expired_before_positive_and_real_incoming() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w);warm(w,s);
    f::buy(w,sim::set_building_operation(s.first,true));f::buy(w,sim::set_building_operation(s.second,true));w.tick();
    check(trip(w,s.a)&&trip(w,s.b)&&trip(w,s.a)->target!=trip(w,s.b)->target,
        "Two paid actors duplicated a timely same-role moving commitment");
    const auto committed=trip(w,s.a)->target;const auto expected=*trip(w,s.a);
    check(w.courier(s.a).phase==sim::CourierPhase::ToWarehouse&&w.courier(s.b).phase==sim::CourierPhase::ToWarehouse,
        "Coordination did not use actual physical outbound actors");
    f::buy(w,sim::set_building_operation(s.first,false));
    until(w,[&]{return w.courier(s.b).phase==sim::CourierPhase::IdleAtWorkshop;},"Second coordinated actor failed to return");
    check(!trip(w,s.a)->arrival_tick&&w.courier_dispatch_status(s.b).selected_target!=committed,
        "A later source pause invalidated a still-moving timely commitment");
    f::buy(w,sim::set_building_operation(s.second,false));
    until(w,[&]{return trip(w,s.a)->arrival_tick.has_value();},"Paused source stopped an already begun service arrival");
    check(trip(w,s.a)->visit_applied&&trip(w,s.a)->transfer==expected.transfer&&witness(w,committed),
        "Actual paused-source arrival failed its real visit");
    auto late=fxt.world();const auto ls=scene(late);warm(late,ls);through(late,2690);
    check(house(late,ls.homes[2]).ancestor==0&&house(late,ls.homes[0]).ancestor>0,
        "Paid expiry-vs-fresh setup did not retain distinct actual timers");
    f::buy(late,sim::set_building_operation(ls.first,true));const auto decision=late.courier_dispatch_status(ls.a);
    check(decision.selected_target==ls.homes[2],"Expired real coverage lost to a positive timer refresh");
    const auto last=witness(late,ls.homes[2])->arrival_tick;late.tick();
    check(trip(late,ls.a)->target==ls.homes[2]&&witness(late,ls.homes[2])->arrival_tick==last&&house(late,ls.homes[2]).ancestor==0,
        "Deadline selection granted a visit before actual physical arrival");invariant(late);
}
void incoming_exact_expiry_and_query_purity() {
    Fixture fxt;
    for(const bool one_tick_late:{false,true}) {
        auto w=fxt.world();const auto s=scene(w);through(w,610);warm(w,s);
        const auto far=s.homes[2];const auto arrived=witness(w,far)->arrival_tick;
        check(arrived==700,"Paid boundary fixture did not earn the intended real arrival");
        // Arrival700 follows countdown699: its40th later countdown is2739.
        // The paid House/source natural fires have already ended by2600.
        constexpr std::uint64_t expiry=2739;
        through(w,one_tick_late ? 2650:2649);
        check(!w.building_on_fire(far)&&house(w,far).ancestor>0,
            "Boundary target is not a live, eligible, positively protected House");
        f::buy(w,sim::set_building_operation(s.first,true));w.tick();
        check(trip(w,s.a)&&trip(w,s.a)->target==far&&!trip(w,s.a)->arrival_tick,
            "Boundary setup did not dispatch the real far incoming visitor");
        const auto& incoming=w.courier(s.a);
        const auto eta=w.ticks()+(incoming.path.size()-1-incoming.path_vertex)*5-
            static_cast<std::uint64_t>(incoming.edge_progress);
        check(eta==expiry+static_cast<unsigned>(one_tick_late),
            "Boundary incoming physical ETA differs from exact expiry/one tick late");
        f::buy(w,sim::set_building_operation(s.second,true));
        const auto authority=w.snapshot();const auto canonical=w.canonical_state();const auto caches=w.couriers();
        for(int repeat=0;repeat<8;++repeat) {
            const auto decision=w.courier_dispatch_status(s.b);
            check(decision.status==sim::CourierDispatchStatus::Ready&&
                (one_tick_late ? decision.selected_target==far:decision.selected_target!=far),
                "Exact-expiry incoming was not coalesced, or late incoming blocked a physically earlier rescue");
            for(const auto& row:caches) {
                const auto& live=w.courier(row.id);
                check(live.cached_revision==row.cached_revision&&live.cached_route==row.cached_route&&
                    live.target_routes==row.target_routes&&live.dynamic_target_routes==row.dynamic_target_routes,
                    "Read-only deadline status query changed a prepared route cache");
            }
            check(w.snapshot()==authority&&w.canonical_state()==canonical,
                "Read-only deadline status query changed paid authority or canonical bytes");
        }
        w.tick();
        check(trip(w,s.b)&&
            (one_tick_late ? trip(w,s.b)->target==far:trip(w,s.b)->target!=far),
            "Actual pre-movement dispatcher disagrees with the completed-tick boundary query");
        f::buy(w,sim::set_building_operation(s.first,false));f::buy(w,sim::set_building_operation(s.second,false));
        through(w,expiry-1);
        check(house(w,far).ancestor==(one_tick_late ? 39:1),
            "Real rescue/old witnessed coverage differs immediately before phase35 expiry");
        w.tick();
        check(house(w,far).ancestor==(one_tick_late ? 38:39),
            "Exact expiry arrival did not grant40 then decay, or early real rescue was lost");
        invariant(w);
    }
}
void paid_route_wait_repair_and_begun_movement() {
    Fixture fxt;auto w=fxt.world();const auto s=scene(w);warm(w,s);
    f::buy(w,sim::set_building_operation(s.first,true));w.tick();
    check(trip(w,s.a)->target==s.homes[2]&&w.courier(s.a).edge_progress>0,"Paid cut scene lacks a begun service edge");
    const auto initial=w.courier(s.a);f::buy(w,{sim::CommandType::RemoveRoad,{15,4}});w.tick();
    check(w.courier(s.a).edge_progress==initial.edge_progress+1,
        "Future road removal stopped or teleported the existing begun edge");
    until(w,[&]{return w.courier(s.a).route_pending;},"Cut service actor did not physically wait at its missing road");
    check(!trip(w,s.a)->arrival_tick,"Waiting route fabricated service arrival");
    f::buy(w,sim::set_building_operation(s.second,true));w.tick();
    check(trip(w,s.b)&&trip(w,s.b)->target==s.homes[2],"A road-waiting actor created a false incoming promise");
    f::buy(w,sim::set_building_operation(s.second,false));
    until(w,[&]{return trip(w,s.b)->arrival_tick.has_value();},"Independent paid source did not physically reach the waiting target");
    check(trip(w,s.b)->visit_applied&&!trip(w,s.a)->arrival_tick,"Independent real visit was lost or waiting actor claimed it");
    f::buy(w,{sim::CommandType::PlaceRoad,{15,4}});f::buy(w,sim::set_building_operation(s.first,false));auto replay=fxt.restore(w);
    for(int i=0;i<220;++i){w.tick();replay.tick();check(w.snapshot()==replay.snapshot(),"Repaired service road restore differs");}
    check(trip(w,s.a)->arrival_tick&&trip(w,s.a)->visit_applied&&!w.courier(s.a).route_pending,
        "Paid repair did not finish the original fixed actual destination");invariant(w);
}
void paid_source_fire_does_not_cancel_begun_visit() {
    Fixture fxt;auto w=fxt.world();roads(w);
    const auto provider=buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{10,5});f::buy(w,sim::set_building_operation(provider,false));
    through(w,600);const auto home=buy(w,sim::CommandType::PlaceHousehold,{27,2});through(w,1990);
    f::buy(w,sim::set_building_operation(provider,true));const auto id=actor(w,provider);w.tick();
    check(trip(w,id)&&!trip(w,id)->arrival_tick,"Paid fire scene lacks a genuine outward visit");
    through(w,2000);check(w.building_on_fire(provider)&&!trip(w,id)->arrival_tick,"Source did not naturally ignite before the arrival");
    until(w,[&]{return trip(w,id)->arrival_tick.has_value();},"Source fire stopped its begun service journey");
    check(trip(w,id)->visit_applied&&w.building_on_fire(provider)&&house(w,home).ancestor>0,
        "Actual visit during later source fire did not grant only real service");
    until(w,[&]{return w.courier(id).phase==sim::CourierPhase::IdleAtWorkshop;},"Burning source cancelled actual physical return");
    const auto sequence=trip(w,id)->transfer.sequence;through(w,w.ticks()+30);
    check(trip(w,id)->transfer.sequence==sequence&&w.courier_dispatch_status(id).status==sim::CourierDispatchStatus::OnFire,
        "Burning source created another dispatch");invariant(w);
}
void paid_watch_service_scarcity_priority_and_frozen_staffing() {
    Fixture fxt;auto w=fxt.world();roads(w);
    const auto home=buy(w,sim::CommandType::PlaceHousehold,{13,2});
    const auto shrine=buy(w,sim::CommandType::PlaceOriginalAncestorShrine,{12,5});
    const auto watch=buy(w,sim::CommandType::PlaceFireWatch,{16,5});
    const auto visitor=actor(w,shrine),inspector=actor(w,watch);
    f::buy(w,sim::set_building_operation(shrine,false));
    f::buy(w,sim::set_building_operation(watch,false));
    check(w.construction_spent_total()==274&&w.treasury()==11726&&!w.total_population(),
        "Competition scene did not pay37Roads/House/Shrine/Watch before any immigration");
    through(w,159);
    check(w.total_population()==2&&w.workforce_required(shrine)==4&&w.workforce_required(watch)==2&&
        w.original_production_state()->services.trips.empty()&&
        w.original_production_state()->fire_protection->arrivals.empty(),
        "Competition setup did not earn only two real residents without visits");
    f::buy(w,sim::set_building_operation(shrine,true));
    f::buy(w,sim::set_building_operation(watch,true));
    check(w.active_workforce_required()==6&&w.workers_assigned(shrine)==0&&w.workers_assigned(watch)==2&&
        w.courier_dispatch_status(visitor).status==sim::CourierDispatchStatus::Unstaffed&&
        w.courier_dispatch_status(inspector).status==sim::CourierDispatchStatus::Ready,
        "Two earned workers did not staff only the complete two-worker Watch");
    w.tick();
    check(w.ticks()==160&&w.total_population()==4&&w.courier(inspector).phase==sim::CourierPhase::ToWarehouse&&
        w.courier(inspector).edge_progress==1&&!trip(w,visitor)&&
        w.workers_assigned(shrine)==4&&w.workers_assigned(watch)==0,
        "Tick160 used post-immigration workers for dispatch instead of the frozen two-worker allocation");
    const auto& fire_trip=w.original_production_state()->fire_protection->trips.front();
    const auto fire_target=fire_trip.target;
    check(fire_trip.courier_id==inspector&&fire_trip.dispatched_tick==160&&fire_trip.population==2&&
        fire_trip.assigned_workers==2&&!w.building_fire_protected(fire_target)&&house(w,home).ancestor==0,
        "Actual Inspector departure lost its pre-admission staffing origin or granted early protection/service");
    const auto& moving=w.courier(inspector);
    const auto fire_eta=w.ticks()+(moving.path.size()-1-moving.path_vertex)*5-
        static_cast<std::uint64_t>(moving.edge_progress);
    check(fire_eta>181&&fire_eta<200,"Paid competition route does not cross both priority reversals");
    w.tick();
    check(trip(w,visitor)&&trip(w,visitor)->dispatched_tick==161&&trip(w,visitor)->target==home&&
        w.courier(visitor).path.size()==3&&!trip(w,visitor)->arrival_tick&&
        w.courier(inspector).phase==sim::CourierPhase::ToWarehouse,
        "Four earned workers did not dispatch the real two-edge service trip while the unstaffed Inspector kept moving");
    const auto first_service=trip(w,visitor)->transfer;
    f::buy(w,sim::set_building_workforce_priority(watch,sim::WorkforcePriority::High));
    check(w.workforce_supply()==4&&w.active_workforce_required()==6&&w.workforce_used()==2&&
        w.workers_assigned(watch)==2&&w.workers_assigned(shrine)==0,
        "High Watch priority did not reverse the all-or-none scarce allocation");
    through(w,170);
    check(trip(w,visitor)->arrival_tick==170&&trip(w,visitor)->visit_applied&&
        witness(w,home)&&witness(w,home)->arrival_tick==170&&house(w,home).ancestor>0&&
        w.workers_assigned(shrine)==0&&!w.building_fire_protected(fire_target),
        "Staff loss cancelled the begun service arrival or an outgoing Inspector granted early protection");
    through(w,181);
    check(w.courier(visitor).phase==sim::CourierPhase::IdleAtWorkshop&&trip(w,visitor)->transfer==first_service&&
        w.courier_dispatch_status(visitor).status==sim::CourierDispatchStatus::Unstaffed&&
        w.courier(inspector).phase==sim::CourierPhase::ToWarehouse,
        "Unstaffed returned service actor redispatched or did not finish its ordinary physical return");
    f::buy(w,sim::set_building_workforce_priority(watch,sim::WorkforcePriority::Normal));
    check(w.workers_assigned(shrine)==4&&w.workers_assigned(watch)==0&&
        w.courier_dispatch_status(visitor).status==sim::CourierDispatchStatus::Ready,
        "Restoring normal priority did not restore the earlier paid service source's real dispatch ability");
    w.tick();
    check(trip(w,visitor)->dispatched_tick==182&&trip(w,visitor)->transfer.sequence==first_service.sequence+1&&
        !trip(w,visitor)->arrival_tick&&witness(w,home)->arrival_tick==170&&
        w.courier(inspector).phase==sim::CourierPhase::ToWarehouse,
        "Priority reversal did not cause a real service departure or fabricated its next visit");
    through(w,fire_eta-1);
    check(!w.building_fire_protected(fire_target)&&
        w.original_production_state()->fire_protection->arrivals.empty(),
        "Unstaffed moving Inspector protected a destination before its actual arrival");
    w.tick();
    const auto& arrived=w.original_production_state()->fire_protection->arrivals;
    check(arrived.size()==1&&arrived.front().tick==fire_eta&&arrived.front().trip.courier_id==inspector&&
        arrived.front().trip.assigned_workers==2&&arrived.front().trip.population==2&&
        w.workers_assigned(watch)==0&&w.fire_protection_remaining(fire_target)==2400&&
        w.courier(inspector).phase==sim::CourierPhase::Returning,
        "Later Watch staffing loss cancelled its begun actual protection arrival or rewrote its frozen origin");
    f::buy(w,sim::set_building_operation(shrine,false));
    f::buy(w,sim::set_building_operation(watch,false));auto replay=fxt.restore(w);
    for(int step=0;step<100&&(w.courier(visitor).phase!=sim::CourierPhase::IdleAtWorkshop||
        w.courier(inspector).phase!=sim::CourierPhase::IdleAtWorkshop);++step) {
        w.tick();replay.tick();check(w.snapshot()==replay.snapshot(),"Scarce staffing/priority replay changed begun returns");
    }
    check(w.courier(visitor).phase==sim::CourierPhase::IdleAtWorkshop&&
        w.courier(inspector).phase==sim::CourierPhase::IdleAtWorkshop&&trip(w,visitor)->arrival_tick==191&&
        w.original_production_state()->fire_protection->arrivals.size()==1&&
        w.construction_spent_total()==274&&w.treasury()==11726,
        "Paused actors did not finish exact paid visits/returns or priorities changed Funds");
    invariant(w);
}
}
int main() {
    try {
        paid_watch_service_scarcity_priority_and_frozen_staffing();
        exact_generation_and_unchanged16();paid_actual_phase35_arrival_and_expiry();
        expired_before_positive_and_real_incoming();incoming_exact_expiry_and_query_purity();
        paid_route_wait_repair_and_begun_movement();
        paid_source_fire_does_not_cancel_begun_visit();
        std::cout<<"Rule17 paid deadlines/phase35/real incoming/scarce Watch-service staffing/source-fire/road repair and exact16 control PASS\n";return 0;
    }catch(const std::exception& e){std::cerr<<"Deadline Service World FAIL: "<<e.what()<<'\n';return 1;}
}
