#include "simulation/World.h"
#include "simulation/RoadConnectivity.h"
#include "maps/MapRulesCheck.h"
#include "persistence/SandboxSave.h"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace maps=openemperor::maps;
namespace save=openemperor::persistence;

void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class F> void rejects(F action,std::string_view expected) {
    try { action(); } catch (const std::exception& error) {
        if (std::string_view(error.what()).find(expected)==std::string_view::npos)
            throw std::runtime_error(std::string("Wrong Rule-7 refusal: ")+error.what());
        return;
    }
    throw std::runtime_error("Rule-7 boundary accepted invalid input");
}

constexpr std::array profiles{
    sim::RulesProfile::LogisticsV1,sim::RulesProfile::ProductionV2,
    sim::RulesProfile::HouseholdV3,sim::RulesProfile::SettlementV4,
    sim::RulesProfile::IndustryV5,sim::RulesProfile::CityV6,
    sim::RulesProfile::CityV7,sim::RulesProfile::CityV8,
    sim::RulesProfile::CityV9,sim::RulesProfile::CityV10,
    sim::RulesProfile::CityV11,sim::RulesProfile::CityV12,
    sim::RulesProfile::CityV13,sim::RulesProfile::CityV14,
    sim::RulesProfile::CityV15,sim::RulesProfile::CityV16};

constexpr int width=24,height=16;
std::shared_ptr<const sim::MapPermissions> policy(std::uint32_t version,
                                                bool infrastructure=false) {
    std::vector<sim::MapCellPermission> cells(width*height,
        {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});
    if (infrastructure) {
        for (int y=0;y<height;++y) for (int x=0;x<8;++x)
            cells[static_cast<std::size_t>(y*width+x)].height=1;
        for (int y=0;y<height;++y) {
            auto& water=cells[static_cast<std::size_t>(y*width+11)];
            water.road_allowed=water.building_allowed=false;
            water.bridge_water=version==3;
        }
    }
    return std::make_shared<const sim::MapPermissions>(width,height,version,
        std::move(cells),std::vector<sim::FixedGatePassage>{});
}

