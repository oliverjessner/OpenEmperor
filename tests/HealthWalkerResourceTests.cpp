#include "app/SandboxView.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/FireSpriteSet.h"
#include "renderer/WalkerSpriteSet.h"
#include "FireInspectorWalkerFixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string_view>

// Observe the actual two optional texture owners. All successful operations
// still call real SDL; injected failures affect only candidate preparation.
namespace observer {
std::size_t creates=0,uploads=0,destroys=0,fail_create=0,fail_upload=0;
std::set<SDL_Texture*> live,drawn;
std::vector<SDL_Texture*> destroyed_order;
SDL_Texture* create(SDL_Renderer* renderer,SDL_PixelFormat format,
                    SDL_TextureAccess access,int width,int height) {
    ++creates;
    if (creates==fail_create) { SDL_SetError("Injected candidate texture creation failure");return nullptr; }
    auto* texture=SDL_CreateTexture(renderer,format,access,width,height);
    if (texture) live.insert(texture);
    return texture;
}
bool upload(SDL_Texture* texture,const SDL_Rect* rect,const void* pixels,int pitch) {
    ++uploads;
    if (uploads==fail_upload) return SDL_SetError("Injected candidate texture upload failure");
    return SDL_UpdateTexture(texture,rect,pixels,pitch);
}
void destroy(SDL_Texture* texture) {
    ++destroys;live.erase(texture);destroyed_order.push_back(texture);SDL_DestroyTexture(texture);
}
bool draw(SDL_Renderer* renderer,SDL_Texture* texture,const SDL_FRect* source,
          const SDL_FRect* destination) {
    drawn.insert(texture);return SDL_RenderTexture(renderer,texture,source,destination);
}
}
#define SDL_CreateTexture observer::create
#define SDL_UpdateTexture observer::upload
#define SDL_DestroyTexture observer::destroy
#define SDL_RenderTexture observer::draw
#define live_textures resource_test_walker_live_textures
#include "../src/renderer/WalkerSpriteSet.cpp"
#undef live_textures
#define live_textures resource_test_fire_live_textures
#include "../src/renderer/FireSpriteSet.cpp"
#undef live_textures
#undef SDL_CreateTexture
#undef SDL_UpdateTexture
#undef SDL_DestroyTexture
#undef SDL_RenderTexture

