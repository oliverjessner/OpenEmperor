#include "IrrigationFixture.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>

namespace {
namespace q=oe_irrigation_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace orig=openemperor::original;
using q::check;

void paid_complete_bodies_and_intake() {
    for(std::uint8_t direction=0;direction<4;++direction) {
        q::Fixture fixture(direction);auto world=fixture.world();
        const auto funds=world.treasury();const auto spent=world.construction_spent_total();
        const auto courier_count=world.couriers().size();
        const auto id=q::pump(world,fixture.origin,direction);
        const auto shape=orig::pump_geometry(fixture.origin,direction);
        check(world.treasury()==funds-17 && world.construction_spent_total()==spent+17 &&
            world.couriers().size()==courier_count,"Pump did not pay its prepared price or created an unsupported courier");
        for(const auto cell:shape.footprint)
            check(world.object_at(cell)==sim::Object::OriginalIrrigationPump && world.building_owner_at(cell)==id,
                "Pump failed to claim every land and water cell of its complete2x2 body");
        for(const auto cell:shape.water) {
            const bool body=std::find(shape.footprint.begin(),shape.footprint.end(),cell)!=shape.footprint.end();
            check(body || !world.building_owner_at(cell),"External intake context incorrectly became occupied body cells");
        }
        const auto status=world.irrigation_pump_status(id);
        check(status.supported && status.intake_valid && status.orientation==direction && !status.staffed && !status.supplying,
            "An unstaffed Pump supplied water or lost its actual physical orientation");
        q::denied(world,sim::demolish_building(id));
        q::denied(world,{sim::CommandType::PlaceIrrigationDitch,shape.land[0]});
        sim::Command wrong{sim::CommandType::PlaceIrrigationPump,fixture.origin};wrong.orientation=4;q::denied(world,wrong);
        wrong.orientation=0;wrong.cell={std::numeric_limits<int>::max(),std::numeric_limits<int>::min()};q::denied(world,wrong);
        q::valid(world);
    }
    q::Fixture broken;
    const auto external=orig::pump_geometry(broken.origin,0).water[0];
    broken.raw[std::size_t(external.y)*228U+std::size_t(external.x)]=0x80;broken.rebind();
    auto world=broken.world();q::denied(world,{sim::CommandType::PlaceIrrigationPump,broken.origin});
}

void atomic_ditch_payment_boundaries_and_crossings() {
    q::Fixture fixture;auto world=fixture.world();
    const std::array<sim::Cell,3> cells{{{114,100},{114,101},{114,102}}};
    const auto before=world.snapshot();const auto batch=world.execute_irrigation_batch(cells);
    check(batch.accepted && batch.changed && world.treasury()==before.treasury-9 &&
        world.irrigation_state()->ditches_placed_total==3 && world.irrigation_state()->ditch_construction_spent==9 &&
        world.command_sequence()==before.command_sequence+3 && world.road_revision()==before.road_revision+3,
        "Atomic ditch gesture did not allocate three paid source-cell identities and one exact total cost");
    const auto installed=world.snapshot();const auto duplicate=world.execute_irrigation_batch(cells);
    check(duplicate.accepted && !duplicate.changed && world.snapshot()==installed,
        "Duplicate ditch gesture spent money, allocated identities or changed chronology");
    const std::array<sim::Cell,2> bad{{{115,101},{0,0}}};
    check(!world.execute_irrigation_batch(bad).accepted && world.snapshot()==installed,
        "A boundary failure partially published an irrigation gesture");
    std::array<sim::Cell,257> excessive{};
    check(!world.execute_irrigation_batch(excessive).accepted && world.snapshot()==installed,
        "A gesture exceeded its existing256-cell budget");
    q::denied(world,{sim::CommandType::RemoveIrrigationDitch,{115,101}});
    q::denied(world,{sim::CommandType::PlaceOriginalWaterWell,{114,100}});
    const auto removed_funds=world.treasury();f::buy(world,{sim::CommandType::RemoveIrrigationDitch,cells[1]});
    check(world.treasury()==removed_funds && world.irrigation_state()->ditch_construction_spent==9 &&
        world.irrigation_state()->ditches_removed_total==1,"Ditch removal refunded construction or discarded paid history");
    q::ditch(world,cells[1]);check(world.irrigation_state()->ditches_placed_total==4 &&
        world.irrigation_state()->ditch_construction_spent==12,"Replaced ditch reused a retired paid identity");

    auto crossing=fixture.world();
    for(int x=110;x<=118;++x) f::buy(crossing,{sim::CommandType::PlaceRoad,{x,106}});
    const std::array<sim::Cell,3> vertical{{{114,105},{114,106},{114,107}}};
    check(crossing.execute_irrigation_batch(vertical).accepted,"Perpendicular paid road/ditch crossing was refused");
    q::denied(crossing,{sim::CommandType::PlaceIrrigationDitch,{113,106}});
    q::denied(crossing,{sim::CommandType::PlaceRoad,{114,105}});
    q::valid(crossing);q::valid(world);
    q::Fixture original;
    original.raw[std::size_t(110)*228U+110U]|=0x800U;original.rebind();
    auto source=original.world();q::denied(source,{sim::CommandType::PlaceIrrigationDitch,{110,110}});
    q::Fixture expensive;auto priced=q::authored_model();
    const auto price=priced.find("202,Irrigation Ditch,{,3,");check(price!=std::string::npos,"Independent ditch price row missing");
    priced.replace(price,std::string("202,Irrigation Ditch,{,3,").size(),"202,Irrigation Ditch,{,1000,");
    f::write_text(expensive.root.data/"Model/EmperorBuildingModels.txt",priced);
    expensive.model=q::save::prepare_original_irrigation_rules_model(expensive.root.data);
    auto poor=expensive.world();std::array<sim::Cell,20> unaffordable{};
    for(std::size_t i=0;i<unaffordable.size();++i) unaffordable[i]={100+static_cast<int>(i),112};
    const auto poor_before=poor.snapshot();const auto cost=poor.validate_irrigation_batch(unaffordable);
    check(!cost.accepted && cost.total_cost>poor.treasury() && !poor.execute_irrigation_batch(unaffordable).accepted &&
        poor.snapshot()==poor_before,"Insufficient aggregate funds partially purchased ditch cells or changed paid identities");
}

void active_pump_phase_network_cut_and_pause() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    const std::array<sim::Cell,10> chain{{{114,100},{114,101},{114,102},{114,103},{114,104},
        {114,105},{114,106},{114,107},{114,108},{114,109}}};
    check(world.execute_irrigation_batch(chain).accepted,"Connected paid ditch chain could not cross the real road");
    q::ditch(world,{121,109});
    check(!world.irrigation_ditch_wet(chain.front()) && !world.irrigation_ditch_wet(chain.back()),
        "Construction or nearby immutable water granted wet state without workers and a source update");
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},"Actual paid residents never staffed a Pump and supplied its connected ditch network");
    const auto wet=world.irrigation_pump_status(scene.pump);
    check(wet.assigned_workers>0 && wet.supplying && wet.last_phase_tick && *wet.last_phase_tick>=28 &&
        (*wet.last_phase_tick-28)%51==0 && !world.irrigation_ditch_wet({121,109}),
        "Water escaped the cardinal paid network or used an unproved update phase");
    const auto field=world.irrigation_field_status({119,109});
    check(field.water_available && field.supplying_pump==scene.pump && !field.yield_effect_active &&
        !world.irrigation_field_status({120,109}).water_available,
        "Observed five-cell query range, supplying-Pump identity or numeric-yield lock changed");
    const auto supply_tick=world.ticks();
    f::buy(world,sim::set_building_operation(scene.pump,false));
    check(!world.irrigation_ditch_wet(chain.front()) && !world.irrigation_field_status({119,109}).water_available &&
        !world.irrigation_state()->pumps.front().eligible_since_tick,
        "Paused Pump retained a current supplied-water grant");
    f::buy(world,sim::set_building_operation(scene.pump,true));
    check(!world.irrigation_ditch_wet(chain.back()) && world.irrigation_state()->pumps.front().eligible_since_tick==supply_tick+1,
        "Pump resume bypassed the next source irrigation phase");
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},"Resumed Pump never reached its next valid phase");
    f::buy(world,{sim::CommandType::RemoveIrrigationDitch,{114,102}});
    check(world.irrigation_ditch_wet(chain.front()) && !world.irrigation_ditch_wet(chain.back()),
        "Removing one physical ditch failed to dry its disconnected downstream component");
    q::ditch(world,{114,102});
    check(!world.irrigation_ditch_wet(chain.back()),"A newly bought reconnecting ditch skipped its phase27 admission");
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},"Reconnected paid ditch never became supplied at a later phase");
    const auto before_road=world.irrigation_state()->pumps.front().eligible_since_tick;
    f::buy(world,{sim::CommandType::RemoveRoad,{116,99}});
    check(world.irrigation_ditch_wet(chain.back()) && world.irrigation_state()->pumps.front().eligible_since_tick==before_road,
        "Road removal introduced an unproved Pump entrance prerequisite");
    f::buy(world,{sim::CommandType::PlaceRoad,{116,99}});
    check(world.irrigation_ditch_wet(chain.back()),"Unrelated road restoration reset an eligible Pump supply epoch");
    const auto restored=fixture.restore(world.snapshot());
    check(restored.snapshot()==world.snapshot() && restored.irrigation_ditch_wet(chain.back()) &&
        restored.irrigation_field_status({119,109}).supplying_pump==scene.pump,
        "Snapshot restore changed paid authority or failed to derive identical supplied-water state");
    q::until(world,[&]{return world.building_on_fire(scene.pump);},"Unprotected paid Pump never entered the existing natural fire lifecycle",3000);
    check(!world.irrigation_ditch_wet(chain.front()) && !world.irrigation_state()->pumps.front().eligible_since_tick &&
        !world.irrigation_pump_status(scene.pump).supplying,
        "A genuinely burning Pump retained an assigned supply epoch or wet network");
    const auto burning=fixture.restore(world.snapshot());
    check(burning.building_on_fire(scene.pump) && !burning.irrigation_ditch_wet(chain.back()) && burning.snapshot()==world.snapshot(),
        "Restoring a genuine burning Pump invented water or changed fire/paid authority");
}

