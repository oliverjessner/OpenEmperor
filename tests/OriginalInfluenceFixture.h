#pragma once
// Authored test geography and model values; existing schema23 goldens stay exact.
#include "OriginalEconomySaveFixture.h"
#include "persistence/SandboxSave.h"
#include "simulation/World.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace oe_influence_fixture {
namespace fs=std::filesystem;
namespace sim=openemperor::simulation;
namespace save=openemperor::persistence;
inline constexpr int width=24,height=20;
inline constexpr const char* map_relative="Cities/Influence.map";
inline void check(bool ok,const std::string& message) { if (!ok) throw std::runtime_error(message); }
inline std::string read_text(const fs::path& path) {
    std::ifstream in(path,std::ios::binary);check(in.good(),"Missing fixture input: "+path.string());
    return {std::istreambuf_iterator<char>(in),{}};
}
inline void write_text(const fs::path& path,const std::string& bytes) {
    std::ofstream out(path,std::ios::binary);out.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    check(out.good(),"Cannot write authored influence fixture");
}
inline std::string authored_model_text() {
    auto text=oe_save_fixture::authored_model_text();
    const std::string zero="{,0,0,0,0,0,0,0,0,0,0,0,0,0,},,,\n";
    for (const auto label:{"0,Very Easy Modifiers,","1,Easy Modifiers,","2,Normal Modifier,",
            "3,Hard Modifiers,","4,Very Hard Modifiers,"}) {
        const auto at=text.find(std::string(label)+zero);check(at!=std::string::npos,"Authored modifier row missing");
        text.replace(at+std::string(label).size(),zero.size(),"{,100,100,100,100,100,100,100,100,100,100,100,100,100,},,,\n");
    }
    const auto at=text.find("72,Well,"+zero);check(at!=std::string::npos,"Authored Well row missing");
    // Independent positive input: DES12/STP2/DELTA-2/RGE5; no original row copied.
    text.replace(at+8,zero.size(),"{,0,12,2,-2,5,0,0,0,0,0,0,0,0,},,,\n");
    return text;
}
inline void write_model(const fs::path& root) {
    fs::create_directories(root/"Model");write_text(root/"Model/EmperorBuildingModels.txt",authored_model_text());
}
inline std::shared_ptr<const sim::MapPermissions> permissions() {
    return std::make_shared<const sim::MapPermissions>(width,height,3,
        std::vector<sim::MapCellPermission>(width*height,
            {true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None}),
        std::vector<sim::FixedGatePassage>{});
}
struct Root {
    static fs::path reserve_path() {
        const auto parent=fs::canonical(fs::temp_directory_path());
        const auto stamp=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for (unsigned attempt=0;attempt<64;++attempt) {
            const auto candidate=parent/("openemperor-influence-"+stamp+"-"+std::to_string(attempt));
            std::error_code error;
            // Claim ownership atomically: parallel fixtures must never share or
            // remove another fixture's authored model, even on a coarse clock.
            if (fs::create_directory(candidate,error)) return candidate;
            if (error && error!=std::errc::file_exists)
                throw fs::filesystem_error("Cannot reserve authored influence fixture",candidate,error);
        }
        throw std::runtime_error("Cannot reserve a unique authored influence fixture");
    }
    fs::path path=reserve_path();
    fs::path data=path/"data";
    Root() {
        try {
            fs::create_directories(data/"Cities");
            write_text(data/map_relative,"Authored24x20 policy3 influence acceptance geography; not an original map.\n");
            write_model(data);
        } catch (...) {
            std::error_code ignored;fs::remove_all(path,ignored);throw;
        }
    }
    Root(const Root&)=delete;
    Root& operator=(const Root&)=delete;
    ~Root() { std::error_code ignored;fs::remove_all(path,ignored); }
};
inline void buy(sim::World& world,sim::Command command) {
    const auto result=world.execute(command);
    check(result.accepted && result.changed,"Paid influence fixture: "+result.reason);
}
inline sim::BuildingId source(sim::World& world,sim::Cell cell) {
    buy(world,{sim::CommandType::PlaceOriginalInfluenceSource,cell});
    return *world.building_owner_at(cell);
}
inline void roads(sim::World& world) {
    for (int x=0;x<24;++x) buy(world,{sim::CommandType::PlaceRoad,{x,4}});
    for (int y=0;y<=16;++y) if (y!=4) buy(world,{sim::CommandType::PlaceRoad,{20,y}});
    check(world.treasury()==1920 && world.construction_spent_total()==80,"Forty paid roads accounting");
}
inline void houses(sim::World& world) {
    for (int row=0;row<4;++row) for (int col=0;col<5;++col)
        buy(world,{sim::CommandType::PlaceHousehold,{2+3*col,2+3*row}});
    check(world.treasury()==320 && world.construction_spent_total()==1680,"Twenty paid houses accounting");
}
struct Scene { sim::BuildingId a{},b{}; };
inline Scene build_scene(sim::World& world) {
    check(world.treasury()==2000 && world.ticks()==0,"New influence contract start funds/time");
    roads(world);houses(world);const auto a=source(world,{18,6});
    check(world.treasury()==260,"First source cost");const auto b=source(world,{18,11});
    check(world.treasury()==200 && world.construction_spent_total()==1800 &&
        static_cast<unsigned>(a)==21 && static_cast<unsigned>(b)==22,"Two-source paid scene ledger");
    return {a,b};
}
inline void same_document(const save::SaveDocument& a,const save::SaveDocument& b) {
    check(a.world==b.world && a.map_relative==b.map_relative && a.map_sha256==b.map_sha256 &&
        a.buildable_sha256==b.buildable_sha256 &&
        a.map_permissions_policy_version==b.map_permissions_policy_version &&
        a.map_permissions_sha256==b.map_permissions_sha256 && !b.migrated_from_schema1,
        "Complete influence SaveDocument authority changed");
}
} // namespace oe_influence_fixture