namespace {
namespace fs=std::filesystem;
namespace assets=openemperor::assets;
namespace maps=openemperor::maps;
namespace sim=openemperor::simulation;
namespace perf=openemperor::performance;
namespace authored=openemperor::testing::inspector;
using Fixture=authored::Fixture;
using View=openemperor::SandboxView;
using Source=openemperor::VisualProfileSource;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
using authored::check;
using authored::u16;
using authored::u32;
using authored::write;
std::uint64_t observed_bytes() {
    std::uint64_t bytes=0;
    for (auto* texture:observer::live) {
        float width=0,height=0;
        check(SDL_GetTextureSize(texture,&width,&height),"actual optional texture dimensions");
        bytes+=static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)*4;
    }
    return bytes;
}
template<class F> void rejects(F action,const char* reason) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
Json market() {
    auto role=Fixture::inspector()["roles"]["fire_inspector"];
    for (auto& frame:role["frames"]) frame["archive"]="DATA/market.sg3";
    role["clip_id"]="authored-shared-market-family";
    return {{"schema_version",4},{"mode","curated_walker_preview"},
        {"roles",{{"supplier",role},{"distributor",role}}}};
}
fs::path market_profile(Fixture& fixture) {
    write(fixture.data/"DATA/market.sg3",fixture.archive);
    write(fixture.data/"DATA/market.555",fixture.bitmap);
    const auto path=fixture.root/"market.json";Fixture::save(path,market());return path;
}
fs::path fire_profile(Fixture& fixture) {
    write(fixture.data/"DATA/effect.sg3",fixture.archive);
    write(fixture.data/"DATA/effect.555",fixture.bitmap);
    const auto path=fixture.root/"fire.json";
    Fixture::save(path,{{"schema_version",1},{"mode","curated_fire_presentation"},
        {"clip_id","authored-candidate-effect"},{"evidence","Independent resource lifecycle fixture."},
        {"ticks_per_frame",2},{"frames",Json::array({
            {{"archive","DATA/effect.sg3"},{"image_index",1},{"anchor",{5,15}}},
            {{"archive","DATA/effect.sg3"},{"image_index",3},{"anchor",{6,17}}}})}});
    return path;
}
void ground(Fixture& fixture) {
    Bytes archive(40680+2*72,0),bitmap(3204,0);
    u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,2);u32(archive,16,2);u32(archive,20,1);
    const std::string group="authored-ground.bmp";
    std::copy(group.begin(),group.end(),archive.begin()+680);u32(archive,804,2);
    const auto at=40680+72;u32(archive,at,4);u32(archive,at+4,3200);u32(archive,at+8,3200);
    u16(archive,at+20,78);u16(archive,at+22,40);u16(archive,at+50,30);archive[at+55]=1;
    for (std::size_t i=4;i<bitmap.size();i+=2) u16(bitmap,i,0x18c6);
    write(fixture.data/"DATA/ground.sg3",archive);write(fixture.data/"DATA/ground.555",bitmap);
}
fs::path road_profile(Fixture& fixture) {
    const auto path=fixture.root/"roads.json";
    Fixture::save(path,{{"schema_version",1},{"mode","curated_road_preview"},
        {"tiles",{{"0x0",{{"archive","DATA/ground.sg3"},{"image_index",1},
                           {"ground_anchor",{39,20}},{"evidence","Authored budget tile."}}}}}});
    return path;
}
maps::StoredMapSession session(Fixture& fixture,std::uint64_t stored_bytes=12480) {
    fs::create_directories(fixture.data/"Cities");
    { std::ofstream out(fixture.data/"Cities/Authored.map");out<<"Independent optional-resource fixture"; }
    ground(fixture);
    maps::ParsedEmperorMap map;map.declared_map_size=84;
    maps::StoredGraphicsPlan plan;plan.data_root=fixture.data;plan.profile=maps::StoredGraphicsProfile::Slot8;
    plan.border=maps::MapGeometry{84}.border;
    plan.status_by_storage.assign(228U*228U,maps::StoredStatus::Excluded);
    plan.cell_by_storage.resize(228U*228U);
    const auto asset=[&](const char* archive,std::uint32_t record,int width,int height,
                         std::uint32_t length,std::uint16_t type) {
        assets::AssetRecord metadata;metadata.id={archive,record};
        metadata.width=static_cast<std::int16_t>(width);metadata.height=static_cast<std::int16_t>(height);
        metadata.data_length=length;metadata.image_type=type;
        if (type==30) metadata.uncompressed_length=3200;
        maps::StoredAsset item;item.record=metadata;plan.assets.push_back(std::move(item));
    };
    if (stored_bytes==12480) asset("DATA/ground.sg3",1,78,40,3200,30);
    else {
        // Actual decoded/uploaded buffers reach the exact requested budget;
        // the plan's logical byte field is never forged.
        check(stored_bytes%4==0,"budget pixel alignment");
        std::vector<std::pair<int,int>> dimensions;
        auto pixels=stored_bytes/4;
        while (pixels>=1024U*4096U) { dimensions.emplace_back(1024,4096);pixels-=1024U*4096U; }
        if (pixels>=1024) { dimensions.emplace_back(1024,static_cast<int>(pixels/1024));pixels%=1024; }
        if (pixels) dimensions.emplace_back(static_cast<int>(pixels),1);
        Bytes archive(40680+(dimensions.size()+1)*72,0),bitmap{0,0,0,0};
        u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
        u32(archive,12,static_cast<std::uint32_t>(dimensions.size()+1));
        u32(archive,16,static_cast<std::uint32_t>(dimensions.size()+1));u32(archive,20,1);
        const std::string group="authored-budget.bmp";
        std::copy(group.begin(),group.end(),archive.begin()+680);
        u32(archive,804,static_cast<std::uint32_t>(dimensions.size()+1));
        for (std::size_t i=0;i<dimensions.size();++i) {
            const auto [width,height]=dimensions[i];Bytes payload;
            for (int y=0;y<height;++y) for (int remaining=width;remaining>0;) {
                const auto count=std::min(remaining,255);payload.push_back(255);
                payload.push_back(static_cast<std::uint8_t>(count));remaining-=count;
            }
            const auto record=static_cast<std::uint32_t>(i+1);
            const auto at=40680+std::size_t(record)*72;
            u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));
            u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
            u16(archive,at+20,static_cast<std::uint16_t>(width));
            u16(archive,at+22,static_cast<std::uint16_t>(height));u16(archive,at+50,256);
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
            asset("DATA/budget.sg3",record,width,height,static_cast<std::uint32_t>(payload.size()),256);
        }
        write(fixture.data/"DATA/budget.sg3",archive);write(fixture.data/"DATA/budget.555",bitmap);
    }
    for (std::uint32_t y=110;y<=114;++y) for (std::uint32_t x=110;x<=114;++x) {
        const auto selected=static_cast<std::size_t>((x+y)%plan.assets.size());
        const auto& metadata=plan.assets[selected].record;
        maps::StoredCell cell;cell.storage={x,y};cell.cell_index=std::size_t(y)*228+x;
        cell.terrain_raw=0x80;cell.status=maps::StoredStatus::DecodePending;cell.asset_index=selected;
        cell.footprint_index=plan.footprints.size();cell.world=maps::terrain_world(cell.storage,plan.border);
        cell.image_origin=maps::stored_image_origin(cell.world,static_cast<unsigned>(metadata.width),
                                                    static_cast<unsigned>(metadata.height));
        const auto index=plan.cells.size();plan.cell_by_storage[cell.cell_index]=index;
        plan.status_by_storage[cell.cell_index]=cell.status;plan.cells.push_back(cell);
        maps::PlacedFootprint footprint;footprint.id=plan.footprints.size();footprint.asset_index=selected;
        footprint.origin=cell.storage;footprint.cell_indices={index};footprint.image_origin=cell.image_origin;
        footprint.status=maps::StoredStatus::DecodePending;plan.footprints.push_back(footprint);
    }
    return {std::move(map),std::move(plan)};
}
void start(View& view,Fixture& fixture,SDL_Window* window,SDL_Renderer* renderer) {
    view.configure_save(fixture.data,"Cities/Authored.map",fixture.root/"save.json");
    view.set_walker_visuals(fixture.core,Source::Builtin);view.initialize(window,renderer);
}
void pure_render(View& view) {
    const auto before=view.world().snapshot();perf::set_enabled(true);perf::reset();
    check(view.render(),"ordinary production render failed");
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"render performed IO/decode/upload/World/navigation work");
    perf::set_enabled(false);check(view.world().snapshot()==before,"render mutated full World");
}
Json service(bool shared=false) {
    auto role=Fixture::inspector()["roles"]["fire_inspector"];
    role["clip_id"]="authored-independent-service-family";
    for(auto& frame:role["frames"]) {
        frame["archive"]=shared ? "DATA/walker.sg3":"DATA/service.sg3";
        frame["flip_x"]=false;
    }
    return {{"schema_version",5},{"mode","curated_walker_preview"},{"roles",{{"service",role}}}};
}
fs::path service_profile(Fixture& fixture) {
    write(fixture.data/"DATA/service.sg3",fixture.archive);
    write(fixture.data/"DATA/service.555",fixture.bitmap);
    const auto path=fixture.root/"service.json";Fixture::save(path,service());return path;
}
std::set<SDL_Texture*> difference(const std::set<SDL_Texture*>& a,const std::set<SDL_Texture*>& b) {
    std::set<SDL_Texture*> result;
    std::set_difference(a.begin(),a.end(),b.begin(),b.end(),std::inserter(result,result.end()));return result;
}

