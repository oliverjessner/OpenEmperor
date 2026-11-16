// Negative World/persistence controls for the still unadmitted Original raw
// Food origin. These are not physical Food Grower end-to-end acceptance tests.
#include "IrrigationFixture.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
namespace a=oe_agriculture_fixture;
namespace p=oe_plantations_fixture;
namespace i=oe_irrigation_fixture;
namespace f=oe_influence_fixture;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
using Json=nlohmann::json;
using f::check;

template<class Action> void refuses(Action action,const char* reason) {
    try {action();} catch(const std::exception&) {return;}
    throw std::runtime_error(reason);
}

std::array<sim::BuildingId,5> paid_fields(sim::World& world) {
    const std::array commands{sim::CommandType::PlaceOriginalWheatField,
        sim::CommandType::PlaceOriginalMilletField,sim::CommandType::PlaceOriginalRiceField,
        sim::CommandType::PlaceOriginalCabbageField,sim::CommandType::PlaceOriginalSoybeanField};
    std::array<sim::BuildingId,3> farms{};
    for(std::size_t n=0;n<farms.size();++n)
        farms[n]=a::purchase(world,sim::CommandType::PlaceOriginalFoodFarm,{90+7*static_cast<int>(n),95});
    const std::array<sim::Cell,5> cells{{{93,95},{100,95},{107,95},{107,96},{107,97}}};
    std::array<sim::BuildingId,5> fields{};
    for(std::size_t n=0;n<fields.size();++n) {
        sim::Command command{commands[n],cells[n]};command.building_id=farms[std::min(n,std::size_t{2})];
        f::buy(world,command);fields[n]=*world.building_owner_at(cells[n]);
        check(static_cast<std::size_t>(a::field(world,fields[n]).crop)==n,
              "Paid field construction changed the five distinct crop identities");
    }
    check(world.construction_spent_total()>0,"Negative control did not pay for its Farm/fields");
    return fields;
}

template<class Fixture> void historical_origin_boundary(Fixture& fixture,std::uint32_t rule,unsigned schema) {
    auto world=fixture.world();const auto fields=paid_fields(world);
    const auto source=f::read_text(fixture.root.data/f::map_relative);
    const auto model=f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt");
    // Advance the real World, not a CropGrowerRules DTO with supplied outcomes.
    // With no settlement this is only a negative admission control, not a
    // positive staffing, navigation, harvest or blocked-route demonstration.
    for(int tick=0;tick<102;++tick)world.tick();
    const auto before=world.snapshot();
    check(before.rule_version==rule && !sim::agriculture_production_clock_verified &&
          world.original_production_state()->food==sim::OriginalFoodState{},
          "Historical World acquired a parallel Original Food origin");
    for(const auto field:fields) {
        const auto& row=a::field(world,field);const auto view=world.agriculture_season_status(field);
        check(!row.quantity && !row.productive_events && !row.last_harvest_cycle && view &&
              !view->actual_grower_active && !view->yield_effect_active,
              "Historical Food field acquired unproved raw work or a phantom Grower");
    }
    const auto canonical=fixture.root.path/"historical-grower-boundary.json";
    const auto hostile=fixture.root.path/"unadmitted-grower.json";
    save::write_save(canonical,fixture.document(world),fixture.root.data,fixture.mask());
    const auto bytes=f::read_text(canonical);const auto json=Json::parse(bytes);
    check(json.at("schema_version")==schema && fixture.restore(save::read_save(canonical)).snapshot()==before,
          "Historical raw-origin control changed its canonical version or restored World");
    const auto unchanged=[&] {
        check(world.snapshot()==before && f::read_text(canonical)==bytes &&
              f::read_text(fixture.root.data/f::map_relative)==source &&
              f::read_text(fixture.root.data/"Model/EmperorBuildingModels.txt")==model,
              "Refused raw authority changed World, save or source bytes");
    };
    // Supplying source arithmetic results must not turn existing processed-Food
    // fields into a raw origin, even with an otherwise valid paid identity.
    for(std::size_t n=0;n<fields.size();++n)for(unsigned member=0;member<3;++member) {
        auto document=fixture.document(world);auto& row=document.world.agriculture->fields[n];
        if(member==0)row.quantity=10000;
        if(member==1)row.productive_events=1;
        if(member==2)row.last_harvest_cycle=1;
        refuses([&]{(void)fixture.restore(document);},"Historical restore admitted Original raw field authority");
        refuses([&]{save::write_save(canonical,document,fixture.root.data,fixture.mask());},
                "Historical writer published Original raw field authority");
        unchanged();
    }
    const auto reject=[&](auto edit) {
        auto document=json;edit(document);const auto submitted=document.dump();f::write_text(hostile,submitted);
        refuses([&]{(void)save::read_save(hostile);},"Historical reader admitted unsupported raw Grower authority");
        refuses([&]{save::write_save(hostile,fixture.document(world),fixture.root.data,fixture.mask());},
                "Historical writer overwrote an unsupported raw Grower save");
        check(f::read_text(hostile)==submitted,"Rejected raw Grower target was overwritten");unchanged();
    };
    // Unknown native-looking members cannot supply IDs, pooled cargo, a second
    // stock, claims or counter corrections through historical JSON admission.
    for(const auto* member:{"food_growers","raw_farm_bins","source_counters","raw_cargo"})
        reject([&](auto& j){j["world"]["agriculture"][member]=Json::array({1});});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["assigned_actor_word"]=1;});
    reject([](auto& j){j["world"]["agriculture"]["fields"][0]["harvest_history"]=Json::array({75});});
    reject([](auto& j){j["rules"]["version"]=27;j["schema_version"]=43;});
    auto future=fixture.document(world);future.world.rule_version=27;
    refuses([&]{(void)fixture.restore(future);},"Historical full restore silently selected Rule27");
    refuses([&]{save::write_save(canonical,future,fixture.root.data,fixture.mask());},
            "An unimplemented successor acquired save publication authority");unchanged();
    auto restored=fixture.restore(save::read_save(canonical));
    for(int tick=0;tick<153;++tick) {
        world.tick();restored.tick();
        check(world.snapshot()==restored.snapshot(),"Historical negative-origin continuation diverged");
    }
}
} // namespace

int main() {
    try {
        a::Fixture agriculture;historical_origin_boundary(agriculture,24,40);
        p::Fixture plantations;historical_origin_boundary(plantations,25,41);
        i::Fixture irrigation;historical_origin_boundary(irrigation,26,42);
        std::cout<<"Historical raw Grower admission controls passed (no positive gameplay claim)\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
