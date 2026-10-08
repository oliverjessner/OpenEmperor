#include "assets/WalkerVisualProfile.h"
#include "FireInspectorWalkerFixture.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/TextureCompatibility.h"
#include "renderer/WalkerSpriteSet.h"
#include "simulation/World.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

// Observe the real SDL texture owner rather than a second renderer or fake
// result. Failure injection verifies that optional tails cannot discard core.
namespace observer {
std::size_t creates=0,uploads=0,destroys=0,draws=0;
std::size_t fail_create=0,fail_upload=0;
std::set<SDL_Texture*> live;
std::map<SDL_Texture*,std::size_t> per_texture_uploads;
SDL_Texture* create(SDL_Renderer* renderer,SDL_PixelFormat format,
                    SDL_TextureAccess access,int width,int height) {
    ++creates;
    if (creates==fail_create) { SDL_SetError("Injected optional creation failure");return nullptr; }
    auto* result=SDL_CreateTexture(renderer,format,access,width,height);
    if (result) live.insert(result);
    return result;
}
bool upload(SDL_Texture* texture,const SDL_Rect* rect,const void* pixels,int pitch) {
    ++uploads;
    if (uploads==fail_upload) return SDL_SetError("Injected optional upload failure");
    const bool okay=SDL_UpdateTexture(texture,rect,pixels,pitch);
    if (okay) ++per_texture_uploads[texture];
    return okay;
}
void destroy(SDL_Texture* texture) {
    ++destroys;live.erase(texture);per_texture_uploads.erase(texture);
    SDL_DestroyTexture(texture);
}
bool draw(SDL_Renderer* renderer,SDL_Texture* texture,const SDL_FRect* source,
          const SDL_FRect* destination) {
    ++draws;return SDL_RenderTexture(renderer,texture,source,destination);
}
}
#define SDL_CreateTexture observer::create
#define SDL_UpdateTexture observer::upload
#define SDL_DestroyTexture observer::destroy
#define SDL_RenderTexture observer::draw
#include "../src/renderer/WalkerSpriteSet.cpp"
#undef SDL_CreateTexture
#undef SDL_UpdateTexture
#undef SDL_DestroyTexture
#undef SDL_RenderTexture

