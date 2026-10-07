#include "assets/CompatibilityProfile.h"
#include "app/VisualSelection.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace {
namespace fs=std::filesystem;
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
void write(const fs::path& path,const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream stream(path,std::ios::binary);stream<<text;
    check(static_cast<bool>(stream),"fixture write failed");
}
struct Temp {
    fs::path path=fs::temp_directory_path()/("openemperor-compatibility-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { fs::create_directories(path); }
    ~Temp() { std::error_code error;fs::remove_all(path,error); }
};
const nlohmann::json fingerprints={
    {{"path","files/A"},{"sha256","ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"}},
    {{"path","files/B"},{"sha256","3e23e8160039594a33894f6564e1b1348bbd7a0088d42c4acb73eeaed59c009d"}},
    {{"path","files/C"},{"sha256","2e7d2c03a9507ae265ecf5b5356885a53393a2029d241394997265a1a25aefc6"}}
};
void pack(const fs::path& resources,nlohmann::json files=fingerprints) {
    const auto root=resources/"Compatibility/test-pack";
    write(root/"manifest.json",nlohmann::json{{"schema_version",1},{"id","test-pack"},
        {"evidence","synthetic test metadata"},{"files",std::move(files)},
        {"profiles",{{"walker","walkers.json"},{"building","buildings.json"},
                     {"road","roads.json"}}}}.dump());
    write(root/"walkers.json","{}");write(root/"buildings.json","{}");write(root/"roads.json","{}");
}
void data(const fs::path& root) {
    write(root/"files/A","a");write(root/"files/B","b");write(root/"files/C","c");
}
const nlohmann::json fire_fingerprints={
    {{"path","files/D"},{"sha256","18ac3e7343f016890c510e93f935261169d9e3f565436429830faf0934f4f8e4"}},
    {{"path","files/E"},{"sha256","3f79bb7b435b05321651daefd374cdc681dc06faa65e374e38337b88ca046dea"}}
};
void fire_pack(const fs::path& resources,nlohmann::json optional={}) {
    pack(resources);
    const auto root=resources/"Compatibility/test-pack";
    std::ifstream input(root/"manifest.json");
    auto document=nlohmann::json::parse(input);
    document["optional_fire"]=optional.empty() ? nlohmann::json{
        {"profile","fire.json"},{"files",fire_fingerprints}}:std::move(optional);
    write(root/"manifest.json",document.dump());
    write(root/"fire.json","{}");
}
void validate_optional_fire() {
    Temp temp;const auto resources=temp.path/"resources",root=temp.path/"data";
    data(root);write(root/"files/D","d");write(root/"files/E","e");fire_pack(resources);
    const auto detect=[&] { return openemperor::assets::detect_compatibility(root,resources/"Compatibility"); };
    const auto core_preserved=[&](const auto& result) {
        check(result.compatible() && result.files_hashed==3,"optional fire changed core compatibility");
        const auto selected=openemperor::select_visual_profiles(result);
        check(selected.walker_source==openemperor::VisualProfileSource::Builtin &&
              selected.building_source==openemperor::VisualProfileSource::Builtin &&
              selected.road_source==openemperor::VisualProfileSource::Builtin,
              "optional fire disabled existing categories");
    };
    auto exact=detect();core_preserved(exact);
    check(exact.fire_compatible() && exact.fire_files_hashed==2,
          "optional fire pair not independently fingerprinted");
    auto selected=openemperor::select_visual_profiles(exact);
    check(selected.fire_source==openemperor::VisualProfileSource::Builtin &&
          selected.fire==exact.profile->fire_profile && selected.fire_fallback_reason.empty(),
          "matching optional fire not automatically selected");
    const auto detection_before=openemperor::assets::compatibility_detection_count();
    for (int i=0;i<1000;++i) (void)openemperor::select_visual_profiles(exact);
    check(openemperor::assets::compatibility_detection_count()==detection_before,
          "fire frame selection rehashed source files");
    for (const char* missing:{"D","E"}) {
        fs::remove(root/"files"/missing);const auto result=detect();core_preserved(result);
        check(!result.fire_compatible() && result.fire_status==openemperor::assets::CompatibilityStatus::MissingFile,
              "missing optional fire file did not produce independent fallback");
        selected=openemperor::select_visual_profiles(result);
        check(selected.fire.empty() && selected.fire_source==openemperor::VisualProfileSource::Fallback &&
              !selected.fire_fallback_reason.empty(),"optional missing-file fallback is not named");
        write(root/"files"/missing,std::string(missing==std::string("D") ? "d":"e"));
    }
    write(root/"files/E","changed");auto changed=detect();core_preserved(changed);
    check(changed.fire_status==openemperor::assets::CompatibilityStatus::FingerprintMismatch &&
          changed.fire_files_hashed==2,"second optional file was inferred from first fingerprint");
    write(root/"files/E","e");
    fs::remove(root/"files/E");write(temp.path/"outside","e");
    std::error_code symlink_error;fs::create_symlink(temp.path/"outside",root/"files/E",symlink_error);
    if (!symlink_error) {
        const auto unsafe=detect();core_preserved(unsafe);
        check(!unsafe.fire_compatible(),"optional fire symlink accepted");
    }
    fs::remove(root/"files/E");write(root/"files/E","e");
    const auto malformed=[&](nlohmann::json optional) {
        fire_pack(resources,std::move(optional));const auto result=detect();core_preserved(result);
        check(!result.fire_compatible() && !result.profile->fire_metadata_error.empty(),
              "malformed optional metadata did not preserve core with named fallback");
    };
    malformed({{"profile","../fire.json"},{"files",fire_fingerprints}});
    malformed({{"profile","fire.json"},{"files",nlohmann::json::array()}});
    auto unsafe_files=fire_fingerprints;unsafe_files[0]["path"]="../outside";
    malformed({{"profile","fire.json"},{"files",unsafe_files}});
    auto bad_hash=fire_fingerprints;bad_hash[0]["sha256"]="not-a-hash";
    malformed({{"profile","fire.json"},{"files",bad_hash}});
    auto duplicates=fire_fingerprints;duplicates.push_back(duplicates[0]);
    malformed({{"profile","fire.json"},{"files",duplicates}});
    auto too_many=fire_fingerprints;for(int i=0;i<3;++i)too_many.push_back(fire_fingerprints[0]);
    malformed({{"profile","fire.json"},{"files",too_many}});
    fire_pack(resources);fs::remove(resources/"Compatibility/test-pack/fire.json");
    const auto missing_resource=detect();core_preserved(missing_resource);
    check(!missing_resource.fire_compatible() && !missing_resource.fire_detail.empty(),
          "missing fire metadata resource rejected the existing pack");
    fire_pack(resources);write(root/"files/C","different");
    const auto unknown=detect();check(!unknown.compatible() && !unknown.fire_compatible(),
        "unknown core revision inferred compatibility from optional assets");
    check(openemperor::select_visual_profiles(unknown).fire.empty(),"unknown data activated fire");
    pack(resources);write(root/"files/C","c");const auto legacy=detect();core_preserved(legacy);
    check(!legacy.fire_compatible() && openemperor::select_visual_profiles(legacy).fire.empty(),
          "legacy compatibility pack changed its optional-fire default");
}
bool safe_relative(const fs::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& part:path) if (part==".." || part==".") return false;
    return true;
}
void reject_blob_keys(const nlohmann::json& value) {
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        check(key.find("blob")==std::string::npos && key.find("binary")==std::string::npos &&
              key.find("pixels")==std::string::npos && key.find("base64")==std::string::npos,
              "built-in profile contains a binary/blob field");
        reject_blob_keys(child);
    } else if (value.is_array()) for (const auto& child:value) reject_blob_keys(child);
}
void validate_archive_entries(const nlohmann::json& value,std::size_t& count) {
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        if (key=="archive") {
            check(child.is_string(),"archive path is not text");
            const fs::path path(child.get<std::string>());
            check(safe_relative(path) && path.extension()==".sg3","unsafe built-in archive path");
            ++count;
        }
        if (key=="image_index") check(child.is_number_unsigned(),"image index is not uint32");
        validate_archive_entries(child,count);
    } else if (value.is_array()) for (const auto& child:value) validate_archive_entries(child,count);
}
void validate_committed_pack(const fs::path& resource_root) {
    const auto root=resource_root/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    const auto profile=openemperor::assets::load_compatibility_profile(root/"manifest.json");
    check(profile.id=="gog-derived-2.0.0.2-en-assetset-1" && profile.files.size()==6,
          "committed pack identity or fingerprint count changed");
    const std::set<std::string> expected={"DATA/SprMain.sg3","DATA/SprMain.555",
        "DATA/China_General.sg3","DATA/China_General.555","DATA/China_Terrain.sg3",
        "DATA/China_Terrain.555"};
    std::set<std::string> actual;
    for (const auto& item:profile.files) actual.insert(item.relative_path.generic_string());
    check(actual==expected,"committed pack fingerprints unexpected files");
    const std::map<std::string,std::string> expected_hashes={
        {"DATA/SprMain.sg3","3e2817d2644453acc92068d6c1e26532212be213b43b848ebe95ba21654adef8"},
        {"DATA/SprMain.555","d84eb6759e9ce50b0ad75596773b9224f9c1a9b8dd80b691c066c3de7e8df438"},
        {"DATA/China_General.sg3","c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46"},
        {"DATA/China_General.555","3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2"},
        {"DATA/China_Terrain.sg3","56e9fe6bbbbdc93d28f44883bd9bd8f9cfe0198f024a5af1e4589d47319f374d"},
        {"DATA/China_Terrain.555","c5f5a28b8c2663af06ca4c63f18191e4b2fecec65bd77ee7f51d1e2dc5ae23ff"}
    };
    for (const auto& item:profile.files)
        check(expected_hashes.at(item.relative_path.generic_string())==item.sha256,
              "committed compatibility fingerprint changed");
    std::ifstream walker_stream(profile.walker_profile),building_stream(profile.building_profile),
        road_stream(profile.road_profile);
    const auto walkers=nlohmann::json::parse(walker_stream);
    const auto buildings=nlohmann::json::parse(building_stream);
    const auto roads=nlohmann::json::parse(road_stream);
    check(walkers.value("schema_version",0)==2 && walkers.value("mode","")=="curated_walker_preview" &&
          walkers.at("roles").size()==3,"walker compatibility schema changed");
    check(buildings.value("schema_version",0)==1 && buildings.at("buildings").size()==13,
          "building compatibility schema changed");
    check(roads.value("schema_version",0)==1 && roads.at("tiles").size()==16,
          "road compatibility schema changed");
    check(walkers.at("roles").contains("clay") && walkers.at("roles").contains("pottery") &&
          walkers.at("roles").contains("household"),"unknown/missing walker role");
    const std::set<std::string> building_roles={"clay_source","pottery","warehouse","household",
        "farm","service_post","market","fire_watch",
        "household_level_0","household_level_1","household_level_2","well","health_post"};
    std::set<std::string> actual_buildings;
    for (const auto& [key,value]:buildings.at("buildings").items()) {
        (void)value;actual_buildings.insert(key);
    }
    check(actual_buildings==building_roles,"unknown/missing building role");
    const std::set<std::string> road_masks={"0x0","0x1","0x2","0x3","0x4","0x5",
        "0x6","0x7","0x8","0x9","0xa","0xb","0xc","0xd","0xe","0xf"};
    std::set<std::string> actual_roads;
    for (const auto& [key,value]:roads.at("tiles").items()) { (void)value;actual_roads.insert(key); }
    check(actual_roads==road_masks,"built-in road masks changed");
    const std::map<std::string,std::uint32_t> expected_buildings={{"clay_source",2789},
        {"pottery",2810},{"warehouse",637},{"household",1512},{"farm",2415},
        {"service_post",2046},{"market",645},{"fire_watch",383},
        {"household_level_0",1512},{"household_level_1",1516},{"household_level_2",1520},{"well",1559},{"health_post",1580}};
    const std::map<std::string,std::array<int,2>> expected_building_anchors={
        {"clay_source",{79,76}},{"pottery",{79,120}},
        {"warehouse",{79,116}},{"household",{79,79}},{"farm",{39,41}},
        {"service_post",{39,28}},{"market",{39,78}},{"fire_watch",{39,59}},
        {"household_level_0",{79,76}},{"household_level_1",{79,90}},
        {"household_level_2",{79,114}},{"well",{79,112}},{"health_post",{79,116}}};
    for (const auto& [role,index]:expected_buildings) {
        const auto& item=buildings.at("buildings").at(role);
        check(item.at("image_index").get<std::uint32_t>()==index &&
              item.at("ground_anchor").get<std::array<int,2>>()==expected_building_anchors.at(role),
              "curated building record or anchor changed");
    }
    for (const char* role:{"well","health_post"})
        check(buildings.at("buildings").at(role).at("footprint_side")==2,
            "built-in Safety roles lost their rule-aware 2x2 constraints");
    const std::map<std::string,std::uint32_t> expected_roads={{"0x0",794},{"0x1",790},
        {"0x2",791},{"0x3",786},{"0x4",792},{"0x5",782},{"0x6",787},
        {"0x7",795},{"0x8",793},{"0x9",789},{"0xa",783},{"0xb",798},
        {"0xc",788},{"0xd",797},{"0xe",796},{"0xf",799}};
    for (const auto& [mask,index]:expected_roads) {
        const auto& item=roads.at("tiles").at(mask);
        check(item.at("image_index").get<std::uint32_t>()==index &&
              item.at("ground_anchor").get<std::array<int,2>>()==std::array<int,2>{39,20},
              "curated road record or anchor changed");
    }
    const std::map<std::string,std::array<std::uint32_t,16>> expected_walkers={
        {"clay",{109,117,125,133,110,118,126,134,111,119,127,135,112,120,128,136}},
        {"pottery",{217,225,233,241,218,226,234,242,219,227,235,243,220,228,236,244}},
        {"household",{433,441,449,457,434,442,450,458,435,443,451,459,436,444,452,460}}};
    for (const auto& [role,indices]:expected_walkers) {
        const auto& item=walkers.at("roles").at(role);
        check(item.at("ticks_per_frame").get<unsigned>()==4,"walker timing changed");
        std::array<std::uint32_t,16> actual_indices{};
        const auto& frames=item.at("frames");
        for (std::size_t i=0;i<actual_indices.size();++i)
            actual_indices[i]=frames.at(i).at("image_index").get<std::uint32_t>();
        check(actual_indices==indices,"curated walker records changed");
    }
    std::size_t archives=0;
    for (const auto* document:{&walkers,&buildings,&roads}) {
        reject_blob_keys(*document);validate_archive_entries(*document,archives);
    }
    check(archives==77,"built-in profile archive entry count changed");
    const std::map<std::string,std::string> expected_fire_files={
        {"DATA/destruction.sg3","f5539108b6d585b31fc6f807e4da10f4ac796ee805f0bf065379923f821d035b"},
        {"DATA/destruction.555","0668087731938c626fe01211cc02e102c93022e728e9ea649eb9ed9322cf1a94"}};
    check(profile.fire_files.size()==2 && profile.fire_metadata_error.empty(),
          "committed optional fire identity is incomplete");
    for(const auto& item:profile.fire_files)
        check(expected_fire_files.at(item.relative_path.generic_string())==item.sha256,
              "optional fire source hash changed");
    std::ifstream fire_stream(profile.fire_profile);const auto fire=nlohmann::json::parse(fire_stream);
    reject_blob_keys(fire);std::size_t fire_archives=0;validate_archive_entries(fire,fire_archives);
    check(fire.value("schema_version",0)==1 && fire.value("mode","")=="curated_fire_presentation" &&
          fire.value("ticks_per_frame",0)==2 && fire.at("frames").size()==50 && fire_archives==50,
          "committed fire clip bounds or timing changed");
    for(std::uint32_t i=0;i<50;++i) {
        const auto& frame=fire.at("frames").at(i);
        check(frame.at("archive")=="DATA/destruction.sg3" &&
              frame.at("image_index").get<std::uint32_t>()==201+i &&
              frame.at("anchor").get<std::array<int,2>>()==std::array<int,2>{40,80},
              "committed fire explicit physical frame order or authored anchor changed");
    }
}
}

