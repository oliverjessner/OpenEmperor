#include "PhysicalFoodGrowersFixture.h"

#include <iostream>
#include <map>

namespace {
namespace q=oe_physical_food_growers_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
using f::check;
using Phase=sim::PhysicalFoodGrowerPhase;

void each_crop_earns_real_work_cargo_return_and_house_consumption() {
    for(std::size_t crop=0;crop<5;++crop) {
        q::Fixture fixture;auto world=fixture.world();const std::array<std::size_t,1> crops{crop};const auto scene=q::build(world,crops);
        f::buy(world,sim::set_building_operation(scene.market,false));
        const auto channel=q::farm(world,scene.farm).courier_id;
        check(!q::farm(world,scene.farm).spawned && !world.total_population() && !q::work(world,scene.fields[0]).productive_ticks,
            "Paid Farm/Field construction granted a worker, residents or crop work");
        bool outward=false,waiting=false,working=false,loaded=false;std::optional<std::uint64_t> arrived;
        std::map<std::uint64_t,sim::Cell> last;std::uint64_t first_id=0;
        for(int tick=0;tick<12000 && !loaded;++tick) {
            const auto before=q::work(world,scene.fields[0]).productive_ticks;world.tick();
            if(!q::active(world,scene.farm))continue;
            const auto trip=q::worker(world,scene.farm);const auto actor=world.courier(channel);
            if(!first_id)first_id=trip.grower_id.value;
            check(trip.field_id==scene.fields[0] && static_cast<std::size_t>(trip.crop)==crop && trip.grower_id.value==first_id,
                "A live Grower changed its immutable Field/crop/lifetime");
            if(!actor.path.empty()) {
                const auto cell=actor.path.at(actor.path_vertex);
                if(const auto previous=last.find(trip.grower_id.value);previous!=last.end())
                    check(std::abs(previous->second.x-cell.x)+std::abs(previous->second.y-cell.y)<=1,"Grower teleported between land cells");
                last[trip.grower_id.value]=cell;
            }
            outward|=trip.phase==Phase::Outward && actor.edge_progress>0;
            if(trip.phase==Phase::Waiting) {
                waiting=true;arrived=trip.arrived_tick;
                check(arrived && actor.path.at(actor.path_vertex)==world.building(scene.fields[0]).cell && !actor.edge_progress &&
                    q::work(world,scene.fields[0]).productive_ticks==before && !world.physical_food_grower_cargo(channel),
                    "Waiting Grower worked, loaded cargo or lacked actual Field arrival");
            }
            if(q::work(world,scene.fields[0]).productive_ticks>before) {
                working=true;check(arrived && world.ticks()>=*arrived+sim::physical_food_grower_wait_ticks &&
                    actor.path.at(actor.path_vertex)==world.building(scene.fields[0]).cell &&
                    (!actor.edge_progress || (trip.phase==Phase::Returning && trip.harvested_tick==world.ticks())) &&
                    q::work(world,scene.fields[0]).productive_ticks==before+1,"Crop work bypassed physical arrival, wait or one-worker labor");
            }
            loaded=world.physical_food_grower_cargo(channel)==1;
            check(!q::receipt_field(world,scene.fields[0]).returned && q::h::stock(world,scene.farm).amounts[crop]==0,
                "Crop credited Farm Food before real Grower return");
            if(tick%97==0)q::valid(world);
        }
        check(outward && waiting && working && loaded && q::field(world,scene.fields[0]).harvested==1 &&
            q::worker(world,scene.farm).harvested_tick && q::worker(world,scene.farm).phase==Phase::Returning,
            "A paid crop skipped outward movement, wait, real work or one-portion return cargo");
        const auto loaded_trip=q::worker(world,scene.farm);f::buy(world,sim::set_building_operation(scene.farm,false));
        q::until(world,[&]{return q::receipt_field(world,scene.fields[0]).returned==1;},"Loaded Grower failed actual return while Farm work was paused");
        const auto receipt=q::receipt_field(world,scene.fields[0]).last_receipt;
        check(receipt && receipt->grower_id==loaded_trip.grower_id && receipt->crop==loaded_trip.crop && receipt->courier_id==channel &&
            receipt->harvested_tick<receipt->returned_tick && receipt->work_at_harvest==80 &&
            q::h::stock(world,scene.farm).amounts[crop]==1 && !world.physical_food_grower_cargo(channel),
            "Actual physical Farm receipt changed crop/lifetime, credited twice or lost the reserved portion");
        q::ticks(world,31);check(q::receipt_field(world,scene.fields[0]).returned==1 && q::h::stock(world,scene.farm).amounts[crop]==1,
            "Retired Grower duplicated its settled Food receipt");
        f::buy(world,sim::set_building_operation(scene.market,true));f::buy(world,sim::set_building_operation(scene.farm,true));
        bool source_cart=false,house_cart=false;
        q::until(world,[&]{
            for(const auto& transfer:world.hunting_food_state()->trips) {
                const auto& courier=world.courier(transfer.transfer.courier_id);
                if(transfer.loaded[crop]>0 && transfer.phase==sim::OriginalGoodsTransferPhase::Loaded && courier.edge_progress>0) {
                    source_cart|=courier.role==sim::CourierRole::OriginalFoodToMarket;
                    house_cart|=courier.role==sim::CourierRole::OriginalFoodToHouse;
                }
            }
            return source_cart && house_cart && q::consumed(world)[crop]>0;
        },"Returned typed crop did not travel through both physical carts into actual House consumption");
        for(const auto& stock:world.hunting_food_state()->stocks)for(std::size_t other=0;other<7;++other)if(other!=crop)
            check(!stock.amounts[other] && !stock.received[other] && !stock.consumed[other],"Single-crop worker converted its physical output type");
        q::valid(world);
    }
}

void three_crop_rotation_pause_partial_work_and_finite_headroom() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);std::uint64_t previous=0,last_id=0;
    std::vector<sim::BuildingId> assignments;
    for(int tick=0;tick<20000;++tick) {
        world.tick();const auto total=q::farm_work(world,scene.farm);check(total-previous<=1,"One physical Farm earned extra labor from multiple fields");previous=total;
        if(q::active(world,scene.farm) && q::worker(world,scene.farm).grower_id.value!=last_id) {
            last_id=q::worker(world,scene.farm).grower_id.value;assignments.push_back(q::worker(world,scene.farm).field_id);
        }
        if(q::consumed(world)[0] && q::consumed(world)[1] && q::consumed(world)[4])break;
    }
    check(assignments.size()>=3 && assignments[0]==scene.fields[0] && assignments[1]==scene.fields[1] && assignments[2]==scene.fields[2],
        "One physical worker did not rotate canonically through its three paid crops");
    for(const auto id:scene.fields)check(q::receipt_field(world,id).returned>0,"A sibling crop starved without an actual Farm return");
    check(q::consumed(world)[0] && q::consumed(world)[1] && q::consumed(world)[4],"Three physical crop origins did not reach real House sinks");
    q::until(world,[&]{return q::phase(world,scene.farm,Phase::Working) && q::worker(world,scene.farm).work_ticks>7 &&
        q::work(world,q::worker(world,scene.farm).field_id).productive_ticks%80<65;},"No intermediate field work for empty cancellation");
    const auto chosen=q::worker(world,scene.farm).field_id;const auto retained=q::work(world,chosen).productive_ticks;
    const auto retired=q::farm(world,scene.farm).retired;f::buy(world,sim::set_building_operation(chosen,false));
    q::until(world,[&]{return q::farm(world,scene.farm).retired>retired;},"Paused Field did not physically retire its empty worker");
    q::ticks(world,80);check(q::work(world,chosen).productive_ticks==retained,"Field pause lost or advanced retained partial labor");
    f::buy(world,sim::set_building_operation(chosen,true));
    f::buy(world,sim::set_building_operation(scene.market,false));
    q::until(world,[&]{return q::volume(q::h::stock(world,scene.farm).amounts)==7 && q::phase(world,scene.farm,Phase::Working);},
        "Finite bin control never exposed seven held portions plus its worker's last reserved slot");
    check(sim::physical_food_grower_reservations(&*world.physical_food_growers_state(),scene.farm)==1,
        "Working Grower did not retain its sole real Farm return reservation");
    q::until(world,[&]{return q::volume(q::h::stock(world,scene.farm).amounts)==8 && !q::active(world,scene.farm);},"Actual Grower returns did not fill the finite Farm bin");
    const auto full=*world.physical_food_growers_state();const auto work=*world.multi_field_agriculture_state();q::ticks(world,150);
    check(*world.physical_food_growers_state()==full && *world.multi_field_agriculture_state()==work,"Full Farm created a worker, work or duplicate harvest");
    f::buy(world,sim::set_building_operation(scene.market,true));
    q::until(world,[&]{return q::volume(q::h::stock(world,scene.farm).amounts)==4 && std::any_of(world.couriers().begin(),world.couriers().end(),[&](const auto& actor){
        return actor.owner==scene.farm && actor.role==sim::CourierRole::OriginalFoodToMarket && actor.edge_progress>0 && world.original_food_cargo(actor.id)==4;});},
        "Full Farm failed to expose actual four-portion outgoing cart return reservation");
    check(!q::active(world,scene.farm) && q::farm(world,scene.farm).spawned==q::farm(world,scene.farm).retired,
        "Visible free bin space bypassed the actual outgoing Cart return reservation");q::valid(world);
}

