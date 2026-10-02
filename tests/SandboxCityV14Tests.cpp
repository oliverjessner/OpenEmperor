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
sim::World fresh(sim::RulesProfile profile=sim::RulesProfile::CityV14) {
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
        w.fire_state_valid(),"City-v14 invariant at tick "+std::to_string(w.ticks()));
    std::uint64_t taxes=w.snapshot().demolition_history.taxes;
    for (const auto& b:w.buildings()) {
        if (b.kind==sim::Object::Household) taxes+=b.taxes_paid_total;
        else check(b.taxes_paid_total==0,"non-House tax authority");
    }
    if (sim::desirability_profile(w.profile()))
        check(taxes==w.taxes_collected_total(),"tax history conservation");
    check(w.buildings().size()<=44 && w.couriers().size()<=24 &&
        w.route_cache_entries()<=24*38,"collection/cache budget");
}
void advance(sim::World& w,std::uint64_t tick) { while (w.ticks()<tick) w.tick();invariants(w); }
template<class P> void until(sim::World& w,P done,int budget=3000) {
    while (budget-- && !done()) w.tick();
    check(done(),"state wait timed out at "+std::to_string(w.ticks()));invariants(w);
}
sim::World starter(sim::RulesProfile profile=sim::RulesProfile::CityV14) {
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
void geometry_and_infrastructure() {
    auto w=fresh();const auto home=put(w,sim::CommandType::PlaceHousehold,{10,10});
    const auto well=put(w,sim::CommandType::PlaceWell,{16,10});
    check(w.building_distance(home,well)==5 && w.household_has_water(home),"footprint boundary5 (origin6)");
    check(w.nearest_water_source(home)==well && w.workforce_required(well)==0 &&
        w.couriers().empty() && !sim::World::operation_controllable(sim::Object::Well) &&
        !sim::fire_eligible(sim::Object::Well) && sim::desirability_impact(sim::Object::Well)==0,
        "Well acquired operation, goods, fire or desirability semantics");
    for (int d=0;d<=6;++d) {
        const sim::Cell origin{16+d,10};
        check(w.household_has_water_at(origin)==(d<=5),"water distance0-6");
    }
    check(!w.household_service_active(home) && w.building(home).pottery_stock==0 &&
        w.building(home).food_stock==0 && w.snapshot().roads.empty(),"water fabricated road supply");
    check(w.execute({sim::CommandType::PlaceRoad,{12,10}}).accepted &&
        w.execute({sim::CommandType::RemoveRoad,{12,10}}).accepted &&
        w.household_has_water(home),"road break removed water");
    const auto score=w.household_desirability(home);
    check(!w.execute(sim::set_building_operation(well,false)).accepted &&
        !w.execute(sim::set_building_workforce_priority(well,sim::WorkforcePriority::High)).accepted,
        "Well operation commands accepted");
    const auto nearer=put(w,sim::CommandType::PlaceWell,{10,8});
    check(w.nearest_water_source(home)==nearer,"nearest distance selection");
    const auto tie=put(w,sim::CommandType::PlaceWell,{8,10});
    check(w.building_distance(home,nearer)==2 && w.building_distance(home,tie)==2 &&
        w.nearest_water_source(home)==nearer,"stable ID tie break");
    const auto fourth=put(w,sim::CommandType::PlaceWell,{20,20});
    const auto before=w.snapshot();
    check(!w.execute({sim::CommandType::PlaceWell,{22,20}}).accepted && w.snapshot()==before,"fifth Well changed World");
    check(w.execute(sim::demolish_building(fourth)).accepted,"empty Well demolition");
    const auto new_id=put(w,sim::CommandType::PlaceWell,{22,20});
    check(new_id>fourth && w.snapshot().demolition_history.construction_spent==60 &&
        w.snapshot().demolition_history.taxes==0,"Well IDs/spending history");
    check(w.household_desirability(home)==score,"Well changed desirability");
    advance(w,2400);
    check(w.building(home).fulfilled_demand==0 && w.building(home).missed_demand==6 &&
        w.taxes_collected_total()==0 && w.household_has_water(home) && w.building_on_fire(home),
        "water replaced service/fire demand");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Well)
        check(b.fire_risk==0 && b.fire_until_tick==0 && b.population==0 && b.output==0 && b.taxes_paid_total==0,
            "Well gained authority");
    const auto saved=w.snapshot();
    openemperor::performance::set_enabled(true);openemperor::performance::reset();
    for (int n=0;n<1000;++n) {
        (void)w.nearest_water_source(home);(void)w.household_has_water(home);
        (void)w.household_has_water_at({10,10});(void)w.well_coverage_at({11,8});
    }
    for (const auto counter:{openemperor::performance::Counter::WorldCopies,
        openemperor::performance::Counter::WorldRestores,openemperor::performance::Counter::WorldExecutes,
        openemperor::performance::Counter::BfsCalls,openemperor::performance::Counter::RouteRefreshes,
        openemperor::performance::Counter::AssetDecodes,openemperor::performance::Counter::FileReads})
        check(openemperor::performance::counter(counter)==0,"water query did work/mutation");
    openemperor::performance::set_enabled(false);check(w.snapshot()==saved,"water query mutated World");
    rejects([&]{w.nearest_water_source(well);},"non-House query accepted");
    // A Well neither changes the House's risk clock nor protects it from fire.
    auto dry=fresh(),wet=fresh();
    const auto dry_home=put(dry,sim::CommandType::PlaceHousehold,{10,10});
    const auto wet_home=put(wet,sim::CommandType::PlaceHousehold,{10,10});
    put(wet,sim::CommandType::PlaceWell,{16,10});
    for (int tick=0;tick<4000;++tick) {
        dry.tick();wet.tick();
        check(dry.building(dry_home)==wet.building(wet_home),"water changed unsupplied House/fire authority");
    }
}
void starter_and_housing() {
    auto w=starter();check(w.treasury()==20 && w.construction_spent_total()==1280 &&
        w.ticks()==0 && w.water_covered_households()==0 && w.total_population()==24 &&
        w.workforce_used()==24,"paid starter changed");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(!w.household_has_water(b.id),"free starter water");
    until(w,[&]{return w.taxes_collected_total()>0;});const auto first_tax=w.ticks();
    until(w,[&]{return w.treasury()>=60;});
    const auto purchase=w.ticks();const auto poor=put(w,sim::CommandType::PlaceWell,{15,5});
    check(w.water_covered_households()==1,"poor placement must cover exactly one");
    check(w.execute(sim::demolish_building(poor)).accepted,"paid poor Well removal");
    until(w,[&]{return w.treasury()>=60;});const auto well=put(w,sim::CommandType::PlaceWell,{8,1});
    check(w.water_covered_households()==4,"central placement must cover four");
    const auto good=*w.building_owner_at({9,5});const auto neutral=*w.building_owner_at({6,2});
    advance(w,10000);
    check(w.historical_household_level(good)==2 && w.household_level(good)==2 &&
        w.household_level(neutral)==1 && w.building(good).population==16,"water/desirability development");
    const auto record=w.building(good);check(w.execute(sim::demolish_building(well)).accepted,"Well removal");
    check(w.household_level(good)==0 && w.household_population_capacity(good)==6 &&
        w.building(good)==record && w.historical_household_level(good)==2,"instant truncation/history lost");
    const auto tax=record.taxes_paid_total,fulfilled=record.fulfilled_demand;
    advance(w,10400);
    check(w.building(good).population==15 && w.building(good).fulfilled_demand==fulfilled+1 &&
        w.building(good).taxes_paid_total==tax+25,"dry House actual tax/overcap decline");
    put(w,sim::CommandType::PlaceWell,{8,1});
    check(w.household_level(good)==2 && w.household_population_capacity(good)==16 &&
        w.building(good).population==15,"restored water did not immediately uncap");
    advance(w,10800);
    check(w.building(good).population==16 && w.building(good).taxes_paid_total==tax+85 &&
        w.building(good).fulfilled_demand==fulfilled+2,"wet House actual60 tax/recovery growth");
    // Paid empty industry is spatially harmful even when paused. No snapshot fixtures.
    const auto a=put(w,sim::CommandType::PlacePottery,{12,5});operation(w,a,false);
    const auto b=put(w,sim::CommandType::PlaceClaySource,{12,2});operation(w,b,false);
    check(w.household_has_water(good) && w.household_desirability(good)<-20 &&
        w.household_level(good)==0,"water bypassed poor quality");
    check(w.execute(sim::demolish_building(b)).accepted && w.household_level(good)==1,"neutral water cap");
    check(w.execute(sim::demolish_building(a)).accepted && w.household_level(good)==2,"good water cap");
    const auto watch=kind(w,sim::Object::FireWatch);operation(w,watch,false);
    until(w,[&]{return w.building_on_fire(good);},6000);
    check(w.household_has_water(good),"fire changed water");
    const auto burning=w.building(good);
    advance(w,w.ticks()+static_cast<unsigned>(400-burning.demand_progress));
    check(w.building(good).missed_demand==burning.missed_demand+1 &&
        w.building(good).taxes_paid_total==burning.taxes_paid_total,"burning water House earned tax");
    std::cout<<"first_tax_tick="<<first_tax<<" first_well_purchase_tick="<<purchase
        <<" poor_coverage=1 central_coverage=4; actual dry25/wet60 tax, 16->15->16 residents\n";
}
struct Temp {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-city14-"+
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
    check(doc.source_schema_version==16,"schema16 identity");
    auto restored=save::restore_save(doc,t.root/"data",mask());
    check(restored.snapshot()==before && w.snapshot()==before,"save/load mutation");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(restored.household_has_water(b.id)==w.household_has_water(b.id) &&
            restored.nearest_water_source(b.id)==w.nearest_water_source(b.id) &&
            restored.household_level(b.id)==w.household_level(b.id),"derived water changed on load");
    auto control=sim::World::restore(before,mask());
    for (int i=0;i<500;++i) {restored.tick();control.tick();check(restored.snapshot()==control.snapshot(),"resume diverged");}
}
void persistence() {
    Temp t;auto w=starter();advance(w,1200);const auto well=put(w,sim::CommandType::PlaceWell,{8,1});
    advance(w,6000);roundtrip(t,w);
    const auto original=w.snapshot();
    const auto invalid=[&](auto edit) {auto bad=original;edit(bad);rejects([&]{sim::World::restore(bad,mask());},"invalid Well authority accepted");};
    const auto modify=[&](auto edit) {invalid([&](auto& s) {for (auto& b:s.buildings) if (b.id==well) edit(b);});};
    modify([](auto& b){b.population=1;});modify([](auto& b){b.output=1;});
    modify([](auto& b){b.food_stock=1;});modify([](auto& b){b.reserved_incoming=1;});
    modify([](auto& b){b.fire_risk=1;});modify([](auto& b){b.fire_until_tick=7000;});
    modify([](auto& b){b.fire_protection_until_tick=7000;});modify([](auto& b){b.taxes_paid_total=25;});
    modify([](auto& b){b.operating_enabled=false;});modify([](auto& b){b.workforce_priority=sim::WorkforcePriority::High;});
    modify([](auto& b){b.kind=static_cast<sim::Object>(255);});
    invalid([](auto& s){s.profile=sim::RulesProfile::CityV13;});
    nlohmann::json j;{std::ifstream in(t.root/"save.json");in>>j;}
    check(j.dump().find("water")==std::string::npos,"coverage serialized");
    const auto bad_json=[&](auto edit) {
        auto copy=j;edit(copy);{std::ofstream out(t.root/"bad.json");out<<copy;}
        rejects([&]{save::restore_save(save::read_save(t.root/"bad.json"),t.root/"data",mask());},"invalid schema/profile/Well accepted");
    };
    bad_json([](auto& x){x["schema_version"]=15;});
    bad_json([](auto& x){x["rules"]["id"]=sim::city_v13_profile_name;});
    bad_json([](auto& x){x["world"]["buildings"].back()["kind"]=255;});
    auto old=starter(sim::RulesProfile::CityV13);advance(old,6000);
    check(old.household_level(*old.building_owner_at({9,5}))==2,"City13 gained water cap");
    const auto before=old.snapshot();check(!old.execute({sim::CommandType::PlaceWell,{8,1}}).accepted && old.snapshot()==before,"City13 Well accepted");
    save::write_save(t.root/"old.json",document(t,old),t.root/"data",mask());
    check(save::read_save(t.root/"old.json").source_schema_version==15 &&
        save::restore_save(save::read_save(t.root/"old.json"),t.root/"data",mask()).snapshot()==before,"schema15 changed");
    check(w.execute(sim::demolish_building(well)).accepted,"saved Well removal");roundtrip(t,w);
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    check(autosave.begin(document(t,w),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"recovery start");
    advance(w,w.ticks()+1200);const auto state=w.snapshot();
    check(autosave.poll(document(t,w),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved && w.snapshot()==state,"checkpoint mutation");
    const auto catalog=autosave.store().catalog();const auto& entry=catalog.histories.at(0).entries.front();
    check(entry.schema==16 && entry.profile==sim::city_v14_profile_name &&
        save::restore_save(save::read_save(entry.path),t.root/"data",mask()).snapshot()==state,"recovery schema16 mismatch");
}
void districts() {
    auto w=starter();advance(w,20000);put(w,sim::CommandType::PlaceWell,{8,1});
    put(w,sim::CommandType::PlaceClaySource,{40,2});put(w,sim::CommandType::PlacePottery,{40,5});
    put(w,sim::CommandType::PlaceWarehouse,{43,2});put(w,sim::CommandType::PlaceFarm,{44,5});
    put(w,sim::CommandType::PlaceMarket,{43,5});put(w,sim::CommandType::PlaceServicePost,{45,5});road(w,40,54);
    for (const auto cell:{sim::Cell{46,2},sim::Cell{46,5},sim::Cell{49,2},sim::Cell{49,5}})
        put(w,sim::CommandType::PlaceHousehold,cell);
    put(w,sim::CommandType::PlaceFireWatch,{54,5});
    const auto a=*w.building_owner_at({9,5}),b=*w.building_owner_at({49,5});
    advance(w,28000);
    check(w.household_desirability(a)==w.household_desirability(b) &&
        w.historical_household_level(a)==2 && w.historical_household_level(b)==2 &&
        w.household_level(a)==2 && w.household_level(b)==0 && w.household_service_active(a) &&
        w.household_service_active(b) && !w.building_on_fire(b),"equal supplied districts water difference");
    const auto aa=w.building(a),bb=w.building(b);advance(w,30000);
    check(w.building(a).fulfilled_demand-aa.fulfilled_demand==5 && w.building(b).fulfilled_demand-bb.fulfilled_demand==5 &&
        w.building(a).taxes_paid_total-aa.taxes_paid_total==300 && w.building(b).taxes_paid_total-bb.taxes_paid_total==125,
        "district taxes60/25 without identical supply");
    put(w,sim::CommandType::PlaceWell,{48,1});
    check(w.household_level(b)==2 && w.water_covered_households()==8,"second district not unlocked immediately");
    advance(w,34000);check(w.building(b).population==16,"second district did not grow");
    std::cout<<"two equally supplied desirable districts: wet60/dry25 actual taxes, second Well unlocks immediately\n";
}
void determinism() {
    auto a=starter(),b=starter();sim::BuildingId well{},industry{},remote{};bool relocated=false,burned=false;
    for (int i=0;i<20000;++i) {
        if (i==1200) {well=put(a,sim::CommandType::PlaceWell,{8,1});check(put(b,sim::CommandType::PlaceWell,{8,1})==well,"ID mismatch");}
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
        a.tick();b.tick();burned=burned || (remote!=sim::BuildingId{} && a.building_on_fire(remote));
        check(a.snapshot()==b.snapshot(),"20k snapshots diverged");
    }
    check(relocated && burned,"20k missing fire/relocation");invariants(a);
    std::cout<<"20,000 deterministic ticks, Well placement/removal/relocation, fire, quality, housing\n";
}
void endurance() {
    Temp t;auto w=starter();advance(w,1200);auto well=put(w,sim::CommandType::PlaceWell,{8,1});advance(w,40000);
    const std::array<std::tuple<sim::CommandType,sim::Object,unsigned>,9> types{{
        {sim::CommandType::PlaceClaySource,sim::Object::ClaySource,4},{sim::CommandType::PlacePottery,sim::Object::Pottery,4},
        {sim::CommandType::PlaceWarehouse,sim::Object::Warehouse,2},{sim::CommandType::PlaceFarm,sim::Object::Farm,2},
        {sim::CommandType::PlaceServicePost,sim::Object::ServicePost,2},{sim::CommandType::PlaceMarket,sim::Object::Market,4},
        {sim::CommandType::PlaceHousehold,sim::Object::Household,20},{sim::CommandType::PlaceFireWatch,sim::Object::FireWatch,2},
        {sim::CommandType::PlaceWell,sim::Object::Well,4}}};
    int row=0;sim::BuildingId remote{};
    for (const auto [type,object,limit]:types) {
        const auto count=static_cast<unsigned>(std::ranges::count_if(w.buildings(),[&](const auto& b){return b.kind==object;}));
        for (unsigned i=count;i<limit;++i) {
            const auto id=put(w,type,{35+3*static_cast<int>(i),row});
            if (object==sim::Object::Pottery && remote==sim::BuildingId{}) {remote=id;operation(w,id,false);}
        }
        row+=2;
    }
    check(w.buildings().size()==44 && w.couriers().size()==24,"full44/24 limits");
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    check(autosave.begin(document(t,w),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"endurance start");
    bool fire=false,replaced=false;unsigned trips=0,rebuilds=0;
    for (int i=0;i<100000;++i) {
        w.tick();if (i%100==0) invariants(w);
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
    check(fire && replaced && trips==20 && rebuilds==20 && w.taxes_collected_total()>0,"endurance coverage incomplete");
    invariants(w);std::cout<<"100,000 ticks,44 buildings/24 couriers,20 paid Well relocations,fire,industry,20 save/autosave/recovery roundtrips\n";
}
}
int main(int argc,char* argv[]) {
    try {
        const std::string mode=argc>1 ? argv[1]:"base";
        if (mode=="base") {geometry_and_infrastructure();starter_and_housing();}
        else if (mode=="persistence") persistence();else if (mode=="districts") districts();
        else if (mode=="determinism") determinism();else if (mode=="endurance") endurance();
        else throw std::invalid_argument("unknown mode");return 0;
    } catch (const std::exception& e) {std::cerr<<"City-v14: "<<e.what()<<'\n';return 1;}
}
