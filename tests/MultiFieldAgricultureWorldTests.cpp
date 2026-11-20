#include "MultiFieldAgricultureFixture.h"

#include <iostream>

namespace {
namespace n=oe_multi_field_fixture;
namespace h=oe_hunting_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
using f::check;

bool all(const auto& flags) {return std::all_of(flags.begin(),flags.end(),[](bool v){return v;});}
void paid_fields_bounds_and_historical_one_field_contract() {
    n::Fixture fixture;auto world=fixture.world();
    for(int x=19;x<=80;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
    const auto first=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    const auto funds=world.treasury();
    const auto wheat=n::crop(world,first,0,{23,25});
    check(world.treasury()<funds && !n::work(world,wheat).productive_ticks && !n::field(world,wheat).harvested,
        "Field construction did not charge funds or granted Food/work");
    for(int i=1;i<8;++i)n::crop(world,first,static_cast<std::size_t>(i)%5,
        i<6 ? sim::Cell{23,25+i}:sim::Cell{24,25+i-6});
    sim::Command ninth{sim::CommandType::PlaceOriginalCabbageField,{24,27}};ninth.building_id=first;n::denied(world,ninth);
    for(int farm=1;farm<4;++farm) {
        const int x=20+12*farm;const auto id=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{x,25});
        for(int i=0;i<8;++i)n::crop(world,id,static_cast<std::size_t>(i)%5,
            i<6 ? sim::Cell{x+3,25+i}:sim::Cell{x+4,25+i-6});
    }
    const auto fifth=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{68,25});
    sim::Command global{sim::CommandType::PlaceOriginalWheatField,{71,25}};global.building_id=fifth;n::denied(world,global);
    n::denied(world,{sim::CommandType::PlaceOriginalFoodFarm,{80,25}});
    check(world.hunting_food_state()->fields.size()==32 && world.hunting_food_state()->farms.size()==5,
        "Multi-field mode changed the bounded global Farm/Field counts");
    n::denied(world,sim::demolish_building(wheat));n::denied(world,sim::demolish_building(first));
    n::valid(world);
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Bounded paid fields did not restore exactly");

