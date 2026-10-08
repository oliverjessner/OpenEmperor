#include "assets/CompatibilityProfile.h"
#include "app/VisualSelection.h"
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>

// Tiny independently authored bytes exercise identity/selection only. Actual
// SG3 decode, live pixels and supply behavior are checked by separate targets.
namespace {
namespace fs=std::filesystem;
namespace assets=openemperor::assets;
using Json=nlohmann::json;
using Source=openemperor::VisualProfileSource;
void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
void write(const fs::path& path,const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);out<<text;check(bool(out),"compatibility fixture write failed");
}
Json read(const fs::path& path) { std::ifstream in(path);return Json::parse(in); }
constexpr const char* hash_a="ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb";
constexpr const char* hash_b="3e23e8160039594a33894f6564e1b1348bbd7a0088d42c4acb73eeaed59c009d";
constexpr const char* hash_c="2e7d2c03a9507ae265ecf5b5356885a53393a2029d241394997265a1a25aefc6";
constexpr const char* hash_d="18ac3e7343f016890c510e93f935261169d9e3f565436429830faf0934f4f8e4";
constexpr const char* hash_e="3f79bb7b435b05321651daefd374cdc681dc06faa65e374e38337b88ca046dea";
Json pair(const char* stem) {
    return Json::array({{{"path",std::string("DATA/")+stem+".sg3"},{"sha256",hash_d}},
                        {{"path",std::string("DATA/")+stem+".555"},{"sha256",hash_e}}});
}
Json family(const char* archive,std::size_t count=2) {
    Json result={{"clip_id","authored-compatibility-family"},{"ticks_per_frame",2},
        {"evidence","Authored metadata for fingerprint/selection tests only."},
        {"idle","frame0"},{"frames",Json::array()},
        {"clips",{{"pos_x",{"frame0","frame1"}},{"neg_x",{"frame0","frame1"}},
                  {"pos_y",{"frame0","frame1"}},{"neg_y",{"frame0","frame1"}}}}};
    for (std::size_t i=0;i<count;++i) result["frames"].push_back({{"alias","frame"+std::to_string(i)},
        {"archive",archive},{"image_index",i+1},{"foot_anchor",{1,3}}});
    return result;
}
Json health_resource(std::size_t count=2) {
    return {{"schema_version",6},{"mode","curated_walker_preview"},
        {"roles",{{"health_worker",family("DATA/health.sg3",count)}}}};
}
Json market_resource() {
    return {{"schema_version",4},{"mode","curated_walker_preview"},
        {"roles",{{"supplier",family("DATA/market.sg3")},
                  {"distributor",family("DATA/market.sg3")}}}};
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-health-compatibility-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",resources=root/"resources",pack=resources/"Compatibility/authored-pack";
    fs::path manifest=pack/"manifest.json",health=pack/"health-walker.json";
    Fixture() { reset(); }
    ~Fixture() { std::error_code ignored;fs::remove_all(root,ignored); }
    Json document() const {
        return {{"schema_version",1},{"id","authored-pack"},{"evidence","Authored fingerprint fixture."},
            {"files",Json::array({{{"path","core/A"},{"sha256",hash_a}},
                                  {{"path","core/B"},{"sha256",hash_b}},
                                  {{"path","core/C"},{"sha256",hash_c}}})},
            {"profiles",{{"walker","walkers.json"},{"building","buildings.json"},{"road","roads.json"}}},
            {"optional_fire",{{"profile","fire.json"},{"files",pair("fire")}}},
            {"optional_fire_inspector",{{"profile","fire-inspector.json"},{"files",pair("inspector")}}},
            {"optional_market_walkers",{{"profile","market-walkers.json"},{"files",pair("market")}}},
            {"optional_service_walkers",{{"profile","service-walker.json"},{"files",pair("service")}}},
            {"optional_health_walkers",{{"profile","health-walker.json"},{"files",pair("health")}}}};
    }
    void reset() const {
        write(data/"core/A","a");write(data/"core/B","b");write(data/"core/C","c");
        for (const auto stem:{"fire","inspector","market","service","health","other"}) {
            write(data/"DATA"/(std::string(stem)+".sg3"),"d");
            write(data/"DATA"/(std::string(stem)+".555"),"e");
        }
        for (const auto name:{"walkers.json","buildings.json","roads.json","fire.json"}) write(pack/name,"{}");
        write(pack/"fire-inspector.json",Json{{"schema_version",3},
            {"roles",{{"fire_inspector",family("DATA/inspector.sg3")}}}}.dump());
        write(pack/"market-walkers.json",market_resource().dump());
        write(pack/"service-walker.json",Json{{"schema_version",5},{"roles",{{"service",family("DATA/service.sg3")}}}}.dump());
        write(health,health_resource().dump());write(manifest,document().dump());
    }
    assets::CompatibilityResult detect() const {
        return assets::detect_compatibility(data,resources/"Compatibility");
    }
};
void preserved(const Fixture& fixture,const assets::CompatibilityResult& result) {
    check(result.compatible() && result.files_hashed==3 && result.profile->files.size()==3,
          "optional health failure changed core identity");
    check(result.fire_compatible() && result.fire_files_hashed==2 &&
          result.fire_inspector_compatible() && result.fire_inspector_files_hashed==2 &&
          result.market_walker_compatible() && result.market_walker_files_hashed==2 &&
          result.service_walker_compatible() && result.service_walker_files_hashed==2,
          "health failure disabled independently valid fire/Inspector");
    const auto selected=openemperor::select_visual_profiles(result);
    check(selected.walker_source==Source::Builtin && selected.building_source==Source::Builtin &&
          selected.road_source==Source::Builtin && selected.fire_source==Source::Builtin &&
          selected.fire_inspector_source==Source::Builtin && selected.market_walker_source==Source::Builtin &&
          selected.market_walker==fixture.pack/"market-walkers.json" && selected.service_walker_source==Source::Builtin &&
          selected.service_walker==fixture.pack/"service-walker.json" && selected.walker==fixture.pack/"walkers.json" &&
          selected.fire==fixture.pack/"fire.json" && selected.fire_inspector==fixture.pack/"fire-inspector.json",
          "optional health failure displaced existing selections");
}
void fallback(const Fixture& fixture,const assets::CompatibilityResult& result) {
    preserved(fixture,result);
    const auto selected=openemperor::select_visual_profiles(result);
    check(!result.health_walker_compatible() && selected.health_walker.empty() &&
          selected.health_walker_source==Source::Fallback && !selected.health_walker_fallback_reason.empty(),
          "optional health fallback was activated or lacks a reason");
}
void selection_and_dependencies(Fixture& fixture) {
    const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.health_walker_compatible() && exact.health_walker_files_hashed==2 &&
          exact.profile->health_walker_files.size()==2,"health pair not independently fingerprinted");
    const auto builtin=openemperor::select_visual_profiles(exact);
    check(builtin.health_walker==fixture.health && builtin.health_walker_source==Source::Builtin &&
          builtin.health_walker_fallback_reason.empty(),"matching health metadata not selected automatically");
    const auto custom=openemperor::select_visual_profiles(exact,"explicit-custom.json");
    check(custom.walker_source==Source::Custom && custom.walker=="explicit-custom.json" &&
          custom.health_walker.empty() && custom.health_walker_source==Source::Custom &&
          !custom.health_walker_fallback_reason.empty() && custom.fire_inspector.empty() &&
          custom.fire_inspector_source==Source::Custom && custom.market_walker.empty() &&
          custom.market_walker_source==Source::Custom && custom.service_walker.empty() &&
          custom.service_walker_source==Source::Custom && custom.fire_source==Source::Builtin,
          "explicit custom walker gained hidden supplements or displaced fire");
    const auto other=openemperor::select_visual_profiles(exact,{},"building-custom.json","road-custom.json");
    check(other.health_walker==builtin.health_walker && other.health_walker_source==Source::Builtin &&
          other.fire_inspector_source==Source::Builtin,"other-category custom override suppressed health walkers");
    const auto detected=openemperor::detect_and_select_visual_profiles(fixture.data,fixture.resources);
    check(detected.health_walker==fixture.health && detected.health_walker_source==Source::Builtin,
          "normal resource-root selection omitted health metadata");
    const auto before=assets::compatibility_detection_count();
    for (int i=0;i<1000;++i) (void)openemperor::select_visual_profiles(exact);
    check(assets::compatibility_detection_count()==before,"pure selection repeated fingerprint detection");
    for (const auto extension:{".sg3",".555"}) {
        const auto file=fixture.data/"DATA"/(std::string("health")+extension);
        fs::remove(file);const auto missing=fixture.detect();fallback(fixture,missing);
        check(missing.health_walker_status==assets::CompatibilityStatus::MissingFile,
              "missing health dependency lacks independent missing-file status");
        write(file,std::string_view(extension)==".sg3" ? "d":"e");
        write(file,"changed");const auto changed=fixture.detect();fallback(fixture,changed);
        check(changed.health_walker_status==assets::CompatibilityStatus::FingerprintMismatch &&
              changed.health_walker_files_hashed==(std::string_view(extension)==".sg3" ? 1U:2U),
              "health dependency identity inferred from another file");
        write(file,std::string_view(extension)==".sg3" ? "d":"e");
    }
    const auto file=fixture.data/"DATA/health.555",outside=fixture.root/"outside.555";
    fs::rename(file,outside);fs::create_symlink(outside,file);
    auto unsafe=fixture.detect();fallback(fixture,unsafe);
    check(unsafe.health_walker_status==assets::CompatibilityStatus::FingerprintMismatch,
          "escaping dependency symlink was trusted");
    fs::remove(file);fs::rename(outside,file);
    // Even an in-root link is outside the strict automatic fingerprint contract.
    fs::rename(file,fixture.data/"DATA/inside.555");fs::create_symlink("inside.555",file);
    unsafe=fixture.detect();fallback(fixture,unsafe);
    fs::remove(file);fs::rename(fixture.data/"DATA/inside.555",file);
    auto doc=fixture.document();doc.erase("optional_health_walkers");write(fixture.manifest,doc.dump());
    const auto legacy=fixture.detect();fallback(fixture,legacy);
    check(legacy.health_walker_files_hashed==0,"old metadata inferred health activation");
    fixture.reset();write(fixture.data/"core/C","unknown");const auto unknown=fixture.detect();
    check(!unknown.compatible() && !unknown.health_walker_compatible() &&
          openemperor::select_visual_profiles(unknown).health_walker.empty(),
          "matching optional health assets overrode unknown core revision");
    fixture.reset();
}
void malformed_metadata(Fixture& fixture) {
    const auto invalid_section=[&](Json section) {
        auto doc=fixture.document();doc["optional_health_walkers"]=std::move(section);
        write(fixture.manifest,doc.dump());const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->health_walker_metadata_error.empty() && result.health_walker_files_hashed==0 &&
              result.profile->health_walker_profile.empty() && result.profile->health_walker_files.empty(),
              "malformed health metadata was partially published");
    };
    invalid_section({{"profile","../health-walker.json"},{"files",pair("health")}});
    invalid_section({{"profile","health-walker.json"},{"files",Json::array()}});
    invalid_section({{"profile","health-walker.json"},{"files",Json::array()},
                     {"core_files",{"core/A","core/B"}}});
    auto files=pair("health");files[0]["path"]="../outside";
    invalid_section({{"profile","health-walker.json"},{"files",files}});
    files=pair("health");files[0]["sha256"]=std::string(64,'A');
    invalid_section({{"profile","health-walker.json"},{"files",files}});
    files=pair("health");files.push_back(files[0]);
    invalid_section({{"profile","health-walker.json"},{"files",files}});
    files=pair("health");for (int i=0;i<3;++i) files.push_back(files[0]);
    invalid_section({{"profile","health-walker.json"},{"files",files}});
    files=pair("health");files.erase(files.begin()+1);
    invalid_section({{"profile","health-walker.json"},{"files",files}});
    const auto invalid_resource=[&](Json resource) {
        write(fixture.manifest,fixture.document().dump());write(fixture.health,resource.dump());
        const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->health_walker_metadata_error.empty(),"bad resource lacks metadata reason");
    };
    auto bad=health_resource();for(int schema:{1,2,3,4,5,7}){bad["schema_version"]=schema;invalid_resource(bad);}
    bad=health_resource();bad["roles"].erase("health_worker");invalid_resource(bad);
    bad=health_resource();bad["roles"]["fire_inspector"]=family("DATA/inspector.sg3");invalid_resource(bad);
    bad=health_resource();bad["roles"]["health_worker"]["frames"]=Json::array();invalid_resource(bad);
    bad=health_resource(321);invalid_resource(bad);
    bad=health_resource();bad["roles"]["health_worker"]["frames"][1]["archive"]="DATA/other.sg3";
    invalid_resource(bad);
    bad=health_resource();bad["roles"]["health_worker"]["frames"][1]["archive"]="../escape.sg3";
    invalid_resource(bad);
    bad=health_resource();bad["roles"]["health_worker"]["frames"][1]["archive"]="DATA/health.555";
    invalid_resource(bad);
    write(fixture.health,"{bad-json");const auto broken=fixture.detect();fallback(fixture,broken);
    check(!broken.profile->health_walker_metadata_error.empty(),"malformed JSON rejected whole compatibility");
    fixture.reset();fs::remove(fixture.health);const auto missing=fixture.detect();fallback(fixture,missing);
    check(!missing.profile->health_walker_metadata_error.empty(),"missing resource not separately diagnosed");
    fixture.reset();const auto outside=fixture.root/"outside-resource.json";
    fs::rename(fixture.health,outside);fs::create_symlink(outside,fixture.health);
    const auto unsafe=fixture.detect();fallback(fixture,unsafe);
    check(!unsafe.profile->health_walker_metadata_error.empty(),"resource symlink activated health metadata");
    fs::remove(fixture.health);fs::rename(outside,fixture.health);
    // Exactly320 references and four individually pinned dependencies are legal.
    auto doc=fixture.document();files=pair("health");
    for (const auto& fingerprint:pair("other")) files.push_back(fingerprint);
    doc["optional_health_walkers"]["files"]=files;write(fixture.manifest,doc.dump());
    auto resource=health_resource(320);
    for (std::size_t i=128;i<320;++i) resource["roles"]["health_worker"]["frames"][i]["archive"]="DATA/other.sg3";
    write(fixture.health,resource.dump());const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.health_walker_compatible() && exact.health_walker_files_hashed==4 &&
          exact.profile->health_walker_files.size()==4,"exact reference/dependency bounds rejected");
    fixture.reset();
}
void no_blob_fields(const Json& value) {
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        check(key.find("pixels")==std::string::npos && key.find("base64")==std::string::npos &&
              key.find("blob")==std::string::npos && key.find("binary")==std::string::npos,
              "public health metadata embeds original binary pixels");
        no_blob_fields(child);
    } else if (value.is_array()) for (const auto& child:value) no_blob_fields(child);
}
void public_pack(const fs::path& resources) {
    const auto pack=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    const auto profile=assets::load_compatibility_profile(pack/"manifest.json");
    check(profile.health_walker_metadata_error.empty() && !profile.health_walker_profile.empty() &&
          profile.health_walker_files.size()==2&&profile.files.size()==6&&!profile.fire_profile.empty()&&!profile.fire_inspector_profile.empty()&&
          !profile.market_walker_profile.empty()&&!profile.service_walker_profile.empty(),"public Health supplement/dependency pair is missing or invalid");
    const auto resource=read(profile.health_walker_profile);no_blob_fields(resource);
    check(resource.at("schema_version")==6 && resource.at("roles").size()==1 &&
          resource.at("roles").contains("health_worker"),"public Health role/schema changed");
    const std::map<std::string,std::string> expected_fingerprints={
        {"DATA/SprMain.sg3","3e2817d2644453acc92068d6c1e26532212be213b43b848ebe95ba21654adef8"},
        {"DATA/SprMain.555","d84eb6759e9ce50b0ad75596773b9224f9c1a9b8dd80b691c066c3de7e8df438"}};
    std::set<fs::path> dependencies;
    for(const auto& fingerprint:profile.health_walker_files) {
        check(expected_fingerprints.at(fingerprint.relative_path.generic_string())==fingerprint.sha256,
            "public Health separate dependency fingerprint changed");dependencies.insert(fingerprint.relative_path);
    }
    const auto& role=resource.at("roles").at("health_worker");
    check(role.at("clip_id")=="curated-sprmain-health-worker-walk"&&role.at("ticks_per_frame")==2&&
        role.at("frames").size()==48,"public Health cadence/clip/count changed");
    std::map<std::string,Json> references;std::set<std::uint32_t> physical;
    for(const auto& frame:role.at("frames")) {
        fs::path archive(frame.at("archive").get<std::string>()),bitmap=archive;bitmap.replace_extension(".555");
        check(dependencies.contains(archive)&&dependencies.contains(bitmap)&&frame.at("flip_x").is_boolean()&&
            frame.at("foot_anchor").is_array()&&frame.at("foot_anchor").size()==2&&
            references.emplace(frame.at("alias").get<std::string>(),frame).second,
            "public Health frame lacks pinned dependencies, explicit feet/flip or unique alias");
        physical.insert(frame.at("image_index").get<std::uint32_t>());
    }
    for(const auto* direction:{"pos_x","neg_x","pos_y","neg_y"}) {
        const auto& clip=role.at("clips").at(direction);check(clip.size()==12,"public Health direction lost twelve phases");
        for(std::size_t phase=0;phase<clip.size();++phase) {
            const auto& frame=references.at(clip[phase].get<std::string>());const auto dir=std::string_view(direction);
            const auto column=dir=="pos_x"||dir=="pos_y"?2U:0U;
            check(frame.at("image_index").get<std::size_t>()==2925U+column+8U*phase&&
                frame.at("flip_x").get<bool>()==(dir=="neg_x"||dir=="pos_y"),
                "public Health references a mirror record or wrong display transform");
        }
    }
    check(physical.size()==24,"public Health lost native physical dedupe");
    std::cout<<"Public Health metadata/fingerprint coverage PASS (no original pixel decoding claimed).\n";

}

}
int main(int argc,char* argv[]) {
    try {
        check(argc==2,"resource-root argument required");Fixture fixture;
        selection_and_dependencies(fixture);malformed_metadata(fixture);public_pack(fs::canonical(argv[1]));
        std::cout<<"PASS independent health fingerprints/resources, atomic fallback, exact bounds, custom-only selection.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"HealthWalkerCompatibilityTests: "<<error.what()<<'\n';return 1;
    }
}
