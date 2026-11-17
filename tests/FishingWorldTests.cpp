#include "FishingFixture.h"

#include <iostream>
#include <map>

namespace {
namespace n=oe_fishing_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using f::check;

std::uint64_t caught(const sim::World& world) {
    std::uint64_t total=0;for(const auto& site:world.fishing_state()->sites)total+=site.caught;return total;
}
std::uint64_t received_fish(const sim::World& world,bool houses) {
    std::uint64_t total=0;
    for(const auto& row:world.fishing_food_state()->stocks) {
        const auto kind=world.building(row.building_id).kind;
        if(houses ? sim::original_house_kind(kind):kind==sim::Object::Market) total+=row.received[sim::fishing_food_index];
    }
    return total;
}
void all_orientations_and_atomic_refusals() {
    for(std::uint8_t orientation=0;orientation<4;++orientation) {
        n::Fixture fixture(false);
        const auto shape=orig::fishing_quay_geometry(n::quay_origin,orientation);
        for(const auto cell:shape.water)fixture.raw[std::size_t(cell.y)*228U+std::size_t(cell.x)]=4;
        fixture.rebind();auto world=fixture.world();
        const auto funds=world.treasury();const auto cost=world.construction_cost(sim::CommandType::PlaceOriginalFishingQuay);
        const auto quay=n::quay(world,n::quay_origin,orientation);
        check(world.treasury()==funds-cost && world.workforce_required(quay)==10,
              "Quay ignored its independently authored source price/workforce");
        check(world.fishing_quay_cells(quay).size()==5,"Quay omitted its additional dock claim");
        for(const auto cell:shape.occupied)
            check(world.building_owner_at(cell)==quay && world.object_at(cell)==sim::Object::OriginalFishingQuay,
                  "Quay lost an oriented body or dock occupancy cell");
        const auto restored=fixture.restore(world.snapshot());
        check(restored.snapshot()==world.snapshot(),"Oriented Quay restore lost paid geometry");
        sim::Command duplicate{sim::CommandType::PlaceOriginalFishingQuay,n::quay_origin};duplicate.orientation=orientation;
        n::denied(world,duplicate);
        sim::Command invalid_orientation=duplicate;invalid_orientation.orientation=4;n::denied(world,invalid_orientation);
        sim::Command outside{sim::CommandType::PlaceOriginalFishingQuay,{-1,0}};n::denied(world,outside);
        n::denied(world,{sim::CommandType::PlaceRoad,shape.dock});
        n::denied(world,sim::demolish_building(quay));
        auto broken_fixture=n::Fixture(false);
        for(const auto cell:shape.water)broken_fixture.raw[std::size_t(cell.y)*228U+std::size_t(cell.x)]=4;
        const auto missing=shape.water.back();broken_fixture.raw[std::size_t(missing.y)*228U+std::size_t(missing.x)]=0x80;
        broken_fixture.rebind();auto broken=broken_fixture.world();
        sim::Command rejected{sim::CommandType::PlaceOriginalFishingQuay,n::quay_origin};rejected.orientation=orientation;
        n::denied(broken,rejected);n::valid(world);n::valid(broken);
    }
}

void paid_boat_catch_return_food_and_five_crops() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,true);
    bool outward=false,working=false,loaded=false,returning=false,dock=false,credited=false,quality=false;
    std::map<std::uint64_t,sim::Cell> previous;
    std::uint64_t previous_house_receipts=0;
    for(int tick=0;tick<40000;++tick) {
        world.tick();
        const auto& fish=*world.fishing_state();const auto& source=n::source(world,scene.quay);
        const auto dock_cell=orig::fishing_quay_geometry(world.building(scene.quay).cell,source.orientation).dock;
        for(const auto& boat:fish.boats) {
            const auto old=previous.find(boat.id.value);
            if(old==previous.end()) check(boat.current==dock_cell && boat.launched_tick==world.ticks(),"Boat spawned away from its actual dock");
            else check(std::abs(old->second.x-boat.current.x)+std::abs(old->second.y-boat.current.y)<=1,"Boat teleported across water cells");
            previous[boat.id.value]=boat.current;
            outward|=boat.phase==sim::FishingBoatPhase::Outbound && boat.current!=dock_cell;
            working|=boat.phase==sim::FishingBoatPhase::Fishing && boat.work_progress>0;
            loaded|=boat.cargo>0;
            returning|=boat.phase==sim::FishingBoatPhase::Returning && boat.cargo>0 && boat.current!=fish.sites[boat.site_index].cell;
            dock|=boat.phase==sim::FishingBoatPhase::Unloading && boat.cargo>0 && boat.current==dock_cell;
            if(boat.cargo && !source.unloaded)
                check(n::stock(world,scene.quay).amounts[5]==0 && !received_fish(world,false),"Boat cargo was credited before actual dock unloading");
        }
        credited|=source.unloaded>0;
        const auto receipts=received_fish(world,true);
        if(receipts>previous_house_receipts) {
            for(const auto& row:world.fishing_food_state()->stocks) if(sim::original_house_kind(world.building(row.building_id).kind) &&
                row.quality_witness && row.quality_witness->accepted[5]>0) {
                const auto home=std::find_if(world.original_economy_state()->houses.begin(),world.original_economy_state()->houses.end(),
                    [&](const auto& h){return h.building_id==row.building_id;});
                quality|=home!=world.original_economy_state()->houses.end() && home->house.food_quality>0;
            }
        }
        previous_house_receipts=receipts;
        bool every_food=true;for(std::size_t i=0;i<6;++i)every_food&=n::consumed(world,i)>0;
        if(every_food && outward && working && loaded && returning && dock && credited && quality) break;
    }
    for(std::size_t i=0;i<6;++i)check(n::consumed(world,i)>0,"One of six paid Food chains failed actual House consumption");
    for(const auto& field:world.fishing_food_state()->fields)check(field.harvested>0,"Inherited crop stopped producing in Fishing mode");
    check(outward && working && loaded && returning && dock && credited && quality && received_fish(world,false)>0,
          "Fishing end-to-end witness omitted actual boating, work, return, delivery or quality");
    n::valid(world);auto replay=fixture.restore(world.snapshot());
    for(int i=0;i<12000;++i) {
        world.tick();replay.tick();
        if(i%97==0)check(world.snapshot()==replay.snapshot(),"Fishing deterministic long continuation diverged after restore");
    }
    check(world.snapshot()==replay.snapshot(),"Fishing long-run restored state mismatch");n::valid(world);n::valid(replay);
}

