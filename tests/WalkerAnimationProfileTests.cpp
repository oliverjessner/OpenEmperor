#include "assets/WalkerVisualProfile.h"
#include "app/WalkerPose.h"
#include "FireInspectorWalkerFixture.h"
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string_view>

// Original-free files exercise the production SG3 loader and its versioned
// manifest parser. No hand-built decoded profile substitutes for these checks.
namespace {
namespace assets=openemperor::assets;
namespace sim=openemperor::simulation;
namespace fs=std::filesystem;
using Fixture=openemperor::testing::inspector::Fixture;
using Json=nlohmann::json;
using Role=assets::WalkerVisualRole;
using openemperor::testing::inspector::check;
using openemperor::testing::inspector::u32;
using openemperor::testing::inspector::write;
constexpr std::array names{"clay","pottery","household","fire_inspector","supplier",
    "distributor","service","health_worker","music","acrobat","drama","venue_music"};
template<class F> void rejects(F action,const char* reason) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
Json native_role() {
    Json role={{"ticks_per_frame",2},{"evidence","Independent native silhouettes; chosen preview feet and cadence."},
        {"clip_id","authored-compact-direction-cycle"},{"frames",Json::array()},
        {"clips",Json::object()},{"idle","a0"},{"idle_flip_x",true}};
    for (std::size_t direction=0;direction<2;++direction) for (int phase=0;phase<2;++phase) {
        auto frame=Fixture::frame(direction,phase);
        frame["alias"]=std::string(direction ? "b":"a")+std::to_string(phase);
        frame["foot_anchor"]={phase ? 3.75:2.25,phase ? 16.5:14.5};
        role["frames"].push_back(std::move(frame));
    }
    role["clips"]["pos_x"]={{"frames",{"a1","a0"}},{"flip_x",false}};
    role["clips"]["neg_x"]={{"frames",{"b0","b1"}},{"flip_x",true}};
    role["clips"]["pos_y"]={{"frames",{"a1","a0"}},{"flip_x",true}};
    role["clips"]["neg_y"]={{"frames",{"b0","b1"}},{"flip_x",false}};
    return role;
}
Json compact() {
    Json result={{"schema_version",7},{"mode","curated_walker_preview"},{"roles",Json::object()}};
    for (const auto* name:names) result["roles"][name]=native_role();
    return result;
}
assets::WalkerVisualProfile load(Fixture& fixture,const Json& document) {
    Fixture::save(fixture.supplement,document);
    return assets::load_walker_visual_profile(fixture.data,fixture.supplement);
}
void compact_contract(Fixture& fixture) {
    static_assert(assets::walker_schema6_role_count==8 && assets::walker_visual_role_count==12);
    static_assert(assets::walker_role_index(Role::HealthWorker)==7 &&
        assets::walker_role_index(Role::Music)==8 && assets::walker_role_index(Role::Acrobat)==9 &&
        assets::walker_role_index(Role::Drama)==10 && assets::walker_role_index(Role::VenueMusic)==11);
    static_assert(assets::walker_max_frame_aliases==320 && assets::walker_max_unique_assets==256 &&
        assets::walker_max_rgba_bytes==64U*1024U*1024U);
    const auto profile=load(fixture,compact());
    check(profile.schema_version==7 && profile.unique_images.size()==4,"Compact roles did not share native physical images");
    std::size_t aliases=0;
    for (std::size_t i=0;i<names.size();++i) {
        const auto role=static_cast<Role>(i);const auto* visual=profile.find(role);
        check(visual && std::string_view(assets::walker_role_name(role))==names[i],"Versioned visual role identity changed");
        aliases+=visual->frames.size();
        check(visual->frames.size()==4 && visual->idle_flip_x && visual->clips[0]==std::vector<std::size_t>{1,0} &&
            visual->clips[2]==visual->clips[0] && visual->clips[1]==std::vector<std::size_t>{2,3} &&
            visual->clips[3]==visual->clips[1] &&
            visual->clip_flip_x==std::array<bool,4>{false,true,true,false},
            "Direction clips lost explicit order, alias reuse or display transform");
        for (const auto& frame:visual->frames) {
            const auto width=profile.unique_images.at(frame.image_index).width;
            const auto native=assets::walker_frame_transform(frame,width);
            const auto reflected=assets::walker_frame_transform(frame,width,true);
            check(!native.flip_x && native.foot_x==frame.foot_x && native.foot_y==frame.foot_y &&
                reflected.flip_x && reflected.foot_x==width-frame.foot_x && reflected.foot_y==frame.foot_y,
                "Clip display transform changed source frame or used a discrete foot");
        }
    }
    check(aliases==48 && assets::walker_rgba_bytes(profile)==3008,
        "Compact aliases or physical RGBA cost includes duplicated reflected pixels");
    for (const auto& [logical,visual]:std::array{
        std::pair{sim::CourierRole::OriginalMusic,Role::Music},
        std::pair{sim::CourierRole::OriginalMusicPerformer,Role::Music},
        std::pair{sim::CourierRole::OriginalAcrobat,Role::Acrobat},
        std::pair{sim::CourierRole::OriginalDrama,Role::Drama},
        std::pair{sim::CourierRole::OriginalVenueMusic,Role::VenueMusic}}) {
        sim::CourierState courier;courier.role=logical;courier.enabled=true;
        courier.phase=sim::CourierPhase::ToWarehouse;courier.path={{2,2},{2,3}};
        const auto pose=openemperor::walker_pose(courier,0,profile);
        check(openemperor::walker_visual_role(logical)==visual && pose.role==visual && pose.frame==1 &&
            pose.clip_flip_x && pose.moving,"Culture role did not use its actual channel and explicit direction clip");
        assets::WalkerVisualProfile historical;historical.schema_version=6;
        check(!openemperor::walker_pose(courier,0,historical).frame,
            "Historical missing culture role invented a sprite fallback");
    }
    check(openemperor::walker_visual_role(sim::CourierRole::OriginalVenueMusic)!=
        openemperor::walker_visual_role(sim::CourierRole::OriginalMusicPerformer),
        "Distinct Theatre Actor36 sprite binding silently became Musician34");
}
void historical_contract(Fixture& fixture) {
    for (std::uint32_t version=1;version<=6;++version) {
        auto role=native_role();
        for (auto& clip:role["clips"]) clip=clip["frames"];
        Json document={{"schema_version",version},{"mode","curated_walker_preview"}};
        if (version==1) {
            document=role;document["schema_version"]=1;document["mode"]="curated_walker_preview";document["role"]="clay";
        } else {
            const std::size_t count=version==2 ? 3:version==3 ? 4:version==4 ? 6:version==5 ? 7:8;
            for (std::size_t i=0;i<count;++i) document["roles"][names[i]]=role;
        }
        const auto parsed=load(fixture,document);
        check(parsed.schema_version==version && parsed.unique_images.size()==4,"Historical native sequence changed");
        for (const auto& entry:parsed.roles) if (entry)
            check(entry->clip_flip_x==std::array<bool,4>{} && !entry->idle_flip_x,
                "Historical schema acquired implicit direction/idle reflection");
        if (version==1) continue;
        for (const auto* name:{"music","acrobat","drama","venue_music"}) {
            auto invalid=document;invalid["roles"]=Json::object({{name,role}});
            rejects([&]{load(fixture,invalid);},"Historical schema accepted a new culture role");
        }
        auto invalid=document;invalid["roles"]["clay"]["clips"]["pos_x"]=native_role()["clips"]["pos_x"];
        rejects([&]{load(fixture,invalid);},"Historical schema accepted a direction object");
    }
}
void strict_contract(Fixture& fixture) {
    const auto reject=[&](Json document,const char* reason) { rejects([&]{load(fixture,document);},reason); };
    for (const auto& value:std::array<Json,6>{0,1,"true",nullptr,Json::array({true}),Json::object()}) {
        auto invalid=compact();invalid["roles"]["music"]["clips"]["neg_x"]["flip_x"]=value;
        reject(invalid,"Nonboolean direction reflection activated");
        invalid=compact();invalid["roles"]["music"]["idle_flip_x"]=value;
        reject(invalid,"Nonboolean idle reflection activated");
    }
    for (const auto* field:{"frames","flip_x"}) {
        auto invalid=compact();invalid["roles"]["music"]["clips"]["neg_x"].erase(field);
        reject(invalid,"Incomplete direction object activated");
    }
    for (const auto* key:{"flip_y","rotate","scale"}) {
        auto invalid=compact();invalid["roles"]["music"]["clips"]["neg_x"][key]=true;
        reject(invalid,"Unknown direction transform activated");
    }
    auto unknown=compact();unknown["pixels"]="untrusted-extra";
    reject(unknown,"Unknown schema7 top-level field activated");
    unknown=compact();unknown["roles"]["music"]["animation_speed"]=1;
    reject(unknown,"Unknown schema7 role field activated");
    unknown=compact();unknown["roles"]["music"]["clips"]["north"]=native_role()["clips"]["pos_x"];
    reject(unknown,"Unknown schema7 direction key activated");
    for (const auto* name:{"clay","pottery","household","fire_inspector","music","acrobat","drama","venue_music"}) {
        auto invalid=compact();invalid["roles"][name]["frames"][0]["flip_x"]=false;
        reject(invalid,"New clip reflection widened historical per-frame flip admission");
    }
    for (const auto* name:{"clay","music","acrobat","drama"}) {
        auto invalid=compact();invalid["roles"][name]["clips"].erase("neg_y");
        reject(invalid,"Schema7 incomplete direction family activated");
        invalid=compact();invalid["roles"][name]["frames"][1]["image_index"]=1;
        reject(invalid,"Duplicate pixels with changed anchors fabricated an animated direction");
    }
    auto invalid=compact();invalid["roles"]["music"]["frames"][0]["flip_y"]=true;
    reject(invalid,"Unknown frame transform activated");
    invalid=compact();invalid["roles"]["music"]["frames"][0]["image_index"]=0;
    reject(invalid,"Dummy physical source activated");
    invalid=compact();invalid["roles"]["music"]["frames"][0]["archive"]="../walker.sg3";
    reject(invalid,"Source traversal activated");
    invalid=compact();invalid["schema_version"]=8;
    reject(invalid,"Future visual schema activated");
    auto archive=fixture.archive;u32(archive,40680+72+16,1);write(fixture.data/"DATA/walker.sg3",archive);
    reject(compact(),"An SG3 mirror-reference record was decoded as native pixels");
    write(fixture.data/"DATA/walker.sg3",fixture.archive);
    invalid=compact();auto& aliases=invalid["roles"]["clay"]["frames"];
    for (std::size_t i=48;i<320;++i) {
        auto alias=aliases[0];alias["alias"]="shared-unused-"+std::to_string(i);aliases.push_back(std::move(alias));
    }
    const auto at_limit=load(fixture,invalid);
    check(at_limit.unique_images.size()==4,"Alias boundary duplicated a physical image");
    auto excess=aliases[0];excess["alias"]="one-too-many";aliases.push_back(std::move(excess));
    reject(invalid,"Unchanged global 320-alias limit was exceeded");
    auto arrays=compact();
    for (auto& role:arrays["roles"]) for (auto& clip:role["clips"]) clip=clip["frames"];
    const auto unreflected=load(fixture,arrays);
    for (const auto& role:unreflected.roles) if (role)
        check(role->clip_flip_x==std::array<bool,4>{},"Schema7 native array clip acquired an implicit reflection");
    auto duplicate=compact().dump();const auto key=duplicate.find("\"pos_x\":");
    check(key!=std::string::npos,"Duplicate-key negative fixture construction");
    duplicate.insert(key,"\"pos_x\":[\"a0\",\"a1\"],");
    {
        std::ofstream output(fixture.supplement);output<<duplicate;
        check(bool(output),"Duplicate-key negative fixture write");
    }
    rejects([&]{assets::load_walker_visual_profile(fixture.data,fixture.supplement);},
        "Duplicate direction key silently changed a compact clip");
}
std::vector<std::uint8_t> read(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),{}};
}
}
int main() {
    try {
        Fixture fixture;const auto before_archive=read(fixture.data/"DATA/walker.sg3");
        const auto before_bitmap=read(fixture.data/"DATA/walker.555");
        compact_contract(fixture);historical_contract(fixture);strict_contract(fixture);
        check(read(fixture.data/"DATA/walker.sg3")==before_archive && read(fixture.data/"DATA/walker.555")==before_bitmap,
            "Loader/failed admission changed source bytes");
        std::cout<<"PASS schema7 compact direction aliases, source dedup, culture mapping, strict transforms, historical schemas and budgets\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