void no_source_no_free_crop_effect_and_historical_boundary() {
    q::Fixture fixture;auto dry=fixture.world();
    const std::array<sim::Cell,3> water_adjacent{{{114,100},{114,101},{114,102}}};
    check(dry.execute_irrigation_batch(water_adjacent).accepted,"Paid water-adjacent dry-control ditches failed");
    q::ticks(dry,180);
    check(!dry.irrigation_ditch_wet(water_adjacent.front()) && !dry.irrigation_field_status({115,101}).water_available,
        "Water proximity supplied a ditch without an installed staffed Pump");
    auto legacy=sim::World::create_original_plantations(fixture.policy,fixture.model,fixture.terrain);
    q::denied(legacy,{sim::CommandType::PlaceIrrigationDitch,{114,100}});
    q::denied(legacy,{sim::CommandType::PlaceIrrigationPump,fixture.origin});
    check(!legacy.irrigation_state() && !legacy.irrigation_field_status({114,100}).supported,
        "Rule25 acquired Rule26 irrigation authority");
    q::valid(legacy);
}
void hostile_pump_payload_and_identity_restore() {
    q::Fixture fixture;auto world=fixture.world();const auto id=q::pump(world);
    const auto original=world.snapshot();
    const auto refuses=[&](auto change) {
        auto bad=original;change(bad);
        bool rejected=false;try {(void)fixture.restore(bad);} catch(const std::exception&) {rejected=true;}
        check(rejected && world.snapshot()==original,"Hostile Pump restore changed live authority or admitted phantom payload/identity");
    };
    refuses([](auto& s){s.buildings.back().output=1;});
    refuses([](auto& s){s.buildings.back().food_stock=1;});
    refuses([](auto& s){s.buildings.back().reserved_incoming=1;});
    refuses([](auto& s){s.buildings.back().progress=1;});
    refuses([](auto& s){s.buildings.back().population=1;});
    refuses([](auto& s){s.buildings.back().service_until_tick=1;});
    refuses([](auto& s){s.irrigation->pumps.front().eligible_since_tick=1;});
    refuses([](auto& s){s.irrigation->pumps.front().orientation=1;});
    refuses([](auto& s){s.irrigation->pumps.push_back(s.irrigation->pumps.front());});
    refuses([&](auto& s){
        sim::CourierSnapshot c;c.id=static_cast<sim::CourierId>(s.next_courier_id++);c.owner=id;c.target=id;
        s.couriers.push_back(c);
    });
    q::denied(world,{sim::CommandType::PlaceRoad,{std::numeric_limits<int>::max(),std::numeric_limits<int>::min()}});
    const std::array<sim::Cell,1> hostile{{{std::numeric_limits<int>::max(),std::numeric_limits<int>::max()}}};
    check(!world.execute_road_batch(hostile).accepted && world.snapshot()==original,
        "Unbounded road-crossing coordinates overflowed or changed an irrigation World");
}
void exterior_intake_bridge_remains_saveable() {
    q::Fixture fixture;auto world=fixture.world();const auto scene=q::build(world);
    const std::array<sim::Cell,3> chain{{{114,100},{114,101},{114,102}}};
    check(world.execute_irrigation_batch(chain).accepted,"Exterior-intake bridge scenario ditch build failed");
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},"Bridge scenario never earned real Pump supply");
    const auto epoch=world.irrigation_state()->pumps.front().eligible_since_tick;
    f::buy(world,{sim::CommandType::PlaceRoad,{112,97}});f::buy(world,{sim::CommandType::PlaceRoad,{117,97}});
    f::buy(world,sim::place_bridge({112,97},{117,97}));
    check(world.bridge_at({114,97}) && world.irrigation_ditch_wet(chain.back()) &&
        world.irrigation_state()->pumps.front().eligible_since_tick==epoch && world.irrigation_pump_status(scene.pump).intake_valid,
        "A legal bridge over unclaimed exterior water invalidated the observed terrain-water intake context");
    const auto restored=fixture.restore(world.snapshot());
    check(restored.snapshot()==world.snapshot() && restored.irrigation_ditch_wet(chain.back()),
        "A legal exterior intake-water bridge produced an unsaveable or unsupplied World");
    f::buy(world,{sim::CommandType::PlaceRoad,{112,98}});f::buy(world,{sim::CommandType::PlaceRoad,{117,98}});
    q::denied(world,sim::place_bridge({112,98},{117,98}));
    auto before=fixture.world();
    f::buy(before,{sim::CommandType::PlaceRoad,{112,97}});f::buy(before,{sim::CommandType::PlaceRoad,{117,97}});
    f::buy(before,sim::place_bridge({112,97},{117,97}));(void)q::pump(before);
    check(fixture.restore(before.snapshot()).snapshot()==before.snapshot(),
        "Buying a Pump beside an already paid exterior-water bridge changed its exact restore contract");
    q::valid(world);q::valid(before);
}
void signed_heights_preserve_source_irrigation_and_save() {
    q::Fixture fixture;auto permissions=fixture.policy->cells();
    const auto shape=orig::pump_geometry(fixture.origin,0);
    for(std::size_t i=0;i<shape.water.size();++i)
        permissions[std::size_t(shape.water[i].y)*228U+std::size_t(shape.water[i].x)].height=static_cast<std::int8_t>(-20+static_cast<int>(i));
    permissions[std::size_t(shape.land[0].y)*228U+std::size_t(shape.land[0].x)].height=-3;
    permissions[std::size_t(shape.land[1].y)*228U+std::size_t(shape.land[1].x)].height=7;
    const std::array<sim::Cell,10> chain{{{114,100},{114,101},{114,102},{114,103},{114,104},
        {114,105},{114,106},{114,107},{114,108},{114,109}}};
    for(std::size_t i=0;i<chain.size();++i)
        permissions[std::size_t(chain[i].y)*228U+std::size_t(chain[i].x)].height=static_cast<std::int8_t>(-8+2*static_cast<int>(i));
    fixture.policy=std::make_shared<const sim::MapPermissions>(228,228,3,std::move(permissions),std::vector<sim::FixedGatePassage>{});
    auto world=fixture.world();const auto scene=q::build(world);
    check(world.execute_irrigation_batch(chain).accepted,"Permitted signed-height ditch cells acquired an unproved flat-height veto");
    q::until(world,[&]{return world.irrigation_ditch_wet(chain.back());},"Valid signed-height Pump/ditch geometry could not supply its cardinal network");
    check(world.irrigation_pump_status(scene.pump).intake_valid && world.map_permissions()->cell_height(shape.land[0])==-3 &&
        world.map_permissions()->cell_height(shape.land[1])==7 && !fixture.policy->transport_edge_allowed(chain[0],chain[1]),
        "Irrigation flattened signed source heights or weakened the separate shared transport-edge contract");
    const auto snapshot=world.snapshot();const auto restored=fixture.restore(snapshot);
    check(restored.snapshot()==snapshot && restored.irrigation_ditch_wet(chain.back()) &&
        restored.map_permissions()->cell_height(shape.water[0])==-20,
        "Signed-height Pump/ditch snapshot restore changed source geometry or supplied water");
    const auto path=fixture.root.path/"signed-height-irrigation.json";
    q::save::write_save(path,fixture.document(world),fixture.root.data,fixture.mask());
    const auto loaded=fixture.restore(q::save::read_save(path));
    check(loaded.snapshot()==snapshot && loaded.irrigation_ditch_wet(chain.back()) &&
        loaded.map_permissions()->cell_height(shape.land[0])==-3,
        "Schema42 save/load lost signed-height permissions or derived irrigation state");
}
void inherited_rule25_food_and_material_production() {
    q::Fixture fixture;auto text=q::authored_model();
    // Positive independently authored demand inputs: the actual paid House
    // must receive Food, Water and Ancestor service before its luxury stage.
    for(const unsigned field:{3U,8U,21U}) oe_production_fixture::set_house_field(text,2,field,1);
    f::write_text(fixture.root.data/"Model/EmperorBuildingModels.txt",text);
    fixture.model=q::save::prepare_original_irrigation_rules_model(fixture.root.data);
    auto world=fixture.world();
    auto historical=sim::World::create_original_plantations(fixture.policy,fixture.model,fixture.terrain);
    struct ProductiveScene {
        oe_plantations_fixture::Scene materials;
        sim::BuildingId wheat_field{},millet_field{};
    };
    const auto build=[](sim::World& city) {
        ProductiveScene scene;scene.materials=q::p::build(city);
        for(int x=19;x<=32;++x) f::buy(city,{sim::CommandType::PlaceRoad,{x,20}});
        for(int y=21;y<24;++y) f::buy(city,{sim::CommandType::PlaceRoad,{19,y}});
        const auto wheat=q::purchase(city,sim::CommandType::PlaceOriginalFoodFarm,{20,17});
        const auto millet=q::purchase(city,sim::CommandType::PlaceOriginalFoodFarm,{27,17});
        sim::Command crop{sim::CommandType::PlaceOriginalWheatField,{23,17}};crop.building_id=wheat;
        f::buy(city,crop);scene.wheat_field=*city.building_owner_at(crop.cell);
        crop={sim::CommandType::PlaceOriginalMilletField,{30,17}};crop.building_id=millet;
        f::buy(city,crop);scene.millet_field=*city.building_owner_at(crop.cell);return scene;
    };
    const auto scene=build(world);(void)build(historical);
    const auto same_historical=[&] {
        auto normalized=world.snapshot();normalized.rule_version=sim::kOriginalPlantationsRuleVersion;normalized.irrigation.reset();
        check(normalized==historical.snapshot(),"Rule26 changed inherited Rule25 food, material, workforce, transport, fire or household authority");
    };
    const auto produced=[&] {
        bool wheat_consumed=false,millet_consumed=false;
        for(const auto& row:world.original_agriculture_state()->stocks) if(sim::original_house_kind(world.building(row.building_id).kind)) {
            wheat_consumed|=row.consumed[0]>0;millet_consumed|=row.consumed[1]>0;
        }
        return q::a::field(world,scene.wheat_field).harvested>0 && q::a::field(world,scene.millet_field).harvested>0 &&
            wheat_consumed && millet_consumed && q::p::plant(world,scene.materials.tea_plant).returned>0 &&
            q::p::plant(world,scene.materials.lacquer_plant).returned>0 && q::p::plant(world,scene.materials.mulberry_plant).returned>0;
    };
    same_historical();
    for(int i=0;i<16000 && !produced();++i) {world.tick();historical.tick();same_historical();}
    check(produced(),"Rule26 did not retain actual Wheat/Millet harvest/House consumption and Tea/Lacquer/Mulberry pickup-return production");
    check(q::p::industry(world,scene.materials.tea_source).completed>0 &&
        q::p::industry(world,scene.materials.lacquer_source).completed>0 && q::p::industry(world,scene.materials.mulberry_source).completed>0,
        "Returned material plants failed to credit their inherited authoritative production accounts");
    for(const auto& field:world.original_agriculture_state()->fields)
        check(!field.quantity && !field.productive_events && !field.last_harvest_cycle,
            "Rule26 relabeled its inherited authored Wheat/Millet adapter as Original crop timing");
    check(world.original_production_state()->food==sim::OriginalFoodState{},"Rule26 duplicated typed Food into the historical two-crop ledger");
    q::p::valid(world);q::p::valid(historical);
}
} // namespace

int main() {
    try {
        paid_complete_bodies_and_intake();atomic_ditch_payment_boundaries_and_crossings();
        active_pump_phase_network_cut_and_pause();no_source_no_free_crop_effect_and_historical_boundary();
        hostile_pump_payload_and_identity_restore();exterior_intake_bridge_remains_saveable();
        signed_heights_preserve_source_irrigation_and_save();inherited_rule25_food_and_material_production();
        std::cout<<"Paid irrigation: complete Pump bodies/intakes, atomic ditch accounting, phase27 supply, crossings, cuts, pause and historical isolation PASS\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Irrigation World FAIL: "<<error.what()<<'\n';return 1;
    }
}