Json health(bool shared=false) {
    auto role=Fixture::inspector()["roles"]["fire_inspector"];
    role["clip_id"]="authored-independent-health-worker-family";
    for(auto& frame:role["frames"]) {
        frame["archive"]=shared ? "DATA/walker.sg3":"DATA/health.sg3";
        frame["flip_x"]=false;
    }
    return {{"schema_version",6},{"mode","curated_walker_preview"},{"roles",{{"health_worker",role}}}};
}
fs::path health_profile(Fixture& fixture) {
    write(fixture.data/"DATA/health.sg3",fixture.archive);
    write(fixture.data/"DATA/health.555",fixture.bitmap);
    const auto path=fixture.root/"health.json";Fixture::save(path,health());return path;
}
struct Prepared {std::array<std::set<SDL_Texture*>,4> tails;};
using Paths=std::array<fs::path,4>;
Paths paths(Fixture& fixture) {
    return {fixture.supplement,market_profile(fixture),service_profile(fixture),health_profile(fixture)};
}
void select(View& view,std::size_t role,const fs::path& path) {
    if(role==0) view.set_fire_inspector_visuals(path);
    else if(role==1) view.set_market_walker_visuals(path);
    else if(role==2) view.set_service_walker_visuals(path);
    else view.set_health_walker_visuals(path);
}
bool configured(const View& view,std::size_t role) {
    if(role==0) return view.fire_inspector_display_stats().configured;
    if(role==1) return view.market_walker_display_stats().configured;
    if(role==2) return view.service_walker_display_stats().configured;
    return view.health_walker_display_stats().configured;
}
Prepared append_all(View& view,const Paths& path,std::array<int,4> order) {
    Prepared prepared;
    for(const auto role:order) {
        const auto before=observer::live;select(view,static_cast<std::size_t>(role),path[static_cast<std::size_t>(role)]);
        prepared.tails[static_cast<std::size_t>(role)]=difference(observer::live,before);
    }
    check(configured(view,0)&&configured(view,1)&&configured(view,2)&&configured(view,3)&&
        view.walker_texture_count()==32&&observed_bytes()==24064,"four supplements measured global physical/byte budgets differ");
    check(prepared.tails[0].size()==7&&prepared.tails[1].size()==8&&prepared.tails[2].size()==8&&prepared.tails[3].size()==8,
        "four supplements did not deduplicate exact native source identity");return prepared;
}
void retained(const std::set<SDL_Texture*>& foreign,const char* reason) {
    check(std::includes(observer::live.begin(),observer::live.end(),foreign.begin(),foreign.end()),reason);
}
void cleanup(View& view) {
    view.shutdown();check(observer::live.empty()&&openemperor::WalkerSpriteSet::live_texture_count()==0&&
        openemperor::FireSpriteSet::live_texture_count()==0,"Health session leaked actual optional textures");
}
void lifecycle(SDL_Window* window,SDL_Renderer* renderer) {
    std::array<int,4> order{0,1,2,3};std::size_t sessions=0;
    do {
        Fixture fixture;const auto path=paths(fixture);const auto effect=fire_profile(fixture);
        View view(session(fixture),false,sim::RulesProfile::CityV15,1);start(view,fixture,window,renderer);
        auto prepared=append_all(view,path,order);const auto walkers=observer::live;view.set_fire_visuals(effect);
        auto flame_tail=difference(observer::live,walkers);check(flame_tail.size()==2&&view.fire_display_stats().animated,"prepared flames missing");
        const auto full_snapshot=view.world().snapshot();const auto foreign=difference(observer::live,prepared.tails[3]);
        const auto no_uploads=observer::uploads;view.set_health_walker_visuals({});
        check(!configured(view,3)&&configured(view,0)&&configured(view,1)&&configured(view,2)&&
            observer::live==foreign&&observer::uploads==no_uploads&&view.fire_display_stats().animated,
            "Health removal destroyed or reuploaded another prepared owner");
        for(const bool create_failure:{false,true}) {
            if(create_failure) observer::fail_create=observer::creates+2;
            else observer::fail_upload=observer::uploads+2;
            view.set_health_walker_visuals(path[3]);observer::fail_create=observer::fail_upload=0;
            check(observer::live==foreign&&!configured(view,3)&&configured(view,0)&&configured(view,1)&&configured(view,2)&&
                !view.health_walker_display_stats().fallback_reason.empty()&&view.fire_display_stats().animated,
                "failed Health SDL create/upload published tail or damaged foreign textures");
        }
        view.set_health_walker_visuals(path[3]);check(configured(view,3),"Health candidate restoration failed");
        prepared.tails[3]=difference(observer::live,foreign);
        for(const auto& archive:{"walker","market","service","health"})for(const auto& extension:{".sg3",".555"})
            fs::rename(fixture.data/"DATA"/(std::string(archive)+extension),fixture.data/"DATA"/(std::string(archive)+"-unavailable"+extension));
        const auto unavailable=observer::live,without_flame=difference(unavailable,flame_tail);
        const auto no_reload=observer::uploads,no_destroy=observer::destroys;
        const auto malformed=fixture.root/"invalid-fire.json";Fixture::save(malformed,Json{{"schema_version",1}});
        for(const auto& bad:{malformed,fixture.root/"missing-fire.json"}) {
            view.set_fire_visuals(bad);check(observer::live==without_flame&&observer::uploads==no_reload&&
                configured(view,0)&&configured(view,1)&&configured(view,2)&&configured(view,3),
                "invalid/missing fire discarded unavailable prepared walker families");
        }
        check(observer::destroys==no_destroy+flame_tail.size(),"invalid fire destroyed a foreign owner");
        view.set_fire_visuals(effect);flame_tail=difference(observer::live,without_flame);
        check(view.fire_display_stats().animated&&flame_tail.size()==2,"valid fire reread unavailable foreign sources");
        const auto stable_uploads=observer::uploads;pure_render(view);
        // Reversing each of all24 append permutations also covers all24 distinct
        // removal permutations, with foreign source files already unavailable.
        for(auto it=order.rbegin();it!=order.rend();++it) {
            const auto role=static_cast<std::size_t>(*it);const auto before=observer::live;
            if(role==3) {
                view.set_health_walker_visuals(path[3]);
                check(!view.health_walker_display_stats().fallback_reason.empty(),"missing Health dependency lacks named fallback");
                view.set_health_walker_visuals(fixture.root/"missing-health.json");
            } else select(view,role,{});
            check(observer::live==difference(before,prepared.tails[role])&&observer::uploads==stable_uploads&&!configured(view,role),
                "removing optional role destroyed or reread another unavailable prepared family");
            retained(flame_tail,"optional removal destroyed existing flame texture pointers");pure_render(view);
        }
        check(view.walker_texture_count()==1&&view.world().snapshot()==full_snapshot,"optional lifecycle changed core or complete World");
        cleanup(view);++sessions;
    }while(std::next_permutation(order.begin(),order.end()));
    check(sessions==24,"four-role append/removal permutation coverage incomplete");
}
void shared_and_aliases(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto path=paths(fixture);View view(session(fixture),false,sim::RulesProfile::CityV15,1);start(view,fixture,window,renderer);
    for(std::size_t i=0;i<3;++i)select(view,i,path[i]);
    const auto foreign=observer::live;const auto no_uploads=observer::uploads;
    const auto shared=fixture.root/"shared-health.json";Fixture::save(shared,health(true));view.set_health_walker_visuals(shared);
    check(configured(view,3)&&view.health_walker_display_stats().additional_assets==0&&view.walker_texture_count()==24&&
        observer::live==foreign&&observer::uploads==no_uploads,"Health shared native IDs created duplicate images/uploads");
    view.set_health_walker_visuals({});check(observer::live==foreign&&observer::uploads==no_uploads,"Health shared removal destroyed foreign physical IDs");
    auto exact=health();auto& frames=exact["roles"]["health_worker"]["frames"];const auto pattern=frames[0];
    // Core3 +Inspector8 +Market16 +Service8 =35. Health285 fills exactly320.
    for(std::size_t i=8;i<285;++i){auto extra=pattern;extra["alias"]="unused-"+std::to_string(i);frames.push_back(std::move(extra));}
    const auto exact_path=fixture.root/"exact320.json";Fixture::save(exact_path,exact);view.set_health_walker_visuals(exact_path);
    check(configured(view,3)&&view.walker_texture_count()==32,"exact schema6 global320 aliases rejected");
    auto extra=pattern;extra["alias"]="overflow321";frames.push_back(extra);const auto overflow=fixture.root/"overflow321.json";Fixture::save(overflow,exact);
    const auto before_overflow=observer::uploads;view.set_health_walker_visuals(overflow);
    check(!configured(view,3)&&configured(view,0)&&configured(view,1)&&configured(view,2)&&observer::live==foreign&&
        observer::uploads==before_overflow,"schema6 global321 partially published or harmed foreign owners");
    // The explicitly unchanged old standalone schema5 remains bounded at256.
    auto legacy=service();auto& old_frames=legacy["roles"]["service"]["frames"];const auto old_pattern=old_frames[0];
    for(std::size_t i=8;i<256;++i){auto alias=old_pattern;alias["alias"]="old-"+std::to_string(i);old_frames.push_back(alias);}
    const auto old=fixture.root/"legacy256.json";Fixture::save(old,legacy);check(assets::load_walker_visual_profile(fixture.data,old).find(assets::WalkerVisualRole::Service)->frames.size()==256,
        "historical schema5 exact256 standalone aliases rejected");auto old_extra=old_pattern;old_extra["alias"]="old257";old_frames.push_back(old_extra);Fixture::save(old,legacy);
    rejects([&]{(void)assets::load_walker_visual_profile(fixture.data,old);},"old schema5 standalone257 aliases silently expanded");
    view.set_health_walker_visuals(path[3]);const auto walkers=observer::live;const auto effect=fire_profile(fixture);view.set_fire_visuals(effect);
    const auto flame=difference(observer::live,walkers);view.set_walker_visuals(fixture.core,Source::Custom);
    check(!configured(view,0)&&!configured(view,1)&&!configured(view,2)&&!configured(view,3)&&
        view.walker_texture_count()==1&&view.fire_display_stats().animated,"Custom core received hidden automatic supplements");retained(flame,"Custom override destroyed flames");
    const auto custom_live=observer::live;const auto custom_uploads=observer::uploads;view.set_health_walker_visuals(path[3]);
    check(!configured(view,3)&&observer::live==custom_live&&observer::uploads==custom_uploads,"Custom core was silently supplemented by Health");pure_render(view);cleanup(view);
}

