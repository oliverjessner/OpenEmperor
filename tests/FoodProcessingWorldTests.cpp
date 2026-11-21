#include "FoodProcessingFixture.h"
#include <iostream>

namespace {
namespace t=oe_food_processing_fixture;
namespace sim=openemperor::simulation;
using t::check;
template<class F>void refuses(F&& action,const char* why){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}check(rejected,why);}
void separately_prepared_model_capability(){
    t::Fixture f;const auto historical=t::save::prepare_playable_hunting_rules_model(f.root.data);
    for(const auto kind:{openemperor::original::ModelBuilding::Mill,openemperor::original::ModelBuilding::FoodShop}){
        check(!historical->supports_building(kind)&&f.model->supports_building(kind),"Historical model preparation gained processing capabilities");
        refuses([&]{(void)historical->building_value(kind,openemperor::original::BuildingField::Cost);},"Historical prepared model read a new processing row");
    }
    auto text=t::f::read_text(f.root.data/"Model/EmperorBuildingModels.txt");const auto start=text.find("66,Food Shop,");
    check(start!=std::string::npos,"Independent processing model lacks Food Shop");text.erase(start,text.find('\n',start)-start+1);
    t::f::write_text(f.root.data/"Model/EmperorBuildingModels.txt",text);
    refuses([&]{(void)t::save::prepare_food_processing_rules_model(f.root.data);},"Processing preparation admitted a missing source FoodShop capability");
    check(!t::save::prepare_playable_hunting_rules_model(f.root.data)->supports_building(openemperor::original::ModelBuilding::Mill),"Historical preparation acquired a source capability on missing-row control");
}
void explicit_paid_identity_and_atomic_refusals(){
    t::Fixture f;auto w=f.world();const auto initial=w.snapshot();
    check(w.rule_version()==34&&sim::rule_version_supported(w.profile(),34)&&sim::rule_version_supported(w.profile(),35)&&!sim::rule_version_supported(w.profile(),36),"Processing selection changed supported-generation boundary");
    check(sim::building_footprint(w.profile(),34,sim::Object::OriginalMill53)==sim::BuildingFootprint{5,5}&&
          sim::building_footprint(w.profile(),34,sim::Object::OriginalFoodShop66)==sim::BuildingFootprint{2,2},"Processing lost the complete source-registered footprint");
    check(!w.execute({sim::CommandType::PlaceOriginalFoodShop,{98,110}}).accepted&&w.snapshot()==initial,"Orphan FoodShop changed IDs, funds or state");
    const auto city=t::build(w);const auto& m=t::mill(w,city.mill);check(!m.started&&!m.completed&&!t::meals_produced(w),"Paid Mill construction granted free meals or work");
    check(w.workforce_required(city.mill)==16&&w.workforce_required(city.shop)==4&&w.construction_cost(sim::CommandType::PlaceOriginalMill)==91&&
          w.construction_cost(sim::CommandType::PlaceOriginalFoodShop)==19,"Processing ignored independently authored source prices/staffing");
    check(w.building_cells(city.mill).size()==25&&w.building_cells(city.shop).size()==4,"Paid occupancy omitted Mill/Shop footprint cells");
    const auto before=w.snapshot();check(!w.execute(sim::set_mill_recipe(city.mill,5)).accepted&&w.snapshot()==before,"Unavailable grade5 changed paid recipe state");
    check(!w.execute({sim::CommandType::PlaceOriginalFoodShop,{97,111}}).accepted&&w.snapshot()==before,"Second Shop reused an already bound Market");
    check(f.restore(before).canonical_state()==w.canonical_state(),"Empty paid processing city changed on exact restore");
    auto old=sim::World::create_irrigation_productivity(f.policy,f.model,f.terrain);const auto historic=old.snapshot();
    check(!old.execute({sim::CommandType::PlaceOriginalMill,{119,110}}).accepted&&old.snapshot()==historic,"Rule33 admitted a new paid processor");
    auto foreign=historic;foreign.food_processing=sim::FoodProcessingState{};
    refuses([&]{(void)sim::World::restore_irrigation_productivity(foreign,f.policy,f.model,f.terrain);},"Historical restore admitted processing authority");
    t::valid(w);
}
void physical_ingredients_staffed_work_and_grade_conservation(){
    t::Fixture f;auto w=f.world();const auto city=t::build(w);
    t::until(w,[&]{return t::mill(w,city.mill).active.has_value();},"Paid producers did not physically start a two-ingredient Mill recipe");
    t::f::buy(w,sim::set_building_operation(city.mill,false));const auto first=t::mill(w,city.mill);
    t::ticks(w,11);check(t::mill(w,city.mill)==first&&!t::meals_produced(w),"Paused Mill converted committed inputs to free output");
    for(std::uint8_t grade=1;grade<=4;++grade){
        t::f::buy(w,sim::set_mill_recipe(city.mill,grade));t::f::buy(w,sim::set_building_operation(city.mill,true));
        t::until(w,[&]{const auto& m=t::mill(w,city.mill);return m.active&&m.active->grade==grade;},"Selected recipe never committed actual distinct inputs");
        const auto active=*t::mill(w,city.mill).active;const auto start=t::mill(w,city.mill);const auto held=t::stock(w,city.mill);
        check(active.work>=1&&active.work<20U*grade&&active.sequence==start.started,"Recipe start skipped real work or its monotone identity");
        unsigned input=0;for(auto n:active.ingredients)input+=n;check(input==grade,"Recipe debited wrong distinct ingredient count");
        t::f::buy(w,sim::set_building_operation(city.mill,false));const auto paused=t::mill(w,city.mill);
        t::ticks(w,11);check(t::mill(w,city.mill)==paused,"Paused recipe earned staffed work or consumed ingredients again");
        auto restored=f.restore(w.snapshot());check(restored.canonical_state()==w.canonical_state(),"In-progress physical recipe changed on restore");
        t::f::buy(w,sim::set_building_operation(city.mill,true));
        t::until(w,[&]{return t::mill(w,city.mill).completed_by_grade[grade-1]>start.completed_by_grade[grade-1];},"Actual staffed recipe failed to produce its selected meal grade");
        const auto& receipt=*t::mill(w,city.mill).last_receipt;
        check(receipt.batch.grade==grade&&receipt.batch.ingredients==active.ingredients&&receipt.batch.work==20U*grade&&
              receipt.completed_tick>=active.started_tick+20U*grade-1,"Meal output lost exact debits, grade or completed work");
        t::f::buy(w,sim::set_building_operation(city.mill,false));t::valid(w);
        (void)held;
    }
    t::until(w,[&]{return t::meals_consumed(w)>0;},"Real FoodShop meal arrival never reached actual House consumption");
    check(!w.food_processing_state()->houses.empty(),"Actual meals lost immutable first House arrivals");
    bool consumed=false;for(const auto& row:w.food_processing_state()->houses)consumed|=row.first_consumption.has_value();
    check(consumed,"Actual meal sink lacks its immutable consumption witness");t::valid(w);
}
void unavailable_recipe_and_hostile_histories(){
    t::Fixture f;auto w=f.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::build(w,wheat);
    t::ticks(w,2500);check(!t::mill(w,city.mill).started&&!t::meals_produced(w),"Two-base recipe manufactured a missing ingredient");
    check(t::stock(w,city.mill).amounts[0]<=2,"Single early producer exhausted Mill diversity headroom");
    const auto legal=w.snapshot();
    const auto reject=[&](auto edit){auto bad=legal;edit(bad);refuses([&]{(void)f.restore(bad);},"Forged processing state passed strict atomic restore");check(w.snapshot()==legal,"Rejected restore mutated live World");};
    reject([](auto& s){s.food_processing->mills[0].started=1;});
    reject([](auto& s){s.food_processing->mills[0].completed_by_grade[3]=1;});
    reject([](auto& s){s.food_processing->mills[0].ingredients_consumed[0][0]=1;});
    reject([](auto& s){s.processed_food->stocks[0].amounts[11]=1;});
    reject([](auto& s){s.hunting_food=sim::HuntingFoodState{};});
    reject([](auto& s){s.food_processing->shops[0].market_id=s.food_processing->shops[0].shop_id;});
    reject([](auto& s){s.food_processing->shops.push_back(s.food_processing->shops[0]);});
    t::valid(w);
}
void twelve_thousand_tick_earned_replay(){
    t::Fixture f;auto a=f.world(),b=f.world();const auto city=t::build(a),other=t::build(b);check(city.mill==other.mill,"Deterministic paid identities differ");
    for(int tick=0;tick<12000;++tick){
        if(tick==2500||tick==5500||tick==8500){const auto grade=static_cast<std::uint8_t>(tick==2500?3:tick==5500?4:2);
            t::f::buy(a,sim::set_mill_recipe(city.mill,grade));t::f::buy(b,sim::set_mill_recipe(other.mill,grade));}
        if(tick==1800||tick==1850){const bool enabled=tick==1850;t::f::buy(a,sim::set_building_operation(city.shop,enabled));t::f::buy(b,sim::set_building_operation(other.shop,enabled));}
        a.tick();b.tick();
        if(tick%503==0){t::valid(a);t::valid(b);check(a.canonical_state()==b.canonical_state(),"Paid processing replay diverged");b=f.restore(b.snapshot());}
    }
    check(t::meals_produced(a)>0&&t::meals_consumed(a)>0&&a.canonical_state()==b.canonical_state(),"12k processing replay lacks actual earned meals/consumption or exact continuation");
    t::valid(a);t::valid(b);
}
}
int main(){try{
    const auto run=[](const char* name,auto action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}};
    run("model capability",separately_prepared_model_capability);
    run("paid identities",explicit_paid_identity_and_atomic_refusals);
    run("physical recipes",physical_ingredients_staffed_work_and_grade_conservation);
    run("unavailable recipe",unavailable_recipe_and_hostile_histories);
    run("12k replay",twelve_thousand_tick_earned_replay);
    std::cout<<"Food processing paid construction, recipes, transport, consumption and12k replay passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