void version_matrix() {
    for (const auto profile:profiles) {
        const auto current=profile==sim::RulesProfile::ProductionV2 ? 2U:
            profile==sim::RulesProfile::CityV11 ? 3U:
            profile==sim::RulesProfile::CityV16 ? 4U:1U;
        check(sim::current_rule_version(profile)==current,"Legacy default rule changed");
        for (std::uint32_t version=0;version<=24;++version) {
            const bool expected=profile==sim::RulesProfile::ProductionV2 ? version==2:
                profile==sim::RulesProfile::CityV11 ? version>=1 && version<=4:
                profile==sim::RulesProfile::CityV16 ? (version>=1 && version<=15) || version==16 || version==17 || version==18 || version==19 || version==20 || version==21 || version==22 || version==23:version==1;
            check(sim::rule_version_supported(profile,version)==expected,
                "Explicit profile/rule support matrix changed");
            check(sim::original_economy_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==7 || version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Original-economy gate escaped its exact profile/version");
            check(sim::original_influence_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==8 || version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Original-influence gate escaped its exact profile/version");
            check(sim::original_goods_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==9 || version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Original-goods gate escaped its exact profile/version");
            check(sim::original_production_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==10 || version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Original-production gate escaped its exact profile/version");
            check(sim::original_fire_protection_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==11 || version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Original-fire-protection gate escaped its explicit Rule11/12/13/14/15 opt-ins");
            check(sim::original_sustainable_economy_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==12 || version==13 || version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Sustainable economy escaped its explicit Rule12/13/14/15 opt-ins");
            check(sim::original_safe_demolition_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==19),
                "Safe demolition escaped its exact Rule19 opt-in");
            check(sim::original_music_venues_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==22 || version==23)),
                "Music venues escaped their Rule22/23 capability");
            check(sim::original_venue_house_service_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==23),
                "Venue House service escaped its exact Rule23 opt-in");
            check(sim::original_deadline_food_dispatch_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==18),
                "Deadline-aware Food dispatch escaped its exact Rule18 opt-in");
            check(sim::original_deadline_service_dispatch_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==17),
                "Deadline-aware service dispatch escaped its exact Rule17 opt-in");
            check(sim::original_deadline_fire_dispatch_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==16),
                "Deadline-aware fire dispatch escaped its exact Rule16 opt-in");
            check(sim::original_inspector_coordination_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==15),
                "Inspector coordination escaped its exact Rule15 opt-in");
            check(sim::original_cardinal_water_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==14),
                "Historical exact Rule14 identity changed");
            check(sim::original_cardinal_terrain_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && (version==14 || version==15 || version==16 || version==17 || version==18 || version==19 || version==22 || version==23)),
                "Cardinal terrain capability escaped its explicit Rule14/15 opt-ins");
            check(sim::bridges_rule_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && ((version>=5 && version<=15) || version==16 || version==17 || version==18 || version==19 || version==20 || version==21 || version==22 || version==23)),
                "Bridge gate inherited an unknown profile/version");
            check(sim::ramps_rule_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && ((version>=6 && version<=15) || version==16 || version==17 || version==18 || version==19 || version==20 || version==21 || version==22 || version==23)),
                "Ramp gate inherited an unknown profile/version");
            check(sim::source_cell_ramps_enabled(profile,version)==
                (profile==sim::RulesProfile::CityV16 && version==20),
                "Source-cell ramps escaped their exact classic Rule20 opt-in");
            if (!expected) {
                rejects([&] { (void)sim::profile_rules(profile,version); },"unsupported");
                continue;
            }
            const auto rules=sim::profile_rules(profile,version);
            if (profile==sim::RulesProfile::CityV16 && version>=2)
                check(rules==sim::ProfileRules{version,32,64,32,5,800},
                    "City-v16 timing/prices contract changed during version reservation");
        }
        check(!sim::rule_version_supported(profile,UINT32_MAX) &&
            !sim::original_economy_enabled(profile,UINT32_MAX) &&
            !sim::original_influence_enabled(profile,UINT32_MAX) &&
            !sim::original_goods_enabled(profile,UINT32_MAX) &&
            !sim::original_production_enabled(profile,UINT32_MAX) &&
            !sim::original_fire_protection_enabled(profile,UINT32_MAX) &&
            !sim::original_deadline_fire_dispatch_enabled(profile,UINT32_MAX) &&
            !sim::original_deadline_service_dispatch_enabled(profile,UINT32_MAX) &&
            !sim::original_deadline_food_dispatch_enabled(profile,UINT32_MAX) &&
            !sim::original_safe_demolition_enabled(profile,UINT32_MAX) &&
            !sim::original_music_venues_enabled(profile,UINT32_MAX) &&
            !sim::original_venue_house_service_enabled(profile,UINT32_MAX) &&
            !sim::bridges_rule_enabled(profile,UINT32_MAX) &&
            !sim::ramps_rule_enabled(profile,UINT32_MAX),
            "Maximum unknown rule version activated a reserved gate");
    }
    const sim::World implicit(4,4,std::vector<std::uint8_t>(16,1));
    check(implicit.profile()==sim::RulesProfile::LogisticsV1 && implicit.rule_version()==1,
        "Default World constructor changed its legacy profile");
    sim::World city(policy(2));
    check(city.profile()==sim::RulesProfile::CityV16 && city.rule_version()==4 &&
        city.treasury()==1300,"City-v16 default became an unfinished economy");
    for (const std::uint32_t version:{4U,5U,6U}) {
        sim::World old(policy(version==4 ? 2:3),sim::RulesProfile::CityV16,version);
        check(old.rule_version()==version && old.active_rules().version==version &&
            old.treasury()==1300 && old.bridges_supported()==(version>=5) &&
            old.ramps_supported()==(version==6) && !old.original_economy_enabled(),
            "Historical rule4/5/6 identity changed");
        check(sim::World::restore(old.snapshot(),old.map_permissions()).snapshot()==old.snapshot(),
            "Historical rule4/5/6 snapshot restore changed");
        for (const std::uint32_t permissions_version:{1U,2U,3U}) {
            if (permissions_version==(version==4 ? 2U:3U)) continue;
            rejects([&] { sim::World denied(policy(permissions_version),
                sim::RulesProfile::CityV16,version); },"");
        }
        rejects([&] { sim::World denied(width,height,std::vector<std::uint8_t>(width*height,1),
            sim::RulesProfile::CityV16,version); },"");
    }
}

