#include "assets/CompatibilityProfile.h"
#include "app/VisualSelection.h"
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
Json market_resource(std::size_t supplier=2,std::size_t distributor=2) {
    return {{"schema_version",4},{"mode","curated_walker_preview"},
        {"roles",{{"supplier",family("DATA/market.sg3",supplier)},
                  {"distributor",family("DATA/market.sg3",distributor)}}}};
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-market-compatibility-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",resources=root/"resources",pack=resources/"Compatibility/authored-pack";
    fs::path manifest=pack/"manifest.json",market=pack/"market-walkers.json";
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
            {"optional_market_walkers",{{"profile","market-walkers.json"},{"files",pair("market")}}}};
    }
    void reset() const {
        write(data/"core/A","a");write(data/"core/B","b");write(data/"core/C","c");
        for (const auto stem:{"fire","inspector","market","other"}) {
            write(data/"DATA"/(std::string(stem)+".sg3"),"d");
            write(data/"DATA"/(std::string(stem)+".555"),"e");
        }
        for (const auto name:{"walkers.json","buildings.json","roads.json","fire.json"}) write(pack/name,"{}");
        write(pack/"fire-inspector.json",Json{{"schema_version",3},
            {"roles",{{"fire_inspector",family("DATA/inspector.sg3")}}}}.dump());
        write(market,market_resource().dump());write(manifest,document().dump());
    }
    assets::CompatibilityResult detect() const {
        return assets::detect_compatibility(data,resources/"Compatibility");
    }
};
void preserved(const Fixture& fixture,const assets::CompatibilityResult& result) {
    check(result.compatible() && result.files_hashed==3 && result.profile->files.size()==3,
          "optional market failure changed core identity");
    check(result.fire_compatible() && result.fire_files_hashed==2 &&
          result.fire_inspector_compatible() && result.fire_inspector_files_hashed==2,
          "market failure disabled independently valid fire/Inspector");
    const auto selected=openemperor::select_visual_profiles(result);
    check(selected.walker_source==Source::Builtin && selected.building_source==Source::Builtin &&
          selected.road_source==Source::Builtin && selected.fire_source==Source::Builtin &&
          selected.fire_inspector_source==Source::Builtin && selected.walker==fixture.pack/"walkers.json" &&
          selected.fire==fixture.pack/"fire.json" && selected.fire_inspector==fixture.pack/"fire-inspector.json",
          "optional market failure displaced existing selections");
}
void fallback(const Fixture& fixture,const assets::CompatibilityResult& result) {
    preserved(fixture,result);
    const auto selected=openemperor::select_visual_profiles(result);
    check(!result.market_walker_compatible() && selected.market_walker.empty() &&
          selected.market_walker_source==Source::Fallback && !selected.market_walker_fallback_reason.empty(),
          "optional market fallback was activated or lacks a reason");
}
void selection_and_dependencies(Fixture& fixture) {
    const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.market_walker_compatible() && exact.market_walker_files_hashed==2 &&
          exact.profile->market_walker_files.size()==2,"market pair not independently fingerprinted");
    const auto builtin=openemperor::select_visual_profiles(exact);
    check(builtin.market_walker==fixture.market && builtin.market_walker_source==Source::Builtin &&
          builtin.market_walker_fallback_reason.empty(),"matching market metadata not selected automatically");
    const auto custom=openemperor::select_visual_profiles(exact,"explicit-custom.json");
    check(custom.walker_source==Source::Custom && custom.walker=="explicit-custom.json" &&
          custom.market_walker.empty() && custom.market_walker_source==Source::Custom &&
          !custom.market_walker_fallback_reason.empty() && custom.fire_inspector.empty() &&
          custom.fire_inspector_source==Source::Custom && custom.fire_source==Source::Builtin,
          "explicit custom walker gained hidden supplements or displaced fire");
    const auto other=openemperor::select_visual_profiles(exact,{},"building-custom.json","road-custom.json");
    check(other.market_walker==builtin.market_walker && other.market_walker_source==Source::Builtin &&
          other.fire_inspector_source==Source::Builtin,"other-category custom override suppressed market walkers");
    const auto detected=openemperor::detect_and_select_visual_profiles(fixture.data,fixture.resources);
    check(detected.market_walker==fixture.market && detected.market_walker_source==Source::Builtin,
          "normal resource-root selection omitted market metadata");
    const auto before=assets::compatibility_detection_count();
    for (int i=0;i<1000;++i) (void)openemperor::select_visual_profiles(exact);
    check(assets::compatibility_detection_count()==before,"pure selection repeated fingerprint detection");
    for (const auto extension:{".sg3",".555"}) {
        const auto file=fixture.data/"DATA"/(std::string("market")+extension);
        fs::remove(file);const auto missing=fixture.detect();fallback(fixture,missing);
        check(missing.market_walker_status==assets::CompatibilityStatus::MissingFile,
              "missing market dependency lacks independent missing-file status");
        write(file,std::string_view(extension)==".sg3" ? "d":"e");
        write(file,"changed");const auto changed=fixture.detect();fallback(fixture,changed);
        check(changed.market_walker_status==assets::CompatibilityStatus::FingerprintMismatch &&
              changed.market_walker_files_hashed==(std::string_view(extension)==".sg3" ? 1U:2U),
              "market dependency identity inferred from another file");
        write(file,std::string_view(extension)==".sg3" ? "d":"e");
    }
    const auto file=fixture.data/"DATA/market.555",outside=fixture.root/"outside.555";
    fs::rename(file,outside);fs::create_symlink(outside,file);
    auto unsafe=fixture.detect();fallback(fixture,unsafe);
    check(unsafe.market_walker_status==assets::CompatibilityStatus::FingerprintMismatch,
          "escaping dependency symlink was trusted");
    fs::remove(file);fs::rename(outside,file);
    // Even an in-root link is outside the strict automatic fingerprint contract.
    fs::rename(file,fixture.data/"DATA/inside.555");fs::create_symlink("inside.555",file);
    unsafe=fixture.detect();fallback(fixture,unsafe);
    fs::remove(file);fs::rename(fixture.data/"DATA/inside.555",file);
    auto doc=fixture.document();doc.erase("optional_market_walkers");write(fixture.manifest,doc.dump());
    const auto legacy=fixture.detect();fallback(fixture,legacy);
    check(legacy.market_walker_files_hashed==0,"old metadata inferred market activation");
    fixture.reset();write(fixture.data/"core/C","unknown");const auto unknown=fixture.detect();
    check(!unknown.compatible() && !unknown.market_walker_compatible() &&
          openemperor::select_visual_profiles(unknown).market_walker.empty(),
          "matching optional market assets overrode unknown core revision");
    fixture.reset();
}
void malformed_metadata(Fixture& fixture) {
    const auto invalid_section=[&](Json section) {
        auto doc=fixture.document();doc["optional_market_walkers"]=std::move(section);
        write(fixture.manifest,doc.dump());const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->market_walker_metadata_error.empty() && result.market_walker_files_hashed==0 &&
              result.profile->market_walker_profile.empty() && result.profile->market_walker_files.empty(),
              "malformed market metadata was partially published");
    };
    invalid_section({{"profile","../market-walkers.json"},{"files",pair("market")}});
    invalid_section({{"profile","market-walkers.json"},{"files",Json::array()}});
    invalid_section({{"profile","market-walkers.json"},{"files",Json::array()},
                     {"core_files",{"core/A","core/B"}}});
    auto files=pair("market");files[0]["path"]="../outside";
    invalid_section({{"profile","market-walkers.json"},{"files",files}});
    files=pair("market");files[0]["sha256"]=std::string(64,'A');
    invalid_section({{"profile","market-walkers.json"},{"files",files}});
    files=pair("market");files.push_back(files[0]);
    invalid_section({{"profile","market-walkers.json"},{"files",files}});
    files=pair("market");for (int i=0;i<3;++i) files.push_back(files[0]);
    invalid_section({{"profile","market-walkers.json"},{"files",files}});
    files=pair("market");files.erase(files.begin()+1);
    invalid_section({{"profile","market-walkers.json"},{"files",files}});
    const auto invalid_resource=[&](Json resource) {
        write(fixture.manifest,fixture.document().dump());write(fixture.market,resource.dump());
        const auto result=fixture.detect();fallback(fixture,result);
        check(!result.profile->market_walker_metadata_error.empty(),"bad resource lacks metadata reason");
    };
    auto bad=market_resource();bad["schema_version"]=3;invalid_resource(bad);
    bad=market_resource();bad["roles"].erase("distributor");invalid_resource(bad);
    bad=market_resource();bad["roles"]["fire_inspector"]=family("DATA/inspector.sg3");invalid_resource(bad);
    bad=market_resource();bad["roles"]["supplier"]["frames"]=Json::array();invalid_resource(bad);
    bad=market_resource(257,2);invalid_resource(bad);
    bad=market_resource(128,129);invalid_resource(bad);
    bad=market_resource();bad["roles"]["distributor"]["frames"][1]["archive"]="DATA/other.sg3";
    invalid_resource(bad);
    bad=market_resource();bad["roles"]["distributor"]["frames"][1]["archive"]="../escape.sg3";
    invalid_resource(bad);
    bad=market_resource();bad["roles"]["distributor"]["frames"][1]["archive"]="DATA/market.555";
    invalid_resource(bad);
    write(fixture.market,"{bad-json");const auto broken=fixture.detect();fallback(fixture,broken);
    check(!broken.profile->market_walker_metadata_error.empty(),"malformed JSON rejected whole compatibility");
    fixture.reset();fs::remove(fixture.market);const auto missing=fixture.detect();fallback(fixture,missing);
    check(!missing.profile->market_walker_metadata_error.empty(),"missing resource not separately diagnosed");
    fixture.reset();const auto outside=fixture.root/"outside-resource.json";
    fs::rename(fixture.market,outside);fs::create_symlink(outside,fixture.market);
    const auto unsafe=fixture.detect();fallback(fixture,unsafe);
    check(!unsafe.profile->market_walker_metadata_error.empty(),"resource symlink activated market metadata");
    fs::remove(fixture.market);fs::rename(outside,fixture.market);
    // Exactly256 references and four individually pinned dependencies are legal.
    auto doc=fixture.document();files=pair("market");
    for (const auto& fingerprint:pair("other")) files.push_back(fingerprint);
    doc["optional_market_walkers"]["files"]=files;write(fixture.manifest,doc.dump());
    auto resource=market_resource(128,128);
    for (auto& frame:resource["roles"]["distributor"]["frames"]) frame["archive"]="DATA/other.sg3";
    write(fixture.market,resource.dump());const auto exact=fixture.detect();preserved(fixture,exact);
    check(exact.market_walker_compatible() && exact.market_walker_files_hashed==4 &&
          exact.profile->market_walker_files.size()==4,"exact reference/dependency bounds rejected");
    fixture.reset();
}
void no_blob_fields(const Json& value) {
    if (value.is_object()) for (const auto& [key,child]:value.items()) {
        check(key.find("pixels")==std::string::npos && key.find("base64")==std::string::npos &&
              key.find("blob")==std::string::npos && key.find("binary")==std::string::npos,
              "public market metadata embeds original binary pixels");
        no_blob_fields(child);
    } else if (value.is_array()) for (const auto& child:value) no_blob_fields(child);
}
void public_pack(const fs::path& resources) {
    const auto pack=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    const auto manifest=pack/"manifest.json";
    const auto document=read(manifest);
    if (!document.contains("optional_market_walkers")) {
        std::cout<<"Public market resource not declared; original integration remains pending.\n";return;
    }
    const auto profile=assets::load_compatibility_profile(manifest);
    check(profile.market_walker_metadata_error.empty() && !profile.market_walker_profile.empty() &&
          profile.market_walker_files.size()>=1 && profile.market_walker_files.size()<=4,
          "public market section is invalid or lacks independent dependencies");
    check(profile.files.size()==6 && !profile.fire_profile.empty() && !profile.fire_inspector_profile.empty(),
          "public optional section changed existing core/fire/Inspector metadata");
    const auto resource=read(profile.market_walker_profile);no_blob_fields(resource);
    check(resource.at("schema_version")==4 && resource.at("mode")=="curated_walker_preview" &&
          resource.at("roles").size()==2 && resource.at("roles").contains("supplier") &&
          resource.at("roles").contains("distributor"),"public resource declared wrong families/schema");
    std::set<fs::path> dependencies;
    for (const auto& fingerprint:profile.market_walker_files) dependencies.insert(fingerprint.relative_path);
    std::size_t aliases=0;
    for (const auto& [name,role]:resource.at("roles").items()) {
        (void)name;const auto& frames=role.at("frames");aliases+=frames.size();
        check(frames.is_array() && !frames.empty() && frames.size()<=256,"invalid public frame references");
        std::set<std::string> frame_aliases;
        for (const auto& frame:frames) {
            fs::path archive(frame.at("archive").get<std::string>()),bitmap=archive;bitmap.replace_extension(".555");
            check(dependencies.contains(archive) && dependencies.contains(bitmap) &&
                  frame_aliases.insert(frame.at("alias").get<std::string>()).second,
                  "public frame lacks pinned dependencies or unique alias");
        }
        for (const auto direction:{"pos_x","neg_x","pos_y","neg_y"}) {
            const auto& clip=role.at("clips").at(direction);
            check(clip.is_array() && clip.size()>=2 && clip.size()<=64,"public moving direction incomplete");
            for (const auto& alias:clip) check(frame_aliases.contains(alias.get<std::string>()),"public clip uses absent alias");
        }
    }
    check(aliases<=256,"public family aliases bypass global budget");
    std::cout<<"Public market metadata/fingerprint coverage PASS (no original pixel decoding claimed).\n";
}
}
int main(int argc,char* argv[]) {
    try {
        check(argc==2,"resource-root argument required");Fixture fixture;
        selection_and_dependencies(fixture);malformed_metadata(fixture);public_pack(fs::canonical(argv[1]));
        std::cout<<"PASS independent market fingerprints/resources, atomic fallback, exact bounds, custom-only selection.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"MarketWalkerCompatibilityTests: "<<error.what()<<'\n';return 1;
    }
}
