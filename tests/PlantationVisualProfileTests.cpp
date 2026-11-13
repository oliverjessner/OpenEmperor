#include "assets/PlantationVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
namespace a=openemperor::assets;
namespace fs=std::filesystem;
using J=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
void u16(Bytes& bytes,std::size_t at,unsigned value) {bytes.at(at)=static_cast<std::uint8_t>(value);bytes.at(at+1)=static_cast<std::uint8_t>(value>>8U);}
void u32(Bytes& bytes,std::size_t at,unsigned value) {u16(bytes,at,value);u16(bytes,at+2,value>>16U);}
std::string hash(const Bytes& bytes) {
    std::array<unsigned char,32> digest{};unsigned length=0;
    check(EVP_Digest(bytes.data(),bytes.size(),digest.data(),&length,EVP_sha256(),nullptr)==1 && length==32,"SHA256");
    std::ostringstream text;text<<std::hex<<std::setfill('0');
    for(auto value:digest)text<<std::setw(2)<<unsigned(value);return text.str();
}
void write(const fs::path& path,const Bytes& bytes) {
    fs::create_directories(path.parent_path());std::ofstream stream(path,std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));check(bool(stream),"Write fixture");
}
void write(const fs::path& path,const J& json) {const auto text=json.dump();write(path,Bytes(text.begin(),text.end()));}
struct Fixture {
    fs::path root=fs::temp_directory_path()/("openemperor-plantation-visual-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"plantations.json";
    Bytes sg3=Bytes(40680+218*64,0),pixels=Bytes(16*3200,0);
    J metadata;
    Fixture() {
        u32(sg3,0,static_cast<unsigned>(sg3.size()));u32(sg3,4,213);u32(sg3,12,218);u32(sg3,16,217);u32(sg3,20,2);
        const std::string system="Zeus_system.bmp";std::copy(system.begin(),system.end(),sg3.begin()+680);
        u32(sg3,804,200);u32(sg3,808,1);u32(sg3,812,200);
        for(unsigned n=0;n<173;++n)u16(sg3,80+2*n,201);
        J plants=J::array();unsigned next=201;
        for(const auto model:{26U,27U,28U}) {
            const unsigned group=model==26 ? 0x432:model==27 ? 0x434:0x433,count=model==26 ? 4:6;
            u16(sg3,80+2*(group%512-1),next);J stages=J::array();
            for(unsigned stage=0;stage<count;++stage,++next) {
                const unsigned height=model==27 && stage==4 ? 86:40+stage,at=40680+next*64;
                u32(sg3,at,(next-201)*3200);u32(sg3,at+4,3200);u32(sg3,at+8,3200);
                u16(sg3,at+20,78);u16(sg3,at+22,height);u16(sg3,at+50,30);
                sg3[at+55]=static_cast<std::uint8_t>(model==27 && stage==4 ? 0:1);
                for(unsigned pixel=0;pixel<1600;++pixel)u16(pixels,(next-201)*3200+pixel*2,0x03e0+stage);
                stages.push_back({{"image_index",next},{"width",78},{"height",height},{"base_bytes",3200},{"ground_anchor",{39,height-20}}});
            }
            plants.push_back({{"model_type",model},{"group_key",group},{"evidence","Independent authored test pixels"},{"stages",stages}});
        }
        write(data/"DATA/China_General.sg3",sg3);write(data/"DATA/China_General.555",pixels);
        metadata={{"schema_version",1},{"mode","plantation_bases"},{"selection",a::plantation_visual_selection_contract},{"anchor_policy","type30_front_cell"},
            {"files",{{{"path","DATA/China_General.sg3"},{"sha256",hash(sg3)}},{{"path","DATA/China_General.555"},{"sha256",hash(pixels)}}}},{"plants",plants}};
    }
    ~Fixture(){std::error_code error;fs::remove_all(root,error);}
};
void synthetic() {
    check(!a::plantation_growth_visual_stage(29,0) && !a::plantation_growth_visual_stage(26,81),"Invalid plant state admitted");
    check(a::plantation_growth_visual_stage(26,0)==0 && a::plantation_growth_visual_stage(26,80)==3 &&
        a::plantation_growth_visual_stage(27,79)==4 && a::plantation_growth_visual_stage(27,80)==5 &&
        a::plantation_growth_visual_stage(28,40)==5,"Source quantity projection boundaries");
    Fixture fixture;write(fixture.manifest,fixture.metadata);a::BuildingVisualProfile profile;
    a::append_plantation_visual_profile(fixture.data,fixture.manifest,profile);
    check(profile.plantation_prepared && profile.unique_images.size()==16 && !profile.original_building_prepared &&
        !profile.original_field_prepared && !profile.original_rice_prepared,"Independent supplement widened historical roles");
    check(a::find_plantation_visual(profile,26,80)->id.image_index==204 &&
        a::find_plantation_visual(profile,27,79)->id.image_index==209 &&
        a::find_plantation_visual(profile,28,80)->id.image_index==216,"Typed source banks merged or zero-hint frame lost");
    check(!a::find_plantation_visual(profile,29,0) && !a::find_plantation_visual(profile,26,0,2),"Wrong type/footprint admitted");
    openemperor::performance::set_enabled(true);
    const auto counters=[] {
        std::array<std::uint64_t,static_cast<std::size_t>(openemperor::performance::Counter::Count)> result{};
        for(std::size_t index=0;index<result.size();++index)
            result[index]=openemperor::performance::counter(static_cast<openemperor::performance::Counter>(index));
        return result;
    };
    const auto quiet=counters();
    for(unsigned repeat=0;repeat<100;++repeat)for(unsigned model=26;model<=28;++model)for(unsigned growth=0;growth<=80;++growth)
        check(a::find_plantation_visual(profile,static_cast<std::uint16_t>(model),growth),"Complete plant selection unavailable");
    check(counters()==quiet,"Prepared selection performed resource work");
    openemperor::performance::set_enabled(false);
    const auto reject=[&](J metadata,std::uint64_t budget=a::building_max_rgba_bytes) {
        write(fixture.manifest,metadata);a::BuildingVisualProfile candidate;
        candidate.unique_images.push_back(profile.unique_images.front());const auto before=hash(candidate.unique_images.front().pixels);
        bool refused=false;try{a::append_plantation_visual_profile(fixture.data,fixture.manifest,candidate,budget);}catch(const std::exception&){refused=true;}
        check(refused && candidate.unique_images.size()==1 && !candidate.plantation_prepared &&
            hash(candidate.unique_images.front().pixels)==before,"Rejected supplement changed prior pool");
    };
    auto bad=fixture.metadata;bad["plants"][0]["model_type"]=29;reject(bad);
    bad=fixture.metadata;bad["plants"][1]["stages"].erase(4);reject(bad);
    bad=fixture.metadata;bad["plants"][0]["stages"][0]["image_index"]=205;reject(bad);
    bad=fixture.metadata;bad["files"][1]["sha256"]=std::string(64,'0');reject(bad);
    bad=fixture.metadata;bad["files"][0]["path"]="../China_General.sg3";reject(bad);
    reject(fixture.metadata,1);
}
void real(const fs::path& data,const fs::path& metadata) {
    a::BuildingVisualProfile profile;a::append_plantation_visual_profile(data,metadata,profile);
    check(profile.unique_images.size()==16 && profile.plantation_prepared,"Real complete plantation bank unavailable");
    for(const auto model:{26U,27U,28U})for(unsigned growth=0;growth<=80;++growth)
        check(a::find_plantation_visual(profile,static_cast<std::uint16_t>(model),growth),"Real authoritative growth has no selected image");
}
}
int main(int argc,char** argv) try {
    check(argc==1 || argc==3,"Expected optional DATA_ROOT METADATA");synthetic();if(argc==3)real(fs::canonical(argv[1]),fs::canonical(argv[2]));
    std::cout<<"Plantation visual identities, source-stage boundaries, complete banks, atomic failure and resource-pure lookup PASS\n";return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