    n::Fixture poor_rice;poor_rice.fertility[25U*228U+23U]=99;poor_rice.rebind();auto dry=poor_rice.world();
    const auto dry_farm=n::purchase(dry,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    sim::Command rice{sim::CommandType::PlaceOriginalRiceField,{23,25}};rice.building_id=dry_farm;n::denied(dry,rice);
    sim::Command far{sim::CommandType::PlaceOriginalWheatField,{26,25}};far.building_id=dry_farm;n::denied(dry,far);
    sim::Command foreign{sim::CommandType::PlaceOriginalWheatField,{23,25}};foreign.building_id=static_cast<sim::BuildingId>(9999);n::denied(dry,foreign);

    h::Fixture historical(false);auto old=historical.world();
    const auto old_farm=h::purchase(old,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
    const auto old_field=n::crop(old,old_farm,0,{23,25});
    sim::Command second{sim::CommandType::PlaceOriginalMilletField,{23,26}};second.building_id=old_farm;h::denied(old,second);
    h::denied(old,sim::set_building_operation(old_field,false));
    check(!old.multi_field_agriculture_state() && old.rule_version()==29,"Historical Rule29 acquired new field authority");h::valid(old);
}

void five_crop_variations_use_one_farm_labor_and_physical_food() {
    for(std::size_t variant=0;variant<5;++variant) {
        n::Fixture fixture;auto world=fixture.world();
        const std::array<std::size_t,3> crops{variant,(variant+1)%5,(variant+2)%5};const auto scene=n::build(world,crops);
        check(world.workforce_required(scene.farm)==12 && !world.total_population(),"Additional fields granted residents or extra labor");
        std::array<bool,3> farm_moved{},house_moved{},market_arrived{},house_arrived{},quality{};
        bool staffed=false,done=false;std::uint64_t previous=0;
        for(int tick=0;tick<24000 && !done;++tick) {
            world.tick();const auto work=n::farm_work(world,scene.farm);
            check(work>=previous && work-previous<=1,"A single Farm created more than one shared work unit in a tick");previous=work;
            staffed|=world.workers_assigned(scene.farm)==12;
            for(std::size_t i=0;i<3;++i) {
                market_arrived[i]|=h::stock(world,scene.market).received[crops[i]]>0;
                for(const auto home:scene.homes) {
                    const auto& stock=h::stock(world,home);house_arrived[i]|=stock.received[crops[i]]>0;
                    quality[i]|=stock.quality_witness && stock.quality_witness->accepted[crops[i]]>0;
                }
            }
            for(const auto& trip:world.hunting_food_state()->trips) {
                const auto& actor=world.courier(trip.transfer.courier_id);
                if(trip.phase!=sim::OriginalGoodsTransferPhase::Loaded || actor.phase!=sim::CourierPhase::ToWarehouse || !actor.edge_progress)continue;
                for(std::size_t i=0;i<3;++i)if(trip.loaded[crops[i]]>0) {
                    if(actor.role==sim::CourierRole::OriginalFoodToMarket)farm_moved[i]=true;
                    if(actor.role==sim::CourierRole::OriginalFoodToHouse)house_moved[i]=true;
                }
            }
            const auto eaten=n::consumed(world);done=staffed && all(farm_moved) && all(house_moved) && all(market_arrived) && all(house_arrived) && all(quality);
            for(const auto index:crops)done&=eaten[index]>0;
            if(tick%257==0)n::valid(world);
        }
        check(done,"One paid Farm did not physically supply all three distinct crops through Market and House consumption");
        std::uint64_t low=UINT64_MAX,high=0;
        for(const auto id:scene.fields) {
            const auto work=n::work(world,id).productive_ticks;low=std::min(low,work);high=std::max(high,work);
            check(n::field(world,id).harvested>0,"A bound sibling field never independently harvested");
        }
        check(high-low<=1,"Unblocked sibling fields did not share deterministic fair labor");
        check(world.hunting_food_state()->farms.size()==1 && !world.irrigation_state()->pumps.size(),
            "Three-crop test received an extra Farm or universal irrigation prerequisite");
        const auto paused=scene.fields.front();f::buy(world,sim::set_building_operation(paused,false));
        const auto stopped=n::work(world,paused).productive_ticks;const auto active=n::work(world,scene.fields[1]).productive_ticks;
        n::ticks(world,47);
        check(n::work(world,paused).productive_ticks==stopped && n::work(world,scene.fields[1]).productive_ticks>active,
            "Paused crop advanced or blocked its operating siblings");
        f::buy(world,sim::set_building_operation(paused,true));
        n::until(world,[&]{return n::work(world,paused).productive_ticks>stopped;},"Resumed sibling never regained a fair work turn");
        const auto state=*world.multi_field_agriculture_state();f::buy(world,sim::set_building_operation(scene.farm,false));n::ticks(world,99);
        check(*world.multi_field_agriculture_state()==state,"Paused Farm changed retained per-field work or cursor");
        f::buy(world,sim::set_building_operation(scene.farm,true));
        n::valid(world);
        if(!variant) {
            const auto second=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{48,25});
            for(std::size_t i=0;i<3;++i)n::crop(world,second,crops[i],{51,25+static_cast<int>(i)});
            std::uint64_t first_before=n::farm_work(world,scene.farm),second_before=0;
            bool both_staffed=false,second_delivered=false;
            for(int tick=0;tick<12000 && !second_delivered;++tick) {
                world.tick();const auto a=n::farm_work(world,scene.farm),b=n::farm_work(world,second);
                check(a-first_before<=1 && b-second_before<=1,"Multiple Farms multiplied another Farm's per-tick labor budget");
                first_before=a;second_before=b;
                both_staffed|=world.workers_assigned(scene.farm)==12 && world.workers_assigned(second)==12;
                for(const auto& trip:world.hunting_food_state()->trips)
                    second_delivered|=trip.source==second && trip.phase==sim::OriginalGoodsTransferPhase::Completed && h::g::volume(trip.delivered)>0;
            }
            check(both_staffed && second_delivered && n::farm_work(world,second)>0,"Second paid multi-field Farm failed real concurrent staffing, work and delivery");
            n::valid(world);
        }
    }
}

void full_source_loaded_cut_and_access_recovery() {
    n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world);
    n::until(world,[&]{return n::consumed(world)[0]>0 && n::consumed(world)[1]>0 && n::consumed(world)[4]>0;},"Three crops did not establish real consumption");
    f::buy(world,sim::set_building_operation(scene.market,false));
    n::until(world,[&]{return h::g::volume(h::stock(world,scene.farm).amounts)==8;},"Disabled Market did not exert finite Farm backpressure");
    const auto full=*world.multi_field_agriculture_state();n::ticks(world,200);
    check(*world.multi_field_agriculture_state()==full && h::g::volume(h::stock(world,scene.farm).amounts)==8,
        "Full source changed per-field work, cursor or physical stock");
    f::buy(world,sim::set_building_operation(scene.market,true));sim::CourierId id{};
    n::until(world,[&]{for(const auto& actor:world.couriers())
        if(actor.owner==scene.farm && actor.role==sim::CourierRole::OriginalFoodToMarket && actor.phase==sim::CourierPhase::ToWarehouse &&
            actor.edge_progress>0 && actor.path_vertex==0 && actor.path.size()>4 && world.original_food_cargo(actor.id)>0){id=actor.id;return true;}return false;},
        "Reopened Market did not earn an actual loaded Farm cart");
    const auto moving=world.courier(id);const auto cargo=n::trip(world,id);const auto cut=moving.path.at(moving.path_vertex+3);
    f::buy(world,{sim::CommandType::RemoveRoad,cut});
    check(world.courier(id).edge_progress==moving.edge_progress && n::trip(world,id).loaded==cargo.loaded,
        "Future road cut rewound the begun edge or changed typed cargo");
    n::ticks(world,150);
    check(n::trip(world,id).transfer==cargo.transfer && n::trip(world,id).phase==sim::OriginalGoodsTransferPhase::Loaded &&
        n::trip(world,id).loaded==cargo.loaded && n::trip(world,id).delivered==sim::HuntingFoodAmounts{},
        "Blocked Food cart delivered, discarded or converted its physical load");
    check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Blocked multi-field cargo was not exactly restorable");
    f::buy(world,{sim::CommandType::PlaceRoad,cut});
    n::until(world,[&]{const auto load=n::trip(world,id);return load.transfer==cargo.transfer && load.phase==sim::OriginalGoodsTransferPhase::Completed;},
        "Paid road repair failed to deliver preserved cart cargo");
    check(n::trip(world,id).delivered==cargo.loaded,"Repaired route changed the actual delivered crop mix");

