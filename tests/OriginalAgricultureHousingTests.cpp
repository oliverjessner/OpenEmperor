// Rule24 admits paid crop identities and typed Food foundations. Its unresolved
// Original tending/harvest clock grants no growth, stock, delivery or quality.
// Every scenario input is authored; no Original data or save state is embedded.
#include "AgricultureFixture.h"

#include <iostream>
#include <set>

namespace {
namespace a=oe_agriculture_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using a::check;

template<class Action> void refuses(Action action,const char* reason) {
    try {action();} catch (const std::exception&) {return;}
    throw std::runtime_error(reason);
}
void denied(sim::World& world,sim::Command command) {
    const auto before=world.snapshot();const auto result=world.execute(command);
    check(!result.accepted && !result.changed && world.snapshot()==before,
          "Rejected Agriculture command changed paid authority, IDs or Funds");
}
void no_unverified_food(const sim::World& world) {
    check(!sim::agriculture_production_clock_verified,"Unproved Agriculture production clock was activated");
    const auto& state=*world.original_agriculture_state();
    for(const auto& field:state.fields)
        check(!field.quantity && !field.productive_events && !field.harvested && !field.last_harvest_cycle,
              "Unverified Agriculture clock granted field growth or a harvest");
    for(const auto& stock:state.stocks)
        check(stock.amounts==sim::AgricultureAmounts{} && stock.received==sim::AgricultureCounters{} &&
              stock.departed==sim::AgricultureCounters{} && stock.returned==sim::AgricultureCounters{} &&
              stock.consumed==sim::AgricultureCounters{} && !stock.quality_witness,
              "Unverified Agriculture clock granted stock, transport, consumption or quality evidence");
    check(state.trips.empty(),"An empty Agriculture source dispatched a physical Food carrier");
    for(const auto& home:world.original_economy_state()->houses)
        check(!home.house.food_quality,"House received unearned Agriculture FoodQuality");
    using Key=std::pair<sim::BuildingId,std::uint64_t>;
    std::set<Key> retained;
    const auto inspect=[&](sim::BuildingId id,const sim::OriginalHousingEvaluation& proof) {
        check(!proof.input.food_quality && !proof.food_quality_witness && proof.food_received==sim::OriginalFoodCounters{} &&
              proof.food_consumed==sim::OriginalFoodCounters{},"Agriculture projected or invented Housing Food evidence");
        retained.emplace(id,proof.tick);
    };
    for(const auto& home:world.original_production_state()->housing.houses) {
        if(home.last_evaluation)inspect(home.building_id,*home.last_evaluation);
        for(const auto& first:home.first_transitions)
            if(first && first->kind==sim::OriginalHousingTransitionKind::Evolution)inspect(home.building_id,first->evaluation);
    }
    check(state.housing_evaluations.size()==retained.size() && retained.size()<=20*16,
          "Agriculture Housing retained a missing or unbounded Food journal");
    auto key=retained.begin();
    for(const auto& row:state.housing_evaluations) {
        check(key!=retained.end() && *key==Key{row.building_id,row.tick},"Agriculture Housing first/latest keys lost canonical order");
        check(!row.quality_witness && row.received==sim::AgricultureCounters{} && row.consumed==sim::AgricultureCounters{},
              "Unverified Agriculture clock fabricated full-width Housing Food input");
        ++key;
    }
    check(world.population_valid() && world.city_economy_valid() && world.production_balance_valid() &&
          world.navigation_valid() && world.fire_state_valid(),"Agriculture actual World invariant failed");
}
void ticks(sim::World& world,int count) {for(int i=0;i<count;++i)world.tick();no_unverified_food(world);}

void paid_construction_and_source_admission() {
    a::Fixture fixture;auto world=fixture.world();
    denied(world,{sim::CommandType::PlaceOriginalCabbageField,{23,25}});
    const auto scene=a::build(world);
    check(sim::building_footprint(world.profile(),world.rule_version(),sim::Object::OriginalFoodFarm)==sim::BuildingFootprint{3,3} &&
          sim::building_footprint(world.profile(),world.rule_version(),sim::Object::OriginalSoybeanField)==sim::BuildingFootprint{1,1},
          "Agriculture paid Farm/field footprints differ from separate actual bodies");
    sim::Command field{sim::CommandType::PlaceOriginalSoybeanField,{60,25}};field.building_id=scene.soy_farm;denied(world,field);
    field.cell={28,26};denied(world,field); // Interior of the complete Farm body.
    field.cell={30,25};denied(world,field); // Another actual paid field.
    denied(world,sim::demolish_building(scene.soy_farm));denied(world,sim::demolish_building(scene.soy_field));
    // Separate paid fields may reference one paid Farm, within its checked area.
    field={sim::CommandType::PlaceOriginalCabbageField,{24,25}};field.building_id=scene.cabbage_farm;f::buy(world,field);
    const auto second=*world.building_owner_at(field.cell);
    check(second!=scene.cabbage_field && a::field(world,second).farm_id==scene.cabbage_farm,
          "Agriculture collapsed distinct field assignments");
    a::Fixture dry;dry.fertility[26*228+24]=99;dry.write_map();
    dry.terrain=a::save::prepare_original_agriculture_terrain_facts(dry.root.data,f::map_relative);
    auto rejected=dry.world();const auto dry_scene=a::build(rejected);
    field={sim::CommandType::PlaceOriginalRiceField,{24,26}};field.building_id=dry_scene.cabbage_farm;denied(rejected,field);
    field.building_id=scene.cabbage_farm;const auto cost=world.construction_cost(field.type),before=world.treasury();
    f::buy(world,field);
    check(a::field(world,*world.building_owner_at(field.cell)).crop==sim::AgricultureCrop::Rice && world.treasury()==before-cost,
          "Source fertility100 did not preserve a separately paid Rice identity");
    no_unverified_food(world);no_unverified_food(rejected);
    // Exercise the existing global field bound without raising the body budget.
    for(const auto farm:{scene.cabbage_farm,scene.soy_farm}) {
        const auto origin=world.building(farm).cell;
        for(int y=origin.y-3;y<=origin.y+5 && world.original_agriculture_state()->fields.size()<sim::agriculture_field_limit;++y)
            for(int x=origin.x-3;x<=origin.x+5 && world.original_agriculture_state()->fields.size()<sim::agriculture_field_limit;++x) {
                field={sim::CommandType::PlaceOriginalCabbageField,{x,y}};field.building_id=farm;
                if(world.validate(field).accepted)f::buy(world,field);
            }
    }
    check(world.original_agriculture_state()->fields.size()==sim::agriculture_field_limit,"Could not exercise bounded paid field capacity");
    field={sim::CommandType::PlaceOriginalSoybeanField,{25,30}};field.building_id=scene.cabbage_farm;denied(world,field);
    no_unverified_food(world);
}

void gated_staffing_pause_fire_and_year_rollover() {
    a::Fixture fixture;auto world=fixture.world();const auto scene=a::build(world);
    sim::Command rice{sim::CommandType::PlaceOriginalRiceField,{24,26}};rice.building_id=scene.cabbage_farm;f::buy(world,rice);
    ticks(world,1000);
    check(world.total_population()>0 && world.building_staffed(scene.soy_farm),"Gated test never earned actual residents/full Farm staff");
    f::buy(world,sim::set_building_operation(scene.soy_farm,false));ticks(world,1000);
    f::buy(world,sim::set_building_operation(scene.soy_farm,true));
    // Cross all twelve existing calendar slots and multiple years. A known
    // end-slot alone must never substitute for actual Original worker proof.
    ticks(world,24000);
    auto replay=sim::World::restore_original_agriculture(world.snapshot(),fixture.policy,fixture.model,fixture.terrain);
    for(int i=0;i<700;++i) {world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Gated Agriculture Housing creator/restore continuation diverged");
    no_unverified_food(world);no_unverified_food(replay);
    auto burning=fixture.world();const auto fire_scene=a::build(burning);
    for(const auto id:fire_scene.watches)
        f::buy(burning,sim::set_building_operation(id,false));
    a::until(burning,[&]{return burning.building_on_fire(fire_scene.soy_farm);},
             "Gated test never reached a real natural Farm fire",12000);
    no_unverified_food(burning);ticks(burning,200);
    auto unstaffed=fixture.world();
    const auto farm=a::purchase(unstaffed,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command field{sim::CommandType::PlaceOriginalSoybeanField,{23,25}};field.building_id=farm;f::buy(unstaffed,field);
    ticks(unstaffed,10000);check(!unstaffed.building_staffed(farm),"No-resident Farm unexpectedly obtained staff");
}

void hostile_unverified_authority_is_rejected() {
    a::Fixture fixture;auto world=fixture.world();const auto scene=a::build(world);ticks(world,1500);
    const auto clean=world.snapshot();
    const auto reject=[&](sim::WorldSnapshot hostile) {
        refuses([&]{(void)sim::World::restore_original_agriculture(hostile,fixture.policy,fixture.model,fixture.terrain);},
                "Unverified or forged Agriculture Food authority restored");
        check(world.snapshot()==clean,"Rejected Agriculture restore changed live authority");
    };
    auto hostile=clean;hostile.agriculture->fields.front().quantity=1;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->fields.front().productive_events=1;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->fields.front().harvested=4;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->fields.front().last_harvest_cycle=1;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->stocks.front().amounts[4]=1;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->stocks.front().received[3]=1;reject(std::move(hostile));
    hostile=clean;hostile.agriculture->housing_evaluations.erase(hostile.agriculture->housing_evaluations.begin());reject(std::move(hostile));
    hostile=clean;hostile.agriculture->housing_evaluations.insert(hostile.agriculture->housing_evaluations.begin(),hostile.agriculture->housing_evaluations.front());reject(std::move(hostile));
    hostile=clean;hostile.agriculture->housing_evaluations.front().building_id=scene.market;reject(std::move(hostile));
    hostile=clean;++hostile.agriculture->housing_evaluations.front().tick;reject(std::move(hostile));
    hostile=clean;++hostile.agriculture->housing_evaluations.front().received[3];reject(std::move(hostile));
    hostile=clean;
    for(auto& home:hostile.original_production->housing.houses)if(home.last_evaluation) {home.last_evaluation->food_received[0]=1;break;}
    reject(std::move(hostile));
    hostile=clean;hostile.original_economy->houses.front().house.food_quality=90;reject(std::move(hostile));
}

void historical_food_keeps_its_own_clock() {
    a::Fixture fixture;
    const auto model=a::save::prepare_original_production_rules_model(fixture.root.data);
    const auto terrain=std::make_shared<const sim::OriginalTerrainFacts>(fixture.raw,fixture.terrain->source_map_sha256());
    auto legacy=sim::World::create_original_deadline_food_dispatch(fixture.policy,model,terrain);
    for(int x=20;x<=44;++x)f::buy(legacy,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<44;x+=3)a::purchase(legacy,sim::CommandType::PlaceHousehold,{x,22});
    const auto farm=a::purchase(legacy,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command field{sim::CommandType::PlaceOriginalMilletField,{23,25}};field.building_id=farm;f::buy(legacy,field);
    a::purchase(legacy,sim::CommandType::PlaceMarket,{33,25});
    field={sim::CommandType::PlaceOriginalSoybeanField,{24,25}};field.building_id=farm;denied(legacy,field);
    for(int i=0;i<2000;++i)legacy.tick();
    check(!legacy.original_agriculture_state() && legacy.original_production_state()->food.farms.front().harvested>0,
          "Agriculture gate altered historical Rule18's independent Wheat/Millet clock");
}
}
int main() {
    try {
        paid_construction_and_source_admission();gated_staffing_pause_fire_and_year_rollover();
        hostile_unverified_authority_is_rejected();historical_food_keeps_its_own_clock();
        std::cout<<"Agriculture paid typed field/source admission, bounded capacity, unverified-clock no-grants through staffing/pause/fire/year rollover, strict Housing replay and historical Food clock PASS\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Agriculture gated World/Housing FAIL: "<<error.what()<<'\n';return 1;}
}
