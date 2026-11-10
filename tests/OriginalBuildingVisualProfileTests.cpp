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
    for (const auto& image:p.unique_images) images.push_back({image.width,image.height,hash(image.pixels)});
    return {{"images",images},{"core",entries(p.entries)},{"houses",entries(p.original_house_entries)},
        {"house_prepared",p.original_house_prepared},{"house_fallbacks",p.original_house_fallbacks},
        {"original",entries(p.original_building_entries)},{"prepared",p.original_building_prepared},
        {"fallbacks",p.original_building_fallbacks}};
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-original-building-profile-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"buildings.json";
    Bytes sg3=Bytes(40680U+205U*64U,0),bitmap=Bytes(4U+12800U+28800U,0);
    a::RgbaImage legacy_image;
    Fixture() {
        u32(sg3,0,static_cast<unsigned>(sg3.size()));u32(sg3,4,213);
        u32(sg3,12,205);u32(sg3,16,204);u32(sg3,20,2);
        const std::string system="Zeus_system.bmp",normal="synthetic.bmp";
        std::copy(system.begin(),system.end(),sg3.begin()+680);std::copy(normal.begin(),normal.end(),sg3.begin()+880);
        u32(sg3,680+124,200);u32(sg3,680+128,1);u32(sg3,680+132,200);
        u32(sg3,880+124,3);u32(sg3,880+128,201);u32(sg3,880+132,203);
        for (unsigned i=0;i<172;++i) u16(sg3,80U+2U*i,201);
        for (const auto& id:a::original_building_visual_identities)
            if (id.footprint_side==3) u16(sg3,80U+2U*((id.group_key%512U)-1U),202);
        const auto record=[&](unsigned index,unsigned side,unsigned height,unsigned offset) {
            const auto at=40680U+64U*index,base=3200U*side*side;
            u32(sg3,at,offset);u32(sg3,at+4,base);u32(sg3,at+8,base);
            u16(sg3,at+20,80U*side-2U);u16(sg3,at+22,height);u16(sg3,at+50,30);
            sg3[at+55]=static_cast<std::uint8_t>(side);sg3[at+56]=1;
            // Real base images may also declare animation metadata. Static
            // selection must retain their complete base without inventing it.
            u16(sg3,at+30,12);u16(sg3,at+34,8);u16(sg3,at+36,10);sg3[at+58]=4;
        };
        record(201,2,90,4);record(202,3,130,12804);record(203,2,90,4);
        for (std::size_t at=4;at<bitmap.size();at+=2)u16(bitmap,at,0x03e0);
        save();legacy_image=a::load_sg3_image({data/"DATA/China_General.sg3",201});
    }
    ~Fixture(){std::error_code e;fs::remove_all(root,e);}
    void save() const {write(data/"DATA/China_General.sg3",sg3);write(data/"DATA/China_General.555",bitmap);}
    Json valid() const {
        Json rows=Json::array();
        for (const auto& id:a::original_building_visual_identities) {
            const unsigned side=id.footprint_side,height=side==2 ? 90:130;
            rows.push_back({{"model_type",id.model_type},{"original_side",side},{"fallback","Explicit synthetic named fallback"},
                {"archive","DATA/China_General.sg3"},{"image_index",side==2 ? 201U:202U},
                {"width",80U*side-2U},{"height",height},{"base_bytes",3200U*side*side},
                {"ground_anchor",{40U*side-1U,height-20U}},{"group_key",id.group_key},{"variant",0U},
                {"evidence","Independent authored static native Type30 fixture"}});
        }
        return {{"schema_version",1U},{"mode","original_building_bases"},{"selection","canonical_static_base"},
            {"anchor_policy","type30_front_cell"},{"evidence","Synthetic pixels; no original content"},
            {"files",{{{"path","DATA/China_General.sg3"},{"sha256",hash(sg3)}},
                      {{"path","DATA/China_General.555"},{"sha256",hash(bitmap)}}}},{"buildings",rows}};
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
    check(p.original_building_prepared && p.unique_images.size()==2,"core/new physical dedup failed");
    check(state(p)["core"]==before["core"] && state(p)["houses"]==before["houses"],"legacy selections changed");
    for (const auto& id:a::original_building_visual_identities) {
        const auto* e=p.find_original_building(id.role,id.footprint_side);check(e!=nullptr,"missing model/side selection");
        check(!p.find_original_building(id.role,1) && !p.fallback_original_building(id.role,1).empty(),"side mismatch drew original art");
        const auto& image=p.unique_images.at(e->image_index);std::size_t opaque=0;
        for(std::size_t i=3;i<image.pixels.size();i+=4){check(image.pixels[i]==0 || image.pixels[i]==255,"native binary alpha changed");opaque+=image.pixels[i]==255;}
        check(opaque==1600U*id.footprint_side*id.footprint_side && image.pixels[3]==0,"native whole base/transparent corner changed");
    }
    auto house=f.legacy();house.entries[a::role_index(a::BuildingVisualRole::Well)]->id.image_index=203;
    house.unique_images.push_back(f.legacy_image);house.original_house_prepared=true;
    house.original_house_entries[1]=a::BuildingVisualEntry{{"DATA/China_General.sg3",201},1,79,70,2,"Existing House fixture"};
    const auto old_houses=state(house)["houses"];
    a::append_original_building_visual_profile(f.data,f.manifest,house);
    check(house.unique_images.size()==3 && state(house)["houses"]==old_houses,"House/new shared physical pool not deduplicated");
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
    bad=f.valid();bad["unknown"]=true;f.reject(bad,"unknown root key accepted");
    bad=f.valid();bad["files"][0]["sha256"]=std::string(64,'0');f.reject(bad,"wrong SHA accepted");
    bad=f.valid();bad["files"][1]=bad["files"][0];f.reject(bad,"duplicate fingerprint accepted");
    bad=f.valid();bad["buildings"][0]["archive"]="../outside.sg3";f.reject(bad,"unsafe path accepted");
    f.reject(f.valid(),"combined pool budget exceeded",158U*90U*4U+238U*130U*4U-1U);
    const auto valid_sg3=f.sg3;u32(f.sg3,40680U+202U*64U+16U,1U);f.save();f.reject(f.valid(),"mirror accepted");f.sg3=valid_sg3;f.save();
    f.sg3[40680U+202U*64U+52U]=3;f.save();f.reject(f.valid(),"unknown external source accepted");f.sg3=valid_sg3;f.save();
    const auto valid_bitmap=f.bitmap;f.bitmap.pop_back();f.save();f.reject(f.valid(),"decoder accepted truncated native payload");f.bitmap=valid_bitmap;f.save();
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
void real(const fs::path& data,const fs::path& resources) {
    const auto pack=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    auto p=a::load_building_visual_profile(data,pack/"buildings.json");
    check(p.unique_images.size()==12 && a::building_role_count==13,"historical core role pool changed");
    a::append_original_house_visual_profile(data,pack/"original-houses.json",p);
    check(p.unique_images.size()==28,"sixteen native House images changed");const auto before=state(p);
    a::append_original_building_visual_profile(data,pack/"original-buildings.json",p);
    check(p.unique_images.size()==37 && state(p)["core"]==before["core"] && state(p)["houses"]==before["houses"],"real extension changed legacy selections or Well physical dedup");
    const std::array<unsigned,10> physical{1559,1593,589,460,501,2245,2258,2309,2811,2768};
    for(std::size_t i=0;i<a::original_building_role_count;++i){
        const auto& id=a::original_building_visual_identities[i];const auto* e=p.find_original_building(id.role,id.footprint_side);
        check(e && e->id.image_index==physical[i],"wrong real static representative");
        const auto decoded=a::load_sg3_image({data/e->id.archive_relative_path,e->id.image_index});const auto& image=p.unique_images.at(e->image_index);
        check(decoded.width==image.width && decoded.height==image.height && decoded.pixels==image.pixels,"whole original RGBA changed");
    }
    const auto detected=a::detect_compatibility(data,resources/"Compatibility");check(detected.original_building_compatible(),"committed pack supplement not declared");
}
}
int main(int argc,char** argv) {
    try{check(argc==1 || (argc==4 && std::string_view(argv[1])=="--real"),"Usage: original-building-profile [--real DATA RESOURCES]");
        synthetic();compatibility();if(argc==4)real(fs::canonical(argv[2]),fs::canonical(argv[3]));
        std::cout<<"Original static building identity, atomic guards, native RGBA, shared budget and compatibility PASS\n";return 0;
    }catch(const std::exception& e){perf::set_enabled(false);std::cerr<<"Original building profile FAIL: "<<e.what()<<'\n';return 1;}
}