int main(int argc,char* argv[]) {
    try {
        check(argc==2,"resource-root argument required");
        Temp temp;const auto resources=temp.path/"resources",root=temp.path/"data";
        pack(resources);data(root);
        openemperor::assets::reset_compatibility_detection_count_for_tests();
        const auto exact=openemperor::assets::detect_compatibility(root,resources/"Compatibility");
        check(exact.compatible() && exact.files_hashed==3,"exact synthetic pack not detected");
        write(root/"irrelevant.bin","extra");
        check(openemperor::assets::detect_compatibility(root,resources/"Compatibility").compatible(),
              "irrelevant file changed detection");
        fs::remove(root/"files/C");
        check(openemperor::assets::detect_compatibility(root,resources/"Compatibility").status==
              openemperor::assets::CompatibilityStatus::MissingFile,"missing fingerprint not reported");
        write(root/"files/C","changed");
        check(openemperor::assets::detect_compatibility(root,resources/"Compatibility").status==
              openemperor::assets::CompatibilityStatus::FingerprintMismatch,"changed byte not rejected");
        fs::remove(root/"files/C");
        std::error_code symlink_error;
        fs::create_symlink(temp.path/"outside",root/"files/C",symlink_error);
        if (!symlink_error) check(openemperor::assets::detect_compatibility(root,resources/"Compatibility").status==
            openemperor::assets::CompatibilityStatus::FingerprintMismatch,"escaping symlink not rejected");
        nlohmann::json traversal=fingerprints;traversal[0]["path"]="../escape";
        pack(resources,traversal);
        bool traversal_rejected=false;
        try { (void)openemperor::assets::load_compatibility_profile(
            resources/"Compatibility/test-pack/manifest.json"); }
        catch (const std::exception&) { traversal_rejected=true; }
        check(traversal_rejected,"manifest traversal accepted");
        const auto before=openemperor::assets::compatibility_detection_count();
        for (int i=0;i<1000;++i) {
            const auto selected=openemperor::select_visual_profiles(exact,
                i==0 ? fs::path("custom.json"):fs::path{});
            if (i==0) check(selected.walker_source==openemperor::VisualProfileSource::Custom &&
                selected.building_source==openemperor::VisualProfileSource::Builtin &&
                selected.road_source==openemperor::VisualProfileSource::Builtin,
                "custom category did not override only built-in walker");
            if (i==1) check(selected.walker_source==openemperor::VisualProfileSource::Builtin,
                "cleared custom profile did not return to auto");
        }
        check(openemperor::assets::compatibility_detection_count()==before,
              "visual/frame selection recomputed fingerprints");
        validate_committed_pack(fs::canonical(argv[1]));
        validate_optional_fire();
        std::cout<<"compatibility fingerprint, atomic selection and metadata checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"CompatibilityProfileTests failed: "<<error.what()<<'\n';return 1;
    }
}