void removal_schema_capacity(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;auto legacy=Fixture::legacy();auto& frames=legacy["roles"]["clay"]["frames"];const auto pattern=frames[0];
    for(std::size_t i=1;i<254;++i){auto f=pattern;f["alias"]="legacy-capacity-"+std::to_string(i);frames.push_back(f);}
    Fixture::save(fixture.core,legacy);const auto path=paths(fixture);
    View view(session(fixture),false,sim::RulesProfile::CityV15,1);start(view,fixture,window,renderer);
    check(view.walker_display_stats().frame_aliases==256&&view.walker_display_stats().frame_alias_limit==256,
        "historical exact256 core fixture does not use unchanged capacity");
    select(view,3,path[3]);select(view,0,path[0]);select(view,1,path[1]);select(view,2,path[2]);
    check(view.walker_display_stats().frame_aliases==296&&view.walker_display_stats().schema_version==6,
        "Health did not establish schema6 capacity for later older supplements");
    const auto healthy=observer::live;const auto uploads=observer::uploads;
    select(view,3,{});const auto stats=view.walker_display_stats();
    check(stats.frame_aliases==288&&stats.schema_version==6&&stats.frame_alias_limit==320&&
        configured(view,0)&&configured(view,1)&&configured(view,2)&&observer::uploads==uploads,
        "removing Health downgraded still-over256 older aliases or dropped/reuploaded valid roles");
    const auto after_health=observer::live;check(difference(healthy,after_health).size()==8,"Health capacity removal destroyed foreign physical assets");
    select(view,2,{});check(view.walker_display_stats().frame_aliases==280&&view.walker_display_stats().schema_version==6&&
        configured(view,0)&&configured(view,1)&&observer::uploads==uploads,"removing Service prematurely downgraded over256 aggregate");
    select(view,1,{});check(view.walker_display_stats().frame_aliases==264&&view.walker_display_stats().schema_version==6&&
        configured(view,0)&&observer::uploads==uploads,"removing Market prematurely downgraded over256 aggregate");
    select(view,0,{});check(view.walker_display_stats().frame_aliases==256&&view.walker_display_stats().schema_version==2&&
        view.walker_display_stats().frame_alias_limit==256&&view.walker_texture_count()==1&&observer::uploads==uploads,
        "last removal failed to restore exactly bounded historical schema/capacity/core images");pure_render(view);cleanup(view);
}
void physical_assets(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto path=paths(fixture);View view(session(fixture),false,sim::RulesProfile::CityV15,1);start(view,fixture,window,renderer);
    for(std::size_t i=0;i<3;++i)select(view,i,path[i]);const auto foreign=observer::live;
    auto archive=fixture.archive;archive.resize(40680+245*72,0);u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,12,245);u32(archive,16,245);u32(archive,804,245);
    for(std::size_t i=20;i<245;++i)std::copy_n(archive.begin()+40752,72,archive.begin()+static_cast<std::ptrdiff_t>(40680+i*72));
    write(fixture.data/"DATA/health.sg3",archive);
    auto exact=health();auto& frames=exact["roles"]["health_worker"]["frames"];const auto pattern=frames[0];
    for(std::size_t i=20;i<244;++i){auto f=pattern;f["alias"]="physical-"+std::to_string(i);f["image_index"]=i;frames.push_back(f);}
    const auto file=fixture.root/"physical256.json";Fixture::save(file,exact);view.set_health_walker_visuals(file);
    check(configured(view,3)&&view.walker_texture_count()==256,"actual256 physical native assets rejected");
    auto f=pattern;f["alias"]="physical257";f["image_index"]=244;frames.push_back(f);Fixture::save(file,exact);const auto uploaded=observer::uploads;view.set_health_walker_visuals(file);
    check(!configured(view,3)&&observer::live==foreign&&observer::uploads==uploaded&&configured(view,0)&&configured(view,1)&&configured(view,2),
        "physical257 partially activated or changed preserved roles");pure_render(view);cleanup(view);
}
fs::path sized_fire(Fixture& fixture,int width,int height,const char* name) {
    Bytes archive(40680+3*72,0),bitmap{0,0,0,0};
    u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,3);u32(archive,16,3);u32(archive,20,1);
    const std::string group="authored-budget-effect.bmp";std::copy(group.begin(),group.end(),archive.begin()+680);u32(archive,804,3);
    for(unsigned i=1;i<=2;++i) {
        Bytes payload;
        for(int y=0;y<height;++y) for(int x=0;x<width;) {
            const auto count=std::min(width-x,254);payload.push_back(static_cast<std::uint8_t>(count));
            for(int n=0;n<count;++n) {payload.push_back(i==1 ? 31:0);payload.push_back(i==1 ? 0:124);}x+=count;
        }
        const auto at=40680+std::size_t(i)*72;u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));
        u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));u16(archive,at+20,static_cast<std::uint16_t>(width));
        u16(archive,at+22,static_cast<std::uint16_t>(height));u16(archive,at+50,256);
        bitmap.insert(bitmap.end(),payload.begin(),payload.end());
    }
    const auto archive_name=std::string("DATA/")+name+".sg3",bitmap_name=std::string("DATA/")+name+".555";
    write(fixture.data/archive_name,archive);write(fixture.data/bitmap_name,bitmap);
    const auto path=fixture.root/(std::string(name)+".json");
    Fixture::save(path,{{"schema_version",1},{"mode","curated_fire_presentation"},{"clip_id","authored-budget-fire"},
        {"evidence","Independently authored actual decoded session budget buffers."},{"ticks_per_frame",2},
        {"frames",Json::array({{{"archive",archive_name},{"image_index",1},{"anchor",{width/2,height}}},
                              {{"archive",archive_name},{"image_index",2},{"anchor",{width/2,height}}}})}});return path;
}

