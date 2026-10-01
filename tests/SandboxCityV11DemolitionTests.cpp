#include "persistence/SandboxSave.h"
#include "simulation/World.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
namespace fs=std::filesystem;
namespace {
constexpr int width=110,height=16;
auto mask() { return std::vector<std::uint8_t>(width*height,1); }
void require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
sim::World fresh(unsigned version=4) { return {width,height,mask(),sim::RulesProfile::CityV11,version}; }
sim::BuildingId put(sim::World& w,sim::CommandType type,sim::Cell cell) {
    const auto result=w.execute({type,cell});
    require(result.accepted && result.changed,"place failed at "+std::to_string(cell.x)+","+std::to_string(cell.y)+": "+result.reason);
    return *w.building_owner_at(cell);
}
void invariants(const sim::World& w) {
    require(w.production_balance_valid() && w.food_balance_valid() && w.city_economy_valid() &&
        w.navigation_valid() && w.service_state_valid() && w.population_valid(),"v4 invariant failed");
    require(w.route_cache_entries()<=sim::Rules::city_v11_courier_limit*sim::Rules::city_v10_household_limit,
        "route cache leak");
}
void rejected(sim::World& w,sim::BuildingId id,const std::string& expected) {
    const auto before=w.snapshot(); const auto canonical=w.canonical_state();
    const auto status=w.demolition_status(id);
    const auto result=w.execute(sim::demolish_building(id));
    require(!status.allowed && !result.accepted && result.reason==status.reason &&
        result.reason.find(expected)!=std::string::npos,"unexpected blocker: "+result.reason);
    require(w.snapshot()==before && w.canonical_state()==canonical,"blocked demolition mutated state");
}
void remove(sim::World& w,sim::BuildingId id) {
    const auto b=w.building(id); const auto before=w.snapshot(); const auto refresh=w.route_refresh_count();
    const auto old_couriers=std::count_if(w.couriers().begin(),w.couriers().end(),[&](const auto& c) {return c.owner==id;});
    const auto result=w.execute(sim::demolish_building(id));
    require(result.accepted && result.changed,"demolition failed: "+result.reason);
    require(w.treasury()==before.treasury && w.construction_spent_total()==before.construction_spent_total &&
        w.next_building_id()==before.next_building_id && w.next_courier_id()==before.next_courier_id &&
        w.couriers().size()+static_cast<std::size_t>(old_couriers)==before.couriers.size() &&
        w.buildings().size()+1==before.buildings.size() && w.route_refresh_count()==refresh+1 &&
        w.road_revision()==before.road_revision+1,"demolition lifecycle/refund/refresh mismatch");
    for (const auto cell:sim::building_footprint_cells(w.profile(),b.kind,b.cell))
        require(w.object_at(cell)==sim::Object::Empty && !w.building_owner_at(cell),"footprint owner remained");
    for (const auto& c:w.couriers()) {
        require(c.owner!=id && c.target!=id && c.last_dispatched_target!=id &&
            c.last_dispatched_pottery!=id,"stale courier ID reference");
        for (const auto& entry:c.dynamic_target_routes) require(entry.first!=id,"stale target cache");
    }
    invariants(w);
}
void empty_types_and_limits() {
    const std::array<std::pair<sim::CommandType,unsigned>,7> types{{
        {sim::CommandType::PlaceClaySource,4},{sim::CommandType::PlacePottery,4},
        {sim::CommandType::PlaceWarehouse,2},{sim::CommandType::PlaceFarm,2},
        {sim::CommandType::PlaceServicePost,2},{sim::CommandType::PlaceMarket,4},
        {sim::CommandType::PlaceHousehold,20}}};
    for (const auto [type,limit]:types) {
        auto w=fresh();
        const auto id=put(w,type,{1,1});
        remove(w,id);
        auto restored=sim::World::restore(w.snapshot(),mask());
        const auto new_id=put(restored,type,{1,1});
        require(new_id>id,"Building ID recycled after restore");
        for (const auto& c:restored.couriers()) require(static_cast<std::uint32_t>(c.id)>=w.next_courier_id(),"Courier ID recycled");
        // Construction limits release occupancy and are based on active entities.
        // Fresh treasury limits are independent; funded_limits covers every role cap.
        unsigned built=1;
        for (;built<limit;++built) {
            const auto result=restored.execute({type,{static_cast<int>(built*3+1),5}});
            if (!result.accepted) { require(result.reason=="Not enough money","unexpected build limit"); break; }
        }
        if (built==limit) {
            require(!restored.validate({type,{1,9}}).accepted,"limit did not apply");
            remove(restored,new_id);
            put(restored,type,{1,1});
        }
        invariants(restored);
    }
    for (unsigned version=1;version<=3;++version) {
        auto w=fresh(version); const auto id=put(w,sim::CommandType::PlaceMarket,{1,1});
        rejected(w,id,"version 4");
    }
    auto w=fresh(); rejected(w,static_cast<sim::BuildingId>(900),"does not exist");
}
sim::World connected() {
    auto w=fresh();
    put(w,sim::CommandType::PlaceClaySource,{0,2});
    put(w,sim::CommandType::PlacePottery,{0,5});
    put(w,sim::CommandType::PlaceWarehouse,{3,2});
    put(w,sim::CommandType::PlaceFarm,{4,5});
    put(w,sim::CommandType::PlaceMarket,{3,5});
    put(w,sim::CommandType::PlaceServicePost,{5,5});
    for (int x=0;x<=14;++x) require(w.execute({sim::CommandType::PlaceRoad,{x,4}}).accepted,"road failed");
    for (const auto cell:{sim::Cell{6,2},sim::Cell{6,5},sim::Cell{9,2},sim::Cell{9,5}})
        put(w,sim::CommandType::PlaceHousehold,cell);
    return w;
}
void funded_limits() {
    auto w=connected();
    for (int tick=0;tick<20000;++tick) w.tick();
    const std::array<std::tuple<sim::CommandType,sim::Object,unsigned>,7> types{{
        {sim::CommandType::PlaceClaySource,sim::Object::ClaySource,4},
        {sim::CommandType::PlacePottery,sim::Object::Pottery,4},
        {sim::CommandType::PlaceWarehouse,sim::Object::Warehouse,2},
        {sim::CommandType::PlaceFarm,sim::Object::Farm,2},
        {sim::CommandType::PlaceServicePost,sim::Object::ServicePost,2},
        {sim::CommandType::PlaceMarket,sim::Object::Market,4},
        {sim::CommandType::PlaceHousehold,sim::Object::Household,20}}};
    int row=0;
    for (const auto [type,object,limit]:types) {
        const auto count=static_cast<unsigned>(std::count_if(w.buildings().begin(),w.buildings().end(),
            [&](const auto& b) {return b.kind==object;}));
        sim::BuildingId newest=static_cast<sim::BuildingId>(0);sim::Cell origin{};
        for (unsigned i=count;i<limit;++i) {
            origin={35+3*static_cast<int>(i),row};newest=put(w,type,origin);
        }
        require(!w.validate({type,{104,row}}).accepted,"role limit missing");
        remove(w,newest);
        const auto replacement=put(w,type,origin);
        require(replacement>newest,"limit rebuild reused ID");
        row+=2;
    }
    require(w.buildings().size()==38 && w.couriers().size()==22,"full limits differ after demolition/rebuild");
    invariants(w);
}
sim::BuildingId kind(const sim::World& w,sim::Object wanted) {
    for (const auto& b:w.buildings()) if (b.kind==wanted) return b.id;
    throw std::runtime_error("kind absent");
}
void toggle(sim::World& w,sim::BuildingId id,bool enabled) {
    require(w.execute(sim::set_building_operation(id,enabled)).accepted,"toggle failed");
}
template<class Predicate> void until(sim::World& w,Predicate done,int limit=3000) {
    for (int tick=0;tick<limit;++tick) { if (done()) return; w.tick(); invariants(w); }
    require(done(),"normal drain/arrival timed out");
}
void physical_safety_and_history() {
    auto w=connected();
    const auto clay=kind(w,sim::Object::ClaySource),pot=kind(w,sim::Object::Pottery),
        store=kind(w,sim::Object::Warehouse),farm=kind(w,sim::Object::Farm),
        market=kind(w,sim::Object::Market),post=kind(w,sim::Object::ServicePost),
        home=kind(w,sim::Object::Household);
    // Exactly one Pottery and Food, created and consumed by the normal simulation.
    toggle(w,farm,false);
    until(w,[&] { return w.couriers().front().phase==sim::CourierPhase::ToWarehouse; });
    rejected(w,clay,"owned courier"); rejected(w,pot,"active target");
    require(w.building(pot).reserved_incoming>0,"inbound reservation missing");
    toggle(w,clay,false);
    until(w,[&] { return w.couriers().front().phase==sim::CourierPhase::Returning; });
    rejected(w,clay,"owned courier");
    toggle(w,clay,true);
    until(w,[&] { return w.clay_extracted_total()==2; });
    toggle(w,clay,false);
    until(w,[&] { return w.building(pot).active_recipe_clay>0; });
    require(w.building(pot).progress==0,"recipe start did not preserve zero progress");
    toggle(w,pot,false);
    until(w,[&] { return w.demolition_status(pot).incoming_deliveries==0; });
    require(w.building(pot).progress==0,"paused active recipe gained progress");
    rejected(w,pot,"active recipe in progress");
    toggle(w,pot,true);
    toggle(w,store,false); // Allow inbound stock but prevent warehouse dispatch.
    until(w,[&] { return w.building(store).pottery_stock>0; });
    rejected(w,store,"Cannot demolish");
    require(!w.demolition_status(store).stored_goods_summary.empty(),"warehouse stock diagnostic absent");
    toggle(w,market,false); toggle(w,store,true);
    until(w,[&] { return w.building(market).pottery_stock>0; });
    rejected(w,market,"Cannot demolish");
    toggle(w,farm,true);
    until(w,[&] { return w.food_produced_total()==1; });
    toggle(w,farm,false);
    until(w,[&] { return w.building(market).food_stock>0; });
    rejected(w,market,"Cannot demolish");
    toggle(w,market,true);
    until(w,[&] { return w.building(home).pottery_stock && w.building(home).food_stock; });
    rejected(w,home,"Cannot demolish");
    until(w,[&] { return w.building(home).fulfilled_demand==1; });
    require(w.taxes_collected_total()==25,"no real tax from controlled production");
    toggle(w,post,false);
    until(w,[&] { return w.demolition_status(post).allowed && w.demolition_status(market).allowed &&
        w.demolition_status(clay).allowed && w.demolition_status(pot).allowed &&
        w.demolition_status(store).allowed && w.demolition_status(farm).allowed && w.demolition_status(home).allowed; });
    const auto expiry=w.building(home).service_until_tick;
    remove(w,post); require(w.building(home).service_until_tick==expiry,"service revoked retrospectively");
    require(w.household_service_active(home),"coverage should still be active");
    const auto population=w.total_population(),departing=w.building(home).population;
    remove(w,home); require(w.total_population()==population-departing && w.workforce_supply()==population-departing,
        "residents did not leave immediately");
    for (const auto id:{clay,pot,store,farm,market}) remove(w,id);
    const auto history=w.snapshot().demolition_history;
    require(history.clay_extracted==2 && history.pottery_completed==1 && history.food_produced==1 &&
        history.pottery_consumed==1 && history.food_consumed==1 && history.taxes==25,
        "retired historical production/consumption/taxes lost");
    auto restored=sim::World::restore(w.snapshot(),mask());
    require(restored.snapshot()==w.snapshot(),"post-drain restore differs");
    const auto new_home=put(restored,sim::CommandType::PlaceHousehold,{6,2});
    require(new_home>home && restored.building(new_home).population==6 &&
        restored.household_level(new_home)==0 && restored.building(new_home).placed_tick==restored.ticks(),"new house inherited old development");
    invariants(restored);
}
void isolated_stock_blockers() {
    // Stocks are produced by ordinary commands/ticks. Stop incoming production
    // and wait for return trips so each rejection tests inventory itself.
    auto clay_world=fresh();
    const auto clay=put(clay_world,sim::CommandType::PlaceClaySource,{0,2});
    put(clay_world,sim::CommandType::PlaceHousehold,{6,2});
    until(clay_world,[&] { return clay_world.building(clay).output>0; });
    rejected(clay_world,clay,"1 Clay");

    auto input_world=connected();
    const auto input_clay=kind(input_world,sim::Object::ClaySource);
    const auto input_pot=kind(input_world,sim::Object::Pottery);
    toggle(input_world,input_pot,false);
    until(input_world,[&] { return input_world.clay_extracted_total()==1; });
    toggle(input_world,input_clay,false);
    until(input_world,[&] { return input_world.building(input_pot).input_clay==1 &&
        input_world.demolition_status(input_pot).incoming_deliveries==0; });
    rejected(input_world,input_pot,"1 input Clay");

    auto output_world=connected();
    remove(output_world,kind(output_world,sim::Object::Warehouse));
    const auto output_pot=kind(output_world,sim::Object::Pottery);
    const auto output_clay=kind(output_world,sim::Object::ClaySource);
    until(output_world,[&] { return output_world.clay_extracted_total()==2; });
    toggle(output_world,output_clay,false);
    until(output_world,[&] { return output_world.building(output_pot).output==1 &&
        output_world.demolition_status(output_pot).incoming_deliveries==0; });
    rejected(output_world,output_pot,"1 Pottery");
    invariants(clay_world);invariants(input_world);invariants(output_world);
}
void stock_and_service_expiry() {
    auto w=connected();
    const auto farm=kind(w,sim::Object::Farm),clay=kind(w,sim::Object::ClaySource),
        pot=kind(w,sim::Object::Pottery),post=kind(w,sim::Object::ServicePost);
    toggle(w,clay,false); toggle(w,pot,false); toggle(w,farm,false);
    const auto market=kind(w,sim::Object::Market);
    toggle(w,market,false); toggle(w,farm,true);
    until(w,[&] { return w.building(farm).output>0; });
    // An active courier or stored Food are both real blockers.
    rejected(w,farm,w.demolition_status(farm).active_couriers ? "courier":"Food");
    toggle(w,farm,false);
    const auto home=kind(w,sim::Object::Household);
    until(w,[&] { return w.household_service_active(home); });
    toggle(w,post,false);
    until(w,[&] { return w.demolition_status(post).allowed; });
    const auto expiry=w.building(home).service_until_tick;
    remove(w,post);
    while (w.ticks()+1<expiry) w.tick();
    require(w.household_service_active(home),"coverage expired early");
    w.tick(); require(!w.household_service_active(home),"demolished service renewed coverage");
    invariants(w);
}
void population_and_districts() {
    auto w=connected();
    // A valid, synthetic restore fixture isolates a developed empty house. Every
    // field reconciles with historical production and real tax rates; no goods added.
    auto s=w.snapshot(); auto& home=*std::find_if(s.buildings.begin(),s.buildings.end(),[](const auto& b){return b.kind==sim::Object::Household;});
    home.population=12; home.fulfilled_demand=8; home.consumed_total=8; home.food_consumed_total=8;
    s.ticks=3200; home.last_demand_status=1;
    s.clay_extracted_total=16; s.pottery_completed_total=8; s.food_produced_total=8;
    for (auto& b:s.buildings) {
        if (b.kind==sim::Object::ClaySource) b.clay_extracted=16;
        if (b.kind==sim::Object::Pottery) b.recipes_completed=8;
        if (b.kind==sim::Object::Farm) b.food_produced=8;
    }
    // Demand tier constants, rather than assuming UI values.
    s.taxes_collected_total=sim::Rules::city_v7_level0_tax+3*sim::Rules::city_v7_level1_tax+4*sim::Rules::city_v7_level2_tax;
    s.treasury=1300+static_cast<std::int64_t>(s.taxes_collected_total)-static_cast<std::int64_t>(s.construction_spent_total);
    w=sim::World::restore(s,mask());
    const auto id=home.id; const auto pop=w.total_population();
    remove(w,id); require(w.total_population()==pop-12 && w.workforce_supply()==pop-12 &&
        w.workforce_used()<w.workforce_required(),"developed house demolition workforce incorrect");
    w.tick(); invariants(w);
    auto district=connected();
    until(district,[&] { return district.taxes_collected_total()>=50; });
    const auto isolated=put(district,sim::CommandType::PlaceMarket,{22,12});
    until(district,[&] { return district.couriers().front().phase!=sim::CourierPhase::IdleAtWorkshop; });
    const auto active=district.snapshot().couriers.front();
    const auto taxes_before=district.taxes_collected_total();
    remove(district,isolated);
    require(district.snapshot().couriers.front()==active,"other district active path changed");
    until(district,[&] { return district.taxes_collected_total()>taxes_before; });
}
void persistence_and_upgrade() {
    const auto root=fs::canonical(fs::temp_directory_path())/("oe-demolition-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"data/Cities");
    struct Cleanup { fs::path path; ~Cleanup(){ std::error_code e;fs::remove_all(path,e); } } cleanup{root};
    {std::ofstream out(root/"data/Cities/Synthetic.map");out<<"independent synthetic map identity";}
    auto v3=fresh(3);put(v3,sim::CommandType::PlaceMarket,{1,1});
    auto source=save::make_document(root/"data","Cities/Synthetic.map",mask(),v3);
    save::write_save(root/"source.json",source,root/"data",mask());
    const auto original=save::read_save(root/"source.json");
    auto copy=save::upgrade_city_v11_v3_to_v4(original);
    auto common=copy.world;common.rule_version=3;
    require(common==original.world,"upgrade changed existing authority");
    auto w=save::restore_save(copy,root/"data",mask());remove(w,w.buildings().front().id);
    save::write_save(root/"copy.json",save::make_document(root/"data","Cities/Synthetic.map",mask(),w),root/"data",mask());
    const auto read=save::read_save(root/"copy.json");
    require(read.source_schema_version==13 && read.world.rule_version==4 &&
        save::restore_save(read,root/"data",mask()).snapshot()==w.snapshot(),"schema13 roundtrip failed");
    require(save::read_save(root/"source.json").world==original.world && original.source_schema_version==12,"source v3 changed");
    auto bad=w.snapshot();bad.demolition_history.buildings=UINT64_MAX;
    bool failed=false;try {(void)sim::World::restore(bad,mask());}catch(const std::exception&){failed=true;}
    require(failed,"overflowing history accepted");
    bad=w.snapshot();bad.rule_version=3;failed=false;
    try {(void)sim::World::restore(bad,mask());}catch(const std::exception&){failed=true;}
    require(failed,"v3 accepted demolition authority");
}
void endurance(int ticks) {
    auto a=connected(),b=connected();
    int cycles=0;
    for (int tick=0;tick<ticks;++tick) {
        if (tick>0 && tick%5000==0) {
            for (auto* w:{&a,&b}) toggle(*w,kind(*w,sim::Object::ServicePost),false);
        }
        if (tick>0 && tick%5000<500 && !a.building(kind(a,sim::Object::ServicePost)).operating_enabled &&
            a.demolition_status(kind(a,sim::Object::ServicePost)).allowed) {
            for (auto* w:{&a,&b}) {
                const auto id=kind(*w,sim::Object::ServicePost);const auto cell=w->building(id).cell;
                remove(*w,id);put(*w,sim::CommandType::PlaceServicePost,cell);
            }
            ++cycles;
        }
        a.tick();b.tick();invariants(a);invariants(b);
        if (tick%137==0) require(a.snapshot()==b.snapshot(),"v4 nondeterminism");
        if (tick%10001==10000) b=sim::World::restore(b.snapshot(),mask());
    }
    require(cycles>=3 && a.taxes_collected_total()>0 && a.snapshot()==b.snapshot(),"endurance did not exercise rebuild cycles");
    std::cout<<ticks<<" deterministic ticks; "<<cycles<<" used Service Post drain/demolish/rebuild cycles\n";
}
}
int main(int argc,char** argv) {
    try {
        if (argc==2 && std::string(argv[1])=="--endurance") { endurance(100000);return 0; }
        empty_types_and_limits();funded_limits();physical_safety_and_history();isolated_stock_blockers();stock_and_service_expiry();
        population_and_districts();persistence_and_upgrade();endurance(20000);
        std::cout<<"City-v11-v4 demolition tests passed\n";return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