namespace {
namespace fs=std::filesystem;
namespace assets=openemperor::assets;
namespace sim=openemperor::simulation;
namespace perf=openemperor::performance;
using Json=nlohmann::json;
using Bytes=std::vector<std::uint8_t>;
void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class F> void rejects(F action,const char* reason) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(reason);
}
using Fixture=openemperor::testing::inspector::Fixture;
using openemperor::testing::inspector::u32;
using openemperor::testing::inspector::write;
bool same_role(const assets::WalkerRoleVisual& a,const assets::WalkerRoleVisual& b) {
    if (a.ticks_per_frame!=b.ticks_per_frame || a.clip_id!=b.clip_id || a.evidence!=b.evidence ||
        a.clips!=b.clips || a.idle_frame!=b.idle_frame || a.frames.size()!=b.frames.size()) return false;
    for (std::size_t i=0;i<a.frames.size();++i) {
        const auto& x=a.frames[i];const auto& y=b.frames[i];
        if (x.alias!=y.alias || x.id!=y.id || x.foot_x!=y.foot_x || x.foot_y!=y.foot_y ||
            x.image_index!=y.image_index) return false;
    }
    return true;
}
bool same_profile(const assets::WalkerVisualProfile& a,const assets::WalkerVisualProfile& b) {
    if (a.schema_version!=b.schema_version || a.unique_images.size()!=b.unique_images.size()) return false;
    for (std::size_t i=0;i<a.roles.size();++i) {
        if (a.roles[i].has_value()!=b.roles[i].has_value()) return false;
        if (a.roles[i] && !same_role(*a.roles[i],*b.roles[i])) return false;
    }
    for (std::size_t i=0;i<a.unique_images.size();++i) {
        const auto& x=a.unique_images[i];const auto& y=b.unique_images[i];
        if (x.width!=y.width || x.height!=y.height || x.pixels!=y.pixels) return false;
    }
    return true;
}
assets::WalkerVisualProfile profile_checks(Fixture& fixture) {
    using assets::WalkerVisualRole;
    static_assert(assets::walker_role_index(WalkerVisualRole::Clay)==0);
    static_assert(assets::walker_role_index(WalkerVisualRole::Pottery)==1);
    static_assert(assets::walker_role_index(WalkerVisualRole::Household)==2);
    static_assert(assets::walker_role_index(WalkerVisualRole::FireInspector)==3);
    static_assert(assets::walker_visual_role_count==8 && assets::walker_schema5_role_count==7 && assets::walker_schema4_role_count==6 && assets::walker_schema3_role_count==4 &&
                  assets::walker_core_role_count==3);
    auto core=assets::load_walker_visual_profile(fixture.data,fixture.core);
    const auto original=core;
    check(core.schema_version==2 && core.unique_images.size()==1 &&
        !core.find(WalkerVisualRole::FireInspector),"legacy core gained an implicit Inspector");
    auto schema1=Fixture::legacy()["roles"]["clay"];
    schema1["schema_version"]=1;schema1["mode"]="curated_walker_preview";schema1["role"]="clay";
    Fixture::save(fixture.core,schema1);
    const auto one=assets::load_walker_visual_profile(fixture.data,fixture.core);
    check(one.schema_version==1 && same_role(*one.roles[0],*core.roles[0]) &&
        !one.roles[1] && !one.roles[2] && !one.roles[3],"schema1 changed its role contract");
    Fixture::save(fixture.core,Fixture::legacy());
    auto invalid=Fixture::inspector();invalid["schema_version"]=2;
    Fixture::save(fixture.supplement,invalid);
    rejects([&]{assets::load_walker_visual_profile(fixture.data,fixture.supplement);},
        "schema2 accepted the new role");
    invalid=Fixture::inspector();invalid["roles"]["trader"]=invalid["roles"]["fire_inspector"];
    Fixture::save(fixture.supplement,invalid);
    rejects([&]{assets::load_walker_visual_profile(fixture.data,fixture.supplement);},
        "schema3 ignored an unknown role");
    const auto failed_append=[&](Json value,const char* reason) {
        Fixture::save(fixture.supplement,value);
        rejects([&]{assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,core);},reason);
        check(same_profile(core,original),"failed optional profile damaged core pixels/roles/indices/schema");
    };
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["clips"].erase("neg_y");
    // Explicit custom schema3 remains partial, while automatic supplementation is complete only.
    Fixture::save(fixture.supplement,invalid);
    const auto partial=assets::load_walker_visual_profile(fixture.data,fixture.supplement);
    check(partial.find(WalkerVisualRole::FireInspector)->clips[3].empty(),"custom profile secretly filled direction");
    failed_append(invalid,"incomplete automatic Inspector activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["clips"]["pos_x"]={"d0-0","d0-0"};
    failed_append(invalid,"static Inspector sequence activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["frames"][7]["image_index"]=17;
    failed_append(invalid,"missing later Inspector frame activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["frames"][7]["archive"]="../escape.sg3";
    failed_append(invalid,"optional path traversal activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["idle"]="missing";
    failed_append(invalid,"optional missing idle activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"].erase("clip_id");
    failed_append(invalid,"Inspector without clip identity activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["clip_id"]=std::string(65,'x');
    failed_append(invalid,"Inspector clip identity exceeded limit");
    invalid=Fixture::inspector();invalid["roles"]["clay"]=Fixture::legacy()["roles"]["clay"];
    failed_append(invalid,"supplement replaced an existing core role");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["frames"][2]["alias"]="d0-0";
    failed_append(invalid,"Inspector duplicate alias activated");
    invalid=Fixture::inspector();invalid["roles"]["fire_inspector"]["clips"]["neg_x"]={"missing"};
    failed_append(invalid,"Inspector missing direction alias activated");
    const auto archive_path=fixture.data/"DATA/walker.sg3";
    auto mirrored=fixture.archive;u32(mirrored,40680+15*72+16,1);write(archive_path,mirrored);
    failed_append(Fixture::inspector(),"optional unsupported mirror activated");write(archive_path,fixture.archive);
    auto external=fixture.archive;external[40680+1*72+52]=1;write(archive_path,external);
    Fixture::save(fixture.supplement,Fixture::inspector());
    bool internal_rejected=false;
    try { assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,core); }
    catch (const std::exception& error) {
        internal_rejected=std::string_view(error.what()).find("internal bitmap dependencies")!=std::string_view::npos;
    }
    check(internal_rejected && same_profile(core,original),
        "automatic optional role consumed an independently unverified external bitmap");
    write(archive_path,fixture.archive);
    const auto bitmap_path=fixture.data/"DATA/walker.555";
    const auto outside=fixture.root/"outside.555";
    fs::rename(bitmap_path,outside);fs::create_symlink(outside,bitmap_path);
    failed_append(Fixture::inspector(),"optional bitmap symlink escape activated");
    fs::remove(bitmap_path);fs::rename(outside,bitmap_path);
    // Existing alias counts are included before decoding the optional role.
    auto bounded=core;
    for (std::size_t i=0;i<246;++i) {
        auto frame=bounded.roles[0]->frames[0];frame.alias="old-"+std::to_string(i);
        bounded.roles[0]->frames.push_back(std::move(frame));
    }
    Fixture::save(fixture.supplement,Fixture::inspector());
    rejects([&]{assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,bounded);},
        "optional aliases bypassed aggregate 256 budget");
    check(!bounded.roles[3] && bounded.unique_images.size()==1,"alias failure changed core");
    auto byte_bounded=core;
    auto& large=byte_bounded.unique_images[0];large.width=4096;large.height=4096;
    large.pixels.resize(static_cast<std::size_t>(assets::walker_max_rgba_bytes),255);
    check(assets::walker_rgba_bytes(byte_bounded)==assets::walker_max_rgba_bytes,"exact core RGBA budget");
    rejects([&]{assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,byte_bounded);},
        "optional RGBA bypassed aggregate 64MiB budget");
    check(!byte_bounded.roles[3] && byte_bounded.unique_images.size()==1,"RGBA failure changed core");
    assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,core);
    check(core.schema_version==3 && core.unique_images.size()==8 && core.roles[3]->frames.size()==8,
        "complete optional Inspector not prepared");
    for (std::size_t role=0;role<assets::walker_core_role_count;++role)
        check(same_role(*core.roles[role],*original.roles[role]),"successful append changed core role/anchor/cadence");
    check(core.roles[3]->frames[0].image_index==core.roles[2]->frames[0].image_index,
        "Inspector did not share existing physical Household image");
    check(assets::walker_rgba_bytes(core)==6016,"deduplicated logical RGBA total");
    const auto complete=core;
    rejects([&]{assets::append_fire_inspector_visual_profile(fixture.data,fixture.supplement,core);},
        "Inspector supplement replaced an already active role");
    check(same_profile(core,complete),"duplicate append changed profile");
    auto custom=Fixture::inspector();custom["roles"]["clay"]=Fixture::legacy()["roles"]["clay"];
    Fixture::save(fixture.supplement,custom);
    const auto explicit_profile=assets::load_walker_visual_profile(fixture.data,fixture.supplement);
    check(explicit_profile.unique_images.size()==8 && explicit_profile.roles[0] &&
        explicit_profile.roles[3] && !explicit_profile.roles[1] && !explicit_profile.roles[2],
        "schema3 custom load silently supplemented missing roles");
    Fixture::save(fixture.supplement,Fixture::inspector());
    return core;
}
void pose_checks(const assets::WalkerVisualProfile& profile) {
    using namespace openemperor;
    check(walker_visual_role(sim::CourierRole::FireInspector)==assets::WalkerVisualRole::FireInspector &&
        walker_visual_role(sim::CourierRole::HealthWorker)==assets::WalkerVisualRole::HealthWorker &&
        walker_visual_role(sim::CourierRole::Food)==assets::WalkerVisualRole::Supplier,
        "Inspector/legacy Food/Health role mapping changed");
    sim::CourierState courier;courier.id=static_cast<sim::CourierId>(901);
    courier.owner=static_cast<sim::BuildingId>(705);courier.target=static_cast<sim::BuildingId>(1024);
    courier.role=sim::CourierRole::FireInspector;courier.enabled=true;
    courier.phase=sim::CourierPhase::ToWarehouse;
    const auto& visual=*profile.roles[3];
    const std::array<sim::Cell,5> corner{{{2,2},{3,2},{3,3},{2,3},{2,2}}};
    constexpr std::array<std::size_t,4> directions{0,2,1,3};
    courier.path.assign(corner.begin(),corner.end());
    for (std::size_t edge=0;edge<4;++edge) {
        courier.path_vertex=edge;
        for (const auto phase:{sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning}) {
            courier.phase=phase;
            const auto first=walker_pose(courier,0,profile),second=walker_pose(courier,2,profile);
            check(first.role==assets::WalkerVisualRole::FireInspector && first.moving && !first.loaded &&
                first.direction==static_cast<assets::StorageDirection>(directions[edge]) &&
                first.frame==visual.clips[directions[edge]][0] && second.frame==visual.clips[directions[edge]][1],
                "Inspector direction/return/corner/tick pose disagrees with current edge");
            check(walker_pose(courier,4,profile).frame==first.frame,"Inspector clip loop did not wrap");
            const auto huge=walker_pose(courier,std::numeric_limits<std::uint64_t>::max(),profile);
            check(huge.frame==visual.clips[directions[edge]][1],"maximum tick overflowed frame arithmetic");
        }
        courier.route_pending=true;courier.edge_progress=3;
        check(walker_pose(courier,2,profile).moving,"Inspector stopped a protected begun edge");
        courier.edge_progress=0;
        const auto waiting=walker_pose(courier,2,profile);
        check(!waiting.moving && waiting.direction==static_cast<assets::StorageDirection>(directions[edge]) &&
            waiting.frame==visual.clips[directions[edge]][0] &&
            walker_pose(courier,1001,profile).frame==waiting.frame,"waiting Inspector walked in place/lost edge facing");
        courier.role=sim::CourierRole::Clay;
        check(!walker_pose(courier,2,*profile.roles[0]).direction &&
            walker_pose(courier,1001,*profile.roles[0]).frame==profile.roles[0]->idle_frame,
            "new Inspector waiting pose changed legacy Clay");
        courier.role=sim::CourierRole::FireInspector;courier.route_pending=false;
    }
    courier.phase=sim::CourierPhase::IdleAtWorkshop;
    check(!walker_pose(courier,7,profile).moving && !walker_pose(courier,7,profile).direction &&
        walker_pose(courier,7,profile).frame==visual.idle_frame,"idle Inspector animated stale path");
    courier.phase=sim::CourierPhase::Returning;courier.path={{3,3}};courier.path_vertex=0;
    check(!walker_pose(courier,7,profile).moving && walker_pose(courier,7,profile).frame==visual.idle_frame,
        "one-point interrupted Inspector route animated");
    courier.path={{3,3},{4,4}};
    check(walker_pose(courier,7,profile).fallback==WalkerFallback::InvalidEdge &&
        !walker_pose(courier,7,profile).frame,"diagonal Inspector edge fabricated movement");
    courier.path={{std::numeric_limits<int>::max(),0},{std::numeric_limits<int>::min(),0}};
    check(walker_pose(courier,7,profile).fallback==WalkerFallback::InvalidEdge,
        "extreme coordinate edge overflowed direction test");
    courier.path={{3,3},{4,3}};
    auto partial=profile;partial.roles[3]->clips[0].clear();
    check(walker_pose(courier,7,partial).fallback==WalkerFallback::UnmappedDirection &&
        !walker_pose(courier,7,partial).frame,"unmapped custom Inspector direction hid fallback");
}
void live_visibility_checks(const assets::WalkerVisualProfile& profile) {
    using namespace openemperor;
    sim::CourierState courier;courier.role=sim::CourierRole::FireInspector;
    courier.enabled=true;courier.phase=sim::CourierPhase::IdleAtWorkshop;
    check(!walker_live_visible(courier) && walker_pose(courier,31,profile).frame==profile.roles[3]->idle_frame,
        "Idle Inspector escaped Watch or lost its separate diagnostic idle frame");
    for (const auto phase:{sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning}) {
        courier.phase=phase;
        for (const bool pending:{false,true}) {
            courier.route_pending=pending;courier.edge_progress=0;courier.path={{3,3}};
            check(walker_live_visible(courier) && !walker_pose(courier,31,profile).moving,
                "active one-cell/static Inspector confused with home Idle");
            courier.path={{3,3},{4,3}};courier.edge_progress=1;
            check(walker_live_visible(courier) && walker_pose(courier,31,profile).moving,
                "begun outgoing/return edge hidden by presentation");
            courier.enabled=false;
            check(!walker_live_visible(courier),"disabled Inspector remained live visible");
            courier.enabled=true;
        }
    }
    constexpr std::array old_roles{sim::CourierRole::Clay,sim::CourierRole::Pottery,
        sim::CourierRole::Household,sim::CourierRole::Food,
        sim::CourierRole::None};
    for (const auto role:old_roles) for (const bool enabled:{false,true})
        for (const auto phase:{sim::CourierPhase::IdleAtWorkshop,sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning}) {
            courier.role=role;courier.enabled=enabled;courier.phase=phase;
            check(walker_live_visible(courier),"Inspector-only visibility filter changed an older role");
        }
}
using Color=std::array<std::uint8_t,4>;
Color pixel(SDL_Renderer* renderer,int x,int y) {
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface!=nullptr,"renderer readback");
    Color result{};const bool okay=SDL_ReadSurfacePixel(surface,x,y,&result[0],&result[1],&result[2],&result[3]);
    SDL_DestroySurface(surface);check(okay,"pixel read");return result;
}
std::uint64_t frame_hash(SDL_Renderer* renderer) {
    auto* surface=SDL_RenderReadPixels(renderer,nullptr);check(surface!=nullptr,"frame readback");
    std::uint64_t hash=1469598103934665603ULL;
    for (int y=0;y<surface->h;++y) for (int x=0;x<surface->w;++x) {
        Color color{};check(SDL_ReadSurfacePixel(surface,x,y,&color[0],&color[1],&color[2],&color[3]),"hash pixel");
        for (const auto channel:color) { hash^=channel;hash*=1099511628211ULL; }
    }
    SDL_DestroySurface(surface);return hash;
}
void zero_counters(const char* reason) {
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,reason);
}
void render_checks(Fixture& fixture,const assets::WalkerVisualProfile& merged,bool metal) {
    using namespace openemperor;
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal ? "cocoa":"dummy") &&
        SDL_SetHint(SDL_HINT_RENDER_DRIVER,metal ? "metal":"software") && SDL_Init(SDL_INIT_VIDEO),"SDL init");
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    check(SDL_CreateWindowAndRenderer("Inspector synthetic pixels",256,256,SDL_WINDOW_HIDDEN,&window,&renderer),
        "renderer creation");
    check(std::string_view(SDL_GetRendererName(renderer))==(metal ? "metal":"software"),"actual backend mismatch");
    auto core=assets::load_walker_visual_profile(fixture.data,fixture.core);
    WalkerSpriteSet sprites;
    perf::set_enabled(true);perf::reset();
    sprites.initialize(renderer,core);
    const auto core_textures=observer::live;
    check(sprites.texture_count()==1 && sprites.logical_bytes()==640 &&
        perf::counter(perf::Counter::TextureUploads)==1,"core eager resource count");
    const auto creates=observer::creates,uploads=observer::uploads;
    rejects([&]{sprites.append(merged,assets::walker_rgba_bytes(merged)-1);},"remaining total budget bypassed");
    check(observer::creates==creates && observer::uploads==uploads && observer::live==core_textures &&
        sprites.texture_count()==1 && sprites.logical_bytes()==640,"budget failure touched active core textures");
    observer::fail_create=observer::creates+2;
    rejects([&]{sprites.append(merged);},"optional texture creation failure ignored");
    observer::fail_create=0;
    check(observer::live==core_textures && WalkerSpriteSet::live_texture_count()==1 && sprites.logical_bytes()==640,
        "creation failure leaked tail or destroyed core");
    observer::fail_upload=observer::uploads+2;
    rejects([&]{sprites.append(merged);},"optional texture upload failure ignored");
    observer::fail_upload=0;
    check(observer::live==core_textures && sprites.texture_count()==1 && sprites.logical_bytes()==640,
        "upload failure leaked tail or destroyed core");
    const auto core_texture=*core_textures.begin();
    check(observer::per_texture_uploads.at(core_texture)==1,"failure reuploaded core image");
    perf::reset();sprites.append(merged);
    check(sprites.texture_count()==8 && sprites.logical_bytes()==6016 &&
        perf::counter(perf::Counter::TextureUploads)==7 && observer::per_texture_uploads.at(core_texture)==1,
        "optional append did not share physical core asset or uploaded core twice");
    for (auto* texture:observer::live) {
        check(observer::per_texture_uploads.at(texture)==1,"active physical texture has multiple uploads");
        const auto properties=SDL_GetTextureProperties(texture);
        const auto expected=texture_compatibility::eager_rgba_access(renderer);
        check(SDL_GetNumberProperty(properties,SDL_PROP_TEXTURE_ACCESS_NUMBER,-1)==expected,
            "Inspector ignored shared runtime/backend texture access");
        SDL_BlendMode blend{};SDL_ScaleMode scale{};std::uint8_t red=0,green=0,blue=0,alpha=0;
        check(SDL_GetTextureBlendMode(texture,&blend) && blend==SDL_BLENDMODE_BLEND &&
            SDL_GetTextureScaleMode(texture,&scale) && scale==SDL_SCALEMODE_NEAREST &&
            SDL_GetTextureColorMod(texture,&red,&green,&blue) && red==255 && green==255 && blue==255 &&
            SDL_GetTextureAlphaMod(texture,&alpha) && alpha==255,"texture sampling/blend/modulation contract");
    }
    const auto& visual=*merged.roles[3];
    const auto clear=[&] { check(SDL_SetRenderDrawColor(renderer,20,40,60,255) && SDL_RenderClear(renderer),"clear"); };
    constexpr std::array<Color,4> colors{{{0,0,255,255},{0,255,0,255},{255,255,0,255},{0,255,255,255}}};
    fs::rename(fixture.data,fixture.root/"unavailable-assets");
    perf::reset();
    for (std::size_t direction=0;direction<4;++direction) {
        std::uint64_t first_hash=0;
        for (std::size_t phase=0;phase<2;++phase) {
            clear();check(sprites.draw(visual.clips[direction][phase],{80,100},1,visual,merged,{0,0},{256,256}),"frame draw");
            check(pixel(renderer,80,91)==colors[direction],"direction selected wrong texture/body position");
            check(pixel(renderer,80,98)==Color{255,255,255,255},"variable canvas shifted foot contact");
            check(pixel(renderer,76,91)==(phase ? Color{20,40,60,255}:colors[direction]),
                "first gait silhouette did not actually differ");
            check(pixel(renderer,84,91)==(phase ? colors[direction]:Color{20,40,60,255}),
                "second gait silhouette did not actually differ");
            check(pixel(renderer,75,85)==Color{20,40,60,255},"transparent sprite rectangle overwrote background");
            const auto shadow=pixel(renderer,84,96);
            check(std::abs(shadow[0]-10)<=1 && std::abs(shadow[1]-20)<=1 && std::abs(shadow[2]-30)<=1,
                "metadata-validated Omega shadow composition changed");
            const auto hash=frame_hash(renderer);
            if (!phase) first_hash=hash;else check(hash!=first_hash,"frame-index change did not change actual pixels");
        }
        std::uint64_t paused=0;
        for (const double zoom:{1.0,1.125,2.0,4.0,1.0}) {
            clear();check(sprites.draw(visual.clips[direction][0],{80,100},zoom,visual,merged,{0,0},{256,256}),"zoom draw");
            check(pixel(renderer,80,static_cast<int>(100-8*zoom))==colors[direction],"zoom lost Inspector torso");
            check(pixel(renderer,80,static_cast<int>(100-2*zoom))==Color{255,255,255,255},"zoom lost anchored foot");
            const auto hash=frame_hash(renderer);
            if (zoom==1.0) { if (!paused) paused=hash;else check(hash==paused,"zoom-back pixels changed at same pose"); }
        }
        for (int repeat=0;repeat<8;++repeat) {
            clear();check(sprites.draw(visual.clips[direction][0],{80,100},1,visual,merged,{0,0},{256,256}),"paused draw");
            check(frame_hash(renderer)==paused,"render frequency advanced paused Inspector pixels");
        }
    }
    clear();
    const auto before_draws=observer::draws;
    for (int instance=0;instance<20;++instance)
        check(sprites.draw(visual.clips[0][1],{20.0+instance*10.0,180},1,visual,merged,{0,0},{256,256}),"shared instance draw");
    check(observer::draws-before_draws==20 && sprites.texture_count()==8,"instances did not share prepared textures");
    clear();check(sprites.draw(visual.clips[0][0],{80,260},1,visual,merged,{0,0},{256,256}),"offscreen-foot draw");
    check(pixel(renderer,80,251)==colors[0],"entering body culled with offscreen foot");
    const auto outside_draws=observer::draws;
    check(sprites.draw(visual.clips[0][0],{80,300},1,visual,merged,{0,0},{256,256}) &&
        observer::draws==outside_draws,"wholly offscreen sprite submitted");
    zero_counters("prepared Inspector rendering performed IO/decode/upload/World/navigation work");
    fs::rename(fixture.root/"unavailable-assets",fixture.data);
    const auto uploads_before_truncate=observer::uploads;
    sprites.truncate(1);
    check(sprites.texture_count()==1 && sprites.logical_bytes()==640 && observer::live==core_textures &&
        observer::uploads==uploads_before_truncate && observer::per_texture_uploads.at(core_texture)==1,
        "optional eviction recreated core or retained tail bytes/textures");
    rejects([&]{sprites.truncate(2);},"truncate grew active texture count");
    clear();check(sprites.draw(0,{80,100},1,*core.roles[0],core,{0,0},{256,256}),"retained core draw");
    check(pixel(renderer,80,91)==colors[0],"optional eviction changed retained core pixels");
    sprites.append(merged);
    check(sprites.texture_count()==8 && sprites.logical_bytes()==6016 &&
        observer::per_texture_uploads.at(core_texture)==1,"reactivation reuploaded shared core");
    perf::set_enabled(false);sprites.shutdown();
    check(sprites.texture_count()==0 && sprites.logical_bytes()==0 && WalkerSpriteSet::live_texture_count()==0 &&
        observer::live.empty(),"shared optional textures leaked after shutdown");
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}