void budget(SDL_Window* window,SDL_Renderer* renderer) {
    constexpr std::uint64_t limit=64U*1024U*1024U;
    for(int pressure=0;pressure<5;++pressure) {
        Fixture fixture;const auto path=paths(fixture);
        const auto effect=pressure==0?fire_profile(fixture):pressure==1?sized_fire(fixture,20,50,"medium-effect"):
            pressure==2?sized_fire(fixture,40,52,"large-effect"):pressure==3?sized_fire(fixture,60,48,"max-effect"):
            sized_fire(fixture,64,48,"oversized-effect");
        View view(session(fixture,limit-24064-12480),false,sim::RulesProfile::CityV15,1);start(view,fixture,window,renderer);
        const auto roads=road_profile(fixture);view.set_road_visuals(roads);const auto prepared=append_all(view,path,{0,1,2,3});
        const auto exact=observer::live;const auto no_upload=observer::uploads;view.set_road_visuals(roads);
        check(observer::live==exact&&observer::uploads==no_upload&&configured(view,3),"exact64MiB zero headroom evicted Health or an existing role");
        if(pressure==4) {
            const auto creates=observer::creates,destroys=observer::destroys;view.set_fire_visuals(effect);
            check(!view.fire_display_stats().animated&&observer::live==exact&&observer::uploads==no_upload&&observer::creates==creates&&observer::destroys==destroys&&
                configured(view,0)&&configured(view,1)&&configured(view,2)&&configured(view,3),"unfit Fire changed budgets or evicted prepared roles");pure_render(view);cleanup(view);continue;
        }
        for(const bool create_failure:{false,true}){
            if(create_failure)observer::fail_create=observer::creates+1;else observer::fail_upload=observer::uploads+1;
            view.set_fire_visuals(effect);observer::fail_create=observer::fail_upload=0;
            check(observer::live==exact&&configured(view,0)&&configured(view,1)&&configured(view,2)&&configured(view,3)&&!view.fire_display_stats().animated,
                "failed prepared Fire candidate evicted optional textures before successful publication");
        }
        for(const auto& archive:{"walker","market","service","health"})for(const auto& extension:{".sg3",".555"})
            fs::rename(fixture.data/"DATA"/(std::string(archive)+extension),fixture.data/"DATA"/(std::string(archive)+"-unavailable"+extension));
        const auto destroyed_begin=observer::destroyed_order.size();view.set_fire_visuals(effect);
        check(view.fire_display_stats().animated&&!configured(view,3)&&configured(view,2)==(pressure==0)&&
            configured(view,1)==(pressure<2)&&configured(view,0)==(pressure<3),"documented Health, Service, Market, Inspector Fire priority changed");
        const auto expected=pressure==0?24U:pressure==1?16U:pressure==2?8U:1U;check(view.walker_texture_count()==expected,"Fire pressure retained wrong physical owner images");
        auto removed=prepared.tails[3];if(pressure>=1)removed.insert(prepared.tails[2].begin(),prepared.tails[2].end());
        if(pressure>=2)removed.insert(prepared.tails[1].begin(),prepared.tails[1].end());if(pressure>=3)removed.insert(prepared.tails[0].begin(),prepared.tails[0].end());
        const std::set<SDL_Texture*> actual(observer::destroyed_order.begin()+static_cast<std::ptrdiff_t>(destroyed_begin),observer::destroyed_order.end());
        check(actual==removed,"Fire pressure removed a surviving/core texture or retained an excluded asset");
        for(int role=0;role<3-pressure;++role)retained(prepared.tails[static_cast<std::size_t>(role)],"Fire pressure destroyed preserved foreign pointers");
        pure_render(view);cleanup(view);
    }
}
}
int main(int argc,char**argv) {
    SDL_Window*window=nullptr;SDL_Renderer*renderer=nullptr;
    try {
        const bool metal=argc==2&&std::string_view(argv[1])=="metal";check(argc==1||metal,"Usage: Health resource tests [metal]");
        if(!metal)SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
        window=SDL_CreateWindow("Independent Health resource tests",1280,720,SDL_WINDOW_HIDDEN);check(window!=nullptr,"SDL hidden window");renderer=SDL_CreateRenderer(window,metal?"metal":"software");check(renderer!=nullptr,"actual renderer");check(std::string_view(SDL_GetRendererName(renderer))==(metal?"metal":"software"),"actual backend mismatch");
        lifecycle(window,renderer);shared_and_aliases(window,renderer);physical_assets(window,renderer);removal_schema_capacity(window,renderer);budget(window,renderer);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"PASS Health atomic resources:24 append/removal permutations, shared IDs,320/321 aliases,256/257 physical assets,64MiB,Health-first Fire pressure and shutdown\n";return 0;
    }catch(const std::exception&error){std::cerr<<error.what()<<'\n';if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
}
