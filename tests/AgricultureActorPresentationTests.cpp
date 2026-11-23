// Paid authored scenes only: presentation never creates, moves or credits actors.
#include "AgricultureReconfigurationFixture.h"
#include "app/AgricultureActorPresentation.h"
#include "core/PerformanceDiagnostics.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <vector>

namespace {
namespace t=oe_agriculture_reconfiguration_fixture;
namespace sim=openemperor::simulation;
namespace oe=openemperor;
using t::check;
using Kind=oe::AgricultureActorKind;
using Action=oe::AgricultureActorAction;
using Cache=oe::AgricultureActorPresentationCache;
using Visual=oe::AgricultureActorVisual;

std::vector<Visual> rows(const Cache& cache){return {cache.entries().begin(),cache.entries().end()};}
void exact_restore(const t::Fixture& fixture,const sim::World& world){
    const auto before=world.snapshot();auto restored=fixture.restore(before);Cache a,b;
    a.prepare(world);b.prepare(restored);check(rows(a)==rows(b),"Fresh restore changed actual actor poses or lifetime identities");
    for(int i=0;i<5;++i)a.prepare(world);
    check(rows(a)==rows(b)&&world.snapshot()==before,"Preparation calls changed pose or World authority");
}
void animals_are_real_bounded_and_historical_visuals_stay_exact(){
    t::Fixture fixture;auto world=fixture.world();Cache cache;const auto before=world.snapshot();cache.prepare(world);
    check(cache.entries().size()==world.hunting_state()->animals.size()&&cache.entries().size()<=58,"Fresh cache invented visible idle channels or expanded the movement budget");
    for(const auto& animal:world.hunting_state()->animals){const auto* shown=cache.find_animal(animal.id);
        check(shown&&shown->id==animal.id.value&&shown->lifetime_id==animal.id.value&&shown->position==world.hunting_animal_position(animal.id)&&!shown->pose.moving&&!shown->pose.working&&!shown->pose.cargo,"Initial wildlife presentation invented movement, work or cargo");
        const auto expected=animal.species==sim::HuntingSpecies::WildPig75?Kind::WildPig:animal.species==sim::HuntingSpecies::Pheasant76?Kind::Pheasant:Kind::Antelope;
        check(shown->pose.kind==expected,"Real wildlife species lost its presentation identity");}
    check(world.snapshot()==before,"Preparing actual wildlife mutated World");
    t::until(world,[&]{return std::any_of(world.hunting_state()->animals.begin(),world.hunting_state()->animals.end(),[](const auto& a){return a.next&&a.edge_progress;});},"No actual animal edge for visual acceptance",80);
    cache.prepare(world);for(const auto& animal:world.hunting_state()->animals)if(animal.next&&animal.edge_progress){const auto* shown=cache.find_animal(animal.id);
        check(shown&&shown->pose.moving&&shown->pose.action==Action::Walking&&shown->pose.phase==animal.edge_progress%4&&shown->position==world.hunting_animal_position(animal.id),"Animal pose did not follow its real begun edge");}
    exact_restore(fixture,world);const auto copy=rows(cache);
    oe::performance::set_enabled(true);oe::performance::reset();
    for(int frame=0;frame<3000;++frame)for(const auto& row:cache.entries())(void)cache.find_animal({row.id});
    for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Cached actor queries performed World/navigation/resource work");
    oe::performance::set_enabled(false);check(rows(cache)==copy,"Frame queries animated stationary cached poses");
    t::w::Fixture historical;const auto old=historical.world();cache.prepare(old);check(cache.entries().empty(),"Rule35 gained successor actor visuals");
    cache.prepare(world);cache.clear();check(cache.entries().empty(),"Actor cache retained a retired session");
}
void actual_food_work_cargo_and_idle_visibility(){
    t::Fixture fixture;auto world=fixture.world();constexpr std::array<std::size_t,1> wheat{0};const auto city=t::q::build(world,wheat);
    const auto courier=t::worker(world,city.farm).courier_id;Cache cache;cache.prepare(world);check(!cache.find_courier(courier),"Idle Farm channel acquired a visible Grower");
    std::array<bool,4> phases{};bool loaded=false,cart=false,retired=false;std::uint64_t lifetime=0;
    for(int n=0;n<12000&&!(loaded&&cart&&retired&&std::all_of(phases.begin(),phases.end(),[](bool v){return v;}));++n){
        world.tick();cache.prepare(world);const auto& worker=t::worker(world,city.farm);
        if(worker.latest_trip&&!worker.latest_trip->returned_tick){const auto trip=*worker.latest_trip;const auto* shown=cache.find_courier(courier);
            check(shown&&shown->pose.kind==Kind::FoodGrower&&shown->lifetime_id==trip.grower_id.value&&shown->owner==city.farm&&shown->target==trip.field_id&&shown->position==*world.courier_position(courier),"Physical Food Grower lost its actual channel/lifetime/position/field");
            phases[static_cast<unsigned>(trip.phase)-1]=true;lifetime=trip.grower_id.value;
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Waiting)check(!shown->pose.moving&&!shown->pose.working&&shown->pose.phase==0,"Waiting Grower falsely tended or walked in place");
            if(trip.phase==sim::PhysicalFoodGrowerPhase::Working){const auto& season=*world.seasonal_agriculture_state();const auto field=std::find_if(season.fields.begin(),season.fields.end(),[&](const auto& f){return f.field_id==trip.field_id;});
                check(field!=season.fields.end()&&shown->pose.action==Action::Working&&!shown->pose.moving&&shown->pose.phase==(std::uint64_t(trip.work_ticks)+field->harvest_progress)%4,"Grower tending pose ignored actual work/harvest counters");}
            if(trip.harvested_tick){check(shown->pose.cargo==1&&shown->pose.action==Action::Returning&&!shown->pose.working,"Loaded Grower lost real unreturned cargo or invented harvest animation");if(!loaded)exact_restore(fixture,world);loaded=true;}
        }else if(lifetime){check(!cache.find_courier(courier),"Retired Grower lifetime remained visible at the source");retired=true;}
        for(const auto& shown:cache.entries())if(shown.pose.kind==Kind::FoodCourier&&shown.pose.cargo>0){const auto id=static_cast<sim::CourierId>(shown.id);
            check(shown.pose.cargo==world.original_food_cargo(id)&&shown.position==*world.courier_position(id)&&shown.lifetime_id>0,"Food cart presentation borrowed stock or lost its real transfer identity");cart=true;}
    }
    check(loaded&&cart&&retired&&std::all_of(phases.begin(),phases.end(),[](bool v){return v;}),"Paid Food scene did not exercise waiting/tending/loaded return/retirement/cart presentation");t::valid(world);
}
void actual_plantation_and_hemp_pickups_have_no_invented_work_loop(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::q::material_city(world);
    for(int y=108;y<=111;++y)t::f::buy(world,{sim::CommandType::PlaceRoad,{119,y}});
    t::f::buy(world,{sim::CommandType::PlaceRoad,{120,110}});
    const auto hemp=t::purchase(world,sim::CommandType::PlaceOriginalHempFarm,{120,111});
    sim::Command field{sim::CommandType::PlaceOriginalHempField,{122,111}};field.building_id=hemp;t::f::buy(world,field);
    Cache cache;bool plant=false,hemp_loaded=false;
    for(int n=0;n<15000&&!(plant&&hemp_loaded);++n){world.tick();cache.prepare(world);
        for(const auto& shown:cache.entries())if(shown.pose.kind==Kind::PlantationGrower||shown.pose.kind==Kind::HempGrower){
            check(!shown.pose.working&&shown.pose.action!=Action::Working&&shown.lifetime_id>0,"Instant pickup invented a sustained plantation/Hemp work loop");
            if(shown.pose.cargo){check(shown.pose.cargo==1&&shown.pose.action==Action::Returning&&shown.target.has_value(),"Picked material cargo lacks its actual return/plant identity");
                if(shown.pose.kind==Kind::PlantationGrower&&shown.owner==city.tea_source)plant=true;
                if(shown.pose.kind==Kind::HempGrower&&shown.owner==hemp)hemp_loaded=true;}}
    }
    check(plant&&hemp_loaded,"Paid Tea/Hemp sources did not earn actual picked cargo for actor visuals");exact_restore(fixture,world);t::valid(world);
}
void actual_boat_hunter_phases_and_paused_unload_freeze(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::q::h::build(world,false);Cache cache;
    std::array<bool,4> boats{},hunters{};bool frozen=false;
    for(int n=0;n<14000&&!(frozen&&std::all_of(boats.begin(),boats.end(),[](bool v){return v;})&&std::all_of(hunters.begin(),hunters.end(),[](bool v){return v;}));++n){
        world.tick();cache.prepare(world);
        std::optional<sim::FishingBoatId> pause_boat;oe::AgricultureActorPose paused_pose;sim::Position paused_position;std::uint16_t paused_work=0;
        for(const auto& boat:world.fishing_state()->boats){const auto* shown=cache.find_boat(boat.id);check(shown&&shown->pose.kind==Kind::Boat&&shown->id==boat.id.value&&shown->owner==boat.quay_id&&shown->position==world.fishing_boat_position(boat.id)&&shown->pose.cargo==boat.cargo,"Boat display changed actual identity/position/cargo");boats[static_cast<unsigned>(boat.phase)]=true;
            if(boat.phase==sim::FishingBoatPhase::Fishing||boat.phase==sim::FishingBoatPhase::Unloading)check(!shown->pose.moving&&shown->pose.phase==boat.work_progress%4,"Stationary Boat pose used frame time instead of saved work");
            if(!frozen&&boat.phase==sim::FishingBoatPhase::Unloading&&boat.cargo&&boat.work_progress>0){pause_boat=boat.id;paused_pose=shown->pose;paused_position=shown->position;paused_work=boat.work_progress;}
        }
        for(const auto& hunter:world.hunting_state()->hunters){const auto* shown=cache.find_hunter(hunter.id);check(shown&&shown->pose.kind==Kind::Hunter&&shown->id==hunter.id.value&&shown->owner==hunter.tent_id&&shown->position==world.hunting_hunter_position(hunter.id)&&shown->pose.cargo==hunter.cargo,"Hunter display changed actual identity/position/cargo");hunters[static_cast<unsigned>(hunter.phase)]=true;
            if(hunter.phase==sim::HuntingHunterPhase::Hunting||hunter.phase==sim::HuntingHunterPhase::Unloading)check(!shown->pose.moving&&shown->pose.phase==hunter.work_progress%4,"Stationary Hunter pose used unrelated clock/counters");}
        if(pause_boat){
            // Values above are copied before any command/tick invalidates rows.
            t::operate(world,city.quay,false);t::ticks(world,12);cache.prepare(world);const auto* paused=cache.find_boat(*pause_boat);
            check(paused&&paused->pose==paused_pose&&paused->position==paused_position,"Source-paused unloading Boat animated despite frozen work/edge counters");
            const auto current=std::find_if(world.fishing_state()->boats.begin(),world.fishing_state()->boats.end(),[&](const auto& b){return b.id==*pause_boat;});check(current!=world.fishing_state()->boats.end()&&current->work_progress==paused_work,"Pause control did not freeze actual Boat unloading");
            exact_restore(fixture,world);t::operate(world,city.quay,true);frozen=true;
        }
    }
    check(frozen&&std::all_of(boats.begin(),boats.end(),[](bool v){return v;})&&std::all_of(hunters.begin(),hunters.end(),[](bool v){return v;}),"Paid hunting/fishing scene missed an actual lifecycle phase or frozen-unload control");exact_restore(fixture,world);t::valid(world);
}
}
int main(){try{animals_are_real_bounded_and_historical_visuals_stay_exact();actual_food_work_cargo_and_idle_visibility();actual_plantation_and_hemp_pickups_have_no_invented_work_loop();actual_boat_hunter_phases_and_paused_unload_freeze();std::cout<<"Agriculture actor saved-state poses, paid work/cargo/phases, bounded caches and exact restore passed\n";return 0;}catch(const std::exception& error){oe::performance::set_enabled(false);std::cerr<<error.what()<<'\n';return 1;}}
