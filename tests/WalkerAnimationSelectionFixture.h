#pragma once

#include "FireInspectorWalkerFixture.h"

#include <map>
#include <set>
#include <string_view>
#include <tuple>

// Entirely authored SG3 tables and Omega pixels. The resource-key positions are
// exercised independently of the physical numbers in any supplied game file.
namespace openemperor::testing::walker_animation {
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Bytes=inspector::Bytes;
using inspector::u16;
using inspector::u32;
using inspector::write;
using inspector::check;

struct Family {const char* role;std::uint32_t slot,key;};
inline constexpr std::array<Family,12> families{{
    {"clay",38,0x4c03},{"pottery",38,0x4c05},{"household",38,0x4c09},
    {"fire_inspector",38,0x4c09},{"supplier",39,0x4e60},{"distributor",39,0x4e75},
    {"service",38,0x4c03},{"health_worker",38,0x4c1e},{"music",38,0x4c67},
    {"acrobat",38,0x4c94},{"drama",38,0x4c6b},{"venue_music",38,0x4ca0}
}};
inline std::uint32_t physical_base(const Family& family) {
    return 1U+96U*((family.key%512U)-1U);
}
struct Fixture {
    fs::path root=fs::canonical(fs::temp_directory_path())/("openemperor-walker-selection-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::path data=root/"data",manifest=root/"walker-animations.json";
    std::map<std::pair<std::uint32_t,std::uint32_t>,std::array<int,2>> canvases;
    explicit Fixture(int width=8,int height=16) {
        check(width>=2 && height>=2 && width<=2048 && height<=2048,"authored canvas dimensions outside2..2048");
        for (const auto& f:families) canvases[{f.slot,f.key}]={width,height};
        fs::create_directories(data/"DATA");rebuild();
    }
    ~Fixture() {std::error_code ignored;fs::remove_all(root,ignored);}
    Fixture(const Fixture&)=delete;
    Fixture& operator=(const Fixture&)=delete;
    void set_family_canvas(std::string_view role,int width,int height) {
        const auto f=std::find_if(families.begin(),families.end(),[&](const auto& value){return value.role==role;});
        check(f!=families.end(),"unknown authored family");
        check(width>=2 && height>=2 && width<=2048 && height<=2048,"authored canvas dimensions outside2..2048");
        canvases[{f->slot,f->key}]={width,height};
    }
    std::array<int,2> size(const Family& f,std::uint32_t phase) const {
        const auto base=canvases.at({f.slot,f.key});
        return {base[0]+static_cast<int>(phase%3U),base[1]+static_cast<int>(phase%4U)};
    }
    void save(const Json& document) const {inspector::Fixture::save(manifest,document);}
    Json document() const {
        Json doc={{"schema_version",7},{"mode","curated_walker_preview"},{"roles",Json::object()}};
        for (const auto& f:families) {
            const std::string name=f.role;
            Json role={{"ticks_per_frame",name=="clay" || name=="pottery" || name=="household" ? 4:2},
                {"clip_id","authored-source-key-"+name},{"evidence","AUTHORED source-key fixture; no Original pixels or profession claim."},
                {"frames",Json::array()},{"clips",Json::object()},{"idle","c0-p0"},
                {"idle_flip_x",name!="supplier" && name!="distributor"}};
            for (const auto column:{0U,2U}) for (std::uint32_t phase=0;phase<12;++phase) {
                const auto s=size(f,phase);
                role["frames"].push_back({{"alias","c"+std::to_string(column)+"-p"+std::to_string(phase)},
                    {"archive",f.slot==38 ? "DATA/SprMain.sg3":"DATA/SprMain2.sg3"},
                    {"image_index",physical_base(f)+8U*phase+column},
                    {"foot_anchor",{s[0]/2,s[1]-(name=="supplier" ? 0:8)}}});
            }
            for (const auto& [direction,column,flip]:{
                    std::tuple{"pos_x",2U,false},std::tuple{"neg_x",0U,true},
                    std::tuple{"pos_y",2U,true},std::tuple{"neg_y",0U,false}}) {
                Json clip=Json::array();
                for (std::uint32_t phase=0;phase<12;++phase)
                    clip.push_back("c"+std::to_string(column)+"-p"+std::to_string(phase));
                role["clips"][direction]={{"frames",clip},{"flip_x",flip}};
            }
            doc["roles"][name]=std::move(role);
        }
        return doc;
    }
    void rebuild() {
        constexpr std::uint32_t capacity=15362,in_use=15360;
        for (const auto slot:{38U,39U}) {
            Bytes archive(40680+72ULL*capacity,0),bitmap{0,0,0,0};
            u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
            u32(archive,12,capacity);u32(archive,16,in_use);u32(archive,20,1);
            const std::string first=slot==38 ? "Peddler":"alligator";
            std::copy(first.begin(),first.end(),archive.begin()+680);
            for (std::uint32_t index=0;index<160;++index)
                u16(archive,80+2U*index,static_cast<std::uint16_t>(1U+96U*index));
            std::set<std::uint32_t> written;
            for (const auto& f:families) if (f.slot==slot && written.insert(f.key).second) {
                const auto base=physical_base(f);
                for (std::uint32_t phase=0;phase<12;++phase) for (const auto column:{0U,2U}) {
                    const auto s=size(f,phase);const int width=s[0],height=s[1];
                    Bytes payload;
                    for (int y=0;y<height;++y) {
                        int x=0;
                        if (y==0) {payload.insert(payload.end(),{255,1});x=1;}
                        while (x<width) {
                            const int count=std::min(254,width-x);payload.push_back(static_cast<std::uint8_t>(count));
                            for (int n=0;n<count;++n) {
                                const auto color=static_cast<std::uint16_t>(y==height-1 && x+n==width-1 ?
                                    0x7c00U:0x03e0U+((phase%8U)<<10U)+column+1U);
                                payload.push_back(static_cast<std::uint8_t>(color));
                                payload.push_back(static_cast<std::uint8_t>(color>>8U));
                            }
                            x+=count;
                        }
                    }
                    const auto at=40680ULL+72ULL*(base+8U*phase+column);
                    u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));
                    u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
                    u16(archive,at+20,static_cast<std::uint16_t>(width));u16(archive,at+22,static_cast<std::uint16_t>(height));
                    u16(archive,at+34,static_cast<std::uint16_t>(width/2));u16(archive,at+36,static_cast<std::uint16_t>(height));
                    u16(archive,at+50,256);archive[at+59]=1;
                    if (column==0 && phase==0) {u16(archive,at+30,12);u16(archive,at+32,8);archive[at+58]=4;}
                    bitmap.insert(bitmap.end(),payload.begin(),payload.end());
                    const auto opposite=40680ULL+72ULL*(base+8U*phase+(column==0 ? 6U:4U));
                    u32(archive,opposite+16,column==0 ? 0xfffffffaU:0xfffffffeU);
                    u16(archive,opposite+20,static_cast<std::uint16_t>(width));u16(archive,opposite+22,static_cast<std::uint16_t>(height));
                    u16(archive,opposite+34,static_cast<std::uint16_t>(width-width/2));
                    u16(archive,opposite+36,static_cast<std::uint16_t>(height));u16(archive,opposite+50,256);archive[opposite+59]=1;
                }
            }
            const auto name=slot==38 ? "SprMain":"SprMain2";
            write(data/"DATA"/(std::string(name)+".sg3"),archive);
            write(data/"DATA"/(std::string(name)+".555"),bitmap);
        }
        save(document());
    }
};
} // namespace openemperor::testing::walker_animation
