#include "QualityAwareFoodDispatchFixture.h"
#include "simulation/original/OriginalModelMapping.h"
#include <iostream>

namespace {
namespace t=oe_quality_aware_food_dispatch_fixture;
namespace sim=t::sim;namespace orig=t::orig;
using t::check;
template<class Action>void rejects(Action action,const char* why){bool refused=false;
    try{action();}catch(const std::invalid_argument&){refused=true;}check(refused,why);}

void explicit_generation_and_bounded_physical_chooser(){
    t::Fixture fixture;auto world=fixture.world();auto old=sim::World::create_irrigated_harvest(fixture.policy,fixture.model,fixture.terrain);
    check(world.rule_version()==38&&sim::profile_rules(world.profile(),38).version==38&&
        world.quality_aware_food_dispatch_enabled()&&!world.irrigated_harvest_enabled()&&
        std::string_view(sim::quality_aware_food_dispatch_contract)=="authored-quality-aware-food-dispatch-v1"&&
        sim::quality_aware_food_dispatch_revision==1&&sim::current_rule_version(world.profile())==4,
        "Quality-aware dispatch lost explicit38 or changed defaults/exact37 selection");
    for(std::uint32_t version=1;version<=37;++version)
        check(!sim::quality_aware_food_dispatch_enabled(world.profile(),version),"Historical generation gained quality-aware selection");
    check(!sim::quality_aware_food_dispatch_enabled(sim::RulesProfile::CityV11,38)&&
        !sim::rule_version_supported(world.profile(),39),"Quality-aware selection widened another profile/future generation");
    auto projected=world.snapshot();projected.rule_version=37;
    check(projected==old.snapshot()&&fixture.restore(world.snapshot()).snapshot()==world.snapshot(),
        "Fresh38 altered inherited empty gameplay or exact restore");
    rejects([&]{sim::World::restore_irrigated_harvest(world.snapshot(),fixture.policy,fixture.model,fixture.terrain);},"Exact37 restored38");
    rejects([&]{fixture.restore(old.snapshot());},"Exact38 migrated37");
    for(const auto kind:{sim::Object::OriginalRiceField,sim::Object::OriginalHuntersTent,sim::Object::OriginalMill53,sim::Object::OriginalFoodShop66})
        check(orig::functional_model_candidate(kind,38).candidate==orig::functional_model_candidate(kind,37).candidate,
            "Quality-aware dispatch lost inherited model capability");
    sim::ProcessedFoodAmounts available{};available[0]=1;available[4]=3;const auto physical=available;
    check(sim::quality_aware_raw_food_load(available,4,70)==sim::food_batch::mixed_portions(available,4)&&available==physical,
        "Qualified historical mixed load changed or pure query debited stock");
    available={};available[0]=3;available[4]=1;sim::ProcessedFoodAmounts one_soy{};one_soy[4]=1;
    check(sim::quality_aware_raw_food_load(available,4,70)==one_soy&&sim::quality_aware_raw_food_load(available,1,70)==one_soy,
        "Unsafe mixed cart did not select its smaller actual qualified subset");
    available={};for(std::size_t g=0;g<7;++g)available[g]=1;
    const auto chosen=sim::quality_aware_raw_food_load(available,4,70);
    check(sim::food_batch::volume(chosen)==4&&chosen[4]==1&&chosen[6]==1&&chosen[1]==1&&chosen[5]==1&&
        sim::agriculture_batch_quality(chosen).value>=70,"Bounded choice failed to use actual highest-quality raw portions");
    available={};available[0]=16;
    check(sim::quality_aware_raw_food_load(available,4,70)==sim::ProcessedFoodAmounts{}&&
        sim::quality_aware_raw_food_load(available,0,0)==sim::ProcessedFoodAmounts{}&&
        sim::quality_aware_raw_food_load(available,4,20)==sim::food_batch::mixed_portions(available,4),
        "Scarcity fabricated qualified food or changed a compatible lower SourceRow need");
    rejects([&]{sim::quality_aware_raw_food_load(available,5,70);},"Chooser widened cart4");
    rejects([&]{sim::quality_aware_raw_food_load(available,4,101);},"Chooser accepted an unsupported source-row requirement");
    available[7]=1;rejects([&]{sim::quality_aware_raw_food_load(available,4,70);},"Raw chooser selected meals");
    available={};available[0]=-1;rejects([&]{sim::quality_aware_raw_food_load(available,4,70);},"Chooser admitted negative physical stock");
    t::valid(world);
}

void actual_wheat_scarcity_preserves_old37_and_skips_unqualified_targets(){
    t::Fixture fixture(70);auto world=fixture.world();auto old=sim::World::create_irrigated_harvest(fixture.policy,fixture.model,fixture.terrain);
    const auto city=t::build_raw(world,0),historical=t::build_raw(old,0);
    check(city.farm==historical.farm&&city.market==historical.market,"Paid paired raw scenes changed actual owner IDs");
    const auto received=[&](const sim::World& value){std::uint64_t count=0;
        for(const auto id:city.homes)count+=t::stock(value,id).received[0];return count;};
    for(int n=0;n<4000&&!received(old);++n){world.tick();old.tick();}
    check(received(old)>0&&received(world)==0&&t::stock(world,city.market).amounts[0]>0&&
        world.physical_food_growers_state()->fields.front().returned>0,
        "Actual low-quality crop scarcity was not earned or historical37 lost its original arrival");
    sim::CourierId distributor{};for(const auto& actor:world.couriers())
        if(actor.owner==city.market&&actor.role==sim::CourierRole::OriginalFoodToHouse)distributor=actor.id;
    const auto before=world.snapshot();const auto decision=world.courier_dispatch_status(distributor);
    check(decision.status==sim::CourierDispatchStatus::NoQualifiedFood&&!decision.selected_target&&
        std::string_view(sim::courier_dispatch_status_name(decision.status))=="No raw Food meeting House quality needs"&&world.snapshot()==before,
        "No qualified raw batch created a target/grant or a dispatch query mutated paid state");
    for(const auto id:city.homes)if(t::house(world,id).occupants>0)
        check(world.original_rules_model()->house_rules().value(orig::house_row(t::house(world,id).type),orig::HouseField::FoodQuality)==70,
            "Raw scarcity used a class/ID shortcut instead of the actual SourceRow need");
    t::valid(world);
}

void actual_qualified_cargo_arrival_and_restored_continuation(){
    t::Fixture fixture(70);auto world=fixture.world();const auto scene=t::earn_loaded_mixed_raw(world);
    const auto loaded=t::trip(world,scene.courier);const auto quantity=sim::food_batch::volume(loaded.loaded);
    const auto baseline=t::stock(world,scene.target).received;
    check(quantity>0&&quantity<=4&&loaded.loaded[4]==quantity&&sim::agriculture_batch_quality(loaded.loaded).value>=70&&
        sim::food_batch::volume(t::stock(world,scene.target).amounts)==0&&
        t::stock(world,scene.city.market).departed[4]>=static_cast<std::uint64_t>(quantity),
        "Raw dispatch did not debit real qualified stock or granted target Food before arrival");
    auto restored=fixture.restore(world.snapshot());bool arrived=false;
    for(int n=0;n<500&&!arrived;++n){world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot(),"Qualified in-flight raw load changed across exact38 restore");
        const auto& stock=t::stock(world,scene.target);
        arrived=stock.quality_witness&&stock.quality_witness->transfer==scene.transfer;
    }
    const auto& stock=t::stock(world,scene.target);const auto& actual=t::trip(world,scene.courier);
    check(arrived&&actual.transfer==scene.transfer&&actual.delivered==loaded.loaded&&
        stock.received[4]-baseline[4]==static_cast<std::uint64_t>(quantity)&&stock.quality_witness->accepted==loaded.loaded,
        "Qualified cargo lacks its same actual arrival/owner/transfer or sole-ledger receipt");
    const auto& witness=*stock.quality_witness;std::uint64_t prior=0;
    for(std::size_t g=0;g<12;++g)prior+=stock.received[g]-static_cast<unsigned>(witness.accepted[g])-witness.consumed_before[g];
    const auto blended=orig::blend_food_quality(witness.previous_quality,sim::agriculture_batch_quality(witness.accepted).value,
        quantity,static_cast<std::int32_t>(prior));
    check(t::house(world,scene.target).food_quality==(sim::food_batch::volume(stock.amounts)?blended:0),
        "Quality-aware choice changed actual arrival blending or empty-stock reset");
    for(int n=0;n<100&&world.courier(scene.courier).phase!=sim::CourierPhase::IdleAtWorkshop;++n){world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot(),"Qualified physical return changed across exact38 restore");}
    check(world.courier(scene.courier).phase==sim::CourierPhase::IdleAtWorkshop&&
        sim::food_batch::volume(t::trip(world,scene.courier).returned)==0,
        "Actual qualified delivery retained cargo or failed its ordinary return");
    t::valid(world);
}
} // namespace
int main(){try{
    explicit_generation_and_bounded_physical_chooser();
    actual_wheat_scarcity_preserves_old37_and_skips_unqualified_targets();
    actual_qualified_cargo_arrival_and_restored_continuation();
    std::cout<<"Rule38 bounded actual raw choice, explicit generations, scarcity and physical arrival/return passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