void blocked_land_edges_repair_and_real_source_access() {
    q::Fixture fixture;
    for(int y=102;y<=110;++y)for(int x=93;x<=97;++x)fixture.raw[std::size_t(y)*228U+std::size_t(x)]=y==107?0x80U:4U;
    fixture.rebind();auto world=fixture.world();
    for(int x=74;x<=92;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,104}});
    for(int x=75;x<=90;x+=3)q::purchase(world,sim::CommandType::PlaceHousehold,{x,102});
    q::purchase(world,sim::CommandType::PlaceFireWatch,{75,105});q::purchase(world,sim::CommandType::PlaceFireWatch,{78,105});
    q::purchase(world,sim::CommandType::PlaceMarket,{81,105});q::purchase(world,sim::CommandType::PlaceOriginalWaterWell,{84,105});
    q::purchase(world,sim::CommandType::PlaceOriginalAncestorShrine,{87,105});
    const auto farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,105});const auto field=q::crop(world,farm,0,{95,107});
    q::source_qualified(world,farm);q::source_qualified(world,field);
    const auto channel=q::farm(world,farm).courier_id;
    q::until(world,[&]{const auto& c=world.courier(channel);return q::phase(world,farm,Phase::Outward) && c.edge_progress>0 && c.path_vertex==0;},"Grower never began real corridor movement");
    const auto begun=world.courier(channel);check(begun.path.size()>=4,"Grower corridor lacks a future removable segment");
    q::denied(world,{sim::CommandType::PlaceIrrigationDitch,begun.path.at(1)});
    const auto cut=begun.path.at(2);f::buy(world,{sim::CommandType::PlaceIrrigationDitch,cut});
    check(world.courier(channel).edge_progress==begun.edge_progress,"Future land cut rewound an already begun Grower edge");
    q::ticks(world,100);check(!q::field(world,field).harvested && !q::receipt_field(world,field).returned &&
        !world.physical_food_grower_cargo(channel),"Blocked outward route became work, harvest or Farm arrival");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cut});
    q::until(world,[&]{const auto& c=world.courier(channel);return world.physical_food_grower_cargo(channel)==1 && c.edge_progress>0 &&
        c.path.size()>c.path_vertex+2;},"Repaired route never earned a real loaded field departure");
    const auto loaded=q::worker(world,farm);const auto returning=world.courier(channel);const auto loaded_cut=returning.path.at(returning.path_vertex+2);
    f::buy(world,{sim::CommandType::PlaceIrrigationDitch,loaded_cut});
    check(world.courier(channel).edge_progress==returning.edge_progress,"Future return cut rewound the Grower's begun loaded edge");q::ticks(world,100);
    check(q::worker(world,farm).grower_id==loaded.grower_id && world.physical_food_grower_cargo(channel)==1 && !q::receipt_field(world,field).returned,
        "Blocked loaded return lost crop/lifetime or falsely credited Farm stock");
    auto replay=fixture.restore(world.snapshot());for(int i=0;i<200;++i){world.tick();replay.tick();}
    check(world.snapshot()==replay.snapshot(),"Blocked physical Grower cargo diverged after exact restore");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,loaded_cut});q::until(world,[&]{return q::receipt_field(world,field).returned>0;},"Land repair did not settle retained physical crop cargo");
    q::until(world,[&]{return q::phase(world,farm,Phase::Working) && q::worker(world,farm).work_ticks>3;},"Source access control never earned partial real work");
    std::vector<sim::Cell> entrances;for(const auto& e:world.building_entrances(farm))if(std::find(entrances.begin(),entrances.end(),e.road_cell)==entrances.end())entrances.push_back(e.road_cell);
    for(const auto cell:entrances)f::buy(world,{sim::CommandType::RemoveRoad,cell});
    const auto work=q::work(world,field).productive_ticks;q::ticks(world,100);
    check(world.building_entrances(farm).empty() && q::work(world,field).productive_ticks==work,"Lost Farm access generated hidden crop work");
    for(const auto cell:entrances)f::buy(world,{sim::CommandType::PlaceRoad,cell});
    q::valid(world);
}

