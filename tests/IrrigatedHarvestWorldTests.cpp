#include "IrrigatedHarvestFixture.h"
#include "simulation/original/OriginalModelMapping.h"

#include <iostream>
#include <string_view>

namespace {
namespace t=oe_irrigated_harvest_fixture;
namespace sim=t::sim;
using t::check;

template<class Function>void invalid(Function action,const char* why){
    bool refused=false;try{action();}catch(const std::invalid_argument&){refused=true;}
    check(refused,why);
}

void explicit_successor_preserves_defaults_and_exact_restore_selection(){
    t::Fixture fixture;const auto world=fixture.world();
    check(world.rule_version()==37&&sim::profile_rules(sim::RulesProfile::CityV16,37).version==37&&
        std::string_view(sim::irrigated_harvest_contract)=="authored-irrigated-harvest-v1"&&sim::irrigated_harvest_revision==1,
        "Irrigated harvest lost its explicit version or immutable contract marker");
    check(sim::current_rule_version(sim::RulesProfile::CityV16)==4&&sim::current_rule_version(sim::RulesProfile::CityV11)==3,
        "Rule37 changed ordinary menu defaults");
    for(std::uint32_t rule=1;rule<=36;++rule)
        check(!sim::irrigated_harvest_enabled(sim::RulesProfile::CityV16,rule),"Historical rule gained irrigated-harvest admission");
    check(!sim::irrigated_harvest_enabled(sim::RulesProfile::CityV11,37)&&
        !sim::rule_version_supported(sim::RulesProfile::CityV16,39),"Rule37 widened another profile or future version");
    for(const auto object:{sim::Object::OriginalRiceField,sim::Object::OriginalCabbageField,sim::Object::OriginalSoybeanField,
        sim::Object::OriginalHuntersTent,sim::Object::OriginalFishingQuay,sim::Object::OriginalIrrigationPump,
        sim::Object::OriginalMill53,sim::Object::OriginalFoodShop66}) {
        const auto current=openemperor::original::functional_model_candidate(object,37);
        const auto inherited=openemperor::original::functional_model_candidate(object,36);
        check(current.candidate&&current.candidate==inherited.candidate&&current.functional==inherited.functional&&
            current.mechanics_identity==inherited.mechanics_identity&&current.presentation_identity==inherited.presentation_identity,
            "Rule37 lost an explicitly inherited model mapping or changed its evidence status");
    }
    t::valid(world);check(fixture.restore(world.snapshot()).snapshot()==world.snapshot(),"Fresh Rule37 lost exact inherited authority");
    invalid([&]{sim::World::restore_agriculture_reconfiguration(world.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "Historical36 restore API accepted a Rule37 snapshot");
    const auto historical=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    invalid([&]{fixture.restore(historical.snapshot());},"Rule37 restore API migrated historical36");
}

void real_wet_early_harvest_keeps_five_work_ticks_cargo_and_actual_return(){
    t::Fixture fixture;auto world=fixture.world();const auto city=t::build_early_food(world);
    const auto ready=world.ticks();const auto paid=world.building(city.field).placed_tick;
    auto restored=fixture.restore(world.snapshot());
    for(int n=0;n<4;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Ready Rule37 continuation diverged");}
    check(!t::field(world,city.field).harvested&&!t::receipt(world,city.field).returned&&
        t::s::field(world,city.field).harvest_progress==4,"Wet readiness bypassed actual five-tick harvest work");
    world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Early-harvest tick diverged after restore");
    check(world.ticks()==ready+5&&world.ticks()-paid<80&&t::field(world,city.field).harvested==1&&
        t::work(world,city.field).productive_ticks==64&&t::i::work(world,city.field).bonus_work==16&&
        t::s::field(world,city.field).harvest_work==5&&t::q::worker(world,city.farm).harvested_tick==world.ticks()&&
        world.physical_food_grower_cargo(t::q::farm(world,city.farm).courier_id)==1&&!t::receipt(world,city.field).returned,
        "Legally earned early harvest lost actual work, cargo or sole return authority");
    t::valid(world);restored=fixture.restore(world.snapshot());
    t::f::buy(world,sim::set_building_operation(city.farm,false));
    t::f::buy(restored,sim::set_building_operation(city.farm,false));
    for(int n=0;n<80&&!t::receipt(world,city.field).returned;++n){world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot(),"Genuine loaded return changed across Rule37 restore");}
    check(t::receipt(world,city.field).returned==1&&!world.physical_food_grower_cargo(t::q::farm(world,city.farm).courier_id)&&
        t::receipt(world,city.field).last_receipt->irrigation_bonus_at_harvest==16&&
        t::receipt(world,city.field).last_receipt->work_at_harvest==64,
        "Source pause lost or duplicated the physically returned bonus-assisted crop");
    t::valid(world);
}

void actual_earned_work_and_water_forgery_still_fail_closed(){
    t::Fixture fixture;auto world=fixture.world();t::build_early_food(world);t::ticks(world,5);
    const auto genuine=world.snapshot();
    auto hostile=genuine;--hostile.multi_field_agriculture->fields.front().productive_ticks;
    invalid([&]{fixture.restore(hostile);},"An early harvest borrowed one unearned base-work tick");
    hostile=genuine;++hostile.irrigation_productivity->work.front().bonus_work;
    invalid([&]{fixture.restore(hostile);},"An early harvest borrowed unearned rational bonus work");
    hostile=genuine;hostile.irrigation_productivity->work.front().wet_work_ticks+=4;
    invalid([&]{fixture.restore(hostile);},"Potential wet age replaced actual base-work supply history");
    hostile=genuine;++hostile.processed_food->fields.front().harvested;
    invalid([&]{fixture.restore(hostile);},"One actual growth cycle admitted a second harvest");
    hostile=genuine;hostile.seasonal_agriculture->fields.front().harvest_work=4;
    invalid([&]{fixture.restore(hostile);},"Bonus growth replaced the fifth physical harvest-work tick");
    hostile=genuine;hostile.irrigation_productivity.reset();
    invalid([&]{fixture.restore(hostile);},"Rule37 admitted a harvest without complete actual wet-work authority");
    check(world.snapshot()==genuine&&fixture.restore(genuine).snapshot()==genuine,
        "Rejected work/bonus histories changed the original paid World or genuine restore");
}

void historical36_retains_its_exact_stale_bound(){
    t::Fixture fixture;auto current=fixture.world();
    auto historical=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    const auto city=t::build_early_food(current),old=t::build_early_food(historical);
    auto expected=current.snapshot();expected.rule_version=36;
    check(expected==historical.snapshot(),"Rule37 altered paid placement, water, staffing or pre-harvest historical behavior");
    for(int n=0;n<4;++n){current.tick();historical.tick();}
    const auto before=historical.snapshot();
    check(sim::World::restore_agriculture_reconfiguration(before,fixture.policy,fixture.model,fixture.terrain).snapshot()==before,
        "Historical36 pre-harvest save domain changed");
    current.tick();bool refused=false;
    try{historical.tick();}catch(const std::invalid_argument& e){
        refused=std::string_view(e.what()).find("AUTHORED harvest exceeds its paid80-tick opportunities")!=std::string_view::npos;
    }
    check(refused&&t::field(current,city.field).harvested==1&&t::field(historical,old.field).harvested==1&&
        historical.ticks()-historical.building(old.field).placed_tick<80,
        "Historical36 was silently given Rule37 early-harvest admission");
    invalid([&]{sim::World::restore_agriculture_reconfiguration(historical.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "Historical36 restore domain silently widened to the early-harvest state");
    t::valid(current);
}

void neutral_source_preserves_actual_hunter_claims(bool begun){
    t::Fixture fixture;auto world=fixture.world();
    auto historical=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    const auto model=fixture.root.data/"Model/EmperorBuildingModels.txt",map=fixture.root.data/t::f::map_relative;
    const auto model_bytes=t::f::read_text(model),map_bytes=t::f::read_text(map);
    const auto model_time=std::filesystem::last_write_time(model),map_time=std::filesystem::last_write_time(map);
    const auto model_mode=std::filesystem::status(model).permissions(),map_mode=std::filesystem::status(map).permissions();
    const auto build=[](sim::World& target){
        constexpr std::array<std::size_t,0> no_fields{};t::q::build(target,no_fields);
        for(int x=84;x<=88;++x)t::f::buy(target,{sim::CommandType::PlaceRoad,{x,46}});
        t::q::purchase(target,sim::CommandType::PlaceOriginalHuntersTent,{84,44});
    };
    build(world);build(historical);
    std::optional<sim::Command> blocked;sim::Cell claimed{};sim::HuntingHunterId hunter_id{};
    for(unsigned n=0;n<3000&&!blocked;++n){
        for(const auto& hunter:world.hunting_state()->hunters){
            if(begun&&(!hunter.edge_progress||hunter.path_vertex+1>=hunter.path.size()))continue;
            const auto cell=begun?hunter.path.at(hunter.path_vertex+1):hunter.current;
            for(int dy=-1;dy<=0&&!blocked;++dy)for(int dx=-1;dx<=0&&!blocked;++dx){
                sim::Command source{sim::CommandType::PlaceOriginalInfluenceSource,{cell.x+dx,cell.y+dy}};
                if(begun&&sim::building_footprint_contains(world.profile(),world.rule_version(),
                    sim::Object::OriginalInfluenceSource,source.cell,hunter.current))continue;
                // The separately paid historical World establishes ordinary
                // source legality without fabricating an actor or work epoch.
                if(historical.validate(source).accepted){blocked=source;claimed=cell;hunter_id=hunter.id;}
            }
            if(blocked)break;
        }
        if(!blocked){world.tick();historical.tick();}
    }
    check(blocked.has_value(),begun?"No genuine Hunter begun edge exposed a geometrically legal neutral-source footprint":
        "No genuine Hunter current cell exposed a geometrically legal neutral-source footprint");
    const auto actor=std::find_if(world.hunting_state()->hunters.begin(),world.hunting_state()->hunters.end(),
        [&](const auto& row){return row.id==hunter_id;});
    check(actor!=world.hunting_state()->hunters.end()&&actor->launched_tick>0&&
        (!begun||(actor->edge_progress&&actor->path.at(actor->path_vertex+1)==claimed&&actor->current!=claimed)),
        "Neutral-source regression lacks its actual live current/begun Hunter witness");
    const auto before=world.snapshot(),old_before=historical.snapshot();auto expected=before;expected.rule_version=36;
    check(expected==old_before&&sim::World::restore_agriculture_reconfiguration(old_before,fixture.policy,fixture.model,fixture.terrain).snapshot()==old_before,
        "Paid paired historical Hunter control changed before the corrected source refusal");
    const auto quote=world.validate(*blocked),refused=world.execute(*blocked);
    check(!quote.accepted&&!quote.changed&&!refused.accepted&&!refused.changed&&
        quote.reason=="Source footprint overlaps an actual actor or its begun edge"&&world.snapshot()==before,
        "Neutral source overwrote a real Hunter claim or a refusal changed Funds, IDs or paid state");
    const auto old_result=historical.execute(*blocked);
    check(old_result.accepted&&old_result.changed&&historical.building_owner_at(claimed).has_value()&&
        historical.building(*historical.building_owner_at(claimed)).kind==sim::Object::OriginalInfluenceSource,
        "Historical36 neutral-source placement admission silently changed");
    invalid([&]{sim::World::restore_agriculture_reconfiguration(historical.snapshot(),fixture.policy,fixture.model,fixture.terrain);},
        "Historical36 unexpectedly admitted its source-overlapped Hunter on restore");
    const auto path=fixture.root.path/(begun?"hunter-begun53.json":"hunter-current53.json");
    openemperor::persistence::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=t::f::read_text(path);const auto document=openemperor::persistence::read_save(path);
    auto restored=openemperor::persistence::restore_irrigated_harvest(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);
    check(document.source_schema_version==53&&restored.snapshot()==before,"Source refusal damaged actual Hunter save/restore");
    for(int n=0;n<64;++n){world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),"Rejected neutral source changed actual Hunter continuation");}
    check(t::f::read_text(path)==bytes&&t::f::read_text(model)==model_bytes&&t::f::read_text(map)==map_bytes&&
        std::filesystem::last_write_time(model)==model_time&&std::filesystem::last_write_time(map)==map_time&&
        std::filesystem::status(model).permissions()==model_mode&&std::filesystem::status(map).permissions()==map_mode,
        "Hunter source refusal/continuation changed manual save or immutable model/map inputs");
    t::valid(world);
}

void neutral_source_cannot_cover_an_existing_paid_ditch(){
    t::Fixture fixture;auto world=fixture.world();
    auto historical=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    const sim::Cell cell{120,150};for(auto* target:{&world,&historical})t::f::buy(*target,{sim::CommandType::PlaceIrrigationDitch,cell});
    const sim::Command source{sim::CommandType::PlaceOriginalInfluenceSource,cell};const auto before=world.snapshot();
    check(historical.validate(source).accepted,"Historical36 paid ditch/source control unexpectedly changed");
    const auto quote=world.validate(source),refused=world.execute(source);
    check(!quote.accepted&&!refused.accepted&&!refused.changed&&world.snapshot()==before&&
        quote.reason=="Source footprint overlaps existing paid infrastructure"&&fixture.restore(before).snapshot()==before,
        "Neutral source erased paid ditch geometry or changed state while refusing");
}

void neutral_source_preserves_actual_food_grower_claims(bool begun){
    t::Fixture fixture;auto world=fixture.world();
    auto historical=sim::World::create_agriculture_reconfiguration(fixture.policy,fixture.model,fixture.terrain);
    const auto model=fixture.root.data/"Model/EmperorBuildingModels.txt",map=fixture.root.data/t::f::map_relative;
    const auto model_bytes=t::f::read_text(model),map_bytes=t::f::read_text(map);
    const auto model_time=std::filesystem::last_write_time(model),map_time=std::filesystem::last_write_time(map);
    const auto model_mode=std::filesystem::status(model).permissions(),map_mode=std::filesystem::status(map).permissions();
    const auto build=[](sim::World& target){
        constexpr std::array<std::size_t,0> no_fields{};const auto city=t::q::build(target,no_fields);
        t::ticks(target,1700);
        check(target.workers_assigned(city.farm)==target.workforce_required(city.farm),
            "Off-road Grower scene lacks genuinely earned full Farm staffing");
        // The paid range3 field is beyond the Farm footprint. Its worker must
        // cross ordinary empty source land, rather than only Road/body cells
        // that would independently refuse the neutral Well footprint.
        const auto field=t::q::crop(target,city.farm,0,{95,115});t::q::source_qualified(target,field);
        return std::pair{city.farm,field};
    };
    const auto [farm,field]=build(world);const auto old_scene=build(historical);
    check(old_scene==std::pair{farm,field},"Paired historical off-road scene changed paid identities");
    const auto courier_id=t::q::farm(world,farm).courier_id;
    std::optional<sim::Command> blocked;sim::Cell claimed{};
    for(unsigned n=0;n<160&&!blocked;++n){
        const auto& actor=world.courier(courier_id);
        if(actor.phase==sim::CourierPhase::ToWarehouse&&!actor.path.empty()&&actor.path_vertex<actor.path.size()&&
            (begun ? actor.edge_progress>0&&actor.path_vertex+1<actor.path.size():actor.edge_progress==0)) {
            const auto current=actor.path.at(actor.path_vertex);
            const auto cell=begun?actor.path.at(actor.path_vertex+1):current;
            if(world.object_at(cell)==sim::Object::Empty&&!world.building_owner_at(cell)&&!world.road_cell(cell))
                for(int dy=-1;dy<=0&&!blocked;++dy)for(int dx=-1;dx<=0&&!blocked;++dx){
                    sim::Command source{sim::CommandType::PlaceOriginalInfluenceSource,{cell.x+dx,cell.y+dy}};
                    const auto contains=[&](sim::Cell at){return sim::building_footprint_contains(world.profile(),world.rule_version(),
                        sim::Object::OriginalInfluenceSource,source.cell,at);};
                    if(begun&&contains(current))continue;
                    // Isolate this particular courier guard from the separate
                    // animal predicate and from other real moving couriers.
                    if(std::any_of(world.hunting_state()->animals.begin(),world.hunting_state()->animals.end(),
                        [&](const auto& animal){return contains(animal.current)||(animal.next&&contains(*animal.next));}))continue;
                    if(std::any_of(world.couriers().begin(),world.couriers().end(),[&](const auto& other){
                        return other.id!=courier_id&&other.phase!=sim::CourierPhase::IdleAtWorkshop&&!other.path.empty()&&
                            (contains(other.path.at(other.path_vertex))||(other.edge_progress&&other.path_vertex+1<other.path.size()&&
                                contains(other.path[other.path_vertex+1])));}))continue;
                    if(historical.validate(source).accepted){blocked=source;claimed=cell;}
                }
        }
        if(!blocked){world.tick();historical.tick();}
    }
    check(blocked.has_value(),begun?"No actual Food Grower begun edge exposed an otherwise legal empty-land Well footprint":
        "No actual Food Grower current cell exposed an otherwise legal empty-land Well footprint");
    const auto& actor=world.courier(courier_id);const auto& trip=t::q::worker(world,farm);
    check(actor.enabled&&actor.owner==farm&&actor.target==field&&actor.role==sim::CourierRole::OriginalFoodGrower&&
        actor.phase==sim::CourierPhase::ToWarehouse&&trip.field_id==field&&trip.grower_id.value>0&&
        trip.dispatched_tick>world.building(field).placed_tick&&trip.phase==sim::PhysicalFoodGrowerPhase::Outward&&
        !trip.arrived_tick&&!trip.harvested_tick&&!trip.returned_tick&&
        (begun ? actor.edge_progress>0&&actor.path.at(actor.path_vertex+1)==claimed&&actor.path.at(actor.path_vertex)!=claimed:
            actor.edge_progress==0&&actor.path.at(actor.path_vertex)==claimed)&&
        world.grower_navigation_tracks().empty()&&world.fishing_state()->boats.empty()&&world.hunting_state()->hunters.empty(),
        "Neutral-source test lacks its actual isolated paid Food Grower current/begun witness");
    const auto grower_id=trip.grower_id;const auto before=world.snapshot();auto expected=before;expected.rule_version=36;
    check(expected==historical.snapshot()&&historical.validate(*blocked).accepted,
        "Paired historical World did not establish ordinary empty-land Well placement legality");
    const auto quote=world.validate(*blocked),refused=world.execute(*blocked);
    check(!quote.accepted&&!quote.changed&&!refused.accepted&&!refused.changed&&
        quote.reason=="Source footprint overlaps an actual courier or its begun edge"&&
        refused.reason==quote.reason&&world.snapshot()==before,
        "Neutral source overwrote a physical Food Grower claim or changed Funds, IDs, crop work or cargo while refusing");
    const auto path=fixture.root.path/(begun?"grower-begun53.json":"grower-current53.json");
    openemperor::persistence::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=t::f::read_text(path);const auto document=openemperor::persistence::read_save(path);
    auto restored=openemperor::persistence::restore_irrigated_harvest(document,fixture.root.data,fixture.mask(),fixture.policy,fixture.model,fixture.terrain);
    check(document.source_schema_version==53&&document.world==before&&restored.snapshot()==before,
        "Rejected Well changed a real moving Food Grower in schema53");
    const auto wheat_balance=[&]{
        const auto returned=t::receipt(world,field).returned;
        const auto grower_cargo=world.physical_food_grower_cargo(courier_id);
        check(grower_cargo>=0&&grower_cargo<=1&&t::field(world,field).harvested==returned+static_cast<unsigned>(grower_cargo),
            "Actual harvest is neither live Grower cargo nor a physically returned origin");
        const auto& source=t::r::stock(world,farm);
        check(source.amounts[0]>=0&&!source.received[0]&&!source.consumed[0]&&
            returned+source.returned[0]==static_cast<std::uint64_t>(source.amounts[0])+source.departed[0],
            "Farm return origin differs from held Food and actual cart departures/returns");
        std::uint64_t held=0,carried=0,consumed=0,departed=0,received=0,cart_returned=0;
        for(const auto& row:world.processed_food_state()->stocks){
            check(row.amounts[0]>=0,"Actual Wheat ledger contains negative held stock");
            held+=static_cast<std::uint64_t>(row.amounts[0]);consumed+=row.consumed[0];
            departed+=row.departed[0];received+=row.received[0];cart_returned+=row.returned[0];
        }
        for(const auto& row:world.processed_food_state()->trips){
            const auto cargo=row.loaded[0]-row.delivered[0]-row.returned[0];
            check(cargo>=0&&cargo<=4,"Actual Wheat cart cargo is outside its physical capacity");
            carried+=static_cast<std::uint64_t>(cargo);
        }
        check(returned==held+carried+consumed&&departed==received+cart_returned+carried,
            "Returned Wheat origin and physical cart transfer histories disagree with the sole Food ledger");
    };
    wheat_balance();
    bool saw_loaded=false;unsigned continued=0;
    for(;continued<256&&(continued<64||!t::receipt(world,field).returned);++continued){
        world.tick();restored.tick();check(world.snapshot()==restored.snapshot(),
            "Actual Food Grower current/begun refusal changed source-bound continuation");
        wheat_balance();
        if(world.physical_food_grower_cargo(courier_id)){
            saw_loaded=true;check(t::field(world,field).harvested==1&&!t::receipt(world,field).returned&&
                t::r::stock(world,farm).received[0]==0,"Moving Grower credited Food before its actual source return");
        }
    }
    const auto& returned=t::receipt(world,field);
    check(continued>=64&&saw_loaded&&returned.returned==1&&returned.last_receipt&&returned.last_receipt->grower_id==grower_id&&
        returned.last_receipt->returned_tick==world.ticks()&&t::q::worker(world,farm).returned_tick==world.ticks()&&
        t::q::worker(world,farm).delivered==1&&t::field(world,field).harvested==1&&
        t::work(world,field).productive_ticks==80&&t::i::work(world,field).bonus_work==0&&
        t::s::field(world,field).harvest_work==5&&
        t::r::stock(world,farm).received[0]==0&&!world.physical_food_grower_cargo(courier_id),
        "Rejected Well prevented or duplicated the genuine Grower harvest/cargo/source-return chain");
    check(t::f::read_text(path)==bytes&&t::f::read_text(model)==model_bytes&&t::f::read_text(map)==map_bytes&&
        std::filesystem::last_write_time(model)==model_time&&std::filesystem::last_write_time(map)==map_time&&
        std::filesystem::status(model).permissions()==model_mode&&std::filesystem::status(map).permissions()==map_mode,
        "Food Grower refusal/continuation changed canonical save or immutable model/map inputs");
    t::valid(world);
}
} // namespace

int main(){try{
    explicit_successor_preserves_defaults_and_exact_restore_selection();
    real_wet_early_harvest_keeps_five_work_ticks_cargo_and_actual_return();
    actual_earned_work_and_water_forgery_still_fail_closed();
    historical36_retains_its_exact_stale_bound();
    neutral_source_preserves_actual_hunter_claims(false);
    neutral_source_preserves_actual_hunter_claims(true);
    neutral_source_cannot_cover_an_existing_paid_ditch();
    neutral_source_preserves_actual_food_grower_claims(false);
    neutral_source_preserves_actual_food_grower_claims(true);
    std::cout<<"Rule37 actual earned wet harvest, physical return, safe neutral-source claims and exact historical36 behavior passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
