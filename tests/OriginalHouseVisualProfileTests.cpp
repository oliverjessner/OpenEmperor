#include "assets/BuildingVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/ResourceGroupLookup.h"
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
namespace m=openemperor::maps;
namespace perf=openemperor::performance;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
Bytes read(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);check(bool(input),"test source unavailable");
    return Bytes(std::istreambuf_iterator<char>(input),{});
}
void write(const fs::path& path,const Bytes& bytes) {
    fs::create_directories(path.parent_path());std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(output),"test fixture write failed");
}
void write(const fs::path& path,const Json& json) {
    const auto text=json.dump();write(path,Bytes(text.begin(),text.end()));
}
std::string hash(const Bytes& bytes) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> digest{};unsigned length=0;
    check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)==1 && length==32,
          "test SHA256 failed");
    std::ostringstream output;output<<std::hex<<std::setfill('0');
    for (unsigned i=0;i<length;++i) output<<std::setw(2)<<static_cast<unsigned>(digest[i]);
    return output.str();
}
void u16(Bytes& bytes,std::size_t at,unsigned value) {
    bytes.at(at)=static_cast<std::uint8_t>(value);bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);
}
void u32(Bytes& bytes,std::size_t at,unsigned value) { u16(bytes,at,value);u16(bytes,at+2,value>>16U); }
unsigned word(const Bytes& bytes,std::size_t at) {
    check(at+2<=bytes.size(),"truncated test word");return bytes[at]|(unsigned(bytes[at+1])<<8U);
}
std::uint32_t dword(const Bytes& bytes,std::size_t at) {
    return word(bytes,at)|(std::uint32_t(word(bytes,at+2))<<16U);
}
// Source-derived type/group relation; physical record translation remains shared.
unsigned group(unsigned type) {
    if (type==2) return 0x47a;
    if (type<=10) return 0x471U+type-3U;
    if (type==11) return 0x479;
    if (type==12) return 0x47b;
    if (type==13) return 0x47c;
    if (type==14) return 0x4ac;
    return 0x47eU+type-15U;
}
Json state(const a::BuildingVisualProfile& profile) {
    Json result={{"prepared",profile.original_house_prepared},{"images",Json::array()},
                 {"roles",Json::array()},{"original",Json::array()},
                 {"fallbacks",profile.original_house_fallbacks}};
    for (const auto& image:profile.unique_images)
        result["images"].push_back({image.width,image.height,hash(image.pixels)});
    const auto add=[](const auto& entry) -> Json {
        if (!entry) return nullptr;
        return {entry->id.archive_relative_path.generic_string(),entry->id.image_index,
                entry->image_index,entry->ground_x,entry->ground_y,entry->footprint_side,entry->evidence};
    };
    for (const auto& entry:profile.entries) result["roles"].push_back(add(entry));
    for (const auto& entry:profile.original_house_entries) result["original"].push_back(add(entry));
    return result;
}
struct Fixture {
    fs::path root=fs::temp_directory_path()/("openemperor-original-house-profile-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"houses.json";
    Bytes sg3=Bytes(40680U+205U*64U,0),bitmap=Bytes(4U+12800U+51200U+12800U,0);
    a::RgbaImage legacy_image;
    Fixture() {
        u32(sg3,0,static_cast<unsigned>(sg3.size()));u32(sg3,4,213);
        u32(sg3,12,205);u32(sg3,16,204);u32(sg3,20,2);
        const std::string system="Zeus_system.bmp",normal="synthetic.bmp";
        std::copy(system.begin(),system.end(),sg3.begin()+680);
        std::copy(normal.begin(),normal.end(),sg3.begin()+880);
        u32(sg3,680+124,200);u32(sg3,680+128,1);u32(sg3,680+132,200);
        u32(sg3,880+124,4);u32(sg3,880+128,201);u32(sg3,880+132,204);
        // Independent synthetic groups deliberately share physical images.
        for (unsigned i=0;i<172;++i) u16(sg3,80U+2U*i,201);
        u16(sg3,80U+2U*((group(2)%512U)-1U),204);
        for (unsigned type=11;type<=17;++type) u16(sg3,80U+2U*((group(type)%512U)-1U),202);
        const auto record=[&](unsigned index,unsigned side,unsigned height,unsigned offset) {
            const auto at=40680U+64U*index;const auto base=3200U*side*side;
            u32(sg3,at,offset);u32(sg3,at+4,base);u32(sg3,at+8,base);
            u16(sg3,at+20,80U*side-2U);u16(sg3,at+22,height);u16(sg3,at+50,30);
            sg3[at+55]=static_cast<std::uint8_t>(side);sg3[at+56]=1;
        };
        record(201,2,90,4);record(202,4,170,12804);record(203,2,90,4);record(204,2,88,64004);
        for (std::size_t at=4;at<bitmap.size();at+=2) u16(bitmap,at,0x03e0);
        save_sources();
        legacy_image=a::load_sg3_image({data/"DATA/test.sg3",201});
    }
    ~Fixture() { std::error_code error;fs::remove_all(root,error); }
    void save_sources() const { write(data/"DATA/test.sg3",sg3);write(data/"DATA/test.555",bitmap); }
    Json valid(bool vacant_native=true) const {
        Json rows=Json::array();
        for (unsigned type=2;type<=17;++type) {
            const unsigned side=type<11 ? 2U:4U,height=type==2 ? 88U:(side==2 ? 90U:170U);
            Json row={{"house_type",type},{"original_side",side},{"fallback","Explicit synthetic fallback"}};
            if (type!=2 || vacant_native) row.update({{"archive","DATA/test.sg3"},{"image_index",type==2 ? 204U:(side==2 ? 201U:202U)},
                {"width",80U*side-2U},{"height",height},{"base_bytes",3200U*side*side},
                {"ground_anchor",{40U*side-1U,height-20U}},{"group_key",group(type)},
                {"variant",0U},{"evidence","Independent synthetic Type30 House fixture"}});
            rows.push_back(std::move(row));
        }
        return {{"schema_version",1U},{"mode","original_house_stages"},
            {"selection","canonical_static_variant_0"},{"anchor_policy","type30_front_cell"},
            {"evidence","Synthetic fixtures contain no original pixels"},
            {"files",{{{"path","DATA/test.sg3"},{"sha256",hash(sg3)}},
                      {{"path","DATA/test.555"},{"sha256",hash(bitmap)}}}},{"stages",rows}};
    }
    a::BuildingVisualProfile legacy() const {
        a::BuildingVisualProfile result;result.unique_images.push_back(legacy_image);
        result.entries[a::role_index(a::BuildingVisualRole::Household)]=a::BuildingVisualEntry{
            {"DATA/test.sg3",201},0,79,70,2,"Independent legacy role"};
        return result;
    }
    void rejects(const Json& json,const char* message,std::uint64_t budget=a::building_max_rgba_bytes) const {
        write(manifest,json);auto profile=legacy();const auto before=state(profile);bool rejected=false;
        try { a::append_original_house_visual_profile(data,manifest,profile,budget); }
        catch (const std::exception&) { rejected=true; }
        check(rejected,message);check(state(profile)==before,"failed House extension was not atomic");
    }
};
void synthetic() {
    Fixture f;write(f.manifest,f.valid());auto profile=f.legacy();const auto legacy=state(profile)["roles"];
    a::append_original_house_visual_profile(f.data,f.manifest,profile);
    check(profile.original_house_prepared && profile.unique_images.size()==3,"House/legacy physical deduplication failed");
    check(state(profile)["roles"]==legacy,"House supplement changed legacy entries");
    const auto* vacant=profile.find_original_house(2,2);
    const auto* shelter=profile.find_original_house(3,2);
    check(vacant && shelter && vacant->id.image_index==204 && shelter->id.image_index==201 &&
          vacant->image_index!=shelter->image_index && vacant->ground_x==79 && vacant->ground_y==68 &&
          profile.unique_images.at(vacant->image_index).height==88,"fresh Type2 aliased Shelter or lost its native anchor");
    check(!profile.find_original_house(2,4) && !profile.fallback_original_house(2,4).empty(),"Type2 image stretched onto side4");
    auto fallback=f.legacy();write(f.manifest,f.valid(false));
    a::append_original_house_visual_profile(f.data,f.manifest,fallback);
    check(!fallback.find_original_house(2,2) && !fallback.fallback_original_house(2,2).empty(),"explicit Type2 fallback inferred an image");
    const auto unprepared=f.legacy();
    check(!unprepared.find_original_house(2,2) && !unprepared.fallback_original_house(2,2).empty(),"absent profile lost Type2 fallback");
    auto vacant_as_shelter=f.valid();vacant_as_shelter["stages"][0]=vacant_as_shelter["stages"][1];
    vacant_as_shelter["stages"][0]["house_type"]=2;
    f.rejects(vacant_as_shelter,"fresh Type2 accepted the ordinary group0x471 Shelter registration");
    auto shelter_as_vacant=f.valid();shelter_as_vacant["stages"][1]=shelter_as_vacant["stages"][0];
    shelter_as_vacant["stages"][1]["house_type"]=3;
    f.rejects(shelter_as_vacant,"Shelter accepted the fresh Type2 purchase registration");
    check(!profile.find_original_house(11,2) && !profile.fallback_original_house(11,2).empty(),"side4 image stretched onto temporary side2 Elite plot");
    for (unsigned type=2;type<=17;++type) {
        const auto side=static_cast<std::uint8_t>(type<11 ? 2:4);
        const auto* entry=profile.find_original_house(static_cast<std::uint16_t>(type),side);
        check(entry && entry->footprint_side==side,"actual type/side image missing");
        const auto& image=profile.unique_images.at(entry->image_index);
        std::size_t opaque=0;
        for (std::size_t i=3;i<image.pixels.size();i+=4) {
            check(image.pixels[i]==0 || image.pixels[i]==255,"Type30 binary alpha changed");
            opaque+=image.pixels[i]==255;
        }
        check(opaque==1600U*side*side,"synthetic native foundation coverage changed");
        check(image.pixels[3]==0,"transparent source corner became selectable/opaque");
        const auto foot=(static_cast<std::size_t>(entry->ground_y)*image.width+
                         static_cast<std::size_t>(entry->ground_x))*4U;
        check(image.pixels.at(foot+1)==255 && image.pixels.at(foot+3)==255,"native front anchor lost opaque foundation");
    }
    perf::set_enabled(true);const auto files=perf::counter(perf::Counter::FileReads),decodes=perf::counter(perf::Counter::AssetDecodes);
    for (int i=0;i<1000;++i) for (unsigned type=2;type<=17;++type) {
        (void)profile.find_original_house(static_cast<std::uint16_t>(type),static_cast<std::uint8_t>(type<11 ? 2:4));
        (void)profile.fallback_original_house(static_cast<std::uint16_t>(type),2);
    }
    check(perf::counter(perf::Counter::FileReads)==files && perf::counter(perf::Counter::AssetDecodes)==decodes,
          "prepared House lookup performed frame file/decode work");perf::set_enabled(false);
    auto bad=f.valid();bad["stages"].erase(0);f.rejects(bad,"missing House type accepted");
    bad=f.valid();bad["stages"][1]["house_type"]=2;f.rejects(bad,"duplicate House type accepted");
    bad=f.valid();bad["stages"][1]["house_type"]=18;f.rejects(bad,"unknown House type accepted");
    bad=f.valid();bad["stages"][1]["original_side"]=4;f.rejects(bad,"Common/Elite footprint mismatch accepted");
    bad=f.valid(false);bad["stages"][0]["width"]=158;f.rejects(bad,"partial fallback image accepted");
    bad=f.valid();bad["stages"][0]["image_index"]=201;f.rejects(bad,"fresh Type2 aliased the Shelter physical record");
    bad=f.valid();bad["stages"][0]["variant"]=1;f.rejects(bad,"fresh Type2 accepted an unevidenced variation");
    bad=f.valid();bad["stages"][0]["ground_anchor"]={79,63};f.rejects(bad,"fresh Type2 accepted the Shelter anchor");
    bad=f.valid();bad["stages"][1]["ground_anchor"]={79,69};f.rejects(bad,"optical House pivot accepted");
    bad=f.valid();bad["stages"][1]["width"]=160;f.rejects(bad,"stretched native House width accepted");
    bad=f.valid();bad["stages"][1]["base_bytes"]=3200;f.rejects(bad,"wrong native House base accepted");
    bad=f.valid();bad["stages"][1]["image_index"]=203;f.rejects(bad,"same-geometry wrong physical variant accepted");
    bad=f.valid();bad["stages"][1]["variant"]=1;f.rejects(bad,"undeclared per-instance variant accepted");
    bad=f.valid();bad["stages"][1]["house_type"]=4;bad["stages"][2]["house_type"]=3;
    f.rejects(bad,"coherent Shelter/Hut identity swap accepted");
    bad=f.valid();bad["stages"][1]["archive"]="../outside.sg3";f.rejects(bad,"escaping archive path accepted");
    bad=f.valid();bad["files"][0]["sha256"]=std::string(64,'0');f.rejects(bad,"changed original asset fingerprint accepted");
    bad=f.valid();bad["files"].push_back(bad["files"][0]);f.rejects(bad,"duplicate fingerprint accepted");
    bad=f.valid();bad["stages"][1]["unknown"]=true;f.rejects(bad,"unknown stage metadata accepted");
    const auto full=158U*90U*4U+318U*170U*4U+158U*88U*4U;
    f.rejects(f.valid(),"deduplicated combined budget overflow accepted",full-1U);
    for (const auto [offset,value]:std::array<std::pair<std::size_t,unsigned>,4>{{
        {16,1},{30,1},{34,1},{58,1}}}) {
        const auto saved=f.sg3;u16(f.sg3,40680U+201U*64U+offset,value);f.save_sources();
        f.rejects(f.valid(),"mirrored or animated source accepted");f.sg3=saved;f.save_sources();
    }
    fs::remove(f.data/"DATA/test.555");f.rejects(f.valid(),"missing original source accepted");f.save_sources();
    write(f.root/"outside.555",f.bitmap);fs::remove(f.data/"DATA/test.555");
    fs::create_symlink(f.root/"outside.555",f.data/"DATA/test.555");
    f.rejects(f.valid(),"linked original source accepted");fs::remove(f.data/"DATA/test.555");f.save_sources();
}
struct Pe {
    Bytes bytes;
    struct Section { std::uint32_t rva,size,offset; };
    std::vector<Section> sections;
    explicit Pe(const fs::path& path):bytes(read(path)) {
        check(hash(bytes)=="6373328bfc5c4886d9abc9544eb706e89e7d465b18176ea8205fe27aaee53c0e","unqualified original EXE");
        const auto at=dword(bytes,0x3c),optional=at+24U;
        check(dword(bytes,at)==0x4550 && word(bytes,at+4)==0x14c && word(bytes,optional)==0x10b &&
              dword(bytes,optional+28)==0x400000,"unexpected original PE identity");
        const auto table=optional+word(bytes,at+20),count=word(bytes,at+6);
        check(count<=96,"PE section budget");
        for (unsigned i=0;i<count;++i) {
            const auto start=table+i*40U;
            sections.push_back({dword(bytes,start+12),dword(bytes,start+16),dword(bytes,start+20)});
        }
    }
    std::size_t at(std::uint32_t va,unsigned count) const {
        for (const auto& section:sections) {
            const auto start=std::uint64_t(0x400000)+section.rva;
            if (va>=start && va-start+count<=section.size && section.offset+va-start+count<=bytes.size())
                return static_cast<std::size_t>(section.offset+va-start);
        }
        throw std::runtime_error("unmapped original source address");
    }
    unsigned u32(std::uint32_t va) const { return dword(bytes,at(va,4)); }
    unsigned byte(std::uint32_t va) const { return bytes.at(at(va,1)); }
};
void real(const fs::path& data,const fs::path& resources) {
    const auto pack=resources/"Compatibility/gog-derived-2.0.0.2-en-assetset-1";
    const auto manifest=pack/"original-houses.json";const auto document=Json::parse(read(manifest));
    const auto exe=Pe(data/"Emperor.exe");
    check(exe.u32(0x7aba38+0x230)==0x518de0,"House setter virtual identity changed");
    const auto archive=a::read_sg3_archive(data/"DATA/China_General.sg3");
    const auto layout=m::build_runtime_archive_layout(2,archive,m::RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
    check(layout.has_value(),"original General registration unavailable");
    auto profile=a::load_building_visual_profile(data,pack/"buildings.json");
    const auto before=state(profile);
    const auto images=profile.unique_images.size();
    check(images==12 && std::count_if(profile.entries.begin(),profile.entries.end(),[](const auto& e){return e.has_value();})==13,
          "protected legacy13-role/12-image profile changed");
    a::append_original_house_visual_profile(data,manifest,profile);
    check(state(profile)["roles"]==before["roles"] && profile.unique_images.size()==images+16,
          "original House extension changed legacy roles or physical deduplication");
    for (const auto& row:document.at("stages")) {
        const auto type=row.at("house_type").get<unsigned>(),side=exe.byte(0x823598U+24U*type);
        check(side==row.at("original_side"),"House side differs from original initializer");
        const auto table=type<=10 ? 0x8232f8U+4U*(type<=3 ? 0U:type-3U):0x85410cU+4U*type;
        auto original_group=exe.u32(table);
        if (type==2) {
            // Bounded byte facts corroborate the documented special purchase;
            // this test does not execute the original or infer a full call graph.
            check(exe.u32(0x8235a0U+24U*type)==0x47a && original_group==0x471 &&
                  exe.byte(0x4b284c)==0x6a && exe.byte(0x4b284d)==2 &&
                  exe.byte(0x4b2899)==0x68 && exe.u32(0x4b289a)==0x47a &&
                  exe.byte(0x4b28c9)==0x6a && exe.byte(0x4b28ca)==2 &&
                  exe.byte(0x4b28cb)==0x6a && exe.byte(0x4b28cc)==2,
                  "fresh Type2 purchase facts changed or ordinary setter was substituted");
            original_group=exe.u32(0x4b289a);
            check(row.at("image_index")==1508 && row.at("width")==158 && row.at("height")==88 &&
                  row.at("base_bytes")==12800 && row.at("ground_anchor")==Json::array({79,68}),
                  "fresh Type2 source geometry changed");
        }
        check(row.at("group_key")==original_group && row.at("variant")==0,"House type does not match actual original setter table");
        const auto resolution=m::resolve_resource_group({original_group},{{2,{&*layout}}});
        check(resolution.status==m::GroupLookupStatus::Resolved && resolution.local_base.has_value(),"original House group unresolved");
        const auto physical=layout->physical_record_for_local(*resolution.local_base);
        check(physical && *physical==row.at("image_index"),"House physical image differs from shared original registration");
        const auto* entry=profile.find_original_house(static_cast<std::uint16_t>(type),static_cast<std::uint8_t>(side));
        check(entry && entry->id.image_index==*physical,"prepared House actual type/side selection differs");
        const auto& metadata=archive.images.at(*physical);
        check(metadata.image_type==30 && metadata.isometric_size_flag==side && metadata.alpha_length==0 &&
              metadata.horizontal_mirror_offset==0 && metadata.animation_sprites==0,"original House source metadata changed");
        const auto decoded=a::load_sg3_image({data/"DATA/China_General.sg3",*physical});
        const auto& actual=profile.unique_images.at(entry->image_index);
        check(decoded.width==actual.width && decoded.height==actual.height && decoded.pixels==actual.pixels,
              "House preparation changed native original RGBA");
        check(entry->ground_x==40U*side-1U && entry->ground_y==actual.height-20U,"House native front anchor changed");
        bool transparent=false,opaque=false;
        for (std::size_t at=3;at<actual.pixels.size();at+=4) {
            check(actual.pixels[at]==0 || actual.pixels[at]==255,"original Type30 alpha is no longer exact");
            transparent|=actual.pixels[at]==0;opaque|=actual.pixels[at]==255;
        }
        check(transparent && opaque,"original source alpha lost transparent or opaque coverage");
    }
    check(!profile.find_original_house(11,2),"original Elite plot image fit was guessed");
    const auto originals=std::array{data/"DATA/China_General.sg3",data/"DATA/China_General.555"};
    for (const auto& path:originals) {
        const auto name=path.filename().string();
        const auto expected=name.ends_with(".sg3") ? "c9ae527102c312098cbfe82932ce1b39c6c96692981c874dc853306835583e46":
            "3cb09cd2c954eb355e2253482845e7117b353c24506575795bcaa1fe45db33f2";
        check(hash(read(path))==expected,"original asset changed during read-only verification");
    }
}
}
int main(int argc,char** argv) {
    try {
        check(argc==1 || (argc==4 && std::string_view(argv[1])=="--real"),
              "Usage: original-house-visual-profile [--real DATA RESOURCE_ROOT]");
        synthetic();if (argc==4) real(fs::canonical(argv[2]),fs::canonical(argv[3]));
        std::cout<<"Original House identity, native RGBA/alpha, strict atomic profile, dedupe/budget and named fallbacks PASS\n";
        return 0;
    } catch (const std::exception& error) {
        perf::set_enabled(false);std::cerr<<"Original House profile FAIL: "<<error.what()<<'\n';return 1;
    }
}