void five_farms_joint_fishing_hunting_and_plantations() {
    q::Fixture fixture(true);auto world=fixture.world();const auto materials=q::material_city(world);
    for(int x=89;x<=123;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,100}});
    for(int y=101;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{89,y}});
    std::array<sim::BuildingId,5> farms{},fields{};constexpr std::array<std::size_t,5> crops{0,2,3,2,3};
    for(std::size_t i=0;i<5;++i){const int x=90+7*static_cast<int>(i);farms[i]=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,97});fields[i]=q::crop(world,farms[i],crops[i],{x+3,97});}
    const auto millet=q::crop(world,farms[0],1,{93,98}),soy=q::crop(world,farms[0],4,{93,99});
    for(const auto farm:farms)q::source_qualified(world,farm);
    for(const auto& field:world.hunting_food_state()->fields)q::source_qualified(world,field.field_id);
    for(int y=38;y<104;++y)f::buy(world,{sim::CommandType::PlaceRoad,{149,y}});
    for(int x=110;x<=139;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,32}});
    for(int y=33;y<38;++y)f::buy(world,{sim::CommandType::PlaceRoad,{139,y}});
    for(int x=139;x<149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,38}});
    for(int x=84;x<149;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,46}});
    const auto quay=q::g::quay(world),tent=q::h::tent(world);
    std::array<bool,5> acted{};bool done=false;
    for(int tick=0;tick<40000 && !done;++tick) {
        world.tick();for(std::size_t i=0;i<5;++i)acted[i]|=q::farm(world,farms[i]).spawned>0 && q::receipt_field(world,fields[i]).returned>0;
        done=std::all_of(acted.begin(),acted.end(),[](bool value){return value;}) && q::receipt_field(world,millet).returned>0 && q::receipt_field(world,soy).returned>0;
        const auto eaten=q::consumed(world);for(const auto value:eaten)done&=value>0;
        done&=q::h::source(world,tent).unloaded>0 && q::p::plant(world,materials.tea_plant).returned>0 && q::p::plant(world,materials.mulberry_plant).returned>0;
        if(tick%257==0)q::valid(world);
    }
    check(done && world.fishing_state()->quays.front().quay_id==quay,"Five physical Farm channels did not coexist with all seven Foods and actual plantation returns");
    q::denied(world,{sim::CommandType::PlaceOriginalFoodFarm,{130,97}});q::valid(world);
}