void place(sim::World& world,sim::Command command) {
    const auto result=world.execute(command);
    if (!result.accepted || !result.changed)
        throw std::runtime_error("Paid infrastructure fixture failed: "+result.reason);
}
void prepare_roads(sim::World& world) {
    for (int x=4;x<=15;++x) if (x!=11)
        place(world,{sim::CommandType::PlaceRoad,{x,5}});
    for (const sim::Cell cell:{sim::Cell{5,4},sim::Cell{14,4}})
        place(world,{sim::CommandType::PlaceRoad,cell});
}
void add_infrastructure(sim::World& world) {
    const auto before_bridge=world.snapshot();
    const auto bridge_refresh=world.route_refresh_count();
    place(world,sim::place_bridge({12,5},{10,5}));
    check(world.treasury()==before_bridge.treasury-60 &&
        world.construction_spent_total()==before_bridge.construction_spent_total+60 &&
        world.command_sequence()==before_bridge.command_sequence+1 &&
        world.road_revision()==before_bridge.road_revision+1 &&
        world.route_refresh_count()==bridge_refresh+1,
        "Inherited bridge price/revision/route refresh changed");
    check(world.transport_edge_blocker({7,5},{8,5})==sim::BuildBlocker::UnsupportedHeightTransition,
        "Bridge alone bypassed unsupported height transition");
    const auto before_ramp=world.snapshot();
    const auto ramp_refresh=world.route_refresh_count();
    place(world,sim::place_ramp({8,5},{7,5}));
    check(world.treasury()==before_ramp.treasury-40 &&
        world.construction_spent_total()==before_ramp.construction_spent_total+40 &&
        world.command_sequence()==before_ramp.command_sequence+1 &&
        world.road_revision()==before_ramp.road_revision+1 &&
        world.route_refresh_count()==ramp_refresh+1,
        "Inherited ramp price/revision/route refresh changed");
    check(world.bridges().size()==1 && world.bridges()[0].id.value==1 &&
        world.bridges()[0].bank_a==sim::Cell{10,5} && world.bridges()[0].paid_cost==60 &&
        world.ramps().size()==1 && world.ramps()[0].id.value==1 &&
        world.ramps()[0].endpoint_a==sim::Cell{7,5} && world.ramps()[0].height_a==1 &&
        world.ramps()[0].height_b==0 && world.ramps()[0].paid_cost==40,
        "Inherited independent bridge/ramp IDs or signed endpoints changed");
    check(world.transport_edge_allowed({10,5},{11,5}) &&
        world.transport_edge_allowed({11,5},{12,5}) &&
        !world.transport_edge_allowed({11,5},{11,4}) &&
        world.transport_edge_allowed({7,5},{8,5}) &&
        world.transport_edge_allowed({8,5},{7,5}) &&
        !world.transport_edge_allowed({7,4},{8,4}),
        "Inherited longitudinal bridge/exact owned ramp graph changed");
    const auto stable=world.snapshot();
    for (const auto command:{sim::place_bridge({10,5},{12,5}),
                             sim::place_ramp({7,5},{8,5}),
                             sim::Command{sim::CommandType::RemoveBridge,{11,5}},
                             sim::Command{sim::CommandType::RemoveRamp,{7,5}}})
        check(!world.execute(command).accepted && world.snapshot()==stable,
            "Duplicate or unsafe removal mutated inherited infrastructure");
}