void no_ground_pause_full_storage_and_real_workforce() {
    {
        n::Fixture fixture(false);
        for(const auto cell:orig::fishing_quay_geometry(n::quay_origin,0).water)
            fixture.raw[std::size_t(cell.y)*228U+std::size_t(cell.x)]=4;
        fixture.rebind();auto world=fixture.world();const auto scene=n::build(world,false);
        n::ticks(world,1500);
        check(world.workers_assigned(scene.quay)==10 && world.fishing_state()->sites.empty() &&
              !world.fishing_state()->launched && !caught(world) && !n::source(world,scene.quay).unloaded,
              "Shoreline geometry without a usable catch ground produced Fish");
    }
    {
        n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
        f::buy(world,sim::set_building_operation(scene.quay,false));n::ticks(world,300);
        check(!caught(world) && !n::source(world,scene.quay).unloaded && world.fishing_state()->boats.empty(),
              "Paused unstaffed Quay spawned a boat or generated Fish");
        f::buy(world,sim::set_building_operation(scene.quay,true));
        n::until(world,[&]{return !world.fishing_state()->boats.empty() &&
            world.fishing_state()->boats.front().phase==sim::FishingBoatPhase::Fishing &&
            world.fishing_state()->boats.front().work_progress>0;},"No actual staffed fishing work");
        const auto first_id=world.fishing_state()->boats.front().id;const auto before=caught(world);
        f::buy(world,sim::set_building_operation(scene.quay,false));n::ticks(world,100);
        check(caught(world)==before && !n::source(world,scene.quay).unloaded,
              "Paused catch work minted Fish or unloaded without staff/operation");
        check(world.fishing_state()->boats.empty() && world.fishing_state()->retired>0,
              "Empty paused boat did not physically return and safely retire");
        f::buy(world,sim::set_building_operation(scene.quay,true));
        f::buy(world,sim::set_building_operation(scene.market,false));
        n::until(world,[&]{return n::stock(world,scene.quay).amounts[5]==8 && world.fishing_state()->boats.empty();},
            "Paused Market did not produce a finite full Quay inventory");
        check(world.fishing_state()->next_boat_id>first_id.value,"Fishing reused a retired boat identity");
        const auto unloaded=n::source(world,scene.quay).unloaded;const auto catches=caught(world);
        n::ticks(world,500);
        check(n::source(world,scene.quay).unloaded==unloaded && caught(world)==catches && n::stock(world,scene.quay).amounts[5]==8,
              "Full reserved Quay inventory permitted additional fishing production");
        f::buy(world,sim::set_building_operation(scene.market,true));
        n::until(world,[&]{return n::consumed(world)>0;},"Real Fish logistics did not resume after full-storage blockage");
    }
    {
        n::Fixture fixture;
        auto text=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
        for(std::size_t row=0;row<3;++row)oe_production_fixture::set_house_field(text,row,17,1);
        f::write_text(fixture.root.data/"Model/EmperorBuildingModels.txt",text);
        fixture.model=n::save::prepare_playable_fishing_rules_model(fixture.root.data);
        auto world=fixture.world();
        for(int x=88;x<=149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,32}});
        for(int x=88;x<148;x+=3)n::purchase(world,sim::CommandType::PlaceHousehold,{x,33});
        const auto quay=n::quay(world);
        n::until(world,[&]{return !world.fishing_state()->boats.empty() && world.fishing_state()->boats.front().work_progress>0;},
            "Regular residents did not staff Quay before workforce competition");
        check(world.workers_assigned(quay)==10 && world.total_population()<=20,"Workforce control did not earn bounded real residents");
        const auto farm=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{100,35});
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
        check(world.workers_assigned(farm)==12 && world.workers_assigned(quay)==0,"Paid priority competition did not remove Quay staffing");
        const auto before=caught(world);n::ticks(world,100);
        check(caught(world)==before,"Understaffed Quay performed catch work");
        f::buy(world,sim::set_building_operation(farm,false));
        n::until(world,[&]{return caught(world)>before;},"Quay failed to recover after real staffing returned");
    }
}