void real_staff_loss_and_natural_fire_cancel_empty_work() {
    q::Fixture fixture(false,12);auto world=fixture.world();
    for(int x=89;x<=104;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,109}});
    const auto farm=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90,110});const auto field=q::crop(world,farm,0,{93,110});q::source_qualified(world,farm);q::source_qualified(world,field);q::ticks(world,200);
    check(!q::farm(world,farm).spawned,"Unstaffed Farm allocated a free physical Grower");q::purchase(world,sim::CommandType::PlaceHousehold,{90,107});
    q::until(world,[&]{return q::phase(world,farm,Phase::Working) && q::worker(world,farm).work_ticks>5;},"Earned residents did not staff actual field work");
    const auto competitor=q::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{97,110});q::crop(world,competitor,1,{100,110});
    f::buy(world,sim::set_building_workforce_priority(competitor,sim::WorkforcePriority::High));f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::Low));
    const auto work=q::work(world,field).productive_ticks;q::ticks(world,100);
    check(!world.workers_assigned(farm) && world.workers_assigned(competitor)==12 && q::work(world,field).productive_ticks==work && !q::field(world,field).harvested,
        "Lost real Farm staff generated crop work or a harvest");
    q::Fixture fire_fixture;auto burning=fire_fixture.world();const auto scene=q::build(burning);
    q::until(burning,[&]{return q::receipt_field(burning,scene.fields[0]).returned>0;},"Natural fire control lacks an earned crop return");
    for(const auto watch:scene.watches)f::buy(burning,sim::set_building_operation(watch,false));
    q::until(burning,[&]{return burning.building_on_fire(scene.farm);},"Unprotected physical Farm never reached actual fire",12000);
    const auto before=q::farm_work(burning,scene.farm);q::ticks(burning,20);
    check(q::farm_work(burning,scene.farm)==before,"Burning Farm created productive physical field work");
}
}
int main() {
    try{each_crop_earns_real_work_cargo_return_and_house_consumption();three_crop_rotation_pause_partial_work_and_finite_headroom();
        blocked_land_edges_repair_and_real_source_access();five_farms_joint_fishing_hunting_and_plantations();real_staff_loss_and_natural_fire_cancel_empty_work();
        std::cout<<"Physical Food Growers World tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
