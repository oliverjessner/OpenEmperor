#include "assets/CompatibilityProfile.h"
#include "app/VisualSelection.h"
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Independently authored tiny byte strings test atomic identity/selection.
// SG3 decode and live World images have separate production integration tests.
namespace {
namespace fs=std::filesystem;
namespace assets=openemperor::assets;
using Json=nlohmann::json;
using Source=openemperor::VisualProfileSource;
void check(bool value,const char* message) {if (!value) throw std::runtime_error(message);}
void write(const fs::path& path,const std::string& text) {
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
    out<<text;check(bool(out),"Compatibility fixture write");
}
constexpr const char* hash_a="ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb";
constexpr const char* hash_b="3e23e8160039594a33894f6564e1b1348bbd7a0088d42c4acb73eeaed59c009d";
constexpr const char* hash_c="2e7d2c03a9507ae265ecf5b5356885a53393a2029d241394997265a1a25aefc6";
constexpr const char* hash_d="18ac3e7343f016890c510e93f935261169d9e3f565436429830faf0934f4f8e4";
constexpr const char* hash_e="3f79bb7b435b05321651daefd374cdc681dc06faa65e374e38337b88ca046dea";
constexpr std::array names{"clay","pottery","household","fire_inspector","supplier",
    "distributor","service","health_worker","music","acrobat","drama","venue_music"};
Json pair(const char* stem) {
    return Json::array({{{"path",std::string("DATA/")+stem+".sg3"},{"sha256",hash_d}},
        {{"path",std::string("DATA/")+stem+".555"},{"sha256",hash_e}}});
}
Json family(const char* stem) {
    return {{"ticks_per_frame",2},{"clip_id","authored-identity-only-family"},
        {"evidence","Original-free dependency-selection metadata; not decoded graphics."},
        {"idle","a"},{"frames",Json::array({{{"alias","a"},{"archive",std::string("DATA/")+stem+".sg3"},
            {"image_index",1},{"foot_anchor",{2,5}}},{{"alias","b"},{"archive",std::string("DATA/")+stem+".sg3"},
            {"image_index",3},{"foot_anchor",{3,6}}}})},
        {"clips",{{"pos_x",{"a","b"}},{"neg_x",{"a","b"}},
            {"pos_y",{"a","b"}},{"neg_y",{"a","b"}}}}};
}
Json complete_resource() {
    Json document={{"schema_version",7},{"mode","curated_walker_preview"},{"roles",Json::object()}};
    for (std::size_t i=0;i<names.size();++i) document["roles"][names[i]]=family(i%2 ? "second":"native");
    return document;
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-animation-compatibility-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",resources=root/"resources",pack=resources/"Compatibility/authored-pack";
    fs::path manifest=pack/"manifest.json",animation=pack/"animations.json";
    Fixture() {reset();}
    ~Fixture() {std::error_code ignored;fs::remove_all(root,ignored);}
    Json document() const {
        auto files=pair("native");for (const auto& row:pair("second")) files.push_back(row);
        Json document={{"schema_version",1},{"id","authored-pack"},{"evidence","Authored compatibility metadata."},
            {"files",Json::array({{{"path","core/A"},{"sha256",hash_a}},
                {{"path","core/B"},{"sha256",hash_b}},{{"path","core/C"},{"sha256",hash_c}}})},
            {"profiles",{{"walker","walkers.json"},{"building","buildings.json"},{"road","roads.json"}}},
            {"optional_walker_animations",{{"profile","animations.json"},{"files",files}}}};
        for (const auto* section:{"optional_fire","optional_fire_inspector","optional_market_walkers",
                "optional_service_walkers","optional_health_walkers"}) {
            const auto profile=std::string_view(section)=="optional_fire" ? "fire.json":
                std::string_view(section)=="optional_fire_inspector" ? "fire-inspector.json":
                std::string_view(section)=="optional_market_walkers" ? "market-walkers.json":
                std::string_view(section)=="optional_service_walkers" ? "service-walker.json":"health-walker.json";
            document[section]={{"profile",profile},{"files",pair("legacy")}};
        }
        return document;
    }
    void reset() const {
        write(data/"core/A","a");write(data/"core/B","b");write(data/"core/C","c");
        for (const auto* stem:{"legacy","native","second","extra"}) {
            write(data/"DATA"/(std::string(stem)+".sg3"),"d");
            write(data/"DATA"/(std::string(stem)+".555"),"e");
        }
        for (const auto* name:{"walkers.json","buildings.json","roads.json","fire.json"}) write(pack/name,"{}");
        write(pack/"fire-inspector.json",Json{{"schema_version",3},{"roles",{{"fire_inspector",family("legacy")}}}}.dump());
        write(pack/"market-walkers.json",Json{{"schema_version",4},
            {"roles",{{"supplier",family("legacy")},{"distributor",family("legacy")}}}}.dump());
        write(pack/"service-walker.json",Json{{"schema_version",5},{"roles",{{"service",family("legacy")}}}}.dump());
        write(pack/"health-walker.json",Json{{"schema_version",6},{"roles",{{"health_worker",family("legacy")}}}}.dump());
        write(animation,complete_resource().dump());write(manifest,document().dump());
    }
    assets::CompatibilityResult detect() const {return assets::detect_compatibility(data,resources/"Compatibility");}
};
void preserved(const Fixture& fixture,const assets::CompatibilityResult& result) {
    check(result.compatible() && result.profile->files.size()>=3 && result.fire_compatible() &&
        result.fire_inspector_compatible() && result.market_walker_compatible() &&
        result.service_walker_compatible() && result.health_walker_compatible(),
        "Optional complete-profile failure changed independently valid legacy identities");
    const auto selected=openemperor::select_visual_profiles(result);
    check(selected.walker==fixture.pack/"walkers.json" && selected.walker_source==Source::Builtin &&
        selected.building==fixture.pack/"buildings.json" && selected.road==fixture.pack/"roads.json" &&
        selected.fire==fixture.pack/"fire.json" && selected.fire_inspector==fixture.pack/"fire-inspector.json" &&
        selected.market_walker==fixture.pack/"market-walkers.json" &&
        selected.service_walker==fixture.pack/"service-walker.json" && selected.health_walker==fixture.pack/"health-walker.json",
        "New profile displaced a historical fallback path");
}
void fallback(const Fixture& fixture,const assets::CompatibilityResult& result) {
    preserved(fixture,result);const auto selected=openemperor::select_visual_profiles(result);
    check(!result.walker_animation_compatible() && selected.walker_animation.empty() &&
        selected.walker_animation_source==Source::Fallback && !selected.walker_animation_fallback_reason.empty(),
        "Partial complete-profile failure published an active path or omitted its reason");
}
void identity_and_selection(Fixture& fixture) {
    const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.walker_animation_compatible() && exact.walker_animation_files_hashed==4 &&
        exact.profile->walker_animation_files.size()==4,"Four complete-profile source files were not individually pinned");
    const auto selected=openemperor::select_visual_profiles(exact);
    check(selected.walker_animation==fixture.animation && selected.walker_animation_source==Source::Builtin,
        "Complete built-in animation profile was not selected");
    const auto custom=openemperor::select_visual_profiles(exact,"custom-walkers.json");
    check(custom.walker=="custom-walkers.json" && custom.walker_source==Source::Custom &&
        custom.walker_animation.empty() && custom.walker_animation_source==Source::Custom &&
        !custom.walker_animation_fallback_reason.empty() && custom.fire_source==Source::Builtin,
        "Custom walker selection gained a hidden complete-profile override");
    const auto others=openemperor::select_visual_profiles(exact,{},"custom-building.json","custom-road.json");
    check(others.walker_animation==fixture.animation && others.walker_animation_source==Source::Builtin,
        "Unrelated custom profiles suppressed native walker animations");
    check(openemperor::detect_and_select_visual_profiles(fixture.data,fixture.resources).walker_animation==fixture.animation,
        "Normal resource-root detection omitted the complete profile");
    const auto detections=assets::compatibility_detection_count();
    for (int i=0;i<1000;++i) (void)openemperor::select_visual_profiles(exact);
    check(assets::compatibility_detection_count()==detections,"Pure profile selection repeated hashing");
    for (const auto* stem:{"native","second"}) for (const auto* suffix:{".sg3",".555"}) {
        const auto file=fixture.data/"DATA"/(std::string(stem)+suffix);
        fs::remove(file);const auto missing=fixture.detect();fallback(fixture,missing);
        check(missing.walker_animation_status==assets::CompatibilityStatus::MissingFile,
            "Missing individual complete-profile dependency was not diagnosed");
        write(file,"changed");const auto altered=fixture.detect();fallback(fixture,altered);
        check(altered.walker_animation_status==assets::CompatibilityStatus::FingerprintMismatch,
            "An individual source mismatch was inferred from its other pair");
        write(file,std::string_view(suffix)==".sg3" ? "d":"e");
    }
    const auto file=fixture.data/"DATA/native.555",outside=fixture.root/"outside.555";
    fs::rename(file,outside);fs::create_symlink(outside,file);fallback(fixture,fixture.detect());
    fs::remove(file);fs::rename(outside,file);
    fs::rename(file,fixture.data/"DATA/inside.555");fs::create_symlink("inside.555",file);
    fallback(fixture,fixture.detect());fs::remove(file);fs::rename(fixture.data/"DATA/inside.555",file);
    auto legacy=fixture.document();legacy.erase("optional_walker_animations");write(fixture.manifest,legacy.dump());
    const auto old=fixture.detect();fallback(fixture,old);
    check(old.walker_animation_files_hashed==0,"Historical manifest inferred the new profile");
    fixture.reset();write(fixture.data/"core/C","changed");const auto unknown=fixture.detect();
    check(!unknown.compatible() && !unknown.walker_animation_compatible() &&
        openemperor::select_visual_profiles(unknown).walker_animation.empty(),"Optional files overrode an unknown core revision");
    fixture.reset();
    auto reused=fixture.document();for (const auto& row:pair("native")) reused["files"].push_back(row);
    reused["optional_walker_animations"]["files"]=pair("second");
    reused["optional_walker_animations"]["core_files"]={"DATA/native.sg3","DATA/native.555"};
    write(fixture.manifest,reused.dump());const auto core_reused=fixture.detect();preserved(fixture,core_reused);
    check(core_reused.walker_animation_compatible() && core_reused.files_hashed==5 &&
        core_reused.walker_animation_files_hashed==2,"Already validated exact core fingerprints were not reused");
    reused["optional_walker_animations"]["files"].push_back(pair("native")[0]);
    write(fixture.manifest,reused.dump());const auto duplicated=fixture.detect();fallback(fixture,duplicated);
    check(!duplicated.profile->walker_animation_metadata_error.empty() && duplicated.walker_animation_files_hashed==0,
        "Dependency duplicated across core and optional lists was partially activated");
    fixture.reset();
}
void malformed_metadata(Fixture& fixture) {
    const auto section=[&](Json value) {
        auto document=fixture.document();document["optional_walker_animations"]=std::move(value);
        write(fixture.manifest,document.dump());const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->walker_animation_metadata_error.empty() &&
            result.profile->walker_animation_profile.empty() && result.profile->walker_animation_files.empty() &&
            result.walker_animation_files_hashed==0,"Malformed metadata was partially published or hashed");
    };
    auto normal=fixture.document()["optional_walker_animations"];
    auto invalid=normal;invalid["profile"]="../animations.json";section(invalid);
    invalid=normal;invalid["pixels"]="not allowed";section(invalid);
    invalid=normal;invalid["files"].erase(invalid["files"].begin());section(invalid);
    invalid=normal;invalid["files"].push_back(invalid["files"][0]);section(invalid);
    invalid=normal;invalid["files"][0]["sha256"]=std::string(64,'A');section(invalid);
    invalid=normal;invalid["files"][0]["path"]="../outside";section(invalid);
    invalid=normal;invalid["core_files"]={"DATA/native.sg3"};section(invalid);
    invalid=normal;invalid["core_files"]={"core/A","core/A"};section(invalid);
    invalid=normal;invalid["files"][0]["path"]="DATA/extra.sg3";section(invalid);
    write(fixture.manifest,fixture.document().dump());
    const auto resource=[&](Json value) {
        write(fixture.animation,value.dump());const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->walker_animation_metadata_error.empty(),"Malformed complete resource lacks an independent reason");
    };
    for (const auto version:{1,2,3,4,5,6,8}) {invalid=complete_resource();invalid["schema_version"]=version;resource(invalid);}
    invalid=complete_resource();invalid["mode"]="other";resource(invalid);
    for (const auto* role:names) {invalid=complete_resource();invalid["roles"].erase(role);resource(invalid);}
    invalid=complete_resource();invalid["roles"]["unknown"]=family("native");resource(invalid);
    invalid=complete_resource();invalid["roles"]["music"]["frames"]=Json::array();resource(invalid);
    invalid=complete_resource();invalid["roles"]["music"]["frames"][0]["archive"]="DATA/extra.sg3";resource(invalid);
    invalid=complete_resource();invalid["roles"]["music"]["frames"][0]["archive"]="DATA/native.555";resource(invalid);
    invalid=complete_resource();invalid["roles"]["music"]["frames"][0]["archive"]="../native.sg3";resource(invalid);
    invalid=complete_resource();
    for (auto& role:invalid["roles"]) for (auto& frame:role["frames"]) frame["archive"]="DATA/native.sg3";
    resource(invalid);
    invalid=complete_resource();auto& aliases=invalid["roles"]["clay"]["frames"];
    for (std::size_t i=24;i<320;++i) {auto alias=aliases[0];alias["alias"]="extra-"+std::to_string(i);aliases.push_back(std::move(alias));}
    write(fixture.animation,invalid.dump());const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.walker_animation_compatible(),"Exact unchanged 320-reference bound was refused");
    auto excess=aliases[0];excess["alias"]="excess";aliases.push_back(std::move(excess));resource(invalid);
    write(fixture.animation,"{bad-json");fallback(fixture,fixture.detect());
    invalid=complete_resource();invalid["extra-size-witness"]=std::string(1024U*1024U,'x');resource(invalid);
    fixture.reset();fs::remove(fixture.animation);fallback(fixture,fixture.detect());
    fixture.reset();const auto outside=fixture.root/"outside-resource.json";
    fs::rename(fixture.animation,outside);fs::create_symlink(outside,fixture.animation);
    fallback(fixture,fixture.detect());fs::remove(fixture.animation);fs::rename(outside,fixture.animation);
    fixture.reset();
}
void no_blob_fields(const Json& value) {
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        check(key.find("pixels")==std::string::npos && key.find("base64")==std::string::npos &&
            key.find("blob")==std::string::npos && key.find("binary")==std::string::npos,
            "Public walker metadata contains an embedded binary/pixel field");
        no_blob_fields(child);
    } else if (value.is_array()) for (const auto& child:value) no_blob_fields(child);
}
void public_metadata(const fs::path& resources) {
    const auto profile=assets::load_compatibility_profile(resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1/manifest.json");
    check(profile.walker_animation_metadata_error.empty() && !profile.walker_animation_profile.empty(),
        "Public complete walker metadata is missing or invalid");
    std::ifstream input(profile.walker_animation_profile);const auto metadata=Json::parse(input);
    no_blob_fields(metadata);
    check(metadata.at("schema_version")==7 && metadata.at("roles").size()==12,
        "Public profile lost its versioned complete twelve-role inventory");
    for (const auto* role:names) check(metadata.at("roles").contains(role),"Public profile lost a declared role");
    check(!profile.fire_inspector_profile.empty() && !profile.market_walker_profile.empty() &&
        !profile.service_walker_profile.empty() && !profile.health_walker_profile.empty(),
        "Public complete profile removed the historical independent supplements");
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Resource root argument required");Fixture fixture;
        identity_and_selection(fixture);malformed_metadata(fixture);public_metadata(fs::canonical(argv[1]));
        std::cout<<"PASS complete-profile identities, atomic legacy fallback, source pairs, strict metadata, custom selection and bounds\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