void competition_depletion_and_regeneration() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false,true);
    bool depleted=false,regenerated=false;
    n::until(world,[&] {
        for(const auto& site:world.fishing_state()->sites) {
            depleted|=site.stock==0;
            regenerated|=site.regenerated>0 && site.caught>static_cast<std::uint64_t>(sim::fishing_site_capacity);
        }
        return depleted && regenerated && n::source(world,scene.quay).unloaded>0 && n::source(world,scene.second_quay).unloaded>0;
    },"Two paid boats failed competition/depletion/regeneration witnesses",30000);
    check(world.fishing_state()->launched>=2 && world.fishing_state()->boats.size()<=2,"Fishing exceeded one real boat per Quay");
    auto replay=fixture.restore(world.snapshot());
    for(int i=0;i<1000;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Competing ecology did not resume deterministically");n::valid(world);
}

void blocked_loaded_land_delivery_preserves_fish() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
    std::optional<sim::CourierId> actor;
    n::until(world,[&] {
        for(const auto& trip:world.fishing_food_state()->trips)if(trip.source==scene.quay && trip.phase==sim::OriginalGoodsTransferPhase::Loaded && trip.loaded[5]>0) {
            const auto& c=world.courier(trip.transfer.courier_id);
            if(c.path_vertex+4<c.path.size()) {actor=c.id;return true;}
        }
        return false;
    },"Quay did not dispatch actual loaded Fish cart");
    const auto c=world.courier(*actor);const auto cut=c.path[c.path_vertex+3];
    check(world.object_at(cut)==sim::Object::Road,"Fishing cart cut did not select a mutable physical road");
    f::buy(world,{sim::CommandType::RemoveRoad,cut});
    n::ticks(world,150);check(world.original_food_cargo(*actor)>0,"Blocked Fish delivery discarded its physical cargo");
    const auto saved=world.snapshot();auto replay=fixture.restore(saved);
    for(int i=0;i<100;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Loaded blocked Fish delivery changed on restore");
    f::buy(world,{sim::CommandType::PlaceRoad,cut});
    n::until(world,[&]{return received_fish(world,false)>0 && n::consumed(world)>0;},
        "Fish cart did not physically finish after paid road repair");
}

void rejected_market_delivery_returns_physical_fish() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
    std::optional<sim::CourierId> actor;
    n::until(world,[&] {
        for(const auto& trip:world.fishing_food_state()->trips)if(trip.source==scene.quay && trip.phase==sim::OriginalGoodsTransferPhase::Loaded && trip.loaded[5]>0) {
            const auto& c=world.courier(trip.transfer.courier_id);
            if(c.path_vertex>=2) {actor=c.id;return true;}
        }
        return false;
    },"No actual outbound Fish before rejected-arrival test");
    const auto before=n::stock(world,scene.quay).returned[5];
    f::buy(world,sim::set_building_operation(scene.market,false));
    bool returning=false;
    n::until(world,[&] {
        const auto& c=world.courier(*actor);
        returning|=c.phase==sim::CourierPhase::Returning && world.original_food_cargo(c.id)>0;
        return n::stock(world,scene.quay).returned[5]>before;
    },"Rejected Market arrival did not return real Fish cargo");
    check(returning && !received_fish(world,false),"Rejected Fish delivery credited a Market or skipped physical return");
    f::buy(world,sim::set_building_operation(scene.market,true));
    n::until(world,[&]{return n::consumed(world)>0;},"Fish did not resume real House supply after Market reopened");
}

