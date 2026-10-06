#include "maps/MapRulesCheck.h"
#include "maps/SandboxPlacement.h"
#include "maps/LandscapeProvenance.h"
#include "maps/StoredMapSession.h"
#include "core/PerformanceDiagnostics.h"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace openemperor;
namespace {
using Bytes=std::vector<std::uint8_t>;
constexpr std::size_t cells=228U*228U;
void check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
void put16(Bytes& bytes,std::size_t at,unsigned value) {
    bytes.at(at)=std::uint8_t(value);bytes.at(at+1)=std::uint8_t(value>>8U);
}
void put32(Bytes& bytes,std::size_t at,unsigned value) {put16(bytes,at,value);put16(bytes,at+2,value>>16U);}
void append32(Bytes& bytes,unsigned value) {const auto at=bytes.size();bytes.resize(at+4);put32(bytes,at,value);}
std::size_t at(unsigned x,unsigned y) {return std::size_t(y)*228U+x;}
Bytes manager(std::string name="",int type=35,unsigned side=0,bool gate=false) {
    Bytes bytes(6,0);put16(bytes,0,1);
    if (name.empty()) return bytes;
    put32(bytes,2,1);
    auto offset=bytes.size();bytes.resize(offset+6+name.size());
    put16(bytes,offset,0xffff);put16(bytes,offset+4,unsigned(name.size()));
    std::copy(name.begin(),name.end(),bytes.begin()+std::ptrdiff_t(offset+6));
    offset=bytes.size();bytes.resize(offset+181+(name=="cIndustrialBldg" || gate ? 130U:0U)+(gate ? 2U:0U),0);
    put16(bytes,offset,4);bytes[offset+2]=3;bytes[offset+5]=std::uint8_t(side);
    put16(bytes,offset+8,71);put16(bytes,offset+10,81);put32(bytes,offset+12,unsigned(at(100,110)));
    put16(bytes,offset+16,unsigned(type));put32(bytes,offset+161,7);
    if (name=="cIndustrialBldg" || gate) {put16(bytes,offset+181,1);put16(bytes,offset+183,1);}
    if (gate) put16(bytes,offset+311,1);
    return bytes;
}
Bytes logical_map(const Bytes& records,bool gate=false) {
    Bytes bytes(std::size_t(maps::original_entities_logical_offset)+records.size(),0);
    constexpr std::array<std::uint8_t,8> signature{5,0,0xfe,0xca,0,0,2,0};
    std::copy(signature.begin(),signature.end(),bytes.begin());put32(bytes,84,170);
    for (std::size_t i=0;i<cells;++i) {
        put32(bytes,std::size_t(maps::terrain_logical_offset)+i*4,0x80);
        bytes[std::size_t(maps::landscape_height_offset)+i]=255;
    }
    if (gate) for (unsigned y=110;y<113;++y) for (unsigned x=100;x<105;++x)
        put32(bytes,std::size_t(maps::terrain_logical_offset)+at(x,y)*4,0x8088);
    std::copy(records.begin(),records.end(),bytes.begin()+std::ptrdiff_t(maps::original_entities_logical_offset));
    return bytes;
}
Bytes compressed_map(const Bytes& bytes) {
    Bytes result;append32(result,0xfedcbaaa);
    for (std::size_t offset=0;offset<bytes.size();offset+=16384) {
        const auto count=std::min<std::size_t>(16384,bytes.size()-offset);
        uLongf length=compressBound(uLong(count));Bytes compressed(length);
        check(compress2(compressed.data(),&length,bytes.data()+offset,uLong(count),6)==Z_OK,"fixture compression failed");
        compressed.resize(length);append32(result,0);append32(result,unsigned(length));append32(result,unsigned(count));
        result.insert(result.end(),compressed.begin(),compressed.end());
    }
    return result;
}
void write(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));
    check(bool(output),"fixture file write failed");
}
struct Root {
    std::filesystem::path path=std::filesystem::temp_directory_path()/
        ("openemperor-map-rules-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Root() {std::filesystem::create_directories(path/"Cities");path=std::filesystem::canonical(path);}
    ~Root() {std::error_code ignored;std::filesystem::remove_all(path,ignored);}
    maps::MapRulesRequest request(std::string file="authored.map",simulation::RulesProfile profile=simulation::RulesProfile::CityV16,
                                  std::uint32_t version=3) const {
        return {path,std::filesystem::path("Cities")/file,profile,version,maps::map_rules_policy_version(profile,version)};
    }
};
std::string producer_failure(const Root& root,const std::string& file) {
    try {
        (void)maps::read_sandbox_map_permissions(root.path,std::filesystem::path("Cities")/file,Bytes(cells,1));
    } catch (const std::exception& error) {return error.what();}
    throw std::runtime_error("producer unexpectedly admitted rejected original input");
}
void positive_and_identity() {
    Root root;const auto bytes=compressed_map(logical_map(manager("cGateHouse",130,1,true),true));
    write(root.path/"Cities/authored.map",bytes);
    const auto request=root.request();
    performance::set_enabled(true);performance::reset();
    const auto result=maps::check_map_rules(request);
    check(result.request==request && result.status==maps::MapRulesStatus::MapRulesChecked,"complete authored gate map must pass its exact rule selection");
    check(result.input_sha256.size()==64 && result.physical_bytes==bytes.size(),"checked identity must hash physical map input");
    check(result.detail.find("session start are still checked when starting")!=std::string::npos,"preflight must not claim full session readiness");
    for (const auto counter:{performance::Counter::WorldCopies,performance::Counter::WorldRestores,
        performance::Counter::WorldExecutes,performance::Counter::BfsCalls,performance::Counter::RouteRefreshes,
        performance::Counter::AssetDecodes,performance::Counter::TextureUploads,performance::Counter::FileWrites,
        performance::Counter::SimulationTicks})
        check(performance::counter(counter)==0,"preflight must not perform World, graphics, tick or save work");
    performance::set_enabled(false);
    check(std::distance(std::filesystem::directory_iterator(root.path/"Cities"),std::filesystem::directory_iterator{})==1,
          "preflight must not create files");
    const auto permissions=maps::read_sandbox_map_permissions(root.path,"Cities/authored.map",Bytes(cells,1));
    check(permissions->gates().size()==1 && permissions->gates()[0].corridor.size()==3 &&
          permissions->cell_height({102,111})==-1,"actual producer must accept the same original gate/height scope");
    const auto source=maps::EmperorContainer::open(root.path/"Cities/authored.map");
    const auto old_map=maps::read_emperor_map(source,0);
    const maps::MapGeometry old_geometry{old_map.declared_map_size};
    maps::StoredGraphicsPlan old_plan;old_plan.data_root=root.path;old_plan.map_relative="Cities/authored.map";
    old_plan.border=old_geometry.border;
    old_plan.height_bytes=source.read_range(0,maps::landscape_height_offset,cells);
    const auto time=std::filesystem::last_write_time(root.path/"Cities/authored.map");
    auto changed=bytes;changed[4]=42; // Ignored block-header word: same size/time and semantics, different input.
    write(root.path/"Cities/authored.map",changed);std::filesystem::last_write_time(root.path/"Cities/authored.map",time);
    const auto revised=maps::check_map_rules(request);
    check(revised.status==maps::MapRulesStatus::MapRulesChecked && revised.physical_bytes==result.physical_bytes &&
          revised.input_sha256!=result.input_sha256,"size/mtime do not replace consistent content identity");
    bool changed_rejected=false;
    performance::set_enabled(true);performance::reset();
    try {
        (void)maps::load_stored_map_session(root.path,"Cities/authored.map",maps::FootprintPolicy::EdgeByte4x4Preview,
            maps::StoredGraphicsProfile::Base,maps::GreatWallPresentationMode::Automatic,result.input_sha256);
    } catch (const std::exception& error) {
        changed_rejected=std::string(error.what())=="Map changed since the map-rules check. Check the selected map again.";
    }
    check(changed_rejected && performance::counter(performance::Counter::AssetDecodes)==0 &&
          performance::counter(performance::Counter::TextureUploads)==0,
          "changed input must be rejected from the owned loading context before graphics preparation");
    performance::set_enabled(false);
    bool reopened_rejected=false;
    try {
        (void)maps::load_sandbox_map_permissions(old_map,old_plan,old_geometry,Bytes(cells,1),1,result.input_sha256);
    } catch (const std::exception& error) {
        reopened_rejected=std::string(error.what())=="Map changed since the map-rules check. Check the selected map again.";
    }
    check(reopened_rejected,"late policy-manager reopen must not mix changed bytes with earlier map/heights");
    check(maps::read_sandbox_map_permissions(root.path,"Cities/authored.map",Bytes(cells,1))->canonical_state()==permissions->canonical_state(),
          "preflight hashing must not change policy/fingerprint input semantics");
    Root other;write(other.path/"Cities/authored.map",bytes);
    const auto duplicate=maps::check_map_rules(other.request());
    check(duplicate.input_sha256==result.input_sha256 && duplicate.request!=request,"same-named maps in distinct roots have distinct selection identities");
}
void unsupported_and_legacy() {
    Root root;write(root.path/"Cities/unknown.map",compressed_map(logical_map(manager("cIndustrialBldg",35,0))));
    const auto unknown=maps::check_map_rules(root.request("unknown.map"));
    check(unknown.status==maps::MapRulesStatus::UnsupportedForRules && unknown.detail.find("cIndustrialBldg type 35")!=std::string::npos,
          "unknown original occupancy is unsupported with a precise source reason");
    check(unknown.detail==producer_failure(root,"unknown.map"),"preflight and producer must use identical occupancy authority");
    for (const auto version:{1U,2U})
        check(maps::check_map_rules(root.request("unknown.map",simulation::RulesProfile::CityV16,version)).status==maps::MapRulesStatus::MapRulesChecked,
              "rule-3 occupancy must not block old City-v16 rules");
    check(maps::check_map_rules(root.request("unknown.map",simulation::RulesProfile::CityV11,3)).status==maps::MapRulesStatus::MapRulesChecked,
          "rule-3 occupancy must not block a consciously selected older profile");
    write(root.path/"Cities/class.map",compressed_map(logical_map(manager("cResWall",35,1))));
    const auto unknown_class=maps::check_map_rules(root.request("class.map"));
    check(unknown_class.status==maps::MapRulesStatus::UnsupportedForRules && unknown_class.detail.find("unsupported MFC class 'cResWall'")!=std::string::npos,
          "unknown class must remain unsupported, not repaired or labelled corrupt input");
    check(unknown_class.detail==producer_failure(root,"class.map"),"preflight and producer must reject the same complete-manager class");
    check(maps::check_map_rules(root.request("class.map",simulation::RulesProfile::CityV10,1)).status==maps::MapRulesStatus::MapRulesChecked,
          "legacy rules do not import the unsupported original manager");
    auto height=logical_map(manager("cGateHouse",130,1,true),true);height[std::size_t(maps::landscape_height_offset)+at(102,111)]=0;
    write(root.path/"Cities/height.map",compressed_map(height));
    const auto invalid_height=maps::check_map_rules(root.request("height.map"));
    check(invalid_height.status==maps::MapRulesStatus::UnsupportedForRules && invalid_height.detail==producer_failure(root,"height.map"),
          "required gate heights must share the exact producer rejection");
}
void input_failures() {
    Root root;
    const auto missing=maps::check_map_rules(root.request("missing.map"));
    check(missing.status==maps::MapRulesStatus::InputError && missing.input_sha256.empty(),"missing map must be retriable InputError");
    auto truncated=logical_map(Bytes{1,0});write(root.path/"Cities/truncated.map",compressed_map(truncated));
    const auto broken=maps::check_map_rules(root.request("truncated.map"));
    check(broken.status==maps::MapRulesStatus::InputError && broken.detail.find("truncated record count")!=std::string::npos,
          "truncated original manager must be InputError, not an unsupported class");
    check(broken.detail==producer_failure(root,"truncated.map"),"same immutable malformed input has the same producer failure");
    auto bad_reference=manager();put32(bad_reference,2,1);bad_reference.resize(8,0);
    write(root.path/"Cities/reference.map",compressed_map(logical_map(bad_reference)));
    check(maps::check_map_rules(root.request("reference.map")).status==maps::MapRulesStatus::InputError,"invalid MFC reference remains a parser input error");
    auto compressed=compressed_map(logical_map(manager()));compressed[16]^=0xff;
    write(root.path/"Cities/zlib.map",compressed);
    check(maps::check_map_rules(root.request("zlib.map")).status==maps::MapRulesStatus::InputError,"damaged compressed block remains InputError");
    auto request=root.request("missing.map");request.policy_version=0;
    check(maps::check_map_rules(request).status==maps::MapRulesStatus::InputError,"mismatched rule/policy identity cannot be granted");
    request=root.request("missing.map");request.rule_version=0;
    check(maps::check_map_rules(request).status==maps::MapRulesStatus::InputError,"rule version must be explicit");
    request=root.request("missing.map");request.map_relative="../missing.map";
    check(maps::check_map_rules(request).status==maps::MapRulesStatus::InputError,"path escape must not create an unsupported cache entry");
}
} // namespace

int main() {
    try {positive_and_identity();unsupported_and_legacy();input_failures();std::cout<<"Map rules check tests passed\n";return 0;}
    catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
