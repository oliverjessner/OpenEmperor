#include "assets/BuildingVisualProfile.h"
#include "assets/CompatibilityProfile.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
namespace fs=std::filesystem;
namespace a=openemperor::assets;
namespace perf=openemperor::performance;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
Bytes read(const fs::path& p) { std::ifstream in(p,std::ios::binary);check(bool(in),"missing test input");return Bytes(std::istreambuf_iterator<char>(in),{}); }
void write(const fs::path& p,const Bytes& data) {
    fs::create_directories(p.parent_path());std::ofstream out(p,std::ios::binary);
    out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));check(bool(out),"fixture write failed");
}
void text(const fs::path& p,const std::string& value) { write(p,Bytes(value.begin(),value.end())); }
void write(const fs::path& p,const Json& j) { text(p,j.dump()); }
std::string hash(const Bytes& data) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    check(EVP_Digest(data.data(),data.size(),digest.data(),&length,EVP_sha256(),nullptr)==1 && length==32,"test SHA256 failed");
    std::ostringstream result;result<<std::hex<<std::setfill('0');
    for (unsigned i=0;i<length;++i) result<<std::setw(2)<<static_cast<unsigned>(digest[i]);
    return result.str();
}
void u16(Bytes& data,std::size_t at,unsigned n) {data.at(at)=static_cast<std::uint8_t>(n);data.at(at+1)=static_cast<std::uint8_t>(n>>8U);}
void u32(Bytes& data,std::size_t at,unsigned n) {u16(data,at,n);u16(data,at+2,n>>16U);}
Json entries(const auto& rows) {
    Json result=Json::array();
    for (const auto& e:rows) {
        if (!e) result.push_back(nullptr);
        else result.push_back({e->id.archive_relative_path.generic_string(),e->id.image_index,e->image_index,
                              e->ground_x,e->ground_y,e->footprint_side,e->evidence});
    }
    return result;
}
Json state(const a::BuildingVisualProfile& p) {
    Json images=Json::array();
    Json fields=Json::array();for(const auto& stages:p.original_field_entries)fields.push_back(entries(stages));
    for (const auto& image:p.unique_images) images.push_back({image.width,image.height,hash(image.pixels)});
    return {{"images",images},{"core",entries(p.entries)},{"houses",entries(p.original_house_entries)},
        {"house_prepared",p.original_house_prepared},{"house_fallbacks",p.original_house_fallbacks},
        {"original",entries(p.original_building_entries)},{"prepared",p.original_building_prepared},
        {"fallbacks",p.original_building_fallbacks},{"field_prepared",p.original_field_prepared},{"field_stages",fields}};
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-original-building-profile-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"buildings.json";
    Bytes sg3=Bytes(40680U+223U*64U,0),bitmap=Bytes(4U+12800U+28800U+12800U+18U*3200U,0);
    a::RgbaImage legacy_image;
    Fixture() {
        u32(sg3,0,static_cast<unsigned>(sg3.size()));u32(sg3,4,213);
        u32(sg3,12,223);u32(sg3,16,222);u32(sg3,20,2);
        const std::string system="Zeus_system.bmp",normal="synthetic.bmp";
        std::copy(system.begin(),system.end(),sg3.begin()+680);std::copy(normal.begin(),normal.end(),sg3.begin()+880);
        u32(sg3,680+124,200);u32(sg3,680+128,1);u32(sg3,680+132,200);
        u32(sg3,880+124,22);u32(sg3,880+128,201);u32(sg3,880+132,222);
        for (unsigned i=0;i<173;++i) u16(sg3,80U+2U*i,201);
        for (const auto& id:a::original_building_visual_identities)
            if (id.footprint_side==3) u16(sg3,80U+2U*((id.group_key%512U)-1U),202);
        u16(sg3,80U+2U*((0x4adU%512U)-1U),204);
        for(std::size_t index=0;index<a::original_field_role_count;++index) {
            const auto* identity=a::original_building_visual_identity(a::original_field_roles[index]);
            u16(sg3,80U+2U*((identity->group_key%512U)-1U),205U+6U*static_cast<unsigned>(index));
        }
        const auto record=[&](unsigned index,unsigned side,unsigned height,unsigned offset) {
            const auto at=40680U+64U*index,base=3200U*side*side;
            u32(sg3,at,offset);u32(sg3,at+4,base);u32(sg3,at+8,base);
            u16(sg3,at+20,80U*side-2U);u16(sg3,at+22,height);u16(sg3,at+50,30);
            sg3[at+55]=static_cast<std::uint8_t>(side);sg3[at+56]=1;
            // Real base images may also declare animation metadata. Static
            // selection must retain their complete base without inventing it.
            if(side>1){u16(sg3,at+30,12);u16(sg3,at+34,8);u16(sg3,at+36,10);sg3[at+58]=4;}
        };
        record(201,2,90,4);record(202,3,130,12804);record(203,2,90,4);record(204,2,137,41604);
        for(unsigned index=0;index<18;++index)record(205U+index,1,40U+index%6U,54404U+index*3200U);
        for (std::size_t at=4;at<bitmap.size();at+=2)u16(bitmap,at,0x03e0);
        save();legacy_image=a::load_sg3_image({data/"DATA/China_General.sg3",201});
    }
    ~Fixture(){std::error_code e;fs::remove_all(root,e);}
    void save() const {write(data/"DATA/China_General.sg3",sg3);write(data/"DATA/China_General.555",bitmap);}
    Json valid(unsigned schema=2) const {
        Json rows=Json::array();
        for (const auto& id:a::original_building_visual_identities) {
            if (schema==1 && static_cast<std::size_t>(id.role)>=a::original_building_schema1_role_count) break;
            if (!id.static_base_supported) {
                if(schema==3) {
                    Json stages=Json::array();const auto index=*a::original_field_role_index(id.role);
                    for(unsigned stage=0;stage<6;++stage)stages.push_back({{"stage",stage},
                        {"image_index",205U+6U*static_cast<unsigned>(index)+stage},{"width",78U},{"height",40U+stage},
                        {"base_bytes",3200U},{"ground_anchor",{39U,20U+stage}}});
                    rows.push_back({{"model_type",id.model_type},{"original_side",1U},{"native",true},
                        {"fallback","Explicit synthetic authored field fallback"},{"archive","DATA/China_General.sg3"},
                        {"group_key",id.group_key},{"stages",stages},{"evidence","Synthetic source family; AUTHORED progress quantization only"}});
                } else rows.push_back({{"model_type",id.model_type},{"original_side",id.footprint_side},{"native",false},
                        {"fallback","Field state is not reconstructed; explicit authored fallback"},
                        {"evidence","Known original stateful selector is not represented by World progress"}});
                continue;
            }
            const unsigned side=id.footprint_side,height=id.role==a::OriginalBuildingVisualRole::HempFarm ? 137U:(side==2 ? 90U:130U);
            rows.push_back({{"model_type",id.model_type},{"original_side",side},{"fallback","Explicit synthetic named fallback"},
                {"archive","DATA/China_General.sg3"},{"image_index",id.role==a::OriginalBuildingVisualRole::HempFarm ? 204U:(side==2 ? 201U:202U)},
                {"width",80U*side-2U},{"height",height},{"base_bytes",3200U*side*side},
                {"ground_anchor",{40U*side-1U,height-20U}},{"group_key",id.group_key},{"variant",0U},
                {"evidence","Independent authored static native Type30 fixture"}});
            if(schema>=2) rows.back()["native"]=true;
        }
        Json result={{"schema_version",schema},{"mode","original_building_bases"},{"selection","canonical_static_base"},
            {"anchor_policy","type30_front_cell"},{"evidence","Synthetic pixels; no original content"},
            {"files",{{{"path","DATA/China_General.sg3"},{"sha256",hash(sg3)}},
                      {{"path","DATA/China_General.555"},{"sha256",hash(bitmap)}}}},{"buildings",rows}};
        if(schema==3)result["field_selection"]=a::original_field_selection_contract;
        return result;
    }
    a::BuildingVisualProfile legacy() const {
        a::BuildingVisualProfile p;p.unique_images.push_back(legacy_image);
        p.entries[a::role_index(a::BuildingVisualRole::Well)]=a::BuildingVisualEntry{
            {"DATA/China_General.sg3",201},0,79,70,2,"Historical core fixture"};
        return p;
    }
    void reject(const Json& j,const char* why,std::uint64_t budget=a::building_max_rgba_bytes) const {
        write(manifest,j);auto p=legacy();const auto before=state(p);bool rejected=false;
        try{a::append_original_building_visual_profile(data,manifest,p,budget);}catch(const std::exception&){rejected=true;}
        check(rejected,why);check(state(p)==before,"failed extension changed complete prior profile");
    }
};
void synthetic() {
    Fixture f;write(f.manifest,f.valid());auto p=f.legacy();const auto before=state(p);
    a::append_original_building_visual_profile(f.data,f.manifest,p);
    check(p.original_building_prepared && p.unique_images.size()==3,"core/new physical dedup failed");
    check(state(p)["core"]==before["core"] && state(p)["houses"]==before["houses"],"legacy selections changed");
    for (const auto& id:a::original_building_visual_identities) {
        if (!id.static_base_supported) {
            check(!p.find_original_building(id.role,id.footprint_side) && !p.fallback_original_building(id.role,id.footprint_side).empty(),
                "unsupported field drew a guessed native state");
            continue;
        }
        const auto* e=p.find_original_building(id.role,id.footprint_side);check(e!=nullptr,"missing model/side selection");
        check(!p.find_original_building(id.role,1) && !p.fallback_original_building(id.role,1).empty(),"side mismatch drew original art");
        const auto& image=p.unique_images.at(e->image_index);std::size_t opaque=0;
        for(std::size_t i=3;i<image.pixels.size();i+=4){check(image.pixels[i]==0 || image.pixels[i]==255,"native binary alpha changed");opaque+=image.pixels[i]==255;}
        check(opaque==1600U*id.footprint_side*id.footprint_side && image.pixels[3]==0,"native whole base/transparent corner changed");
    }
    // The original exact-ten schema keeps its old native rows and physical
    // pool. Added roles cannot become available implicitly through it.
    write(f.manifest,f.valid(1));auto historical=f.legacy();
    a::append_original_building_visual_profile(f.data,f.manifest,historical);
    check(historical.unique_images.size()==2,"historical schema1 physical pool changed");
    for(const auto& id:a::original_building_visual_identities) {
        const auto* entry=historical.find_original_building(id.role,id.footprint_side);
        check(bool(entry)==(static_cast<std::size_t>(id.role)<a::original_building_schema1_role_count),
            "schema1 role availability changed");
        if(!entry)check(!historical.fallback_original_building(id.role,id.footprint_side).empty(),"historical absent role has no fallback");
    }
    write(f.manifest,f.valid());
    auto house=f.legacy();house.entries[a::role_index(a::BuildingVisualRole::Well)]->id.image_index=203;
    house.unique_images.push_back(f.legacy_image);house.original_house_prepared=true;
    house.original_house_entries[1]=a::BuildingVisualEntry{{"DATA/China_General.sg3",201},1,79,70,2,"Existing House fixture"};
    const auto old_houses=state(house)["houses"];
    a::append_original_building_visual_profile(f.data,f.manifest,house);
    check(house.unique_images.size()==4 && state(house)["houses"]==old_houses,"House/new shared physical pool not deduplicated");
    perf::set_enabled(true);const auto reads=perf::counter(perf::Counter::FileReads),decodes=perf::counter(perf::Counter::AssetDecodes);
    for(int i=0;i<1000;++i)for(const auto& id:a::original_building_visual_identities){(void)p.find_original_building(id.role,id.footprint_side);(void)p.fallback_original_building(id.role,1);}
    check(perf::counter(perf::Counter::FileReads)==reads && perf::counter(perf::Counter::AssetDecodes)==decodes,"prepared lookup did source/decode work");perf::set_enabled(false);
    auto bad=f.valid();bad["buildings"].erase(0);f.reject(bad,"missing role accepted");
    bad=f.valid();bad["buildings"][1]["model_type"]=72;f.reject(bad,"duplicate model accepted");
    bad=f.valid();bad["buildings"][0]["model_type"]=73;f.reject(bad,"unknown model accepted");
    bad=f.valid();bad["buildings"][0]["group_key"]=0x485U;f.reject(bad,"coherent wrong same-side registration accepted");
    bad=f.valid();bad["buildings"][0]["image_index"]=203U;f.reject(bad,"same geometry wrong physical image accepted");
    bad=f.valid();bad["buildings"][0]["original_side"]=3U;f.reject(bad,"wrong side accepted");
    bad=f.valid();bad["buildings"][0]["variant"]=1U;f.reject(bad,"animation variant accepted");
    bad=f.valid();bad["buildings"][0]["ground_anchor"]={79,69};f.reject(bad,"optical anchor accepted");
    bad=f.valid();bad["buildings"][0]["width"]=160U;f.reject(bad,"stretched source accepted");
    bad=f.valid();bad["buildings"][0]["base_bytes"]=3200U;f.reject(bad,"wrong base accepted");
    bad=f.valid();bad["buildings"][0].erase("archive");f.reject(bad,"partial row accepted");
    bad=f.valid();bad["buildings"][0]["unknown"]=true;f.reject(bad,"unknown row key accepted");
    bad=f.valid();bad["schema_version"]=4U;f.reject(bad,"unknown supplement schema accepted");
    bad=f.valid(1);bad["buildings"][0]["native"]=true;f.reject(bad,"schema1 gained schema2 row keys");
    bad=f.valid();bad["schema_version"]=1U;f.reject(bad,"schema1 accepted fifteen rows");
    bad=f.valid(1);bad["buildings"][9]["model_type"]=192U;f.reject(bad,"schema1 accepted appended role");
    bad=f.valid();bad["buildings"][10]["group_key"]=0x43aU;f.reject(bad,"HempFarm accepted Farmhouse group");
    bad=f.valid();bad["buildings"][11]["group_key"]=0x4adU;f.reject(bad,"Farmhouse accepted HempFarm group");
    bad=f.valid();bad["buildings"][10]["variant"]=1U;f.reject(bad,"Farm animation selected as body");
    bad=f.valid();bad["buildings"][10]["original_side"]=1U;f.reject(bad,"Farm body used one-cell preview");
    for(std::size_t i=12;i<15;++i) {
        bad=f.valid();bad["buildings"][i]["native"]=true;f.reject(bad,"stateful field native selection accepted");
        for(const auto* key:{"archive","image_index","width","height","base_bytes","ground_anchor","group_key","variant"}) {
            bad=f.valid();bad["buildings"][i][key]=0;f.reject(bad,"fallback-only field carried native keys");
        }
    }
    bad=f.valid();bad["buildings"][12].erase("native");f.reject(bad,"implicit field state selection accepted");
    bad=f.valid();bad["buildings"][12]["native"]=0;f.reject(bad,"nonboolean native selection accepted");
    bad=f.valid();bad["buildings"][12]["original_side"]=2;f.reject(bad,"fallback role side mismatch accepted");
    bad=f.valid();bad["buildings"][12]["fallback"]="";f.reject(bad,"empty explicit fallback accepted");
    auto fallback=f.valid();fallback["buildings"][10]={{"model_type",192},{"original_side",2},{"native",false},
        {"fallback","Explicit unavailable Farm static base; authored fallback"},{"evidence","Synthetic missing optional body"}};
    write(f.manifest,fallback);auto missing=f.legacy();
    a::append_original_building_visual_profile(f.data,f.manifest,missing);
    check(!missing.find_original_building(a::OriginalBuildingVisualRole::HempFarm,2) &&
        missing.find_original_building(a::OriginalBuildingVisualRole::Farmhouse,3) &&
        !missing.fallback_original_building(a::OriginalBuildingVisualRole::HempFarm,2).empty(),
        "explicit native-eligible fallback damaged remaining roles");
    bad=f.valid();bad["unknown"]=true;f.reject(bad,"unknown root key accepted");
    bad=f.valid();bad["files"][0]["sha256"]=std::string(64,'0');f.reject(bad,"wrong SHA accepted");
    bad=f.valid();bad["files"][1]=bad["files"][0];f.reject(bad,"duplicate fingerprint accepted");
    bad=f.valid();bad["buildings"][0]["archive"]="../outside.sg3";f.reject(bad,"unsafe path accepted");
    f.reject(f.valid(),"combined pool budget exceeded",158U*90U*4U+238U*130U*4U-1U);
    f.reject(f.valid(),"Farm append exceeded remaining combined pool budget",158U*90U*4U+238U*130U*4U+158U*137U*4U-1U);
    write(f.manifest,f.valid());auto exact=f.legacy();
    a::append_original_building_visual_profile(f.data,f.manifest,exact,158U*90U*4U+238U*130U*4U+158U*137U*4U);
    check(exact.unique_images.size()==3,"exact combined Farm budget refused or duplicated physical images");
    const auto valid_sg3=f.sg3;u32(f.sg3,40680U+202U*64U+16U,1U);f.save();f.reject(f.valid(),"mirror accepted");f.sg3=valid_sg3;f.save();
    f.sg3[40680U+202U*64U+52U]=3;f.save();f.reject(f.valid(),"unknown external source accepted");f.sg3=valid_sg3;f.save();
    const auto valid_bitmap=f.bitmap;f.bitmap.resize(54403U);f.save();f.reject(f.valid(),"decoder accepted truncated native payload");f.bitmap=valid_bitmap;f.save();
    fs::remove(f.data/"DATA/China_General.555");f.reject(f.valid(),"missing payload accepted");f.save();
    write(f.root/"outside.555",f.bitmap);fs::remove(f.data/"DATA/China_General.555");fs::create_symlink(f.root/"outside.555",f.data/"DATA/China_General.555");
    f.reject(f.valid(),"symlink source accepted");fs::remove(f.data/"DATA/China_General.555");f.save();
    text(f.manifest,"{\"schema_version\":1,\"schema_version\":1}");auto clean=f.legacy();const auto old=state(clean);bool rejected=false;
    try{a::append_original_building_visual_profile(f.data,f.manifest,clean);}catch(const std::exception&){rejected=true;}
    check(rejected && state(clean)==old,"duplicate metadata keys not atomically rejected");
}
void compatibility() {
    Fixture f;const auto pack=f.root/"resources/Compatibility/synthetic";const auto metadata=pack/"original-buildings.json";
    write(metadata,f.valid());text(f.data/"core","authored-core");
    for(const auto* n:{"walkers.json","buildings.json","roads.json"})text(pack/n,"{}");
    Json doc={{"schema_version",1},{"id","synthetic"},{"evidence","Authored test data"},
        {"files",{{{"path","DATA/China_General.sg3"},{"sha256",hash(f.sg3)}},{{"path","DATA/China_General.555"},{"sha256",hash(f.bitmap)}},{{"path","core"},{"sha256",hash(read(f.data/"core"))}}}},
        {"profiles",{{"walker","walkers.json"},{"building","buildings.json"},{"road","roads.json"}}},
        {"optional_original_buildings",{{"profile","original-buildings.json"},{"core_files",{"DATA/China_General.sg3","DATA/China_General.555"}}}}};
    const auto detect=[&]{write(pack/"manifest.json",doc);return a::detect_compatibility(f.data,pack.parent_path());};
    auto result=detect();check(result.compatible() && result.original_building_compatible() && result.files_hashed==3,"optional bases changed core fingerprints or failed selection");
    write(metadata,f.valid(1));result=detect();check(result.compatible() && result.original_building_compatible(),"historical schema1 optional profile stopped activating");
    write(metadata,f.valid(3));result=detect();check(result.compatible() && result.original_building_compatible(),"explicit schema3 field supplement stopped activating");
    auto unknown=f.valid();unknown["schema_version"]=4;write(metadata,unknown);result=detect();
    check(result.compatible() && !result.original_building_compatible(),"unknown supplement schema activated");
    auto bad=f.valid();bad["files"][0]["sha256"]=std::string(64,'0');write(metadata,bad);result=detect();
    check(result.compatible() && !result.original_building_compatible(),"optional mismatched metadata damaged core or activated");
    write(metadata,f.valid());const auto saved=doc;doc["optional_original_buildings"]["core_files"][1]="core";result=detect();
    check(result.compatible() && !result.original_building_compatible(),"non-General core dependency accepted");doc=saved;
    fs::remove(metadata);result=detect();check(result.compatible() && !result.original_building_compatible(),"missing optional resource damaged historical core");
    write(f.root/"outside.json",f.valid());fs::create_symlink(f.root/"outside.json",metadata);result=detect();
    check(result.compatible() && !result.original_building_compatible(),"linked optional resource accepted");fs::remove(metadata);write(metadata,f.valid());
    doc.erase("optional_original_buildings");result=detect();check(result.compatible() && !result.original_building_compatible(),"legacy pack acquired undeclared bases");doc=saved;
    text(f.data/"core","unknown-core");result=detect();check(!result.compatible() && !result.original_building_compatible(),"unknown core activated supplement");
}
void fields() {
    // Independent boundary pairs for the explicitly authored six equal bins;
    // no Original quantity, calendar, terrain or future harvest is inferred.
    constexpr std::array bounds{std::pair{0,13},std::pair{14,26},std::pair{27,39},
        std::pair{40,53},std::pair{54,66},std::pair{67,79}};
    for(std::size_t stage=0;stage<bounds.size();++stage)
        for(int progress=bounds[stage].first;progress<=bounds[stage].second;++progress)
            check(a::authored_farm_progress_stage(progress)==stage,"authored progress boundary differs");
    for(const int progress:{-1,80,std::numeric_limits<int>::min(),std::numeric_limits<int>::max()})
        check(!a::authored_farm_progress_stage(progress),"invalid progress acquired field artwork");
    Fixture f;write(f.manifest,f.valid(3));auto p=f.legacy();const auto prior=state(p);
    a::append_original_building_visual_profile(f.data,f.manifest,p);
    check(p.original_field_prepared && p.unique_images.size()==21 && state(p)["core"]==prior["core"] &&
        state(p)["houses"]==prior["houses"],"schema3 changed old roles or did not eagerly prepare eighteen fields");
    for(std::size_t crop=0;crop<a::original_field_role_count;++crop) {
        const auto role=a::original_field_roles[crop];
        check(!p.find_original_building(role,1),"historical static lookup silently selected a field stage");
        for(std::uint8_t stage=0;stage<6;++stage) {
            const auto* e=p.find_original_field(role,1,stage);
            check(e && e->id.image_index==205U+6U*crop+stage && e->ground_x==39 && e->ground_y==20U+stage,
                "field crop/offset/front anchor differs");
            const auto& image=p.unique_images.at(e->image_index);std::size_t opaque=0;
            for(std::size_t at=3;at<image.pixels.size();at+=4) {
                check(image.pixels[at]==0 || image.pixels[at]==255,"field alpha ceased being native binary alpha");
                opaque+=image.pixels[at]==255;
            }
            check(image.width==78 && image.height==40U+stage && opaque==1600 && image.pixels[3]==0,
                "field complete one-cell base or transparent corner changed");
        }
        check(!p.find_original_field(role,2,0) && !p.find_original_field(role,1,6),"unsupported field side/stage selected");
    }
    check(!p.find_original_field(a::OriginalBuildingVisualRole::WaterWell,1,0),"static role acquired field selection");
    const auto full=state(p);perf::set_enabled(true);
    const auto reads=perf::counter(perf::Counter::FileReads),decodes=perf::counter(perf::Counter::AssetDecodes);
    for(int cycle=0;cycle<10;++cycle)for(int progress=0;progress<80;++progress)
        for(const auto role:a::original_field_roles)(void)p.find_original_field(role,1,*a::authored_farm_progress_stage(progress));
    check(reads==perf::counter(perf::Counter::FileReads) && decodes==perf::counter(perf::Counter::AssetDecodes),"field stage lookup read/decoded source");
    perf::set_enabled(false);check(state(p)==full,"pure field lookups changed the complete prepared profile");
    for(const auto schema:{1U,2U}) {
        write(f.manifest,f.valid(schema));auto historical=f.legacy();a::append_original_building_visual_profile(f.data,f.manifest,historical);
        check(!historical.original_field_prepared,"historical schema gained a field selector");
        for(const auto role:a::original_field_roles)check(!historical.find_original_field(role,1,0),"historical schema gained native crop art");
    }
    // Reuse a prepared legacy one-cell image by physical identity, not by
    // content or an Original field role. No texture or byte budget is raised.
    write(f.manifest,f.valid(3));auto shared=f.legacy();
    shared.unique_images.push_back(a::load_sg3_image({f.data/"DATA/China_General.sg3",222}));
    shared.entries[a::role_index(a::BuildingVisualRole::Farm)]=a::BuildingVisualEntry{
        {"DATA/China_General.sg3",222},1,39,25,1,"Independent legacy Farm alias"};
    a::append_original_building_visual_profile(f.data,f.manifest,shared);
    check(shared.unique_images.size()==21 && shared.find_original_field(a::OriginalBuildingVisualRole::MilletField,1,5)->image_index==1,
        "shared legacy/field physical image was decoded twice");
    std::uint64_t bytes=0;for(const auto& image:p.unique_images)bytes+=image.pixels.size();
    f.reject(f.valid(3),"field stages escaped combined one-byte-short budget",bytes-1);
    write(f.manifest,f.valid(3));auto exact=f.legacy();a::append_original_building_visual_profile(f.data,f.manifest,exact,bytes);
    check(exact.unique_images.size()==21,"exact combined field budget refused");
    auto bad=f.valid(3);bad.erase("field_selection");f.reject(bad,"implicit authored field policy accepted");
    bad=f.valid(3);bad["field_selection"]="original-crop-state";f.reject(bad,"unsupported Original growth claim accepted");
    for(const auto schema:{1U,2U}){bad=f.valid(schema);bad["field_selection"]=a::original_field_selection_contract;f.reject(bad,"old schema accepted a new selector key");}
    bad=f.valid(3);bad["schema_version"]=2U;bad.erase("field_selection");f.reject(bad,"schema2 accepted native field stage rows");
    bad=f.valid(3);bad["buildings"][12]["group_key"]=0x42eU;f.reject(bad,"Hemp field borrowed Wheat group");
    bad=f.valid(3);bad["buildings"][12]["original_side"]=2U;f.reject(bad,"field used a Farm footprint");
    bad=f.valid(3);bad["buildings"][12]["stages"].erase(5);f.reject(bad,"partial five-stage field accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"].push_back(bad["buildings"][12]["stages"][0]);f.reject(bad,"seven-stage field accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][1]["stage"]=0U;f.reject(bad,"duplicate field stage accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["stage"]=6U;f.reject(bad,"raw terrain-bank variant smuggled as stage");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["image_index"]=211U;f.reject(bad,"same-geometry wrong crop physical accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["image_index"]=206U;f.reject(bad,"wrong source group offset accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["width"]=80U;f.reject(bad,"stretched field accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["base_bytes"]=12800U;f.reject(bad,"wrong field base length accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["ground_anchor"]={39,19};f.reject(bad,"optical field anchor accepted");
    bad=f.valid(3);bad["buildings"][12]["stages"][0]["variant"]=0U;f.reject(bad,"unknown per-stage selector accepted");
    bad=f.valid(3);bad["buildings"][12]["image_index"]=205U;f.reject(bad,"mixed static and field row accepted");
    bad=f.valid(3);bad["buildings"][0]["stages"]=bad["buildings"][12]["stages"];f.reject(bad,"static body smuggled field states");
    auto fallback=f.valid(3);fallback["buildings"][12]={{"model_type",194U},{"original_side",1U},{"native",false},
        {"fallback","Explicit unavailable native field illustrations"},{"evidence","No source images selected"}};
    write(f.manifest,fallback);auto missing=f.legacy();a::append_original_building_visual_profile(f.data,f.manifest,missing);
    check(missing.original_field_prepared && !missing.find_original_field(a::OriginalBuildingVisualRole::HempField,1,0) &&
        missing.find_original_field(a::OriginalBuildingVisualRole::WheatField,1,5),"explicit absent field damaged independent native crop");
    bad=fallback;bad["buildings"][12]["stages"]=f.valid(3)["buildings"][12]["stages"];f.reject(bad,"fallback-only field smuggled stages");
    const auto original=f.sg3;u16(f.sg3,40680U+205U*64U+30U,1);f.save();f.reject(f.valid(3),"field animation metadata accepted");f.sg3=original;f.save();
    u32(f.sg3,40680U+205U*64U+16U,1);f.save();f.reject(f.valid(3),"mirrored field accepted");f.sg3=original;f.save();
    const auto bitmap=f.bitmap;f.bitmap.pop_back();f.save();f.reject(f.valid(3),"final field stage truncated payload accepted");f.bitmap=bitmap;f.save();
}
void real(const fs::path& data,const fs::path& resources) {
    const auto pack=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    auto p=a::load_building_visual_profile(data,pack/"buildings.json");
    check(p.unique_images.size()==12 && a::building_role_count==13,"historical core role pool changed");
    a::append_original_house_visual_profile(data,pack/"original-houses.json",p);
    check(p.unique_images.size()==28,"sixteen native House images changed");const auto before=state(p);
    a::append_original_building_visual_profile(data,pack/"original-buildings.json",p);
    check(p.unique_images.size()==56 && p.original_field_prepared && state(p)["core"]==before["core"] && state(p)["houses"]==before["houses"],"real extension changed legacy selections or physical dedup");
    std::uint64_t rgba_bytes=0;for(const auto& image:p.unique_images)rgba_bytes+=image.pixels.size();
    check(rgba_bytes==4770512,"real eager combined RGBA budget changed");
    const std::array<unsigned,12> physical{1559,1593,589,460,501,2245,2258,2309,2811,2768,825,793};
    for(std::size_t i=0;i<a::original_building_role_count;++i){
        const auto& id=a::original_building_visual_identities[i];const auto* e=p.find_original_building(id.role,id.footprint_side);
        if(!id.static_base_supported){check(!e,"static lookup silently picked real field state");continue;}
        check(e && e->id.image_index==physical[i],"wrong real static representative");
        const auto decoded=a::load_sg3_image({data/e->id.archive_relative_path,e->id.image_index});const auto& image=p.unique_images.at(e->image_index);
        check(decoded.width==image.width && decoded.height==image.height && decoded.pixels==image.pixels,"whole original RGBA changed");
    }
    constexpr std::array<unsigned,3> field_bases{2458,2422,2410};
    for(std::size_t crop=0;crop<field_bases.size();++crop)for(std::uint8_t stage=0;stage<6;++stage) {
        const auto* entry=p.find_original_field(a::original_field_roles[crop],1,stage);
        check(entry && entry->id.image_index==field_bases[crop]+stage,"real crop/source offset differs");
        const auto raw=a::load_sg3_image({data/entry->id.archive_relative_path,entry->id.image_index});
        const auto& image=p.unique_images.at(entry->image_index);
        check(image.width==78 && image.height>=40 && image.width==raw.width && image.height==raw.height && image.pixels==raw.pixels &&
            entry->ground_x==39 && entry->ground_y==image.height-20,"whole real field RGBA or front-cell geometry changed");
    }
    const auto detected=a::detect_compatibility(data,resources/"Compatibility");check(detected.original_building_compatible(),"committed pack supplement not declared");
}
}
int main(int argc,char** argv) {
    try{check(argc==1 || (argc==4 && std::string_view(argv[1])=="--real"),"Usage: original-building-profile [--real DATA RESOURCES]");
        synthetic();fields();compatibility();if(argc==4)real(fs::canonical(argv[2]),fs::canonical(argv[3]));
        std::cout<<"Original static building identity, atomic guards, native RGBA, shared budget and compatibility PASS\n";return 0;
    }catch(const std::exception& e){perf::set_enabled(false);std::cerr<<"Original building profile FAIL: "<<e.what()<<'\n';return 1;}
}