    std::vector<sim::Cell> entrances;for(const auto& entrance:world.building_entrances(scene.farm))
        if(std::find(entrances.begin(),entrances.end(),entrance.road_cell)==entrances.end())entrances.push_back(entrance.road_cell);
    for(const auto cell:entrances)f::buy(world,{sim::CommandType::RemoveRoad,cell});
    check(!entrances.empty() && world.building_entrances(scene.farm).empty(),"Source access control left a road entrance");
    const auto disconnected=*world.multi_field_agriculture_state();n::ticks(world,200);
    check(*world.multi_field_agriculture_state()==disconnected,"Disconnected Farm created field labor");
    // Houses retain already delivered portions across a source road cut.
    // The inherited40/397 consumption phases span816 ticks; observe actual
    // physical exhaustion before checking a full cycle of missed demand.
    n::until(world,[&]{
        for(const auto& stock:world.hunting_food_state()->stocks)
            if(stock.building_id!=scene.farm && h::g::volume(stock.amounts))return false;
        for(const auto& actor:world.couriers())if(world.original_food_cargo(actor.id))return false;
        return true;
    },"Disconnected Food did not physically drain from Market, House and cart accounts",16000);
    const auto missed=[&]{std::uint64_t result=0;for(const auto& stock:world.hunting_food_state()->stocks)result+=stock.missed_demands;return result;};
    const auto starved=n::consumed(world);
    const auto retained=h::stock(world,scene.farm).amounts;
    const auto missed_before=missed();n::ticks(world,816);
    check(n::consumed(world)==starved && missed()>missed_before && *world.multi_field_agriculture_state()==disconnected &&
        h::stock(world,scene.farm).amounts==retained,"Physically exhausted city failed to starve or disconnected Farm changed work/stock");
    for(const auto cell:entrances)f::buy(world,{sim::CommandType::PlaceRoad,cell});
    n::until(world,[&]{const auto eaten=n::consumed(world);return eaten[0]>starved[0] && eaten[1]>starved[1] && eaten[4]>starved[4];},
        "Paid access repair did not recover all three real Food sinks");
}

