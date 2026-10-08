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
struct Prepared {std::array<std::set<SDL_Texture*>,3> tails;};
Prepared append_all(View& view,const Fixture& fixture,const fs::path& market_path,
                    const fs::path& service_path,std::array<int,3> order) {
    Prepared prepared;
    for(const auto role:order) {
        const auto before=observer::live;
        if(role==0) view.set_fire_inspector_visuals(fixture.supplement);
        else if(role==1) view.set_market_walker_visuals(market_path);
        else view.set_service_walker_visuals(service_path);
        prepared.tails[static_cast<std::size_t>(role)]=difference(observer::live,before);
    }
    check(view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
          view.service_walker_display_stats().configured && view.walker_texture_count()==24 &&
          observed_bytes()==18048,"three independent supplement append orders/budgets differ");
    check(prepared.tails[0].size()==7 && prepared.tails[1].size()==8 && prepared.tails[2].size()==8,
          "supplement source identity/global physical dedupe differs");return prepared;
}
void retained(const std::set<SDL_Texture*>& foreign,const char* reason) {
    check(std::includes(observer::live.begin(),observer::live.end(),foreign.begin(),foreign.end()),reason);
}
void cleanup(View& view) {
    view.shutdown();check(observer::live.empty() && openemperor::WalkerSpriteSet::live_texture_count()==0 &&
        openemperor::FireSpriteSet::live_texture_count()==0,"Service session leaked actual optional textures");
}
void lifecycle(SDL_Window* window,SDL_Renderer* renderer) {
    std::array<int,3> order{0,1,2};
    do {
        Fixture fixture;const auto market_path=market_profile(fixture),service_path=service_profile(fixture);
        const auto fire_path=fire_profile(fixture);
        View view(session(fixture),false,sim::RulesProfile::CityV12,1);start(view,fixture,window,renderer);
        const auto prepared=append_all(view,fixture,market_path,service_path,order);
        const auto walker_live=observer::live;view.set_fire_visuals(fire_path);
        const auto fire_tail=difference(observer::live,walker_live);
        check(fire_tail.size()==2 && view.fire_display_stats().animated,"prepared flame missing");
        const auto before=view.world().snapshot();const auto all_live=observer::live;
        const auto foreign=difference(all_live,prepared.tails[2]);const auto uploads=observer::uploads;
        view.set_service_walker_visuals({});
        check(!view.service_walker_display_stats().configured && view.fire_inspector_display_stats().configured &&
            view.market_walker_display_stats().configured && view.fire_display_stats().animated &&
            observer::live==foreign && observer::uploads==uploads,"Service removal destroyed/reuploaded foreign textures");
        for(const bool create_failure:{false,true}) {
            if(create_failure) observer::fail_create=observer::creates+2;
            else observer::fail_upload=observer::uploads+2;
            view.set_service_walker_visuals(service_path);observer::fail_create=observer::fail_upload=0;
            check(observer::live==foreign && !view.service_walker_display_stats().configured &&
                !view.service_walker_display_stats().fallback_reason.empty() && view.fire_inspector_display_stats().configured &&
                view.market_walker_display_stats().configured && view.fire_display_stats().animated,
                "Service candidate create/upload rollback harmed prepared foreign textures");
        }
        view.set_service_walker_visuals(service_path);const auto service_tail=difference(observer::live,foreign);
        const auto current_uploads=observer::uploads;
        view.set_fire_inspector_visuals({});
        retained(service_tail,"Inspector removal destroyed prepared Service textures");
        retained(prepared.tails[1],"Inspector removal destroyed prepared Market textures");
        retained(fire_tail,"Inspector removal destroyed existing flames");
        check(view.service_walker_display_stats().configured && view.market_walker_display_stats().configured &&
            view.walker_texture_count()==17 && observer::uploads==current_uploads,"Inspector removal reloaded foreign roles");
        view.set_fire_inspector_visuals(fixture.supplement);
        const auto restored=observer::live;const auto upload_restored=observer::uploads;
        view.set_market_walker_visuals({});retained(service_tail,"Market removal destroyed Service textures");
        retained(fire_tail,"Market removal destroyed flames");
        check(view.fire_inspector_display_stats().configured && view.service_walker_display_stats().configured &&
            view.walker_texture_count()==16 && observer::uploads==upload_restored,"Market removal reuploaded Service/Inspector");
        view.set_market_walker_visuals(market_path);check(observer::live.size()==restored.size(),"Market restoration resource count");
        for(const auto& archive:{"walker","market","service"}) {
            for(const auto& extension:{".sg3",".555"})
                fs::rename(fixture.data/"DATA"/(std::string(archive)+extension),
                           fixture.data/"DATA"/(std::string(archive)+"-unavailable"+extension));
        }
        const auto unavailable_live=observer::live;const auto no_uploads=observer::uploads,no_destroys=observer::destroys;
        const auto bad_fire=fixture.root/"bad-fire.json";Fixture::save(bad_fire,Json{{"schema_version",1}});
        for(const auto& path:{bad_fire,fixture.root/"missing-fire.json"}) {
            view.set_fire_visuals(path);
            const auto without_fire=difference(unavailable_live,fire_tail);
            check(observer::live==without_fire && observer::uploads==no_uploads &&
                view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
                view.service_walker_display_stats().configured,"invalid flame reread or discarded prepared unavailable walkers");
        }
        check(observer::destroys==no_destroys+fire_tail.size(),"invalid flame removed more than owned effect");
        view.set_fire_visuals(fire_path);check(view.fire_display_stats().animated,"valid fire needs no unavailable walker sources");
        const auto no_reuploads=observer::uploads;
        view.set_fire_inspector_visuals({});retained(service_tail,"unavailable-source Inspector removal destroyed Service");
        check(observer::uploads==no_reuploads && view.service_walker_display_stats().configured &&
            view.market_walker_display_stats().configured,"Inspector removal reread unavailable foreign sources");
        const auto after_inspector=observer::live;
        view.set_service_walker_visuals(fixture.root/"missing-service.json");
        check(!view.service_walker_display_stats().configured && !view.service_walker_display_stats().fallback_reason.empty() &&
            view.market_walker_display_stats().configured && view.fire_display_stats().animated &&
            observer::live==difference(after_inspector,service_tail) && observer::uploads==no_reuploads,
            "missing Service fallback discarded prepared Market/flames or reread removed files");
        pure_render(view);check(view.world().snapshot()==before,"resource lifecycle mutated complete World");cleanup(view);
    } while(std::next_permutation(order.begin(),order.end()));
}
void shared_and_aliases(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto market_path=market_profile(fixture),service_path=service_profile(fixture);
    View view(session(fixture),false,sim::RulesProfile::CityV12,1);start(view,fixture,window,renderer);
    view.set_fire_inspector_visuals(fixture.supplement);view.set_market_walker_visuals(market_path);
    const auto foreign=observer::live;const auto uploads=observer::uploads;
    const auto shared=fixture.root/"shared-service.json";Fixture::save(shared,service(true));
    view.set_service_walker_visuals(shared);
    check(view.service_walker_display_stats().configured && view.service_walker_display_stats().additional_assets==0 &&
        observer::live==foreign && observer::uploads==uploads && view.walker_texture_count()==16,
        "Service shared AssetIds created duplicate textures/uploads");
    view.set_service_walker_visuals({});check(observer::live==foreign && observer::uploads==uploads,
        "shared Service removal destroyed another role's physical texture");
    auto exact=service();auto& frames=exact["roles"]["service"]["frames"];
    const auto pattern=frames[0];
    // Core owns three aliases, Inspector eight, Market sixteen. Service229
    // therefore fills exactly256, independently of its eight shared sources.
    for(std::size_t i=8;i<229;++i) {auto extra=pattern;extra["alias"]="unused-"+std::to_string(i);frames.push_back(std::move(extra));}
    const auto exact_path=fixture.root/"exact-aliases.json";Fixture::save(exact_path,exact);
    view.set_service_walker_visuals(exact_path);
    check(view.service_walker_display_stats().configured && view.walker_texture_count()==24,
        "exact global256 aliases rejected");
    auto extra=pattern;extra["alias"]="overflow-alias";frames.push_back(extra);
    const auto overflow_path=fixture.root/"overflow-aliases.json";Fixture::save(overflow_path,exact);
    const auto before_overflow=observer::uploads;view.set_service_walker_visuals(overflow_path);
    check(!view.service_walker_display_stats().configured && view.fire_inspector_display_stats().configured &&
        view.market_walker_display_stats().configured && observer::live==foreign && observer::uploads==before_overflow,
        "global257 aliases partially activated Service or harmed foreign resources");
    view.set_service_walker_visuals(service_path);const auto before_fire=observer::live;
    const auto fire_path=fire_profile(fixture);view.set_fire_visuals(fire_path);
    const auto fire_tail=difference(observer::live,before_fire);check(fire_tail.size()==2,"custom override flame fixture count");
    view.set_walker_visuals(fixture.core,Source::Custom);
    check(!view.fire_inspector_display_stats().configured && !view.market_walker_display_stats().configured &&
        !view.service_walker_display_stats().configured && view.fire_display_stats().animated &&
        view.walker_texture_count()==1 && observer::live.size()==1+fire_tail.size(),
        "explicit custom core received hidden built-in supplements or discarded flames");
    retained(fire_tail,"explicit Custom core destroyed prepared effect texture pointers");
    const auto custom_live=observer::live;const auto custom_uploads=observer::uploads;
    view.set_service_walker_visuals(service_path);
    check(!view.service_walker_display_stats().configured && observer::live==custom_live && observer::uploads==custom_uploads,
        "explicit Custom core was automatically Service-supplemented");pure_render(view);cleanup(view);
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
    for(int pressure=0;pressure<4;++pressure) {
        Fixture fixture;const auto market_path=market_profile(fixture),service_path=service_profile(fixture);
        const auto effect=pressure==0 ? fire_profile(fixture):pressure==1 ?
            sized_fire(fixture,20,50,"medium-effect"):pressure==2 ?
            sized_fire(fixture,40,52,"large-effect"):sized_fire(fixture,50,52,"oversized-effect");
        View view(session(fixture,limit-18048-12480),false,sim::RulesProfile::CityV12,1);
        start(view,fixture,window,renderer);const auto roads=road_profile(fixture);view.set_road_visuals(roads);
        const auto prepared=append_all(view,fixture,market_path,service_path,{0,1,2});
        const auto exact=observer::live;const auto no_upload=observer::uploads;view.set_road_visuals(roads);
        check(observer::live==exact && observer::uploads==no_upload && view.service_walker_display_stats().configured,
            "exact zero headroom evicted Service or another valid optional role");
        if(pressure==3) {
            const auto creates=observer::creates,destroys=observer::destroys;
            view.set_fire_visuals(effect);
            check(!view.fire_display_stats().animated && observer::live==exact && observer::uploads==no_upload &&
                observer::creates==creates && observer::destroys==destroys &&
                view.service_walker_display_stats().configured && view.fire_inspector_display_stats().configured &&
                view.market_walker_display_stats().configured,"unfit Fire candidate silently raised budget or removed optional textures");
            pure_render(view);cleanup(view);continue;
        }
        for(const bool create_failure:{false,true}) {
            if(create_failure) observer::fail_create=observer::creates+1;
            else observer::fail_upload=observer::uploads+1;
            view.set_fire_visuals(effect);observer::fail_create=observer::fail_upload=0;
            check(observer::live==exact && view.service_walker_display_stats().configured &&
                view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
                !view.fire_display_stats().animated,"failed pressure Fire candidate evicted a prepared optional role");
        }
        for(const auto& archive:{"walker","market","service"})for(const auto& extension:{".sg3",".555"})
            fs::rename(fixture.data/"DATA"/(std::string(archive)+extension),
                       fixture.data/"DATA"/(std::string(archive)+"-unavailable"+extension));
        const auto order_begin=observer::destroyed_order.size();view.set_fire_visuals(effect);
        check(view.fire_display_stats().animated && !view.service_walker_display_stats().configured,
            "genuine Fire pressure did not release Service first");
        check(view.market_walker_display_stats().configured==(pressure==0) &&
            view.fire_inspector_display_stats().configured==(pressure<2),"Service then Market then Inspector priority changed");
        const auto expected_walkers=pressure==0 ? 16U:pressure==1 ? 8U:1U;
        check(view.walker_texture_count()==expected_walkers,"pressure retained wrong physical walker images");
        std::set<SDL_Texture*> expected_removed=prepared.tails[2];
        if(pressure>=1) expected_removed.insert(prepared.tails[1].begin(),prepared.tails[1].end());
        if(pressure>=2) expected_removed.insert(prepared.tails[0].begin(),prepared.tails[0].end());
        const std::set<SDL_Texture*> actually_removed(observer::destroyed_order.begin()+
            static_cast<std::ptrdiff_t>(order_begin),observer::destroyed_order.end());
        check(actually_removed==expected_removed,"pressure destroyed a surviving/core texture or retained an excluded source");
        if(pressure==0) {retained(prepared.tails[0],"small pressure lost Inspector");retained(prepared.tails[1],"small pressure lost Market");}
        if(pressure==1) retained(prepared.tails[0],"medium pressure lost Inspector");
        pure_render(view);cleanup(view);
    }
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        if(!metal) SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
        window=SDL_CreateWindow("Independent Service resource tests",1280,720,SDL_WINDOW_HIDDEN);
        check(window!=nullptr,"SDL hidden window");renderer=SDL_CreateRenderer(window,metal ? "metal":"software");
        check(renderer!=nullptr,"SDL renderer");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual backend mismatch");
        lifecycle(window,renderer);shared_and_aliases(window,renderer);budget(window,renderer);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS Service independent optional lifecycle: six append orders, exact budgets, physical sharing, foreign retention and shutdown\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';if(renderer) SDL_DestroyRenderer(renderer);if(window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
