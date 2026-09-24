#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
constexpr int width=24,height=10;

void check(bool value,const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
std::vector<std::uint8_t> mask() { return std::vector<std::uint8_t>(width*height,1); }
void put(sim::World& world,sim::CommandType type,int x,int y) {
    const auto result=world.execute({type,{x,y}});
    check(result.accepted,"command rejected at "+std::to_string(x)+","+
        std::to_string(y)+": "+result.reason);
}
void road(sim::World& world,int x,int y) { put(world,sim::CommandType::PlaceRoad,x,y); }

std::uint64_t tax_total(std::uint64_t fulfilled) {
    if (!fulfilled) return 0;
    return 25U+std::min<std::uint64_t>(fulfilled-1,3)*40U+
        (fulfilled>4 ? fulfilled-4:0)*60U;
}

sim::World demand_world(std::uint64_t fulfilled,int population,bool pottery,bool food,bool service) {
    sim::World base(width,height,mask(),sim::RulesProfile::CityV9);
    put(base,sim::CommandType::PlaceClaySource,0,0);
    put(base,sim::CommandType::PlacePottery,1,0);
    put(base,sim::CommandType::PlaceHousehold,2,0);
    put(base,sim::CommandType::PlaceFarm,3,0);
    put(base,sim::CommandType::PlaceServicePost,4,0);
    auto s=base.snapshot();
    s.ticks=(fulfilled+1)*sim::Rules::household_demand_ticks-1;
    auto& clay=s.buildings[0];
    auto& pots=s.buildings[1];
    auto& home=s.buildings[3];
    auto& farm=s.buildings[9];
    home.demand_progress=sim::Rules::household_demand_ticks-1;
    home.fulfilled_demand=fulfilled;
    home.consumed_total=fulfilled;
    home.last_demand_status=fulfilled ? 1:0;
    home.population=population;
    home.pottery_stock=pottery ? 1:0;
    home.food_stock=food ? 1:0;
    home.food_consumed_total=fulfilled;
    if (service) home.service_until_tick=s.ticks+200;
    const auto pottery_total=fulfilled+static_cast<std::uint64_t>(pottery);
    pots.recipes_completed=pottery_total;
    s.pottery_completed_total=pottery_total;
    clay.clay_extracted=2*pottery_total;
    s.clay_extracted_total=2*pottery_total;
    farm.food_produced=fulfilled+static_cast<std::uint64_t>(food);
    s.food_produced_total=farm.food_produced;
    s.taxes_collected_total=tax_total(fulfilled);
    s.treasury=1000-static_cast<std::int64_t>(s.construction_spent_total)+
        static_cast<std::int64_t>(s.taxes_collected_total);
    auto world=sim::World::restore(s,mask());
    check(world.population_valid() && world.production_balance_valid() &&
          world.food_balance_valid() && world.service_state_valid() &&
          world.city_economy_valid(),"constructed demand world invalid");
    return world;
}

sim::World starter() {
    sim::World world(width,height,mask(),sim::RulesProfile::CityV9);
    put(world,sim::CommandType::PlaceClaySource,0,2);
    road(world,1,2); road(world,2,2);
    put(world,sim::CommandType::PlacePottery,3,2);
    road(world,4,2); road(world,5,2);
    put(world,sim::CommandType::PlaceWarehouse,6,2);
    road(world,7,2); road(world,8,2); road(world,9,2);
    put(world,sim::CommandType::PlaceHousehold,10,2);
    road(world,7,1); road(world,8,1);
    put(world,sim::CommandType::PlaceHousehold,9,1);
    put(world,sim::CommandType::PlaceFarm,6,4);
    road(world,7,4); road(world,8,4); road(world,8,3); road(world,9,3);
    put(world,sim::CommandType::PlaceHousehold,10,3);
    road(world,9,4); road(world,10,4);
    put(world,sim::CommandType::PlaceServicePost,11,4);
    check(world.construction_spent_total()==980 && world.treasury()==20,
          "City v9 starter cost differs");
    check(world.total_population()==18 && world.workforce_supply()==18 &&
          world.workforce_required()==18 && world.workforce_used()==18,
          "City v9 starter is not exactly staffed at 18/18");
    return world;
}

struct SaveFixture {
    fs::path root;
    fs::path relative="Cities/Synthetic.map";
    fs::path target;
    SaveFixture() {
        root=fs::canonical(fs::temp_directory_path())/
            ("openemperor-city-v9-"+std::to_string(std::random_device{}()));
        fs::create_directories(root/"data/Cities"); fs::create_directories(root/"saves");
        std::ofstream(root/"data"/relative,std::ios::binary)<<"independently authored city v9 map";
        target=root/"saves/city-v9.json";
    }
    ~SaveFixture() { std::error_code error; fs::remove_all(root,error); }
    sim::World roundtrip(const sim::World& world) const {
        save::write_save(target,save::make_document(root/"data",relative,mask(),world),
                         root/"data",mask());
        std::ifstream input(target);
        const auto json=nlohmann::json::parse(input);
        check(json.at("schema_version")==9 &&
              json.at("world").at("buildings").size()==sim::max_buildings &&
              json.at("world").at("couriers").size()==sim::max_couriers,
              "City v9 save structure differs");
        for (const auto& building:json.at("world").at("buildings"))
            check(building.contains("population"),"schema 9 omitted population field");
        const auto document=save::read_save(target);
        check(document.source_schema_version==9,"City v9 save did not use schema 9");
        auto restored=save::restore_save(document,root/"data",mask());
        check(restored.snapshot()==world.snapshot(),"City v9 save roundtrip changed state");
        return restored;
    }
};

void check_profiles_and_schema() {
    auto world=starter();
    for (unsigned id=4;id<7;++id) {
        const auto key=static_cast<sim::BuildingId>(id);
        check(world.household_population(key)==6 &&
              world.household_population_capacity(key)==6,
              "fresh City v9 House population differs");
    }
    check(world.population_valid() && world.total_population_capacity()==18,
          "fresh City v9 population invariant failed");
    SaveFixture saves;
    world=saves.roundtrip(world);
    std::ifstream valid_input(saves.target);
    const auto valid_json=nlohmann::json::parse(valid_input);
    valid_input.close();
    const auto reject=[&](nlohmann::json json,const std::string& label) {
        std::ofstream output(saves.target,std::ios::binary|std::ios::trunc);
        output<<json.dump(); output.close();
        bool rejected=false;
        try { (void)save::read_save(saves.target); }
        catch (const std::exception&) { rejected=true; }
        check(rejected,"schema 9 accepted "+label);
    };
    auto invalid=valid_json;
    invalid["world"]["buildings"][3]["population"]=1;
    reject(invalid,"House population below minimum");
    invalid=valid_json;
    invalid["world"]["buildings"][0]["population"]=1;
    reject(invalid,"non-House population");
    invalid=valid_json;
    invalid["world"]["buildings"][3]["population"]=7;
    reject(invalid,"House population above derived Level-0 capacity");
    invalid=valid_json;
    invalid["world"]["buildings"][3].erase("population");
    reject(invalid,"missing population field");

    sim::World v8(width,height,mask(),sim::RulesProfile::CityV8);
    put(v8,sim::CommandType::PlaceHousehold,1,1);
    check(v8.workforce_supply()==8 && v8.total_population()==0 && v8.population_valid(),
          "City v8 gained dynamic population");
    save::write_save(saves.target,
        save::make_document(saves.root/"data",saves.relative,mask(),v8),saves.root/"data",mask());
    std::ifstream input(saves.target);
    const auto json=nlohmann::json::parse(input);
    check(json.at("schema_version")==8 &&
          !json.at("world").at("buildings").at(3).contains("population"),
          "schema 8 gained population bytes");
}

void check_growth_decline_and_tick_staffing() {
    SaveFixture saves;
    auto growth=demand_world(1,6,true,true,true);
    growth.tick();
    const auto& grown=growth.building(sim::BuildingId::Household);
    check(grown.fulfilled_demand==2 && growth.household_level(grown.id)==1 &&
          grown.population==7 && growth.household_population_capacity(grown.id)==10,
          "second fulfillment did not grow Level-1 House to population 7");
    growth=saves.roundtrip(growth);

    for (std::uint64_t fulfilled=2;fulfilled<11;++fulfilled) {
        const int before=fulfilled<5 ? static_cast<int>(fulfilled)+5:
            std::min(15,static_cast<int>(fulfilled)+5);
        auto step=demand_world(fulfilled,before,true,true,true);
        step.tick();
        const int expected=std::min(sim::Rules::household_level2_capacity,before+1);
        check(step.household_population(sim::BuildingId::Household)==expected,
              "successful demand growth step differs");
    }
    auto capped=demand_world(11,16,true,true,true);
    capped.tick();
    check(capped.household_population(sim::BuildingId::Household)==16,
          "Level-2 population exceeded capacity 16");

    auto decline=demand_world(5,10,false,false,false);
    decline.tick();
    check(decline.household_population(sim::BuildingId::Household)==9,
          "miss did not reduce population by one");
    decline=saves.roundtrip(decline);
    for (int cycle=0;cycle<8;++cycle)
        for (int tick=0;tick<sim::Rules::household_demand_ticks;++tick) decline.tick();
    check(decline.household_population(sim::BuildingId::Household)==2,
          "repeated misses did not stop at population 2");

    auto stock_miss=demand_world(5,10,true,true,false);
    stock_miss.tick();
    const auto& missed=stock_miss.building(sim::BuildingId::Household);
    check(missed.population==9 && missed.pottery_stock==1 && missed.food_stock==1 &&
          missed.missed_demand==1,"Service miss consumed goods or failed to reduce population");

    auto recipe=demand_world(5,10,false,false,false);
    auto state=recipe.snapshot();
    state.buildings[1].active_recipe_clay=sim::Rules::pottery_recipe_clay;
    state.buildings[1].progress=20;
    state.buildings[0].clay_extracted+=sim::Rules::pottery_recipe_clay;
    state.clay_extracted_total+=sim::Rules::pottery_recipe_clay;
    recipe=sim::World::restore(state,mask());
    recipe.tick();
    check(recipe.building(sim::BuildingId::Pottery).progress==21 &&
          recipe.workforce_supply()==9 && !recipe.building_staffed(sim::BuildingId::Pottery),
          "demand-tick staffing snapshot was not held through production");
    recipe.tick();
    check(recipe.building(sim::BuildingId::Pottery).progress==21,
          "unstaffed active recipe did not freeze on following tick");
    auto recovery=recipe.snapshot();
    auto& recovery_home=recovery.buildings[3];
    recovery_home.pottery_stock=1;
    recovery_home.food_stock=1;
    recovery_home.service_until_tick=recovery.ticks+sim::Rules::service_coverage_ticks;
    ++recovery.pottery_completed_total;
    ++recovery.buildings[1].recipes_completed;
    recovery.clay_extracted_total+=sim::Rules::pottery_recipe_clay;
    recovery.buildings[0].clay_extracted+=sim::Rules::pottery_recipe_clay;
    ++recovery.food_produced_total;
    ++recovery.buildings[9].food_produced;
    recipe=sim::World::restore(recovery,mask());
    while (recipe.building(sim::BuildingId::Household).demand_progress<
           sim::Rules::household_demand_ticks-1) {
        const auto frozen=recipe.building(sim::BuildingId::Pottery).progress;
        recipe.tick();
        check(recipe.building(sim::BuildingId::Pottery).progress==frozen,
              "unstaffed recipe advanced before population recovery");
    }
    recipe.tick();
    check(recipe.workforce_supply()==10 &&
          recipe.building(sim::BuildingId::Pottery).progress==21,
          "recovery demand changed staffing inside the same tick");
    recipe.tick();
    check(recipe.building_staffed(sim::BuildingId::Pottery) &&
          recipe.building(sim::BuildingId::Pottery).progress==22,
          "active recipe did not resume on tick after population recovery");
    recipe=saves.roundtrip(recipe);
    check(recipe.population_valid(),"population invalid after decline");
}

void check_service_feedback() {
    auto world=starter();
    const auto home=sim::BuildingId::Household;
    while (world.household_population(home)<=6 && world.ticks()<5000) world.tick();
    check(world.household_population(home)>6,"stable service never produced population growth");
    while (true) {
        const auto& service=world.courier(sim::CourierId::Service);
        const bool protected_cell=service.phase!=sim::CourierPhase::IdleAtWorkshop &&
            (service.path.at(service.path_vertex)==sim::Cell{10,4} ||
             (service.edge_progress>0 && service.path.at(service.path_vertex+1)==sim::Cell{10,4}));
        if (!protected_cell) break;
        world.tick();
    }
    put(world,sim::CommandType::RemoveRoad,10,4);
    const int population_before=world.household_population(home);
    while (world.household_population(home)>=population_before && world.ticks()<9000) world.tick();
    check(world.household_population(home)<population_before,
          "broken Service did not cause population loss");
    put(world,sim::CommandType::PlaceRoad,10,4);
    const int declined=world.household_population(home);
    while (world.household_population(home)<=declined && world.ticks()<13'000) world.tick();
    check(world.household_population(home)>declined,
          "repaired Service did not permit population recovery");
}

struct Metrics {
    std::uint64_t first_service=0,first_fulfilled=0,first_growth=0,first_level1=0;
    std::uint64_t fourth_house=0,production_expansion=0,first_shortage=0,recovered=0;
    std::uint64_t population28=0,population48=0,goal=0;
};

void expand_when_affordable(sim::World& world,Metrics& metrics,bool& house,bool& clay,bool& pottery) {
    if (!house && world.treasury()>=84 &&
        world.ticks()%sim::Rules::household_demand_ticks!=0) {
        road(world,7,5); road(world,8,5);
        put(world,sim::CommandType::PlaceHousehold,9,5);
        house=true; metrics.fourth_house=world.ticks();
    }
    if (house && !clay && world.treasury()>=124) {
        put(world,sim::CommandType::PlaceClaySource,0,6); road(world,1,6); road(world,2,6);
        clay=true;
    }
    if (clay && !pottery && world.treasury()>=188) {
        put(world,sim::CommandType::PlacePottery,3,6);
        road(world,4,6); road(world,5,6); road(world,6,6); road(world,7,6);
        pottery=true; metrics.production_expansion=world.ticks();
    }
}

void check_full_workforce_reaction(const sim::World& source) {
    auto state=source.snapshot();
    int excess=source.total_population()-28;
    for (unsigned id=sim::first_household_id;id<sim::household_id_end && excess>0;++id) {
        auto& candidate=state.buildings[id-1];
        const int reduction=std::min(excess,candidate.population-sim::Rules::household_min_population);
        candidate.population-=reduction;
        excess-=reduction;
    }
    check(excess==0,"workforce reaction fixture cannot be normalized to population 28");
    auto baseline=sim::World::restore(state,mask());
    check(baseline.total_population()==28 && baseline.workforce_required()==28 &&
          baseline.workforce_used()==28,"workforce reaction fixture is not fully staffed at 28");
    state=baseline.snapshot();
    auto& home=state.buildings[6];
    check(home.demand_progress==sim::Rules::household_demand_ticks-1,
          "staggered fourth House is not one tick before demand");
    home.service_until_tick=0;
    if (home.pottery_stock==0) {
        ++home.pottery_stock;
        ++state.pottery_completed_total;
        ++state.buildings[1].recipes_completed;
        state.clay_extracted_total+=sim::Rules::pottery_recipe_clay;
        state.buildings[0].clay_extracted+=sim::Rules::pottery_recipe_clay;
    }
    if (home.food_stock==0) {
        ++home.food_stock;
        ++state.food_produced_total;
        ++state.buildings[9].food_produced;
    }
    auto reaction=sim::World::restore(state,mask());
    const auto fourth=static_cast<sim::BuildingId>(7);
    const auto pottery_before=reaction.building(fourth).pottery_stock;
    const auto food_before=reaction.building(fourth).food_stock;
    reaction.tick();
    check(reaction.total_population()==27 && reaction.workforce_supply()==27 &&
          reaction.workforce_required()==28 &&
          reaction.building(fourth).pottery_stock==pottery_before &&
          reaction.building(fourth).food_stock==food_before,
          "full-buildout demand miss did not produce the exact 28-to-27 shortage");
    for (const auto id:{sim::BuildingId::ClaySource,sim::BuildingId::Pottery,
                        sim::BuildingId::Warehouse,static_cast<sim::BuildingId>(8),
                        static_cast<sim::BuildingId>(9),sim::BuildingId::Farm})
        check(reaction.building_staffed(id),"lower-ID building lost stable staffing priority");
    check(!reaction.building_staffed(sim::BuildingId::ServicePost),
          "highest-ID Service Post did not become the sole unstaffed building");
}

void check_playable_loop_and_determinism() {
    auto world=starter();
    SaveFixture saves;
    (void)saves.roundtrip(world);
    Metrics metrics;
    bool house=false,clay=false,pottery=false,saw_shortage=false;
    bool saved_shortage=false,saved_recovery=false,saved_level2=false;
    int previous_population=world.total_population();
    auto previous_service=world.covered_households();
    while (world.ticks()<50'000 && !world.settlement_goal_reached()) {
        world.tick();
        if (!metrics.first_service && world.covered_households()>previous_service)
            metrics.first_service=world.ticks();
        if (!metrics.first_fulfilled && world.taxes_collected_total())
            metrics.first_fulfilled=world.ticks();
        if (!metrics.first_growth && world.total_population()>previous_population)
            metrics.first_growth=world.ticks();
        if (!metrics.first_level1) for (unsigned id=4;id<8;++id)
            if (world.household_level(static_cast<sim::BuildingId>(id))>=1) {
                metrics.first_level1=world.ticks(); break;
            }
        expand_when_affordable(world,metrics,house,clay,pottery);
        if (pottery && world.workforce_used()<world.workforce_required()) {
            saw_shortage=true;
            if (!metrics.first_shortage) metrics.first_shortage=world.ticks();
            if (!saved_shortage) {
                world=saves.roundtrip(world);
                saved_shortage=true;
            }
        }
        if (saw_shortage && !metrics.recovered &&
            world.workforce_used()==world.workforce_required()) {
            metrics.recovered=world.ticks();
            world=saves.roundtrip(world);
            saved_recovery=true;
        }
        if (!saved_level2 && world.settlement_goal_households_ready()>0) {
            world=saves.roundtrip(world);
            saved_level2=true;
        }
        if (!metrics.population28 && world.total_population()>=28) {
            metrics.population28=world.ticks();
            check_full_workforce_reaction(world);
        }
        if (!metrics.population48 && world.total_population()>=48) metrics.population48=world.ticks();
        previous_population=world.total_population();
        previous_service=world.covered_households();
        check(world.population_valid() && world.production_balance_valid() &&
              world.food_balance_valid() && world.service_state_valid() &&
              world.navigation_valid() && world.city_economy_valid(),
              "City v9 playable-loop invariant failed");
    }
    if (world.settlement_goal_reached()) metrics.goal=world.ticks();
    check(house && clay && pottery && world.workforce_required()==28,
          "normal-command full buildout was not constructed");
    check(metrics.first_fulfilled && metrics.first_fulfilled<5000 && metrics.first_growth &&
          metrics.first_level1 && metrics.fourth_house && metrics.production_expansion,
          "early City v9 milestones missing");
    check(metrics.first_shortage && metrics.recovered && metrics.population28 &&
          metrics.population28<20'000,"workforce shortage/recovery was not observed");
    check(saved_shortage && saved_recovery && saved_level2,
          "City v9 save checkpoints were not exercised");
    check(metrics.population48 && metrics.goal && metrics.goal<50'000 &&
          world.settlement_goal_reached(),"City v9 population goal not reached");
    world=saves.roundtrip(world);

    auto a=sim::World::restore(world.snapshot(),mask());
    auto b=sim::World::restore(world.snapshot(),mask());
    for (int i=0;i<20'000;++i) {
        a.tick(); b.tick();
        if (i%100==0) check(a.snapshot()==b.snapshot(),"City v9 20k continuation diverged");
    }
    check(a.snapshot()==b.snapshot() && a.population_valid() && a.service_state_valid() &&
          a.food_balance_valid() && a.production_balance_valid() && a.navigation_valid() &&
          a.city_economy_valid(),"City v9 20k final state invalid");
    std::cout<<"starter_population=18 starter_workers=18/18 full_worker_requirement=28"
             <<" first_service_visit_tick="<<metrics.first_service
             <<" first_fulfilled_tick="<<metrics.first_fulfilled
             <<" first_population_growth_tick="<<metrics.first_growth
             <<" first_level1_tick="<<metrics.first_level1
             <<" fourth_house_tick="<<metrics.fourth_house
             <<" production_expansion_tick="<<metrics.production_expansion
             <<" first_worker_shortage_tick="<<metrics.first_shortage
             <<" full_staffing_recovered_tick="<<metrics.recovered
             <<" population_28_tick="<<metrics.population28
             <<" population_48_tick="<<metrics.population48
             <<" goal_tick="<<metrics.goal<<" final_treasury="<<a.treasury()<<'\n';
}
}

int main() {
    try {
        check_profiles_and_schema();
        check_growth_decline_and_tick_staffing();
        check_service_feedback();
        check_playable_loop_and_determinism();
        std::cout<<"sandbox City v9 tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"sandbox City v9 test failed: "<<error.what()<<'\n';
        return 1;
    }
}