void cut_water_route_retains_loaded_boat_and_begun_edges() {
    n::Fixture fixture(false);
    const auto fill=[&](int left,int top,int right,int bottom) {
        for(int y=top;y<=bottom;++y)for(int x=left;x<=right;++x)
            fixture.raw[std::size_t(y)*228U+std::size_t(x)]=4U;
    };
    fill(110,25,119,30);fill(114,15,114,24);fill(110,8,119,14);fixture.rebind();
    // Authored immutable source geography, not injected fish or Food. A single
    // marker-qualified ground beyond the canal forces real outward/return use.
    std::vector<std::uint8_t> boundary(fixture.raw.size());
    for(int y=0;y<228;++y)for(int x=0;x<228;++x)
        boundary[std::size_t(y)*228U+std::size_t(x)]=fixture.terrain->irrigation_boundary_allowed({x,y});
    const std::array<sim::Cell,1> markers{{{114,10}}};
    fixture.terrain=std::make_shared<const sim::OriginalTerrainFacts>(fixture.raw,fixture.fertility,
        fixture.terrain->source_map_sha256(),boundary,markers);
    auto world=fixture.world();const auto scene=n::build(world,false);
    check(world.fishing_state()->sites.size()==1 && world.fishing_state()->sites.front().source_marker,
          "Source marker did not provide bounded catch-ground geography");
    f::buy(world,{sim::CommandType::PlaceRoad,{113,20}});f::buy(world,{sim::CommandType::PlaceRoad,{115,20}});
    const auto bridge=sim::place_bridge({113,20},{115,20});const sim::Cell cut{114,20};
    n::until(world,[&] {
        for(const auto& boat:world.fishing_state()->boats)
            if(boat.edge_progress && boat.path_vertex+1<boat.path.size() && boat.path[boat.path_vertex+1]==cut) return true;
        return false;
    },"Boat did not physically begin the required canal edge");
    n::denied(world,bridge);
    n::until(world,[&] {
        for(const auto& boat:world.fishing_state()->boats)
            if(boat.phase==sim::FishingBoatPhase::Returning && boat.cargo>0 && boat.current.y<=18) return true;
        return false;
    },"Boat did not earn actual catch before the water cut");
    const auto id=world.fishing_state()->boats.front().id;
    const auto load=world.fishing_state()->boats.front().cargo;
    f::buy(world,bridge);
    n::until(world,[&] {
        return !world.fishing_state()->boats.empty() && world.fishing_state()->boats.front().movement==sim::FishingMovementResult::Retry &&
            world.fishing_state()->boats.front().path.empty();
    },"Blocked water route was confused with actual dock arrival");
    n::ticks(world,200);
    check(world.fishing_state()->boats.size()==1 && world.fishing_state()->boats.front().id==id &&
          world.fishing_state()->boats.front().cargo==load && !n::source(world,scene.quay).unloaded && n::stock(world,scene.quay).amounts[5]==0,
          "Unreachable loaded boat was teleported, retired, discarded or credited");
    auto replay=fixture.restore(world.snapshot());
    for(int i=0;i<300;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Blocked loaded boat failed exact deterministic recovery");n::valid(world);
}

void natural_fire_stops_actual_work() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world,false);
    n::until(world,[&]{return n::source(world,scene.quay).unloaded>0;},"No earned Fish before natural-fire control");
    for(const auto watch:scene.watches)f::buy(world,sim::set_building_operation(watch,false));
    n::until(world,[&]{return world.building_on_fire(scene.quay);},"Quay never entered actual natural fire",20000);
    const auto before=caught(world),unloaded=n::source(world,scene.quay).unloaded;
    for(int i=0;i<20 && world.building_on_fire(scene.quay);++i) {
        world.tick();check(caught(world)==before && n::source(world,scene.quay).unloaded==unloaded,
              "Burning Quay caught or unloaded Fish");
    }
    n::valid(world);
}
} // namespace

int main() {
    try {
        all_orientations_and_atomic_refusals();
        paid_boat_catch_return_food_and_five_crops();
        no_ground_pause_full_storage_and_real_workforce();
        competition_depletion_and_regeneration();
        cut_water_route_retains_loaded_boat_and_begun_edges();
        blocked_loaded_land_delivery_preserves_fish();
        rejected_market_delivery_returns_physical_fish();
        natural_fire_stops_actual_work();
        std::cout<<"Fishing World PASS: paid oriented Quay, physical boat/catch/return, six Food chains, real consumption, finite ecology, staffing/pause/fire/storage/routes and deterministic replay\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Fishing World FAIL: "<<error.what()<<'\n';return 1;}
}