void development_gate() {
    const auto p3=policy(3);
    for (const auto profile:profiles) for (const auto version:{7U,8U,9U,10U,11U,12U,13U,14U,15U,16U,17U,18U,19U,22U,23U}) {
        const auto reason=profile==sim::RulesProfile::CityV16 ?
            "runtime is not ready":"unsupported";
        rejects([&] { sim::World denied(p3,profile,version); },reason);
        rejects([&] { sim::World denied(width,height,p3->building_mask(),profile,version); },reason);
    }
    rejects([&] { sim::World denied(p3,sim::RulesProfile::CityV16,24); },"unsupported");
    const auto source_cells=sim::World::create_authored_source_cell_ramps(p3);
    check(source_cells.rule_version()==20 && source_cells.source_cell_ramps_supported() &&
        !source_cells.original_economy_enabled(),"Rule20 inherited Original authority");
    rejects([&] { (void)sim::World::create_original_safe_demolition(p3,nullptr,nullptr); },
        "prepared model and immutable terrain facts");
    rejects([&] { (void)sim::World::create_original_economy_development(nullptr); },"complete policy-3");
    for (const std::uint32_t version:{1U,2U})
        rejects([&] { (void)sim::World::create_original_economy_development(policy(version)); },"complete policy-3");
    // Incomplete dimensions/cells cannot become prepared authority at all.
    rejects([&] {
        auto incomplete=std::make_shared<const sim::MapPermissions>(width,height,3,
            std::vector<sim::MapCellPermission>(width*height-1),std::vector<sim::FixedGatePassage>{});
        (void)sim::World::create_original_economy_development(incomplete);
    },"");
    auto development=sim::World::create_original_economy_development(p3);
    check(development.original_economy_enabled() && development.rule_version()==7 &&
        development.profile()==sim::RulesProfile::CityV16 &&
        development.map_permissions()==p3 && development.bridges_supported() &&
        development.ramps_supported(),"Explicit development factory did not bind rule7/policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,7)==3,
        "Rule7 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,8)==3,
        "Rule8 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,9)==3,
        "Rule9 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,10)==3,
        "Rule10 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,11)==3,
        "Rule11 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,12)==3,
        "Rule12 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,13)==3,
        "Rule13 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,14)==3,
        "Rule14 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,17)==3,
        "Rule17 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,18)==3,
        "Rule18 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,19)==3,
        "Rule19 lost the explicit immutable policy3 requirement");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,15)==3,
        "Rule15 map preflight bypasses complete policy3");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,16)==3,
        "Rule16 lost the explicit immutable policy3 requirement");
    check(maps::map_rules_policy_version(sim::RulesProfile::CityV16,20)==3 &&
        maps::map_rules_policy_version(sim::RulesProfile::CityV16,21)==3 &&
        maps::map_rules_policy_version(sim::RulesProfile::CityV16,22)==3 &&
        maps::map_rules_policy_version(sim::RulesProfile::CityV16,23)==3 &&
        maps::map_rules_policy_version(sim::RulesProfile::CityV16,24)==0 &&
        maps::map_rules_policy_version(sim::RulesProfile::CityV11,7)==0 &&
        !sim::original_economy_enabled(static_cast<sim::RulesProfile>(255),7),
        "Unknown profile/version inherited rule7 preflight or economy gate");
    const auto before=development.snapshot();
    for (const auto type:{sim::CommandType::PlaceWorkshop,sim::CommandType::PlaceWarehouse,
            sim::CommandType::PlaceClaySource,sim::CommandType::PlacePottery,
            sim::CommandType::PlaceFarm,
            sim::CommandType::PlaceServicePost,sim::CommandType::PlaceMarket,
            sim::CommandType::SetBuildingOperation,sim::CommandType::SetBuildingWorkforcePriority,
            sim::CommandType::DemolishBuilding,sim::CommandType::PlaceFireWatch,
            sim::CommandType::PlaceWell,sim::CommandType::PlaceHealthPost}) {
        const auto validation=development.validate({type,{1,1}});
        const auto result=development.execute({type,{1,1}});
        check(!validation.accepted && !result.accepted && !result.changed &&
            development.snapshot()==before,
            "Unfinished rule7 producer/house/service or operation gained authority");
    }
    development.tick();
    auto clock_only=before;++clock_only.ticks;
    check(development.snapshot()==clock_only,"Rule7 scheduler changed unfinished economy authority");
    rejects([&] { (void)sim::World::restore(before,p3); },"rule 7 runtime is not ready");
    rejects([&] { (void)sim::World::restore(before,p3->building_mask()); },"rule 7 runtime is not ready");
    rejects([&] { development.import_snapshot(before); },"rule 7 runtime is not ready");
    check(development.snapshot()==clock_only,"Rejected restore changed the development source");
}

