#include "HuntingFixture.h"
#include "simulation/original/HarvestRules.h"

#include <iostream>
#include <map>

namespace {
namespace n=oe_hunting_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
using f::check;

std::uint64_t received(const sim::World& world,bool houses) {
    std::uint64_t result=0;for(const auto& row:world.hunting_food_state()->stocks) {
        const auto kind=world.building(row.building_id).kind;
        if(houses ? sim::original_house_kind(kind):kind==sim::Object::Market)result+=row.received[sim::hunting_food_index];
    }
    return result;
}
void paid_geometry_and_atomic_refusals() {
    check(sim::HuntingHunter::actor_type==openemperor::original::harvest_rule(
        openemperor::original::HarvestProducer::HuntersTent).worker_actor,
        "World Hunter lifetime lost its independently documented Actor40 identity");
    n::Fixture fixture;auto world=fixture.world();
    check(!world.hunting_state()->habitats.empty() && world.hunting_state()->habitats.size()<=4 &&
        world.hunting_state()->animals.size()==world.hunting_state()->habitats.size()*3,
        "Fresh Hunting ecology lacks its finite real initial population");
    const auto wildlife=world.hunting_state()->animals.front().current;
    n::denied(world,{sim::CommandType::PlaceRoad,wildlife});
    n::denied(world,{sim::CommandType::PlaceFireWatch,wildlife});
    n::until(world,[&]{return std::any_of(world.hunting_state()->animals.begin(),world.hunting_state()->animals.end(),
        [](const auto& animal){return animal.next.has_value();});},"Fresh real wildlife did not begin a land edge",80);
    const auto begun=std::find_if(world.hunting_state()->animals.begin(),world.hunting_state()->animals.end(),
        [](const auto& animal){return animal.next.has_value();});
    n::denied(world,{sim::CommandType::PlaceRoad,*begun->next});
    n::denied(world,{sim::CommandType::PlaceIrrigationDitch,*begun->next});
    const auto before=world.treasury();const auto tent=n::tent(world);
    check(before-world.treasury()==29 && world.workforce_required(tent)==15,"Tent ignored independent prepared source cost/workforce");
    const auto cells=sim::building_footprint_cells(world.profile(),world.rule_version(),sim::Object::OriginalHuntersTent,n::tent_origin);
    check(cells.size()==4,"Tent omitted complete2x2 occupancy");
    for(const auto cell:cells)check(world.building_owner_at(cell)==tent && world.object_at(cell)==sim::Object::OriginalHuntersTent,
        "Tent lost complete paid body ownership");
    n::denied(world,{sim::CommandType::PlaceOriginalHuntersTent,{85,45}});
    n::denied(world,{sim::CommandType::PlaceOriginalHuntersTent,{-1,44}});
    n::denied(world,{sim::CommandType::PlaceOriginalHuntersTent,{227,44}});
    sim::Command orientation{sim::CommandType::PlaceOriginalHuntersTent,{88,44}};orientation.orientation=1;n::denied(world,orientation);
    n::denied(world,sim::demolish_building(tent));n::denied(world,{sim::CommandType::PlaceRoad,{85,45}});
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Paid Tent/initial animals did not restore exactly");
    n::valid(world);
    n::Fixture broken;broken.raw[45U*228U+85U]=4;broken.rebind();auto refused=broken.world();
    n::denied(refused,{sim::CommandType::PlaceOriginalHuntersTent,n::tent_origin});
}
void all_seven_physically_held_in_one_market() {
    n::Fixture fixture;auto world=fixture.world();
    // Two ordinary paid road components: residents earn the global workforce,
    // while the source/Market component has no House route. Market operation
    // remains enabled, so only actual physical incoming carts can fill it.
    for(int x=19;x<=79;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    for(int x=20;x<80;x+=3)n::purchase(world,sim::CommandType::PlaceHousehold,{x,22});
    n::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{68,25});
    n::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{72,25});
    n::purchase(world,sim::CommandType::PlaceFireWatch,{30,25});
    for(int x=110;x<=136;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,32}});
    for(int y=33;y<=49;++y)f::buy(world,{sim::CommandType::PlaceRoad,{136,y}});
    for(int x=84;x<=135;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,49}});
    for(int x=84;x<=92;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,46}});
    for(int y=47;y<=48;++y)f::buy(world,{sim::CommandType::PlaceRoad,{92,y}});
    n::purchase(world,sim::CommandType::PlaceFireWatch,{130,50});
    const auto market=n::purchase(world,sim::CommandType::PlaceMarket,{132,50});
    std::array<sim::BuildingId,7> sources{};
    for(std::size_t i=0;i<5;++i) {
        const int x=96+7*static_cast<int>(i);
        sources[i]=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,50});
        n::m::crop(world,sources[i],i,{x+3,50});
    }
    sources[5]=n::g::quay(world);sources[6]=n::tent(world);
    for(const auto source:sources)f::buy(world,sim::set_building_operation(source,false));
    n::until(world,[&]{return world.total_population()>=120;},"Ordinary House immigration did not earn the separate-source workforce");
    for(const auto& home:world.original_economy_state()->houses)
        check(!world.find_building_route(market,home.building_id),"Market stock test accidentally left an outgoing House road route");
    for(std::size_t resource=0;resource<sources.size();++resource) {
        f::buy(world,sim::set_building_operation(sources[resource],true));
        n::until(world,[&]{return n::stock(world,market).received[resource]>0;},"A paid typed source did not physically deliver to the isolated operating Market");
        f::buy(world,sim::set_building_operation(sources[resource],false));
        check(world.building(market).operating_enabled && world.workers_assigned(market)==world.workforce_required(market),
            "Held-stock test disabled or understaffed the Market");
    }
    const auto& stock=n::stock(world,market);
    for(std::size_t resource=0;resource<7;++resource)
        check(stock.amounts[resource]>0 && stock.received[resource]==static_cast<std::uint64_t>(stock.amounts[resource]) && !stock.departed[resource],
            "Seven distinct typed foods were not simultaneously physically held after real Market arrivals");
    check(n::g::volume(stock.amounts)<=16,"Seven-food Market exceeded its shared finite capacity");
    n::valid(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Held seven-food Market changed on exact restore");
}
void paid_physical_hunt_return_delivery_consumption_and_seven_foods() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,true);
    bool moved=false,pursued=false,contact=false,carried=false,returned=false,quality=false,animal_motion=false;
    std::map<std::uint64_t,sim::Cell> positions,animals;
    for(const auto& animal:world.hunting_state()->animals)animals[animal.id.value]=animal.current;
    std::uint64_t last_receipt=0;
    for(int tick=0;tick<40000;++tick) {
        world.tick();const auto& state=*world.hunting_state();const auto& source=n::source(world,scene.tent);
        const auto& body=world.building(scene.tent);const auto home=sim::building_front_cell(world.profile(),world.rule_version(),body.kind,body.cell);
        for(const auto& animal:state.animals) {
            if(const auto at=animals.find(animal.id.value);at!=animals.end())animal_motion|=at->second!=animal.current || animal.edge_progress>0;
            animals[animal.id.value]=animal.current;
        }
        for(const auto& hunter:state.hunters) {
            const auto previous=positions.find(hunter.id.value);
            if(previous==positions.end())check(hunter.current==home && hunter.launched_tick==world.ticks(),"Hunter spawned away from its paid Tent");
            else check(std::abs(previous->second.x-hunter.current.x)+std::abs(previous->second.y-hunter.current.y)<=1,"Hunter teleported between land cells");
            positions[hunter.id.value]=hunter.current;moved|=hunter.edge_progress>0;
            pursued|=hunter.phase==sim::HuntingHunterPhase::Pursuing && hunter.current!=home;
            if(hunter.phase==sim::HuntingHunterPhase::Hunting) {
                const auto animal=std::find_if(state.animals.begin(),state.animals.end(),[&](const auto& row){return hunter.target==row.id;});
                check(animal!=state.animals.end() && animal->reserved_by==hunter.id && animal->current==hunter.current && !animal->next,
                    "Hunt work did not contact its real uniquely reserved animal");
                contact|=hunter.work_progress>0;
            }
            carried|=hunter.cargo==1 && hunter.kill_cell && hunter.killed_tick &&
                hunter.phase==sim::HuntingHunterPhase::Returning;
            returned|=hunter.phase==sim::HuntingHunterPhase::Unloading && hunter.cargo==1 && hunter.current==home;
            if(hunter.cargo && !source.unloaded)check(n::stock(world,scene.tent).amounts[6]==0 && !received(world,false),
                "Hunter cargo was credited before physical return/unload");
        }
        const auto receipts=received(world,true);
        if(receipts>last_receipt)for(const auto& row:world.hunting_food_state()->stocks)
            if(row.quality_witness && row.quality_witness->accepted[6]) {
                const auto home_row=std::find_if(world.original_economy_state()->houses.begin(),world.original_economy_state()->houses.end(),
                    [&](const auto& value){return value.building_id==row.building_id;});
                quality|=home_row!=world.original_economy_state()->houses.end() && home_row->house.food_quality>0;
            }
        last_receipt=receipts;
        bool all=true;for(std::size_t i=0;i<7;++i)all&=n::consumed(world,i)>0;
        if(all && moved && pursued && contact && carried && returned && quality && animal_motion)break;
    }
    for(std::size_t i=0;i<7;++i)check(n::consumed(world,i)>0,"One of seven paid Food chains failed actual household consumption");
    check(moved && pursued && contact && carried && returned && quality && animal_motion && received(world,false)>0 && n::kills(world)>0,
        "Paid Hunting omitted physical actor/animal/work/cargo/return/quality/delivery evidence");
    for(const auto& field:world.hunting_food_state()->fields)check(field.harvested>0,"An inherited crop stopped in Hunting mode");
    n::valid(world);auto replay=fixture.restore(world.snapshot());
    for(int tick=0;tick<12000;++tick) {
        world.tick();replay.tick();if(tick%97==0)check(world.snapshot()==replay.snapshot(),"Hunting12000-tick restored continuation diverged");
    }
    check(world.snapshot()==replay.snapshot(),"Hunting long replay ended with different authority");n::valid(world);n::valid(replay);
}
void contact_clock_pause_full_storage_and_real_workforce() {
    {
        n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
        f::buy(world,sim::set_building_operation(scene.tent,false));n::ticks(world,400);
        check(!world.hunting_state()->launched && !n::kills(world) && !n::source(world,scene.tent).unloaded,"Paused Tent spawned or generated Meat");
        f::buy(world,sim::set_building_operation(scene.tent,true));
        n::until(world,[&]{return !world.hunting_state()->hunters.empty() &&
            world.hunting_state()->hunters.front().phase==sim::HuntingHunterPhase::Hunting &&
            world.hunting_state()->hunters.front().work_progress==1;},"Hunter never began real contact work");
        const auto id=world.hunting_state()->hunters.front().id;const auto prey=*world.hunting_state()->hunters.front().target;
        const auto before=n::kills(world);n::ticks(world,58);
        check(n::kills(world)==before && world.hunting_state()->hunters.front().work_progress==59,"Animal died before60 real contact ticks");
        world.tick();check(n::kills(world)==before+1 && world.hunting_state()->hunters.front().cargo==1 &&
            std::none_of(world.hunting_state()->animals.begin(),world.hunting_state()->animals.end(),[&](const auto& a){return a.id==prey;}),
            "Completed physical hunt did not remove exactly its actual animal into one Meat cargo");
        check(!n::source(world,scene.tent).unloaded,"Kill event credited Tent without actual return");
        n::until(world,[&]{return !world.hunting_state()->hunters.empty() && world.hunting_state()->hunters.front().id==id &&
            world.hunting_state()->hunters.front().phase==sim::HuntingHunterPhase::Unloading;},"Loaded Hunter did not physically return");
        f::buy(world,sim::set_building_operation(scene.tent,false));n::ticks(world,100);
        check(!n::source(world,scene.tent).unloaded && world.hunting_state()->hunters.front().cargo==1,
            "Paused Tent accepted/discarded physical Meat cargo");
        f::buy(world,sim::set_building_operation(scene.tent,true));n::ticks(world,19);
        check(!n::source(world,scene.tent).unloaded,"Meat unloaded before20 real eligible Tent work ticks");
        world.tick();check(n::source(world,scene.tent).unloaded==1,"Actual completed Tent unload did not credit Meat");
        f::buy(world,sim::set_building_operation(scene.market,false));
        n::until(world,[&]{return n::stock(world,scene.tent).amounts[6]==8 && world.hunting_state()->hunters.empty();},
            "Blocked Market did not leave finite full Tent stock");
        const auto kills=n::kills(world),unloaded=n::source(world,scene.tent).unloaded;n::ticks(world,500);
        check(n::kills(world)==kills && n::source(world,scene.tent).unloaded==unloaded && n::stock(world,scene.tent).amounts[6]==8,
            "Full Tent storage admitted unreserved additional kills or stock");
        f::buy(world,sim::set_building_operation(scene.market,true));n::until(world,[&]{return n::consumed(world)>0;},"Meat supply failed after Market reopened");
    }
    {
        n::Fixture fixture;auto text=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
        for(std::size_t row=0;row<3;++row)oe_production_fixture::set_house_field(text,row,17,1);
        f::write_text(fixture.root.data/"Model/EmperorBuildingModels.txt",text);
        fixture.model=n::save::prepare_playable_hunting_rules_model(fixture.root.data);auto world=fixture.world();
        for(int x=79;x<=149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,46}});
        for(int x=90;x<148;x+=3)n::purchase(world,sim::CommandType::PlaceHousehold,{x,47});
        const auto tent=n::tent(world);
        n::until(world,[&]{return !world.hunting_state()->hunters.empty() &&
            world.hunting_state()->hunters.front().phase==sim::HuntingHunterPhase::Hunting &&
            world.hunting_state()->hunters.front().work_progress>0;},"Real residents did not staff paid Tent");
        check(world.workers_assigned(tent)==15 && world.total_population()<=20,"Hunting workforce control did not earn bounded real people");
        const auto farm=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{100,50});
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
        check(world.workers_assigned(tent)<15 && world.workers_assigned(farm)==12,"Paid priority competition did not remove full Tent staffing");
        const auto before=n::kills(world);n::ticks(world,200);check(n::kills(world)==before,"Understaffed Tent completed a hunt");
        f::buy(world,sim::set_building_operation(farm,false));n::until(world,[&]{return n::kills(world)>before;},"Hunter failed to recover after real staffing returned");
    }
}
void exclusive_competition_depletion_and_regeneration() {
    n::Fixture fixture;const std::array<sim::Cell,1> markers{{{96,44}}};n::prey_markers(fixture,markers);
    auto world=fixture.world();const auto scene=n::build(world,false,true);
    check(world.hunting_state()->habitats.size()==1 && world.hunting_state()->habitats.front().source_marker,
        "Source marker did not provide bounded habitat geography");
    bool competing=false,depleted=false,regenerated=false;
    n::until(world,[&] {
        const auto& state=*world.hunting_state();
        if(state.hunters.size()==2 && state.hunters[0].target && state.hunters[1].target) {
            check(state.hunters[0].target!=state.hunters[1].target,"Competing Hunters reserved the same animal");competing=true;
        }
        depleted|=state.animals.empty();regenerated|=n::births(world)>0 && n::kills(world)>3;
        return competing && depleted && regenerated && n::source(world,scene.tent).unloaded>0 && n::source(world,scene.second_tent).unloaded>0;
    },"Two Hunters failed real target competition, depletion, regeneration or return",30000);
    n::valid(world);auto replay=fixture.restore(world.snapshot());
    for(int i=0;i<1000;++i){world.tick();replay.tick();}check(world.snapshot()==replay.snapshot(),"Regenerated ecology changed on restore");
}
void occupied_road_floor_blocks_real_regeneration() {
    n::Fixture fixture;const std::array<sim::Cell,1> markers{{{96,44}}};n::prey_markers(fixture,markers);
    auto world=fixture.world();const auto scene=n::build(world,false,true);
    n::until(world,[&]{return world.hunting_state()->animals.empty();},"Two paid Hunters did not earn actual habitat depletion");
    for(const auto tent:scene.tents)f::buy(world,sim::set_building_operation(tent,false));
    const auto centre=world.hunting_state()->habitats.front().centre;
    const auto in_habitat=[&](sim::Cell cell){return std::abs(cell.x-centre.x)+std::abs(cell.y-centre.y)<=sim::hunting_habitat_radius;};
    n::until(world,[&] {
        for(const auto& hunter:world.hunting_state()->hunters)
            if(in_habitat(hunter.current) || (hunter.edge_progress && in_habitat(hunter.path.at(hunter.path_vertex+1))))return false;
        return true;
    },"Loaded Hunters did not physically leave the depleted habitat");
    check(world.hunting_state()->animals.empty(),"Blocked-spawn setup missed the real depletion interval");
    std::size_t roads=0;
    for(int dy=-sim::hunting_habitat_radius;dy<=sim::hunting_habitat_radius;++dy)
        for(int dx=-sim::hunting_habitat_radius;dx<=sim::hunting_habitat_radius;++dx) {
            const sim::Cell cell{centre.x+dx,centre.y+dy};if(!in_habitat(cell))continue;
            f::buy(world,{sim::CommandType::PlaceRoad,cell});++roads;
        }
    check(roads==25,"Occupied spawn-floor test omitted habitat cells");
    const auto before_births=n::births(world),before_kills=n::kills(world),next_id=world.hunting_state()->next_animal_id;
    std::uint64_t unloaded=0;for(const auto& tent:world.hunting_state()->tents)unloaded+=tent.unloaded;
    n::ticks(world,1000);
    check(world.hunting_state()->animals.empty() && n::births(world)==before_births && n::kills(world)==before_kills &&
        world.hunting_state()->next_animal_id==next_id &&
        world.hunting_state()->habitats.front().regeneration_progress==sim::hunting_regeneration_ticks-1,
        "Paid road occupancy admitted a birth, kill, identity allocation or unbounded regeneration timer");
    std::uint64_t after_unloaded=0;for(const auto& tent:world.hunting_state()->tents)after_unloaded+=tent.unloaded;
    check(after_unloaded==unloaded,"Blocked habitat regeneration generated Meat receipts");
    f::buy(world,{sim::CommandType::RemoveRoad,centre});world.tick();
    check(world.hunting_state()->animals.size()==1 && n::births(world)==before_births+1 && n::kills(world)==before_kills &&
        world.hunting_state()->animals.front().id.value==next_id && world.hunting_state()->next_animal_id==next_id+1 &&
        world.hunting_state()->animals.front().born_tick==world.ticks() && world.hunting_state()->animals.front().current==centre &&
        !world.hunting_state()->habitats.front().regeneration_progress,
        "One genuinely freed grass site did not admit exactly one real monotone animal birth");
    n::valid(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Road-blocked ecology lost exact restore continuation");
}
void blocked_pursuit_loaded_return_and_begun_edge_safety() {
    n::Fixture fixture(false);
    for(int y=0;y<228;++y)for(int x=91;x<228;++x)fixture.raw[std::size_t(y)*228U+std::size_t(x)]=4;
    for(int x=91;x<=110;++x)fixture.raw[45U*228U+std::size_t(x)]=0x80U;
    for(int y=41;y<=49;++y)for(int x=110;x<=118;++x)fixture.raw[std::size_t(y)*228U+std::size_t(x)]=0x80U;
    fixture.rebind();const std::array<sim::Cell,1> markers{{{114,45}}};n::prey_markers(fixture,markers);
    auto world=fixture.world();const auto city=n::m::build(world,false);(void)city;
    for(int y=28;y<=46;++y)f::buy(world,{sim::CommandType::PlaceRoad,{79,y}});
    for(int x=80;x<=90;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,46}});
    const auto tent=n::tent(world);const sim::Cell cut{100,45};
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});n::ticks(world,1200);
    check(world.workers_assigned(tent)==15 && !world.hunting_state()->launched && !n::kills(world),
        "Unreachable real animals granted a Hunter, kill or Meat");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    n::until(world,[&]{return !world.hunting_state()->hunters.empty() &&
        world.hunting_state()->hunters.front().edge_progress && world.hunting_state()->hunters.front().current.x==95;},
        "Hunter did not physically enter the required land corridor");
    const auto actor=world.hunting_state()->hunters.front();
    n::denied(world,{sim::CommandType::PlaceIrrigationDitch,actor.path.at(actor.path_vertex+1)});
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    n::until(world,[&]{return !world.hunting_state()->hunters.empty() &&
        world.hunting_state()->hunters.front().movement==sim::HuntingMovementResult::Retry;},"Blocked pursuit was confused with actual contact");
    n::ticks(world,100);check(!n::kills(world) && !n::source(world,tent).unloaded && world.hunting_state()->hunters.front().target,
        "Blocked pursuit lost its animal reservation or manufactured Meat");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    n::until(world,[&]{return !world.hunting_state()->hunters.empty() && world.hunting_state()->hunters.front().cargo==1 &&
        world.hunting_state()->hunters.front().current.x>=108;},"Hunter did not earn real Meat beyond the corridor");
    const auto id=world.hunting_state()->hunters.front().id;f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    n::until(world,[&]{return !world.hunting_state()->hunters.empty() &&
        world.hunting_state()->hunters.front().movement==sim::HuntingMovementResult::Retry && world.hunting_state()->hunters.front().path.empty();},
        "Blocked loaded Hunter was confused with actual Tent return");
    n::ticks(world,150);check(world.hunting_state()->hunters.front().id==id && world.hunting_state()->hunters.front().cargo==1 &&
        !n::source(world,tent).unloaded && n::stock(world,tent).amounts[6]==0,"Blocked loaded return teleported, discarded or credited Meat");
    auto replay=fixture.restore(world.snapshot());for(int i=0;i<200;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Blocked Hunter/animal reservations failed deterministic recovery");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});n::until(world,[&]{return n::consumed(world)>0;},"Repaired land route did not complete real Meat supply");
}
void real_natural_fire_blocks_hunting() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
    n::until(world,[&]{return n::source(world,scene.tent).unloaded>0;},"No actual Meat before natural-fire control");
    for(const auto watch:scene.watches)f::buy(world,sim::set_building_operation(watch,false));
    n::until(world,[&]{return world.building_on_fire(scene.tent);},"Tent never entered actual natural fire",20000);
    const auto kills=n::kills(world),unloaded=n::source(world,scene.tent).unloaded;
    for(int i=0;i<20 && world.building_on_fire(scene.tent);++i) {
        world.tick();check(n::kills(world)==kills && n::source(world,scene.tent).unloaded==unloaded,"Burning Tent hunted or accepted Meat");
    }
    n::valid(world);
}
} // namespace
int main() {
    try {
        paid_geometry_and_atomic_refusals();
        all_seven_physically_held_in_one_market();
        paid_physical_hunt_return_delivery_consumption_and_seven_foods();
        contact_clock_pause_full_storage_and_real_workforce();
        exclusive_competition_depletion_and_regeneration();
        occupied_road_floor_blocks_real_regeneration();
        blocked_pursuit_loaded_return_and_begun_edge_safety();
        real_natural_fire_blocks_hunting();
        std::cout<<"Hunting World PASS: paid Tent, real animals/pursuit/contact/kill/cargo/return, seven Food chains, finite ecology, real staffing/pause/fire/blocks and12000-tick recovery\n";
        return 0;
    } catch(const std::exception& error){std::cerr<<"Hunting World FAIL: "<<error.what()<<'\n';return 1;}
}
