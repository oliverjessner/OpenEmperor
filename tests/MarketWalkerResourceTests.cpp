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
    ++destroys;live.erase(texture);SDL_DestroyTexture(texture);
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
std::set<SDL_Texture*> both(View& view,const Fixture& fixture,
        const fs::path& market_path,bool market_first) {
    std::set<SDL_Texture*> market_textures;
    const auto append_market=[&] {
        const auto before=observer::live;
        view.set_market_walker_visuals(market_path);
        std::set_difference(observer::live.begin(),observer::live.end(),before.begin(),before.end(),
                            std::inserter(market_textures,market_textures.end()));
    };
    if (market_first) append_market();
    view.set_fire_inspector_visuals(fixture.supplement);
    if (!market_first) append_market();
    check(view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
          view.walker_texture_count()==16 && observed_bytes()==12032,"independent append order/dedupe mismatch");
    check(market_textures.size()==8,"independent Market texture identity");
    return market_textures;
}
void lifecycle(SDL_Window* window,SDL_Renderer* renderer) {
    for (const bool market_first:{false,true}) {
        Fixture fixture;const auto market_path=market_profile(fixture),fire_path=fire_profile(fixture);
        View view(session(fixture),false,sim::RulesProfile::CityV12,1);start(view,fixture,window,renderer);
        const auto market_textures=both(view,fixture,market_path,market_first);
        const auto before=view.world().snapshot();const auto live=observer::live;
        const auto uploads=observer::uploads;
        view.set_fire_inspector_visuals({});
        check(!view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
              view.walker_texture_count()==9 && observed_bytes()==6656 && observer::uploads==uploads,
              "Inspector removal discarded/reuploaded Market or shared core");
        check(std::includes(live.begin(),live.end(),observer::live.begin(),observer::live.end()),"removal recreated foreign textures");
        check(std::includes(observer::live.begin(),observer::live.end(),market_textures.begin(),market_textures.end()),
              "Inspector removal destroyed a prepared Market texture");
        view.set_fire_inspector_visuals(fixture.supplement);
        const auto reuploads=observer::uploads;
        view.set_market_walker_visuals({});
        check(view.fire_inspector_display_stats().configured && !view.market_walker_display_stats().configured &&
              view.walker_texture_count()==8 && observer::uploads==reuploads,"Market removal discarded/reuploaded Inspector");
        const auto inspector_live=observer::live;
        for (const bool create_failure:{false,true}) {
            if (create_failure) observer::fail_create=observer::creates+2;
            else observer::fail_upload=observer::uploads+2;
            view.set_market_walker_visuals(market_path);
            observer::fail_create=observer::fail_upload=0;
            check(observer::live==inspector_live && view.fire_inspector_display_stats().configured &&
                  !view.market_walker_display_stats().configured,"failed Market upload harmed foreign textures");
        }
        view.set_market_walker_visuals(market_path);const auto all_live=observer::live;
        fs::rename(fixture.data/"DATA/walker.sg3",fixture.data/"DATA/walker-unavailable.sg3");
        fs::rename(fixture.data/"DATA/walker.555",fixture.data/"DATA/walker-unavailable.555");
        fs::rename(fixture.data/"DATA/market.sg3",fixture.data/"DATA/market-unavailable.sg3");
        fs::rename(fixture.data/"DATA/market.555",fixture.data/"DATA/market-unavailable.555");
        auto invalid=Json{{"schema_version",1},{"mode","curated_fire_presentation"}};
        const auto bad=fixture.root/"invalid-fire.json";Fixture::save(bad,invalid);
        const auto previous_uploads=observer::uploads,previous_destroys=observer::destroys;
        for (const auto& failed_path:{bad,fixture.root/"missing-fire.json"}) {
            view.set_fire_visuals(failed_path);
            check(observer::live==all_live && observer::uploads==previous_uploads && observer::destroys==previous_destroys &&
                  view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured,
                  "invalid/missing fire reread or discarded prepared foreign supplements");
        }
        for (const bool create_failure:{false,true}) {
            if (create_failure) observer::fail_create=observer::creates+1;
            else observer::fail_upload=observer::uploads+1;
            view.set_fire_visuals(fire_path);observer::fail_create=observer::fail_upload=0;
            check(observer::live==all_live && view.fire_inspector_display_stats().configured &&
                  view.market_walker_display_stats().configured && !view.fire_display_stats().animated,
                  "failed fire candidate destroyed prepared foreign textures");
        }
        pure_render(view);view.set_fire_visuals(fire_path);
        check(view.fire_display_stats().animated && view.walker_texture_count()==16 &&
              view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured,
              "valid fire replacement unnecessarily reloaded missing Walker sources");
        check(view.world().snapshot()==before,"lifecycle operations mutated full World");
        pure_render(view);view.shutdown();
        check(observer::live.empty() && openemperor::WalkerSpriteSet::live_texture_count()==0 &&
              openemperor::FireSpriteSet::live_texture_count()==0,"session shutdown leaked optional textures");
    }
}
void budget(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture fixture;const auto market_path=market_profile(fixture),fire_path=fire_profile(fixture);
    constexpr std::uint64_t limit=64U*1024U*1024U;
    View view(session(fixture,limit-12032-12480),false,sim::RulesProfile::CityV12,1);
    start(view,fixture,window,renderer);const auto roads=road_profile(fixture);view.set_road_visuals(roads);
    both(view,fixture,market_path,false);
    const auto uploads=observer::uploads;const auto live=observer::live;view.set_road_visuals(roads);
    check(view.fire_inspector_display_stats().configured && view.market_walker_display_stats().configured &&
          observer::live==live && observer::uploads==uploads,"legal exact-zero headroom evicted optional walkers");
    fs::rename(fixture.data/"DATA/walker.sg3",fixture.data/"DATA/walker-unavailable.sg3");
    fs::rename(fixture.data/"DATA/walker.555",fixture.data/"DATA/walker-unavailable.555");
    view.set_fire_visuals(fire_path);
    check(view.fire_display_stats().animated && view.fire_inspector_display_stats().configured &&
          !view.market_walker_display_stats().configured && view.walker_texture_count()==8,
          "real pressure failed Market-first eviction or reloaded surviving Inspector");
    pure_render(view);view.shutdown();check(observer::live.empty(),"budget session leaked textures");

    // A historically permitted schema2 over-budget core cannot inflate the
    // replacement allowance by adding its bytes to a saturated-zero remainder.
    Bytes archive(40680+2*72,0),bitmap{0,0,0,0};
    u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,2);u32(archive,16,2);u32(archive,20,1);
    const std::string group="authored-large.bmp";std::copy(group.begin(),group.end(),archive.begin()+680);u32(archive,804,2);
    const auto at=40680+72;u32(archive,at,4);u32(archive,at+4,192);
    u16(archive,at+20,32);u16(archive,at+22,96);u16(archive,at+50,256);
    for (int i=0;i<96;++i) bitmap.insert(bitmap.end(),{255,32});
    write(fixture.data/"DATA/large.sg3",archive);write(fixture.data/"DATA/large.555",bitmap);
    auto legacy=Fixture::legacy();
    for (auto& role:legacy["roles"]) role["frames"]=Json::array({{{"alias","d0-0"},
        {"archive","DATA/large.sg3"},{"image_index",1},{"foot_anchor",{16,95}}}});
    Fixture::save(fixture.core,legacy);
    View old(session(fixture,limit-12032-12480),false,sim::RulesProfile::CityV12,1);
    start(old,fixture,window,renderer);old.set_road_visuals(roads);
    check(observed_bytes()==12288,"authored legacy overflow size");
    legacy["schema_version"]=4;const auto replacement=fixture.root/"replacement.json";Fixture::save(replacement,legacy);
    const auto previous=observer::live;
    rejects([&]{old.set_walker_visuals(replacement);},"saturated remainder admitted oversized schema4 replacement");
    check(observer::live==previous && old.walker_display_stats().schema_version==2,
          "rejected replacement discarded historical active core");
    old.shutdown();check(observer::live.empty(),"legacy replacement session leaked textures");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        if (!metal) SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        check(SDL_Init(SDL_INIT_VIDEO),"SDL initialization");
        window=SDL_CreateWindow("Optional Walker resource tests",1280,720,SDL_WINDOW_HIDDEN);
        check(window!=nullptr,"SDL hidden window");renderer=SDL_CreateRenderer(window,metal ? "metal":"software");
        check(renderer!=nullptr,"SDL renderer");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual backend mismatch");
        lifecycle(window,renderer);budget(window,renderer);
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS independent optional lifecycle, foreign retention, exact session budgets and shutdown\n";return 0;
    } catch (const std::exception& error) {
        std::cerr<<error.what()<<'\n';
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();return 1;
    }
}