void infrastructure_inheritance() {
    const auto p3=policy(3,true);
    const auto immutable=p3->canonical_state();
    sim::World old(p3,sim::RulesProfile::CityV16,6);
    auto development=sim::World::create_original_economy_development(p3);
    prepare_roads(old);prepare_roads(development);
    add_infrastructure(old);add_infrastructure(development);
    auto inherited=development.snapshot();inherited.rule_version=6;inherited.original_economy.reset();
    check(inherited==old.snapshot(),
        "Rule7 inherited infrastructure changed authoritative rule6 state beyond its version");
    check(development.buildings().empty() && development.couriers().empty() &&
        development.ticks()==0 && development.resource_inventory()==old.resource_inventory() &&
        development.city_economy_valid(),
        "Development infrastructure introduced free producers, cargo or clock authority");
    const auto diagnosis=sim::diagnose_road_connectivity(development,
        static_cast<sim::BuildingId>(1),static_cast<sim::BuildingId>(2));
    check(diagnosis.policy_version==3 && diagnosis.rule_version==7 &&
        diagnosis.authoritative_route_queries==0 &&
        diagnosis.unsupported_reason=="Requires two distinct placed building IDs",
        "Rule7 diagnosis bypassed its policy or fabricated missing building endpoints");
    for (int y=0;y<height;++y) for (int x=0;x<width;++x) {
        const sim::Cell from{x,y};
        check(old.transport_cell(from)==development.transport_cell(from) &&
            old.transport_source(from)==development.transport_source(from) &&
            old.transport_height(from)==development.transport_height(from),
            "Rule7 transport cell/source/height differs from unchanged rule6");
        for (const auto delta:{sim::Cell{1,0},sim::Cell{-1,0},sim::Cell{0,1},sim::Cell{0,-1}}) {
            const sim::Cell to{x+delta.x,y+delta.y};
            check(old.transport_edge_blocker(from,to)==development.transport_edge_blocker(from,to),
                "Rule7 transport graph differs from unchanged rule6");
        }
    }
    check(p3->canonical_state()==immutable &&
        p3->transport_edge_blocker({7,5},{8,5})==sim::BuildBlocker::UnsupportedHeightTransition,
        "Development bridge/ramp mutated immutable policy3 authority");
    place(old,{sim::CommandType::RemoveRoad,{7,5}});
    place(development,{sim::CommandType::RemoveRoad,{7,5}});
    check(!old.transport_edge_allowed({7,5},{8,5}) &&
        !development.transport_edge_allowed({7,5},{8,5}),
        "Development ramp traversed a disconnected endpoint");
    place(old,{sim::CommandType::PlaceRoad,{7,5}});
    place(development,{sim::CommandType::PlaceRoad,{7,5}});
    inherited=development.snapshot();inherited.rule_version=6;inherited.original_economy.reset();
    check(inherited==old.snapshot() && development.transport_edge_allowed({7,5},{8,5}),
        "Development paid ramp repair differed from rule6");
    check(sim::World::restore(old.snapshot(),p3).snapshot()==old.snapshot(),
        "Explicit rule7 work broke historical combined bridge/ramp restore");
}

