#include "app/AutosaveController.h"
#include "core/PerformanceDiagnostics.h"
#include "simulation/World.h"
#include "simulation/CityStartGuidance.h"

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
sim::World fresh(sim::RulesProfile profile=sim::RulesProfile::CityV16) {
    return {width,height,mask(),profile};
}
sim::BuildingId put(sim::World& w,sim::CommandType type,sim::Cell cell) {
    const auto r=w.execute({type,cell});
    check(r.accepted && r.changed,"placement failed at "+std::to_string(w.ticks())+" funds "+std::to_string(w.treasury())+" type "+std::to_string(static_cast<int>(type))+": "+r.reason);
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
        w.fire_state_valid() && w.health_state_valid(),"City-v16 invariant at tick "+std::to_string(w.ticks()));
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
sim::World starter(sim::RulesProfile profile=sim::RulesProfile::CityV16) {
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
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-city16-"+
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
    check(doc.source_schema_version==18,"schema18 identity");
    auto restored=save::restore_save(doc,t.root/"data",mask());
    check(restored.snapshot()==before && w.snapshot()==before,"save/load mutation");
    for (const auto& b:w.buildings()) if (b.kind==sim::Object::Household)
        check(restored.household_has_water(b.id)==w.household_has_water(b.id) &&
            restored.nearest_water_source(b.id)==w.nearest_water_source(b.id) &&
            restored.household_level(b.id)==w.household_level(b.id),"derived water changed on load");
    auto control=sim::World::restore(before,mask());
    for (int i=0;i<500;++i) {restored.tick();control.tick();check(restored.snapshot()==control.snapshot(),"resume diverged");}
}
template<class F> void seed(sim::World& w,F edit) {
    auto s=w.snapshot();edit(s);w=sim::World::restore(s,mask());invariants(w);
}
const std::array<std::tuple<sim::CommandType,sim::Object,std::size_t>,10> types{{
    {sim::CommandType::PlaceClaySource,sim::Object::ClaySource,sim::Rules::city_v10_clay_source_limit},
    {sim::CommandType::PlacePottery,sim::Object::Pottery,sim::Rules::city_v10_pottery_limit},
    {sim::CommandType::PlaceWarehouse,sim::Object::Warehouse,sim::Rules::city_v10_warehouse_limit},
    {sim::CommandType::PlaceFarm,sim::Object::Farm,sim::Rules::city_v10_farm_limit},
    {sim::CommandType::PlaceServicePost,sim::Object::ServicePost,sim::Rules::city_v10_service_post_limit},
    {sim::CommandType::PlaceMarket,sim::Object::Market,sim::Rules::city_v11_market_limit},
    {sim::CommandType::PlaceHousehold,sim::Object::Household,sim::Rules::city_v10_household_limit},
    {sim::CommandType::PlaceFireWatch,sim::Object::FireWatch,sim::Rules::fire_watch_limit},
    {sim::CommandType::PlaceWell,sim::Object::Well,sim::Rules::well_limit},
    {sim::CommandType::PlaceHealthPost,sim::Object::HealthPost,sim::Rules::health_post_limit}}};