void actual_staff_competition_and_natural_fire() {
    {
        n::Fixture fixture(12);auto world=fixture.world();
        for(int x=19;x<=34;++x)f::buy(world,{sim::CommandType::PlaceRoad,{x,24}});
        const auto farm=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{20,25});
        for(std::size_t crop=0;crop<3;++crop)n::crop(world,farm,crop,{23,25+static_cast<int>(crop)});
        n::ticks(world,200);check(!world.total_population() && !n::farm_work(world,farm),"Unstaffed paid fields generated Food/work");
        (void)n::purchase(world,sim::CommandType::PlaceHousehold,{20,22});
        n::until(world,[&]{return world.workers_assigned(farm)==12 && n::farm_work(world,farm)>10;},"Real immigration did not staff one multi-field Farm");
        const auto competitor=n::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{27,25});
        (void)n::crop(world,competitor,4,{30,25});
        f::buy(world,sim::set_building_workforce_priority(competitor,sim::WorkforcePriority::High));
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::Low));
        check(world.total_population()==12 && !world.workers_assigned(farm) && world.workers_assigned(competitor)==12,
            "Priority competition failed to remove the actual finite Farm workforce");
        const auto before=n::farm_work(world,farm);n::ticks(world,100);
        check(n::farm_work(world,farm)==before && n::farm_work(world,competitor)>0,"Unstaffed Farm worked or staffed competing Farm failed");
        f::buy(world,sim::set_building_workforce_priority(farm,sim::WorkforcePriority::High));
        f::buy(world,sim::set_building_workforce_priority(competitor,sim::WorkforcePriority::Low));
        n::until(world,[&]{return n::farm_work(world,farm)>before;},"Restored paid staffing did not resume retained field work");
    }
    {
        n::Fixture fixture;auto world=fixture.world();const auto scene=n::build(world);
        n::until(world,[&]{return n::consumed(world)[4]>0;},"Natural fire control never earned Food");
        for(const auto id:scene.watches)f::buy(world,sim::set_building_operation(id,false));
        n::until(world,[&]{return world.building_on_fire(scene.farm);},"Unprotected Farm never reached an actual natural fire",12000);
        bool witnessed=false;for(int tick=0;tick<20 && world.building_on_fire(scene.farm);++tick) {
            const auto before=*world.multi_field_agriculture_state();world.tick();
            if(world.building_on_fire(scene.farm)){check(*world.multi_field_agriculture_state()==before,"Burning Farm changed field work or cursor");witnessed=true;}
        }
        check(witnessed,"Actual Farm fire had no observed stopped work interval");
        n::valid(world);
    }
}
}
int main() {
    try {paid_fields_bounds_and_historical_one_field_contract();five_crop_variations_use_one_farm_labor_and_physical_food();
        full_source_loaded_cut_and_access_recovery();actual_staff_competition_and_natural_fire();
        std::cout<<"Multi-field Agriculture World tests passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