struct TemporaryRoot {
    fs::path path=fs::canonical(fs::temp_directory_path())/
        ("openemperor-original-rule-gate-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TemporaryRoot() { fs::create_directories(path/"data/Cities"); }
    ~TemporaryRoot() { std::error_code ignored;fs::remove_all(path,ignored); }
};
void write_text(const fs::path& path,const std::string& text) {
    std::ofstream output(path,std::ios::binary);
    output.write(text.data(),static_cast<std::streamsize>(text.size()));
    check(output.good(),"Authored save fixture could not be written");
}
std::string read_text(const fs::path& path) {
    const auto size=fs::file_size(path);
    check(size<=1024U*1024U,"Authored save fixture exceeded bound");
    std::ifstream input(path,std::ios::binary);
    std::string text(static_cast<std::size_t>(size),'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    check(input.gcount()==static_cast<std::streamsize>(text.size()) && !input.bad() &&
        input.peek()==std::char_traits<char>::eof(),"Authored save fixture changed during reading");
    return text;
}
void persistence_refusal() {
    TemporaryRoot root;
    const auto data=root.path/"data";
    const fs::path relative="Cities/AuthoredRuleGate.map";
    // No parser claim: this stable authored input only binds the existing
    // prepared-policy persistence API to independently supplied map bytes.
    write_text(data/relative,"OpenEmperor authored rule gate map identity\n");
    const auto p3=policy(3,true);
    sim::World old(p3,sim::RulesProfile::CityV16,6);
    prepare_roads(old);add_infrastructure(old);
    const auto document=save::make_document(data,relative,p3->building_mask(),old);
    const auto protected_save=root.path/"protected.json";
    save::write_save(protected_save,document,data,p3->building_mask());
    const auto saved_bytes=read_text(protected_save);
    auto json=nlohmann::json::parse(saved_bytes);
    check(json.at("schema_version")==22 && json.at("rules").at("version")==6,
        "Historical rule6 writer changed its schema identity");
    check(save::read_save(protected_save).world==old.snapshot(),
        "Historical rule6 writer/readback changed its full snapshot");

    auto development=sim::World::create_original_economy_development(p3);
    rejects([&] { (void)save::make_document(data,relative,p3->building_mask(),development); },
        "schema23 requires a prepared original rules model");
    auto future=document;future.world=development.snapshot();
    for (const std::uint32_t source_schema:{0U,18U,19U,20U,21U,22U,23U}) {
        future.source_schema_version=source_schema;
        rejects([&] { (void)save::restore_save(future,data,p3->building_mask()); },
            "rule 7 runtime is not ready");
        rejects([&] { save::write_save(protected_save,future,data,p3->building_mask()); },
            source_schema==0 || source_schema==23 ? "schema23 requires an original model source identity":
                "rule7 requires schema23");
        check(read_text(protected_save)==saved_bytes,
            "Refused rule7 writer overwrote an existing historical save");
    }
    for (unsigned schema=1;schema<=23;++schema) {
        json["rules"]["version"]=7;json["schema_version"]=schema;
        const auto hostile=root.path/"old-schema-rule7.json";
        write_text(hostile,json.dump());
        rejects([&] { (void)save::read_save(hostile); },"");
    }
    save::write_save(protected_save,document,data,p3->building_mask());
    check(read_text(protected_save)==saved_bytes,
        "Rule7 refusal changed historical canonical save bytes");
}

} // namespace

int main() {
    try {
        version_matrix();development_gate();infrastructure_inheritance();persistence_refusal();
        std::cout<<"Original-economy rule7 gate PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Original-economy rule7 gate: "<<error.what()<<'\n';
        return 1;
    }
}