std::int64_t maximum_rate() {
    std::int64_t sum=0;
    for(const auto& [type,object,limit]:types) { (void)type;sum+=static_cast<std::int64_t>(limit)*sim::maintenance_cost(sim::RulesProfile::CityV16,object); }
    return sum;
}
void billing() {
    const std::array<std::int64_t,13> rates{0,0,0,4,8,12,0,8,4,8,4,2,4};
    for(std::size_t i=0;i<rates.size();++i) {
        const auto object=static_cast<sim::Object>(i);
        check(sim::maintenance_cost(sim::RulesProfile::CityV16,object)==rates[i],"explicit rate table");
        for(int p=0;p<static_cast<int>(sim::RulesProfile::CityV16);++p)
            check(sim::maintenance_cost(static_cast<sim::RulesProfile>(p),object)==0,"maintenance backport");
    }
    check(maximum_rate()==168,"derived max-city rate changed");
    auto w=fresh();advance(w,100);const auto a=put(w,sim::CommandType::PlaceWell,{10,10});
    check(w.maintenance_due_in(a)==400 && w.maintenance_spent_total()==0,"initial prorated bill");
    advance(w,499);check(w.maintenance_spent_total()==0 && w.maintenance_due_in(a)==1,"early bill");
    w.tick();check(w.maintenance_spent_total()==2 && w.maintenance_due_in(a)==0,"first placement-relative bill");
    advance(w,899);check(w.maintenance_spent_total()==2,"second bill early");
    w.tick();check(w.maintenance_spent_total()==4,"second bill missing");
    auto phases=fresh();const auto x=put(phases,sim::CommandType::PlaceWell,{10,10});
    advance(phases,200);put(phases,sim::CommandType::PlaceWell,{12,10});
    advance(phases,400);check(phases.maintenance_spent_total()==2,"phase A400");
    advance(phases,600);check(phases.maintenance_spent_total()==4,"phase B600");
    advance(phases,800);check(phases.maintenance_spent_total()==6,"phase A800");
    const auto paid=phases.maintenance_spent_total();
    check(phases.execute(sim::demolish_building(x)).accepted && phases.maintenance_spent_total()==paid,"demolition refund/history");
    advance(phases,1001);const auto replacement=put(phases,sim::CommandType::PlaceWell,{10,10});
    check(replacement!=x && phases.maintenance_due_in(x)==std::nullopt,"removed due/ID reused");
    advance(phases,1200);check(phases.maintenance_spent_total()==8,"removed building billed");
    advance(phases,1400);check(phases.maintenance_spent_total()==10,"remaining phase changed");
    phases.tick();check(phases.maintenance_spent_total()==12 && phases.maintenance_due_in(replacement)==0,"rebuild phase");
    auto owner=fresh();const auto pottery=put(owner,sim::CommandType::PlacePottery,{10,10});
    advance(owner,399);check(!owner.building_staffed(pottery),"unstaffed fixture");operation(owner,pottery,false);
    owner.tick();check(owner.maintenance_spent_total()==12,"pause/unstaffed exemption");operation(owner,pottery,true);
    advance(owner,799);seed(owner,[&](auto& s){auto& b=s.buildings[0];b.fire_risk=0;b.fire_until_tick=900;b.fire_protection_until_tick=0;});
    owner.tick();check(owner.maintenance_spent_total()==24 && owner.building_on_fire(pottery) && !owner.building_staffed(pottery),"fire exemption");
    const auto snapshot=owner.snapshot();openemperor::performance::set_enabled(true);openemperor::performance::reset();
    for(int i=0;i<1000;++i) {check(owner.current_maintenance_rate()==12 && owner.maintenance_due_in(pottery)==0,"pure query");}
    for(const auto c:{openemperor::performance::Counter::WorldCopies,openemperor::performance::Counter::WorldExecutes,
        openemperor::performance::Counter::WorldRestores,openemperor::performance::Counter::BfsCalls,
        openemperor::performance::Counter::RouteRefreshes,openemperor::performance::Counter::FileReads,
        openemperor::performance::Counter::FileWrites})check(openemperor::performance::counter(c)==0,"expensive maintenance query");
    openemperor::performance::set_enabled(false);check(owner.snapshot()==snapshot,"query mutation");
}
void starter_order_and_regression() {
    auto w=starter();auto legacy=starter(sim::RulesProfile::CityV15);
    check(w.treasury()==20 && w.construction_spent_total()==1280 && w.maintenance_spent_total()==0 &&
        w.current_maintenance_rate()==48 && w.workforce_supply()==24 && w.workforce_used()==24,"dry paid starter");
    advance(w,399);const auto funds=w.treasury();const auto taxes=w.taxes_collected_total();
    w.tick();const auto actual_tax=w.taxes_collected_total()-taxes;
    check(actual_tax>0 && w.maintenance_spent_total()==48 && w.treasury()==funds+static_cast<std::int64_t>(actual_tax)-48,"tax then full upkeep");
    std::cout<<"Starter tick400: tax "<<actual_tax<<", funds "<<funds<<" -> "<<w.treasury()<<", upkeep "<<w.maintenance_spent_total()<<'\n';
    advance(w,800);std::cout<<"Starter tick800: funds "<<w.treasury()<<", taxes "<<w.taxes_collected_total()<<", upkeep "<<w.maintenance_spent_total()<<'\n';
    advance(w,4000);advance(legacy,4000);
    auto a=w.snapshot(),b=legacy.snapshot();a.profile=b.profile;a.treasury=b.treasury;a.maintenance_spent_total=0;
    check(a==b && legacy.maintenance_spent_total()==0,"City15/fire/health/water/stages changed");
    auto oldwell=fresh(sim::RulesProfile::CityV14);put(oldwell,sim::CommandType::PlaceWell,{10,10});advance(oldwell,800);
    check(oldwell.treasury()==1240 && oldwell.maintenance_spent_total()==0 && oldwell.current_maintenance_rate()==0,"City14 maintenance backport");
    check(!w.maintenance_due_in(kind(w,sim::Object::Household)) && !w.maintenance_due_in(static_cast<sim::BuildingId>(999)),"House/missing due");
}
sim::World healthy_starter() {
    auto w=starter();until(w,[&]{return w.treasury()>=60;});const auto wt=w.ticks();
    put(w,sim::CommandType::PlaceWell,{8,1});
    until(w,[&]{return w.treasury()>=120 && w.workforce_supply()>=26;},6000);
    const auto ht=w.ticks();const auto post=put(w,sim::CommandType::PlaceHealthPost,{12,5});
    operation(w,post,true);w.execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));
    std::cout<<"Paid first Well at "<<wt<<", HealthPost at "<<ht<<", funds "<<w.treasury()<<'\n';return w;
}
void paid_debt_commands() {
    auto w=fresh();for(int x:{0,3,6,9})put(w,sim::CommandType::PlaceClaySource,{x,0});
    for(int x:{0,3})put(w,sim::CommandType::PlacePottery,{x,3});
    put(w,sim::CommandType::PlaceWarehouse,{6,3});put(w,sim::CommandType::PlaceFarm,{9,3});
    const auto post=put(w,sim::CommandType::PlaceServicePost,{10,3});road(w,14,14);
    advance(w,400);check(w.treasury()==-24 && w.maintenance_spent_total()==72,"full bill did not create debt");
    auto one=sim::World::restore(w.snapshot(),mask());
    seed(one,[](auto& s){s.maintenance_spent_total-=23;s.treasury=-1;});
    const auto one_before=one.snapshot();
    check(!one.execute({sim::CommandType::PlaceRoad,{14,5}}).accepted &&
        !one.execute({sim::CommandType::PlaceHousehold,{20,5}}).accepted && one.snapshot()==one_before,"Funds -1 paid rejection");
    operation(one,post,false);check(one.execute(sim::demolish_building(post)).accepted,"Funds -1 free pause/demolition");
    const auto before=w.snapshot();check(!w.execute({sim::CommandType::PlaceRoad,{14,5}}).accepted &&
        !w.execute({sim::CommandType::PlaceHousehold,{20,5}}).accepted && w.snapshot()==before,"paid construction in debt");
    const auto noop=w.execute({sim::CommandType::PlaceRoad,{14,4}});
    check(noop.accepted && !noop.changed && w.treasury()==before.treasury && w.construction_spent_total()==before.construction_spent_total,"existing road no-op in debt");
    const std::array<sim::Cell,1> existing{{{14,4}}};check(w.validate_road_batch(existing).accepted,"existing batch in debt");
    const auto existing_before=w.snapshot();const auto batch=w.execute_road_batch(existing);
    check(batch.accepted && !batch.changed && w.snapshot()==existing_before,"all-existing debt batch changed state");
    operation(w,post,false);operation(w,post,true);w.execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));
    check(w.execute(sim::demolish_building(post)).accepted && w.execute({sim::CommandType::RemoveRoad,{14,4}}).accepted,"free controls/demolition/removal in debt");
    advance(w,800);check(w.maintenance_spent_total()==140,"demolished future costs");
}
void balance() {
    auto efficient=healthy_starter();advance(efficient,16000);const auto start=efficient.treasury();advance(efficient,24000);
    check(efficient.treasury()>start,"compact healthy city not sustainable");
    std::cout<<"Compact healthy: funds "<<start<<" at16000 -> "<<efficient.treasury()<<" at24000; installed "<<efficient.current_maintenance_rate()<<"/400t\n";
    auto over=sim::World::restore(efficient.snapshot(),mask());std::vector<sim::BuildingId> duplicates;
    int row=0;
    for(const auto& [type,object,limit]:types) {
        if(object==sim::Object::Household || object==sim::Object::Well || object==sim::Object::HealthPost)continue;
        const auto count=static_cast<std::size_t>(std::ranges::count_if(over.buildings(),[&](const auto& b){return b.kind==object;}));
        for(auto i=count;i<limit;++i) {const auto id=put(over,type,{60+3*static_cast<int>(i),row});duplicates.push_back(id);operation(over,id,false);}
        row+=3;
    }
    const auto baseline=efficient.treasury(),over_start=over.treasury();advance(efficient,28000);advance(over,28000);
    check(over.treasury()-over_start<efficient.treasury()-baseline,"overbuilding had no budget effect");
    std::cout<<"Same4 houses: 4000t net efficient "<<efficient.treasury()-baseline<<", duplicated "<<over.treasury()-over_start<<"; rates "<<efficient.current_maintenance_rate()<<" vs "<<over.current_maintenance_rate()<<'\n';
    // Spend genuine earned funds on paid roads, then remove Service temporarily.
    // No treasury, inventory, tax or population injection in debt acceptance.
    for(int y=0;y<height && over.treasury()>=2;++y)for(int x=80;x<width && over.treasury()>=2;++x)
        check(over.execute({sim::CommandType::PlaceRoad,{x,y}}).accepted,"paid reserve spend");
    const auto service=kind(over,sim::Object::ServicePost);operation(over,service,false);
    until(over,[&]{return over.treasury()<0;},6000);const auto debt=over.treasury();const auto tax=over.taxes_collected_total();
    for(const auto id:duplicates) {
        while(!over.demolition_status(id).allowed && over.building_on_fire(id))over.tick();
        check(over.execute(sim::demolish_building(id)).accepted,"empty overbuilding demolition");
    }
    const auto reduced=over.current_maintenance_rate();operation(over,service,true);
    until(over,[&]{return over.treasury()>=60;},12000);
    check(over.taxes_collected_total()>tax && reduced==efficient.current_maintenance_rate(),"real demand failed to repay debt");
    check(over.execute({sim::CommandType::PlaceRoad,{25,10}}).accepted,"paid construction after tax recovery");
    auto larger=sim::World::restore(efficient.snapshot(),mask());
    road(larger,15,17);road(larger,0,17,7);road(larger,2,2,5);road(larger,2,2,6);
    put(larger,sim::CommandType::PlaceClaySource,{0,8});put(larger,sim::CommandType::PlacePottery,{3,8});
    put(larger,sim::CommandType::PlaceWell,{13,6});
    for(const auto cell:{sim::Cell{12,2},sim::Cell{15,2},sim::Cell{12,8},sim::Cell{15,8}})put(larger,sim::CommandType::PlaceHousehold,cell);
    put(larger,sim::CommandType::PlaceServicePost,{15,6});put(larger,sim::CommandType::PlaceFireWatch,{17,8});
    advance(larger,38000);const auto large_before=larger.treasury();advance(larger,42000);
    check(larger.treasury()>large_before && larger.taxes_collected_total()>efficient.taxes_collected_total(),"larger supplied city not profitable");
    std::cout<<"Larger8-house city: 4000t net "<<larger.treasury()-large_before<<"; rate "<<larger.current_maintenance_rate()<<"/400t\n";
    std::cout<<"Overbuilt debt "<<debt<<"; demolition rate reduced to "<<reduced<<"/400t; real tax recovery funds "<<over.treasury()<<'\n';
}
void persistence() {
    Temp t;auto w=fresh();put(w,sim::CommandType::PlaceWell,{10,10});advance(w,399);
    roundtrip(t,w);openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    check(autosave.begin(document(t,w),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"boundary checkpoint");
    const auto catalog=autosave.store().catalog();auto doc=save::read_save(catalog.histories.at(0).entries.front().path);
    auto recovered=save::restore_save(doc,t.root/"data",mask());sim::TickDriver driver;driver.pause_and_reset();
    check(driver.paused() && recovered.snapshot()==w.snapshot(),"recovery not exact/paused");driver.step_once(recovered);w.tick();check(recovered.snapshot()==w.snapshot(),"checkpoint billing boundary");
    seed(w,[](auto& s){s.maintenance_spent_total=1315;s.treasury=-75;});roundtrip(t,w);
    check(autosave.poll(document(t,w),mask(),true).kind==openemperor::AutosaveResult::Kind::None,"early checkpoint");
    advance(w,1599);check(autosave.poll(document(t,w),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved,"negative checkpoint");
    auto entries=autosave.store().catalog().histories.at(0).entries;doc=save::read_save(entries.front().path);
    check(save::restore_save(doc,t.root/"data",mask()).snapshot()==w.snapshot() && w.treasury()<0,"recovery lost debt");
    const auto negative=t.root/"negative.json";save::write_save(negative,document(t,w),t.root/"data",mask());
    nlohmann::json j;{std::ifstream in(negative);in>>j;}
    const auto bad=t.root/"bad.json";
    const auto reject_json=[&](auto edit) {auto copy=j;edit(copy);std::ofstream(bad)<<copy.dump();rejects([&]{auto d=save::read_save(bad);save::restore_save(d,t.root/"data",mask());},"bad schema18 accepted");};
    reject_json([](auto& x){x["world"]["maintenance_spent_total"]=0;});
    reject_json([](auto& x){x["world"]["maintenance_spent_total"]=-1;});
    reject_json([](auto& x){x["world"]["maintenance_spent_total"]=1.5;});
    reject_json([](auto& x){x["world"].erase("maintenance_spent_total");});
    reject_json([](auto& x){x["world"]["treasury"]=UINT64_MAX;});
    reject_json([](auto& x){x["world"]["treasury"]=-9.2e18;});
    reject_json([](auto& x){x["world"]["treasury"]=true;});
    reject_json([](auto& x){x["schema_version"]=17;});
    reject_json([](auto& x){x["rules"]["id"]="sandbox-city-v15";});
    for(const auto profile:{sim::RulesProfile::CityV15,sim::RulesProfile::CityV14,sim::RulesProfile::CityV13,sim::RulesProfile::CityV12,sim::RulesProfile::CityV11,sim::RulesProfile::CityV10,sim::RulesProfile::CityV9,sim::RulesProfile::CityV8,sim::RulesProfile::CityV7,sim::RulesProfile::CityV6}) {
        auto legacy=fresh(profile);save::write_save(bad,document(t,legacy),t.root/"data",mask());nlohmann::json old;{std::ifstream in(bad);in>>old;}
        check(!old["world"].contains("maintenance_spent_total"),"legacy schema changed");
        old["world"]["treasury"]=-1;std::ofstream(bad)<<old.dump();rejects([&]{save::read_save(bad);},"legacy negative treasury accepted");
        auto snapshot=legacy.snapshot();snapshot.maintenance_spent_total=1;rejects([&]{sim::World::restore(snapshot,mask());},"legacy maintenance authority accepted");
    }
    std::cout<<"Schema18 signed debt, exact continuations, boundary/negative recovery and legacy negative rejection\n";
}
void overflow() {
    // Valid synthetic accounting extremes, never used for gameplay/balance.
    auto w=fresh();put(w,sim::CommandType::PlaceWell,{10,10});advance(w,399);
    seed(w,[](auto& s){s.treasury=INT64_MIN;s.maintenance_spent_total=static_cast<std::uint64_t>(INT64_MAX)+1+1240;});
    check(sim::inspect_city_start(w).suggested_house_funds_missing==INT64_MAX,"minimum-debt guidance overflow");
    const auto minimum=w.snapshot();rejects([&]{w.tick();},"INT64_MIN subtraction wrapped");check(w.snapshot()==minimum,"overflow published partial tick");
    Temp t;save::write_save(t.root/"min.json",document(t,w),t.root/"data",mask());
    check(save::restore_save(save::read_save(t.root/"min.json"),t.root/"data",mask()).snapshot()==minimum,"signed minimum parse");
    auto max=fresh();const auto h=put(max,sim::CommandType::PlaceHousehold,{10,10});check(max.execute(sim::demolish_building(h)).accepted,"extreme history fixture");
    const auto well=put(max,sim::CommandType::PlaceWell,{12,10});advance(max,399);
    seed(max,[](auto& s){s.demolition_history.taxes=UINT64_MAX-5;s.taxes_collected_total=UINT64_MAX-5;
        s.maintenance_spent_total=UINT64_MAX-1;s.treasury=1156;});
    const auto upper=max.snapshot();rejects([&]{max.tick();},"maintenance uint64 wrap");check(max.snapshot()==upper,"counter overflow published tick");
    check(max.maintenance_due_in(well)==1,"near-limit query");
    save::write_save(t.root/"max.json",document(t,max),t.root/"data",mask());
    check(save::restore_save(save::read_save(t.root/"max.json"),t.root/"data",mask()).snapshot()==upper,"uint64 cancellation parse");
    auto tax=starter();advance(tax,800);const auto removed=put(tax,sim::CommandType::PlaceHousehold,{25,10});
    check(tax.execute(sim::demolish_building(removed)).accepted,"tax overflow history fixture");advance(tax,1199);
    seed(tax,[](auto& s){const auto credit=static_cast<std::uint64_t>(INT64_MAX-50-s.treasury);
        s.treasury=INT64_MAX-50;s.demolition_history.taxes=credit;s.taxes_collected_total+=credit;});
    const auto tax_before=tax.snapshot();rejects([&]{tax.tick();},"aggregate signed tax overflow accepted");
    check(tax.snapshot()==tax_before,"tax overflow published production/demand/counter mutation");
    // At the exact signed lower boundary a representable full charge succeeds.
    seed(w,[](auto& s){s.treasury=INT64_MIN+2;s.maintenance_spent_total-=2;});w.tick();check(w.treasury()==INT64_MIN,"checked lower boundary charge");
}
void determinism() {
    auto a=starter(),b=starter();sim::BuildingId well{},post{},extra{};
    bool debt=false,repaid=false,burned=false,sick=false,treated=false;
    for(int i=0;i<20000;++i) {
        const auto old_well=well,old_extra=extra;
        for(auto* w:{&a,&b}) {
            if(i==800)well=put(*w,sim::CommandType::PlaceWell,{8,1});
            if(i==1200){post=put(*w,sim::CommandType::PlaceHealthPost,{12,5});w->execute(sim::set_building_workforce_priority(post,sim::WorkforcePriority::High));}
            if(i==4001){extra=put(*w,sim::CommandType::PlacePottery,{65,10});operation(*w,extra,false);
                for(int x:{40,43,46,49})put(*w,sim::CommandType::PlaceHousehold,{x,10});}
            if(i==5001) {
                for(int y=0;y<height && w->treasury()>=2;++y)for(int x=80;x<width && w->treasury()>=2;++x)
                    check(w->execute({sim::CommandType::PlaceRoad,{x,y}}).accepted,"20k paid reserve spend");
                operation(*w,kind(*w,sim::Object::ServicePost),false);
            }
            if(i==8501)operation(*w,kind(*w,sim::Object::ServicePost),true);
            if(i==6501)operation(*w,post,false);
            if(i==14501)operation(*w,post,true);
            if(i==9001 && w->demolition_status(old_extra).allowed){check(w->execute(sim::demolish_building(old_extra)).accepted,"20k demolition");extra=put(*w,sim::CommandType::PlacePottery,{68,10});operation(*w,extra,false);}
            if(i==12001){check(w->execute(sim::demolish_building(old_well)).accepted,"20k Well demolition");well=put(*w,sim::CommandType::PlaceWell,{8,2});}
        }
        a.tick();b.tick();check(a.snapshot()==b.snapshot(),"20k snapshots diverged");
        debt=debt || a.treasury()<0;if(debt && a.treasury()>0)repaid=true;
        burned=burned || (extra!=sim::BuildingId{} && a.building_on_fire(extra));
        for(const auto& h:a.buildings()) {sick=sick || a.household_sick(h.id);treated=treated || a.household_health_protected(h.id);}
        if(i%100==0)invariants(a);
    }
    // The separate balance acceptance exercises deeper debt; the two worlds
    // here deliberately stop Service early enough to include debt and recovery.
    check(debt && repaid && burned && sick && treated,"20k scenario coverage");
    std::cout<<"20,000 deterministic ticks, phased paid buildings, debt/recovery, Fire/Health/Water and demolition\n";
}
void endurance() {
    Temp t;auto w=healthy_starter();advance(w,16000);sim::BuildingId remote{};
    int row=0;
    for(const auto& [type,object,limit]:types) {
        const auto count=static_cast<std::size_t>(std::ranges::count_if(w.buildings(),[&](const auto& b){return b.kind==object;}));
        for(auto i=count;i<limit;++i) {
            // Deliberately stagger individual ownership clocks.
            advance(w,w.ticks()+7);const auto id=put(w,type,{35+3*static_cast<int>(i),row});
            if(sim::World::operation_controllable(object)){operation(w,id,false);if(object==sim::Object::Pottery && remote==sim::BuildingId{})remote=id;}
        }
        row+=3;
    }
    check(w.buildings().size()==46 && w.couriers().size()==26 && w.current_maintenance_rate()==maximum_rate(),"full46/26 derived rate");
    // Ordinary paid construction spends the remaining reserve without a grant.
    for(int y=0;y<height && w.treasury()>=2;++y)for(int x=80;x<width && w.treasury()>=2;++x)
        check(w.execute({sim::CommandType::PlaceRoad,{x,y}}).accepted,"endurance paid roads");
    check(w.treasury()<2,"endurance reserve not spent");
    const auto service=kind(w,sim::Object::ServicePost),health=kind(w,sim::Object::HealthPost);
    operation(w,service,false);
    openemperor::AutosaveController autosave(t.root/"prefs",t.root/"data",true);
    check(autosave.begin(document(t,w),mask()).kind==openemperor::AutosaveResult::Kind::Saved,"endurance start");
    const auto start_tick=w.ticks();bool debt=false,repaid=false,fire=false,sick=false,treated=false,replaced=false;
    unsigned checkpoints=0;std::uint64_t demolished_payments=0;
    for(int i=0;i<100000;++i) {
        if(i==3000)operation(w,service,true);
        if(i%16000==5000)operation(w,health,false);
        if(i%16000==8500)operation(w,health,true);
        w.tick();debt=debt || w.treasury()<0;if(debt && w.treasury()>0)repaid=true;
        for(const auto& h:w.buildings()){fire=fire || w.building_on_fire(h.id);sick=sick || w.household_sick(h.id);treated=treated || w.household_health_protected(h.id);}
        if(!replaced && i>3000 && w.treasury()>=180 && w.demolition_status(remote).allowed) {
            const auto& building=w.building(remote);
            demolished_payments+=(w.ticks()-building.placed_tick)/sim::Rules::maintenance_interval_ticks*
                static_cast<std::uint64_t>(sim::maintenance_cost(w.profile(),building.kind));
            check(w.execute(sim::demolish_building(remote)).accepted,"endurance demolition");
            remote=put(w,sim::CommandType::PlacePottery,{70,10});operation(w,remote,false);replaced=true;
        }
        if(i%100==0) {
            invariants(w);std::uint64_t expected=demolished_payments;
            for(const auto& building:w.buildings())expected+=(w.ticks()-building.placed_tick)/sim::Rules::maintenance_interval_ticks*
                static_cast<std::uint64_t>(sim::maintenance_cost(w.profile(),building.kind));
            check(expected==w.maintenance_spent_total(),"lifetime placement exposure disagrees with billing");
        }
        if(i%5000==4999) {
            const auto before=w.snapshot();roundtrip(t,w);
            check(autosave.poll(document(t,w),mask(),true).kind==openemperor::AutosaveResult::Kind::Saved && w.snapshot()==before,"endurance autosave mutation");
            const auto entries=autosave.store().catalog().histories.at(0).entries;
            check(save::restore_save(save::read_save(entries.front().path),t.root/"data",mask()).snapshot()==before,"endurance debt recovery snapshot");++checkpoints;
        }
    }
    check(w.ticks()==start_tick+100000 && debt && repaid && fire && sick && treated && replaced && checkpoints==20,"100k scenario coverage");
    std::cout<<"100,000 ticks,46 buildings/26 couriers,168/400t, phased bills,debt/recovery,demolition/rebuild,Fire/Health/Water,20 save/autosave/recovery roundtrips; funds "<<w.treasury()<<", upkeep "<<w.maintenance_spent_total()<<'\n';
}
} // namespace
int main(int argc,char* argv[]) {
    try {const std::string mode=argc>1 ? argv[1]:"base";
        if(mode=="base"){billing();starter_order_and_regression();paid_debt_commands();overflow();}
        else if(mode=="persistence")persistence();else if(mode=="balance")balance();
        else if(mode=="determinism")determinism();else if(mode=="endurance")endurance();
        else throw std::invalid_argument("unknown mode");return 0;
    }catch(const std::exception& e){std::cerr<<"City-v16: "<<e.what()<<'\n';return 1;}
}