constexpr int extent=24;
std::shared_ptr<const sim::MapPermissions> gate_permissions(bool horizontal) {
    const auto axis=[&](sim::Cell cell) { return horizontal ? sim::Cell{cell.y,cell.x}:cell; };
    sim::FixedGatePassage gate;gate.id={101};
    std::vector<sim::MapCellPermission> cells(extent*extent,{true,true,false,0,
        sim::BuildBlocker::None,sim::BuildBlocker::None});
    const auto index=[](sim::Cell cell) { return static_cast<std::size_t>(cell.y*extent+cell.x); };
    for (int y=7;y<=9;++y) for (int x=5;x<=9;++x) {
        const auto cell=axis({x,y});gate.protected_footprint.push_back(cell);
        cells[index(cell)]={false,false,true,0,sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::OriginalStructure};
    }
    for (int y=7;y<=9;++y) { const auto cell=axis({7,y});gate.corridor.push_back(cell);cells[index(cell)].road_allowed=true; }
    gate.openings={axis({7,6}),axis({7,10})};
    for (int x=0;x<extent;++x) if (x<5 || x>9)
        cells[index(axis({x,8}))]={false,false,true,0,sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    return std::make_shared<const sim::MapPermissions>(extent,extent,1,std::move(cells),
        std::vector<sim::FixedGatePassage>{std::move(gate)});
}
sim::BuildingId put(sim::World& world,sim::CommandType type,sim::Cell cell) {
    const auto result=world.execute({type,cell});check(result.accepted && result.changed,"paid fixture placement");
    return *world.building_owner_at(cell);
}
sim::CourierId inspector_id(const sim::World& world,sim::BuildingId owner) {
    for (const auto& courier:world.couriers()) if (courier.owner==owner && courier.role==sim::CourierRole::FireInspector)
        return courier.id;
    throw std::runtime_error("missing existing Inspector");
}
void actual_gate_neutrality(const assets::WalkerVisualProfile& profile,bool horizontal) {
    using namespace openemperor;
    const auto permissions=gate_permissions(horizontal);
    const auto axis=[&](sim::Cell cell) { return horizontal ? sim::Cell{cell.y,cell.x}:cell; };
    sim::World world(permissions);
    const auto target=put(world,sim::CommandType::PlaceClaySource,axis({7,12}));
    for (const auto cell:{sim::Cell{0,0},sim::Cell{3,0},sim::Cell{10,0},sim::Cell{13,0}})
        put(world,sim::CommandType::PlaceHousehold,axis(cell));
    const auto watch=put(world,sim::CommandType::PlaceFireWatch,axis({4,4}));
    check(world.execute(sim::set_building_operation(watch,false)).accepted,"initial Watch pause");
    std::vector<sim::Cell> roads;
    for (const auto cell:{sim::Cell{4,5},sim::Cell{5,5},sim::Cell{6,5},sim::Cell{7,5},sim::Cell{7,6},
        sim::Cell{7,7},sim::Cell{7,8},sim::Cell{7,9},sim::Cell{7,10},sim::Cell{7,11}}) roads.push_back(axis(cell));
    check(world.execute_road_batch(roads).accepted,"paid gate road connection");
    const auto id=inspector_id(world,watch);
    auto plain=sim::World::restore(world.snapshot(),permissions);
    std::array<bool,4> directions{};
    bool gate_seen=false,begun_seen=false,waiting_seen=false,arrival_seen=false,returning_seen=false,returned=false;
    std::uint64_t dispatch_tick=0,arrival_tick=0;
    const auto step=[&] {
        plain.tick();world.tick();
        check(world.snapshot()==plain.snapshot(),"visual queries changed complete policy-bound simulation snapshot");
        const auto& courier=world.courier(id);
        const auto snapshot=world.snapshot();const auto position=world.courier_position(id);
        perf::set_enabled(true);perf::reset();
        for (int repeat=0;repeat<5;++repeat) {
            const auto pose=walker_pose(courier,world.ticks(),profile);
            check(walker_live_visible(courier)==(courier.phase!=sim::CourierPhase::IdleAtWorkshop),
                "real gate Inspector visibility does not follow authoritative trip phase");
            check(pose.role==assets::WalkerVisualRole::FireInspector && pose.frame && !pose.loaded,
                "actual Inspector role/pose became marker or cargo");
            if (pose.moving) {
                directions[assets::direction_index(*pose.direction)]=true;
                if (courier.path_vertex<courier.path.size() && world.fixed_passage(courier.path[courier.path_vertex])) gate_seen=true;
            }
        }
        zero_counters("actual Inspector pose performed World/navigation/asset work");perf::set_enabled(false);
        check(world.snapshot()==snapshot && world.courier_position(id)==position,"pose moved the actual courier");
        check(world.map_permissions()==permissions && plain.map_permissions()==permissions &&
            world.navigation_valid() && world.fire_state_valid() && world.city_economy_valid(),"gate policy/authority lost");
    };
    while (world.ticks()<2000) step();
    check(world.building_on_fire(target) && world.courier(id).phase==sim::CourierPhase::IdleAtWorkshop,
        "natural incident/pause fixture did not reach tick2000");
    check(world.execute(sim::set_building_operation(watch,true)).accepted &&
        plain.execute(sim::set_building_operation(watch,true)).accepted,"normal Watch resume");
    step();dispatch_tick=world.ticks();
    check(world.building_staffed(watch) && world.courier(id).phase==sim::CourierPhase::ToWarehouse &&
        world.courier(id).target==target && world.building_on_fire(target),"dispatch staffing/target/fire guard");
    int budget=200;
    while (budget-- && !(world.courier(id).path[world.courier(id).path_vertex]==axis({7,7}) &&
        world.courier(id).edge_progress==1)) step();
    check(budget>=0,"Inspector did not physically enter the fixed corridor");
    const auto cut=axis({7,10});const auto before=world.courier_position(id);
    check(world.execute({sim::CommandType::RemoveRoad,cut}).accepted &&
        plain.execute({sim::CommandType::RemoveRoad,cut}).accepted,"future road cut failed");
    check(world.courier_position(id)==before && world.courier(id).route_pending &&
        walker_pose(world.courier(id),world.ticks(),profile).moving,"cut interrupted begun Inspector edge");
    begun_seen=true;
    budget=30;
    while (budget-- && !(world.courier(id).route_pending && world.courier(id).edge_progress==0)) step();
    check(budget>=0 && world.fixed_passage(world.courier(id).path[world.courier(id).path_vertex]),
        "Inspector did not wait inside fixed corridor");
    const auto waiting_frame=walker_pose(world.courier(id),world.ticks(),profile).frame;
    const auto waiting_position=world.courier_position(id);waiting_seen=true;
    const auto saved=world.snapshot();auto restored=sim::World::restore(saved,permissions);
    check(restored.snapshot()==saved && restored.map_permissions()==permissions &&
        walker_pose(restored.courier(id),restored.ticks(),profile).frame==waiting_frame &&
        walker_live_visible(restored.courier(id)),
        "waiting restore lost full permissions or directional static pose");
    for (int pause=0;pause<25;++pause) {
        step();restored.tick();
        check(world.snapshot()==restored.snapshot() && world.courier_position(id)==waiting_position &&
            !walker_pose(world.courier(id),world.ticks(),profile).moving &&
            walker_pose(world.courier(id),world.ticks(),profile).frame==waiting_frame &&
            world.building_on_fire(target),"waiting Inspector moved/animated/extinguished or restore diverged");
    }
    check(world.execute({sim::CommandType::PlaceRoad,cut}).accepted &&
        plain.execute({sim::CommandType::PlaceRoad,cut}).accepted,"ordinary gate road repair");
    budget=200;
    while (budget-- && world.building_on_fire(target)) {
        const auto checkpoint=world.snapshot();auto resumed=sim::World::restore(checkpoint,permissions);
        check(resumed.snapshot()==checkpoint && resumed.map_permissions()==permissions &&
            resumed.courier_position(id)==world.courier_position(id) &&
            walker_pose(resumed.courier(id),resumed.ticks(),profile).frame==walker_pose(world.courier(id),world.ticks(),profile).frame,
            "moving save/restore changed path/position/frame");
        step();resumed.tick();check(resumed.snapshot()==world.snapshot(),"moving restored continuation diverged");
    }
    check(budget>=0 && !world.building_on_fire(target) &&
        world.courier(id).phase==sim::CourierPhase::Returning && world.building_fire_protected(target) &&
        world.building(target).fire_until_tick==world.ticks() &&
        world.building(target).fire_protection_until_tick==world.ticks()+2400,"actual arrival did not alone clear fire/protect/start return");
    arrival_tick=world.ticks();arrival_seen=true;returning_seen=true;
    check(world.execute(sim::set_building_operation(watch,false)).accepted &&
        plain.execute(sim::set_building_operation(watch,false)).accepted,"pause new visits after actual arrival");
    budget=200;
    while (budget-- && world.courier(id).phase!=sim::CourierPhase::IdleAtWorkshop) step();
    returned=world.courier(id).phase==sim::CourierPhase::IdleAtWorkshop;
    const auto idle_checkpoint=world.snapshot();const auto idle_restored=sim::World::restore(idle_checkpoint,permissions);
    check(!walker_live_visible(world.courier(id)) && !walker_live_visible(idle_restored.courier(id)) &&
        idle_restored.snapshot()==idle_checkpoint && idle_restored.map_permissions()==permissions,
        "completed return/Idle restore changed visibility or complete gate permissions");
    check(gate_seen && begun_seen && waiting_seen && arrival_seen && returning_seen && returned &&
        std::all_of(directions.begin(),directions.end(),[](bool seen){return seen;}),
        "actual Inspector missed gate, begun edge, wait, arrival, return or storage direction");
    check(world.execute(sim::set_building_operation(watch,true)).accepted &&
        plain.execute(sim::set_building_operation(watch,true)).accepted,"resume ordinary protected patrol");
    step();check(world.courier(id).phase==sim::CourierPhase::ToWarehouse && !world.building_on_fire(target),
        "Inspector did not start next normal patrol");
    std::cout<<(horizontal ? "horizontal":"vertical")<<" Inspector dispatch "<<dispatch_tick
             <<", actual arrival "<<arrival_tick<<", real gate/wait/restore/return neutral\n";
    // No ordinary population means no fabricated Inspector departure.
    sim::World unstaffed(permissions);
    put(unstaffed,sim::CommandType::PlaceClaySource,axis({7,12}));
    const auto empty_watch=put(unstaffed,sim::CommandType::PlaceFireWatch,axis({4,4}));
    check(unstaffed.execute_road_batch(roads).accepted,"unstaffed road setup");
    const auto empty_id=inspector_id(unstaffed,empty_watch);
    for (int tick=0;tick<2050;++tick) {
        unstaffed.tick();const auto stable=unstaffed.snapshot();
        const auto pose=walker_pose(unstaffed.courier(empty_id),unstaffed.ticks(),profile);
        check(!walker_live_visible(unstaffed.courier(empty_id)) && !pose.moving && pose.frame==profile.roles[3]->idle_frame &&
            unstaffed.courier(empty_id).phase==sim::CourierPhase::IdleAtWorkshop &&
            !unstaffed.building_staffed(empty_watch) && unstaffed.snapshot()==stable,"unstaffed sprite invented departure/workforce");
    }
}
}
int main(int argc,char* argv[]) {
    try {
        const bool metal=argc==2 && std::string_view(argv[1])=="metal";
        check(argc==1 || metal,"usage: inspector-tests [metal]");
        Fixture fixture;const auto profile=profile_checks(fixture);pose_checks(profile);live_visibility_checks(profile);
        render_checks(fixture,profile,metal);
        actual_gate_neutrality(profile,false);actual_gate_neutrality(profile,true);
        std::cout<<"Inspector schema/atomic supplementation/directions/pixels/texture lifetime/real gated fire patrol passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
