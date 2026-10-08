#include "app/WalkerPose.h"
#include "assets/WalkerVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include "renderer/WalkerSpriteSet.h"
#include "FireInspectorWalkerFixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>

// Own independently authored native pixels. No original sprite, map or save
// bytes are retained. These are profile/pose/direct-target checks, not a second
// renderer or an ordinary Application/native-human acceptance claim.
namespace {
namespace oe=openemperor;
namespace assets=oe::assets;
namespace sim=oe::simulation;
namespace perf=oe::performance;
namespace authored=oe::testing::inspector;
namespace fs=std::filesystem;
using Json=nlohmann::json;
using Bytes=authored::Bytes;
using Role=assets::WalkerVisualRole;
using Color=std::array<std::uint8_t,4>;
using authored::check;
using authored::u16;
using authored::u32;
using authored::write;
constexpr std::array<const char*,4> directions{"pos_x","neg_x","pos_y","neg_y"};
template<class F> void rejects(F action,const char* reason) {
    try {action();}catch(const std::exception&){return;}
    throw std::runtime_error(reason);
}
bool same_role(const assets::WalkerRoleVisual& a,const assets::WalkerRoleVisual& b) {
    if(a.ticks_per_frame!=b.ticks_per_frame||a.evidence!=b.evidence||a.clip_id!=b.clip_id||
       a.clips!=b.clips||a.idle_frame!=b.idle_frame||a.frames.size()!=b.frames.size())return false;
    for(std::size_t i=0;i<a.frames.size();++i) {
        const auto& x=a.frames[i];const auto& y=b.frames[i];
        if(x.alias!=y.alias||x.id!=y.id||x.foot_x!=y.foot_x||x.foot_y!=y.foot_y||
           x.image_index!=y.image_index||x.flip_x!=y.flip_x)return false;
    }
    return true;
}
bool same_profile(const assets::WalkerVisualProfile& a,const assets::WalkerVisualProfile& b) {
    if(a.schema_version!=b.schema_version||a.unique_images.size()!=b.unique_images.size())return false;
    for(std::size_t i=0;i<a.roles.size();++i) {
        if(a.roles[i].has_value()!=b.roles[i].has_value())return false;
        if(a.roles[i]&&!same_role(*a.roles[i],*b.roles[i]))return false;
    }
    for(std::size_t i=0;i<a.unique_images.size();++i) {
        const auto& x=a.unique_images[i];const auto& y=b.unique_images[i];
        if(x.width!=y.width||x.height!=y.height||x.pixels!=y.pixels)return false;
    }
    return true;
}
struct Fixture {
    authored::Fixture old;
    fs::path health=old.root/"health-worker.json",market=old.root/"market.json",service=old.root/"service.json";
    Bytes archive=Bytes(40680+3*72,0),bitmap{0,0,0,0};
    Fixture() {
        u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
        u32(archive,12,3);u32(archive,16,3);u32(archive,20,1);
        const std::string group="independent-asymmetric-health.bmp";
        std::copy(group.begin(),group.end(),archive.begin()+680);u32(archive,804,3);
        for(int phase=0;phase<2;++phase) {
            const int w=9+4*phase,h=15+4*phase,fx=3+phase,fy=h-1;
            std::vector<std::uint16_t> raster(static_cast<std::size_t>(w*h),0);
            for(int y=fy-11;y<=fy-5;++y)for(int x=0;x<w;++x)
                raster[static_cast<std::size_t>(y*w+x)]=x<w/2?0x001f:0x03e0;
            for(int y=fy-9;y<=fy-7;++y)for(int x=fx-1;x<=fx+1;++x)raster[static_cast<std::size_t>(y*w+x)]=0;
            for(int y=fy-3;y<=fy-1;++y)for(int x=fx-1;x<=fx+1;++x)raster[static_cast<std::size_t>(y*w+x)]=0x7fff;
            for(int y=fy-4;y<=fy-2;++y)for(int x=w-4;x<=w-2;++x)raster[static_cast<std::size_t>(y*w+x)]=0x7c00;
            for(int y=fy-13;y<=fy-11;++y)for(int x=phase?w-4:1;x<=(phase?w-2:3);++x)raster[static_cast<std::size_t>(y*w+x)]=0x7fe0;
            Bytes payload;
            for(int y=0;y<h;++y)for(int x=0;x<w;) {
                const bool empty=raster[static_cast<std::size_t>(y*w+x)]==0;
                int count=1;while(x+count<w&&(raster[static_cast<std::size_t>(y*w+x+count)]==0)==empty)++count;
                payload.push_back(static_cast<std::uint8_t>(empty?255:count));
                if(empty)payload.push_back(static_cast<std::uint8_t>(count));
                else for(int n=0;n<count;++n){const auto c=raster[static_cast<std::size_t>(y*w+x+n)];payload.push_back(static_cast<std::uint8_t>(c));payload.push_back(static_cast<std::uint8_t>(c>>8U));}
                x+=count;
            }
            const auto at=40680+static_cast<std::size_t>(phase+1)*72;
            u32(archive,at,static_cast<std::uint32_t>(bitmap.size()));u32(archive,at+4,static_cast<std::uint32_t>(payload.size()));
            u16(archive,at+20,static_cast<std::uint16_t>(w));u16(archive,at+22,static_cast<std::uint16_t>(h));u16(archive,at+50,256);archive[at+59]=1;
            bitmap.insert(bitmap.end(),payload.begin(),payload.end());
        }
        write(old.data/"DATA/health.sg3",archive);write(old.data/"DATA/health.555",bitmap);
        authored::Fixture::save(health,manifest());authored::Fixture::save(market,market_manifest());authored::Fixture::save(service,service_manifest());
    }
    static Json role() {
        Json frames=Json::array();
        for(bool flip:{false,true})for(int phase=0;phase<2;++phase) {
            const int w=9+4*phase,fx=3+phase,fy=14+4*phase;
            frames.push_back({{"alias",std::string(flip?"flip-":"native-")+std::to_string(phase)},
                {"archive","DATA/health.sg3"},{"image_index",phase+1},
                {"foot_anchor",{flip?w-fx:fx,fy}},{"flip_x",flip}});
        }
        return {{"ticks_per_frame",2},{"clip_id","independent-health-walk"},{"evidence","Independent unequal native canvases, asymmetric edges and transparent interior hole."},
            {"frames",frames},{"idle","native-0"},{"clips",{{"pos_x",{"native-0","native-1"}},
                {"neg_y",{"native-0","native-1"}},{"neg_x",{"flip-0","flip-1"}},{"pos_y",{"flip-0","flip-1"}}}}};
    }
    static Json manifest() {return {{"schema_version",6},{"mode","curated_walker_preview"},{"roles",{{"health_worker",role()}}}};}
    static Json market_manifest() {return {{"schema_version",4},{"mode","curated_walker_preview"},{"roles",{{"supplier",role()},{"distributor",role()}}}};}
    static Json service_manifest() {return {{"schema_version",5},{"mode","curated_walker_preview"},{"roles",{{"service",role()}}}};}
    assets::WalkerVisualProfile core() const {return assets::load_walker_visual_profile(old.data,old.core);}
    assets::WalkerVisualProfile profile() const {return assets::load_walker_visual_profile(old.data,health);}
};
void schema_checks(Fixture& f) {
    static_assert(assets::walker_role_index(Role::Clay)==0&&assets::walker_role_index(Role::Pottery)==1&&assets::walker_role_index(Role::Household)==2);
    static_assert(assets::walker_role_index(Role::FireInspector)==3&&assets::walker_role_index(Role::Supplier)==4&&assets::walker_role_index(Role::Distributor)==5&&assets::walker_role_index(Role::Service)==6&&assets::walker_role_index(Role::HealthWorker)==7);
    static_assert(static_cast<unsigned>(sim::CourierRole::HealthWorker)==11);
    static_assert(assets::walker_core_role_count==3&&assets::walker_schema3_role_count==4&&assets::walker_schema4_role_count==6&&assets::walker_schema5_role_count==7&&assets::walker_visual_role_count==8);
    static_assert(assets::walker_max_frame_aliases==320&&assets::walker_legacy_max_frame_aliases==256&&assets::walker_frame_alias_limit(5)==256&&assets::walker_frame_alias_limit(6)==320&&assets::walker_max_unique_assets==256&&assets::walker_max_rgba_bytes==64U*1024U*1024U);
    const auto good=f.profile();check(good.schema_version==6&&good.find(Role::HealthWorker)&&good.unique_images.size()==2&&assets::walker_rgba_bytes(good)==1528,"schema6 HealthWorker physical dedupe/bytes changed");
    check(std::string_view(assets::walker_role_name(Role::HealthWorker))=="health_worker"&&oe::walker_visual_role(sim::CourierRole::HealthWorker)==Role::HealthWorker&&oe::walker_visual_role(sim::CourierRole::Service)==Role::Service,"append-only HealthWorker logical mapping or Health fallback changed");
    const auto& role=*good.find(Role::HealthWorker);check(role.frames.size()==4&&role.frames[2].image_index==role.frames[0].image_index&&role.frames[3].image_index==role.frames[1].image_index&&role.frames[2].foot_x==6&&role.frames[3].foot_x==9,"explicit reflected feet were transformed again or duplicated images");
    const auto invalid=[&](Json j,const char* reason){authored::Fixture::save(f.health,j);rejects([&]{(void)f.profile();},reason);};
    for(int v:{1,2,3,4,5}) {auto j=Fixture::manifest();j["schema_version"]=v;invalid(j,"older schema admitted HealthWorker role");}
    for(const auto* direction:directions) {auto j=Fixture::manifest();j["roles"]["health_worker"]["clips"].erase(direction);invalid(j,"HealthWorker partial direction activated");}
    auto j=Fixture::manifest();j["roles"]["health_worker"]["clips"]["pos_x"]={"native-0"};invalid(j,"one-frame HealthWorker clip activated");
    j=Fixture::manifest();for(const auto* d:directions)j["roles"]["health_worker"]["clips"][d]={"native-0","flip-0"};invalid(j,"display reflection substituted for different native gait frames");
    for(const auto& value:std::array<Json,4>{Json(1),Json("true"),Json(nullptr),Json::array()}) {j=Fixture::manifest();j["roles"]["health_worker"]["frames"][0]["flip_x"]=value;invalid(j,"nonboolean HealthWorker flip activated");}
    for(const auto* field:{"flip_y","flipx","horizontal_flip","angle"}){j=Fixture::manifest();j["roles"]["health_worker"]["frames"][0][field]=true;invalid(j,"unknown HealthWorker transform activated");}
    j=Fixture::manifest();j["roles"]["health_worker"].erase("clip_id");invalid(j,"HealthWorker missing clip ID activated");
    j=Fixture::manifest();j["roles"]["health_worker"]["ticks_per_frame"]=1001;invalid(j,"HealthWorker cadence exceeds bound");
    j=Fixture::manifest();j["roles"]["health_worker"]["frames"][0]["archive"]="../escape.sg3";invalid(j,"HealthWorker source escaped root");
    j=Fixture::manifest();j["roles"]["health_worker"]["frames"][0]["image_index"]=0;invalid(j,"HealthWorker dummy source activated");
    for(int v:{1,2,3}) {
        auto old=authored::Fixture::legacy();old["schema_version"]=v;
        if(v==1){old=old["roles"]["clay"];old["schema_version"]=1;old["mode"]="curated_walker_preview";old["role"]="clay";old["frames"][0]["flip_x"]=true;}
        else for(auto& [name,value]:old["roles"].items())value["frames"][0]["flip_x"]=true;
        authored::Fixture::save(f.health,old);const auto profile=f.profile();for(const auto& r:profile.roles)if(r)for(const auto& frame:r->frames)check(!frame.flip_x,"schema1–3 historical unknown flip acquired a transform");
    }
    j=authored::Fixture::legacy();j["schema_version"]=4;j["roles"]["clay"]["frames"][0]["flip_x"]=false;invalid(j,"schema4 core role admitted flip field");
    j=Fixture::market_manifest();j["roles"]["supplier"]["clips"].erase("neg_x");authored::Fixture::save(f.health,j);check(f.profile().find(Role::Supplier)->clips[1].empty(),"old schema4 Custom partial directions became stricter");
    j=Fixture::manifest();j["roles"]["supplier"]=Fixture::role();authored::Fixture::save(f.health,j);check(f.profile().find(Role::Supplier)->frames[2].flip_x,"schema6 lost old Market transforms");
    
    // The version boundary is explicit: old manifests keep their256 total,
    // even when aliases reuse one image; schema6 admits320 and rejects321.
    for(int version:{1,2,3,4,5}) {
        auto old=authored::Fixture::legacy();old["schema_version"]=version;
        if(version==1){old=old["roles"]["clay"];old["schema_version"]=1;old["mode"]="curated_walker_preview";old["role"]="clay";}
        auto& frames=version==1?old["frames"]:old["roles"]["clay"]["frames"];
        const auto seed=frames.front();const std::size_t allowed=version==1?256:254;
        while(frames.size()<allowed){auto extra=seed;extra["alias"]="old-bound-"+std::to_string(frames.size());frames.push_back(extra);}
        authored::Fixture::save(f.health,old);const auto legal=f.profile();std::size_t total=0;for(const auto& r:legal.roles)if(r)total+=r->frames.size();
        check(total==256&&legal.schema_version==static_cast<unsigned>(version),"old schema256 alias contract changed");
        auto extra=seed;extra["alias"]="old-overflow";frames.push_back(extra);invalid(old,"old standalone schema admitted257 aliases");
    }
    j=Fixture::manifest();auto& frames=j["roles"]["health_worker"]["frames"];const auto seed=frames.front();
    while(frames.size()<320){auto extra=seed;extra["alias"]="new-bound-"+std::to_string(frames.size());frames.push_back(extra);}
    authored::Fixture::save(f.health,j);const auto limit=f.profile();check(limit.find(Role::HealthWorker)->frames.size()==320&&limit.unique_images.size()==2&&assets::walker_rgba_bytes(limit)==1528,
        "schema6 alias320 boundary inflated native assets/RGBA or was rejected");
    auto extra=seed;extra["alias"]="new-overflow";frames.push_back(extra);invalid(j,"schema6 admitted321 aliases");
    for(const auto* name:{"clay","pottery","household","fire_inspector"}) {
        j=Fixture::manifest();j["roles"][name]=std::string_view(name)=="fire_inspector" ? authored::Fixture::inspector()["roles"]["fire_inspector"]:authored::Fixture::legacy()["roles"][name];
        j["roles"][name]["frames"][0]["flip_x"]=false;invalid(j,"schema6 expanded display flip to a historical unflipped role");
    }
    j=Fixture::manifest();j["roles"]["health_worker"]["frames"][0].erase("flip_x");authored::Fixture::save(f.health,j);
    check(!f.profile().find(Role::HealthWorker)->frames[0].flip_x,"omitted Health display flip is not native");
    j=Fixture::manifest();j["schema_version"]=7;invalid(j,"future walker schema admitted");
    authored::Fixture::save(f.health,Fixture::manifest());
}
void append_checks(Fixture& f) {
    auto core=f.core();const auto original=core;
    const auto fail=[&](Json j,const char* reason){authored::Fixture::save(f.health,j);const auto before=core;rejects([&]{assets::append_health_visual_profile(f.old.data,f.health,core);},reason);check(same_profile(before,core),"failed HealthWorker preparation changed retained profile");};
    auto j=Fixture::manifest();j["roles"]["health_worker"]["clips"].erase("neg_y");fail(j,"partial automatic HealthWorker activated");
    j=Fixture::manifest();j["roles"]["clay"]=authored::Fixture::legacy()["roles"]["clay"];fail(j,"HealthWorker supplement configured a foreign role");
    j=Fixture::manifest();j["schema_version"]=4;fail(j,"HealthWorker append accepted old schema");
    auto bytes=f.archive;bytes[40680+72+52]=1;write(f.old.data/"DATA/health.sg3",bytes);fail(Fixture::manifest(),"automatic HealthWorker consumed unpinned external bitmap");write(f.old.data/"DATA/health.sg3",f.archive);
    bytes=f.archive;u32(bytes,40680+72+16,1);write(f.old.data/"DATA/health.sg3",bytes);fail(Fixture::manifest(),"unsupported SG3 mirror source activated");write(f.old.data/"DATA/health.sg3",f.archive);
    fs::rename(f.old.data/"DATA/health.555",f.old.data/"DATA/unavailable.555");fail(Fixture::manifest(),"missing HealthWorker bitmap activated");fs::rename(f.old.data/"DATA/unavailable.555",f.old.data/"DATA/health.555");
    authored::Fixture::save(f.health,Fixture::manifest());assets::append_fire_inspector_visual_profile(f.old.data,f.old.supplement,core);assets::append_market_visual_profile(f.old.data,f.market,core);assets::append_service_visual_profile(f.old.data,f.service,core);
    const auto prior=core;std::vector<const std::uint8_t*> buffers;for(const auto& image:core.unique_images)buffers.push_back(image.pixels.data());
    assets::append_health_visual_profile(f.old.data,f.health,core);check(core.schema_version==6&&core.unique_images.size()==prior.unique_images.size(),"HealthWorker did not share already prepared Market native pixels");
    for(std::size_t i=0;i<7;++i)check(core.roles[i].has_value()==prior.roles[i].has_value()&&(!core.roles[i]||same_role(*core.roles[i],*prior.roles[i])),"HealthWorker changed an older visual role");
    for(std::size_t i=0;i<buffers.size();++i)check(core.unique_images[i].pixels.data()==buffers[i],"HealthWorker copied or replaced retained native RGBA");
    const auto complete=core;rejects([&]{assets::append_health_visual_profile(f.old.data,f.health,core);},"duplicate HealthWorker append activated");check(same_profile(core,complete),"duplicate HealthWorker rejection mutated profile");
    auto reverse=original;assets::append_health_visual_profile(f.old.data,f.health,reverse);assets::append_service_visual_profile(f.old.data,f.service,reverse);assets::append_market_visual_profile(f.old.data,f.market,reverse);assets::append_fire_inspector_visual_profile(f.old.data,f.old.supplement,reverse);check(reverse.schema_version==6&&assets::walker_rgba_bytes(reverse)==assets::walker_rgba_bytes(core),"HealthWorker-first appends reset schema6 or double-counted native assets");
    // A valid256-alias schema2 prefix may grow to260 only when Health commits
    // schema6. Subsequent older supplements retain the merged6 limit/schema.
    auto historical=original;while(historical.roles[0]->frames.size()<254){auto frame=historical.roles[0]->frames.front();frame.alias="old-prefix-"+std::to_string(historical.roles[0]->frames.size());historical.roles[0]->frames.push_back(frame);}
    assets::append_health_visual_profile(f.old.data,f.health,historical);check(historical.schema_version==6,"Health did not version increased alias capacity");
    assets::append_service_visual_profile(f.old.data,f.service,historical);assets::append_fire_inspector_visual_profile(f.old.data,f.old.supplement,historical);assets::append_market_visual_profile(f.old.data,f.market,historical);
    std::size_t joined=0;for(const auto& r:historical.roles)if(r)joined+=r->frames.size();check(joined>256&&joined<=320&&historical.schema_version==6,"old supplement downgraded merged6 aliases/schema");
    auto saturated=original;saturated.schema_version=6;auto& frames=saturated.roles[0]->frames;while(frames.size()<318){auto frame=frames.front();frame.alias="bound-"+std::to_string(frames.size());frames.push_back(frame);}
    const auto aliases=saturated;rejects([&]{assets::append_health_visual_profile(f.old.data,f.health,saturated);},"HealthWorker exceeded aggregate320 aliases");check(same_profile(saturated,aliases),"alias rejection partially appended HealthWorker");
    while(frames.size()>314)frames.pop_back();assets::append_health_visual_profile(f.old.data,f.health,saturated);std::size_t count=0;for(const auto& r:saturated.roles)if(r)count+=r->frames.size();check(count==320,"legal aggregate320 aliases rejected");
    auto full_assets=original;full_assets.unique_images.resize(256,full_assets.unique_images.front());const auto asset_count=full_assets.unique_images.size();rejects([&]{assets::append_health_visual_profile(f.old.data,f.health,full_assets);},"HealthWorker exceeded aggregate256 native assets");check(full_assets.unique_images.size()==asset_count&&!full_assets.find(Role::HealthWorker)&&full_assets.schema_version==2,"asset budget rejection published a partial role");
    auto full_rgba=original;auto& image=full_rgba.unique_images.front();image.width=image.height=4096;image.pixels.assign(static_cast<std::size_t>(assets::walker_max_rgba_bytes),0);const auto* retained=image.pixels.data();rejects([&]{assets::append_health_visual_profile(f.old.data,f.health,full_rgba);},"HealthWorker exceeded unchanged64MiB budget");check(full_rgba.schema_version==2&&!full_rgba.find(Role::HealthWorker)&&full_rgba.unique_images.size()==1&&full_rgba.unique_images[0].pixels.data()==retained&&assets::walker_rgba_bytes(full_rgba)==assets::walker_max_rgba_bytes,"RGBA rejection displaced retained resources");
}
void zero_counters() {for(std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)check(perf::counter(static_cast<perf::Counter>(i))==0,"pure HealthWorker presentation performed asset/authority/navigation work");}
void pose_checks(const assets::WalkerVisualProfile& profile) {
    const auto& visual=*profile.find(Role::HealthWorker);sim::CourierState c;c.role=sim::CourierRole::HealthWorker;c.enabled=true;c.phase=sim::CourierPhase::ToWarehouse;
    constexpr std::array<sim::Cell,4> ends{{{4,3},{2,3},{3,4},{3,2}}};
    for(std::size_t d=0;d<4;++d) {
        c.path={{3,3},ends[d]};c.path_vertex=0;c.route_pending=false;c.edge_progress=1;
        for(std::uint64_t tick:{0U,1U,2U,3U,4U}) {const auto pose=oe::walker_pose(c,tick,profile);check(pose.role==Role::HealthWorker&&pose.moving&&!pose.loaded&&pose.direction==static_cast<assets::StorageDirection>(d)&&pose.frame==visual.clips[d][static_cast<std::size_t>((tick/2)%2)],"HealthWorker direction/tick boundary changed");}
        const auto huge=std::numeric_limits<std::uint64_t>::max();check(oe::walker_pose(c,huge,profile).frame==visual.clips[d][static_cast<std::size_t>((huge/2)%2)],"HealthWorker large tick overflowed");
        c.route_pending=true;c.edge_progress=0;const auto waiting=oe::walker_pose(c,2,profile);check(!waiting.moving&&waiting.frame==visual.clips[d].front()&&oe::walker_pose(c,999,profile).frame==waiting.frame&&oe::walker_live_visible(c),"waiting HealthWorker walks in place or loses next-edge facing");
        c.edge_progress=1;check(oe::walker_pose(c,2,profile).moving&&oe::walker_live_visible(c),"HealthWorker protected begun edge vanished");
    }
    for(const auto phase:{sim::CourierPhase::IdleAtWorkshop,sim::CourierPhase::ToWarehouse,sim::CourierPhase::Returning})for(bool pending:{false,true})for(int cargo:{0,3}) {
        c.phase=phase;c.route_pending=pending;c.cargo=cargo;c.path={{3,3}};c.edge_progress=0;
        check(oe::walker_live_visible(c)==(phase!=sim::CourierPhase::IdleAtWorkshop)&&!oe::walker_pose(c,8,profile).moving&&oe::walker_pose(c,8,profile).frame==visual.idle_frame,"HealthWorker live visibility inferred cargo/path/movement instead of phase");
    }
    c.phase=sim::CourierPhase::ToWarehouse;c.path={{0,0},{std::numeric_limits<int>::min(),std::numeric_limits<int>::max()}};c.route_pending=false;check(oe::walker_pose(c,4,profile).fallback==oe::WalkerFallback::InvalidEdge,"extreme malformed HealthWorker edge overflowed");
    for(auto role:{sim::CourierRole::Clay,sim::CourierRole::Pottery,sim::CourierRole::Household,sim::CourierRole::Food,sim::CourierRole::None}){c.role=role;c.phase=sim::CourierPhase::IdleAtWorkshop;check(oe::walker_live_visible(c),"HealthWorker visibility changed an older idle role");}
}
Color pixel(SDL_Surface* surface,oe::scene::Point p) {Color result{};check(SDL_ReadSurfacePixel(surface,static_cast<int>(p.x),static_cast<int>(p.y),&result[0],&result[1],&result[2],&result[3]),"owned pixel witness failed");return result;}
void pixel_checks(SDL_Renderer* renderer,Fixture& f) {
    const auto profile=f.profile();const auto& visual=*profile.find(Role::HealthWorker);oe::WalkerSpriteSet sprites;
    perf::set_enabled(true);perf::reset();sprites.initialize(renderer,profile);check(sprites.texture_count()==2&&sprites.logical_bytes()==1528&&perf::counter(perf::Counter::TextureUploads)==2,"HealthWorker flip aliases inflated eager textures or byte budget");perf::set_enabled(false);
    constexpr Color background{80,100,120,255};constexpr oe::scene::Point ground{100,150};
    for(std::size_t d=0;d<4;++d)for(int phase=0;phase<2;++phase)for(double zoom:{1.0,1.15,2.0,4.0}) {
        const int w=9+4*phase,fx=3+phase,fy=14+4*phase;const auto index=visual.clips[d][static_cast<std::size_t>(phase)];const auto& frame=visual.frames[index];
        check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255)&&SDL_RenderClear(renderer),"clear authored target");
        perf::set_enabled(true);perf::reset();check(sprites.draw(index,ground,zoom,visual,profile,{0,0},{256,256}),"prepared HealthWorker native/reflected draw");zero_counters();perf::set_enabled(false);
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> output{SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};check(bool(output),"owned pre-Present pixels");
        const auto sample=[&](int x,int y){const int dx=frame.flip_x?w-1-x:x;return oe::scene::Point{ground.x+(dx+.5-frame.foot_x)*zoom,ground.y+(y+.5-frame.foot_y)*zoom};};
        // Fractional destinations may omit an extreme raster column. Keep the
        // exact two edge columns at integer zoom, and use solid interior
        // witnesses for the fractional raster without changing draw geometry.
        const int left=zoom==1.15?1:0,right=zoom==1.15?w-2:w-1;
        check(pixel(output.get(),sample(left,fy-6))==Color{0,0,255,255}&&pixel(output.get(),sample(right,fy-6))==Color{0,255,0,255},"HealthWorker horizontal flip used wrong source columns or foot registration");
        check(pixel(output.get(),sample(fx,fy-8))==background,"transparent HealthWorker interior hole paints or reflects incorrectly");
        check(pixel(output.get(),sample(fx,fy-2))==Color{255,255,255,255},"unequal HealthWorker canvas jumps its independently authored foot");
        check(pixel(output.get(),sample(phase?w-3:2,fy-12))==Color{255,255,0,255},"different HealthWorker gait silhouette is not actually rendered");
        const auto shadow=pixel(output.get(),sample(w-3,fy-3));check(std::abs(int(shadow[0])-40)<=1&&std::abs(int(shadow[1])-50)<=1&&std::abs(int(shadow[2])-60)<=1,"HealthWorker changed existing native shadow source-over preparation");
    }
    // The real caller owns viewport clipping. Exercise the same prepared draw
    // under an independent raster rectangle, then its whole-image early cull.
    const auto first=visual.clips[0][0];
    check(SDL_SetRenderDrawColor(renderer,background[0],background[1],background[2],255)&&SDL_RenderClear(renderer),"clear Health clipping target");
    const SDL_Rect clip{100,140,3,8};check(SDL_SetRenderClipRect(renderer,&clip),"set independent Health viewport");
    perf::set_enabled(true);perf::reset();check(sprites.draw(first,ground,1,visual,profile,{100,140},{103,148}),"partially clipped Health draw");zero_counters();perf::set_enabled(false);
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> clipped{SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};check(bool(clipped),"Health clipped target read");
    check(pixel(clipped.get(),{102,144})==Color{0,255,0,255}&&pixel(clipped.get(),{100,142})==background&&
          pixel(clipped.get(),{98,144})==background&&pixel(clipped.get(),{105,144})==background,
          "Health viewport clipped its interior, lost its alpha hole or drew outside its map rectangle");
    check(SDL_SetRenderClipRect(renderer,nullptr)&&SDL_RenderClear(renderer),"restore Health viewport");
    perf::set_enabled(true);perf::reset();check(sprites.draw(first,ground,1,visual,profile,{200,200},{220,220}),"whole Health frame cull");zero_counters();perf::set_enabled(false);
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> culled{SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface};check(bool(culled)&&pixel(culled.get(),{105,144})==background,"culled Health sprite was still submitted");
    const auto count=sprites.texture_count();fs::remove(f.old.data/"DATA/health.sg3");fs::remove(f.old.data/"DATA/health.555");fs::remove(f.health);
    perf::set_enabled(true);perf::reset();for(int n=0;n<100;++n)check(sprites.draw(visual.clips[0][0],ground,1,visual,profile,{0,0},{256,256}),"repeated frozen HealthWorker frame");zero_counters();perf::set_enabled(false);check(sprites.texture_count()==count,"HealthWorker frame recreated native textures");sprites.shutdown();check(oe::WalkerSpriteSet::live_texture_count()==0,"HealthWorker textures survived shutdown");
}
auto gate_permissions(bool horizontal) {
    constexpr int extent=24;const auto axis=[&](sim::Cell p){return horizontal?sim::Cell{p.y,p.x}:p;};
    std::vector<sim::MapCellPermission> cells(extent*extent,{true,true,false,0,sim::BuildBlocker::None,sim::BuildBlocker::None});const auto index=[](sim::Cell p){return static_cast<std::size_t>(p.y*extent+p.x);};sim::FixedGatePassage gate;gate.id={37};
    for(int y=7;y<=9;++y)for(int x=5;x<=9;++x){const auto p=axis({x,y});gate.protected_footprint.push_back(p);cells[index(p)]={false,false,true,0,sim::BuildBlocker::GateSolidPart,sim::BuildBlocker::OriginalStructure};}
    for(int y=7;y<=9;++y){const auto p=axis({7,y});gate.corridor.push_back(p);cells[index(p)].road_allowed=true;}gate.openings={axis({7,6}),axis({7,10})};
    for(int x=0;x<extent;++x)if(x<5||x>9)cells[index(axis({x,8}))]={false,false,true,0,sim::BuildBlocker::OriginalStructure,sim::BuildBlocker::OriginalStructure};
    return std::make_shared<const sim::MapPermissions>(extent,extent,1,std::move(cells),std::vector{std::move(gate)});
}
void actual_health(const assets::WalkerVisualProfile& profile,bool horizontal) {
    const auto permissions=gate_permissions(horizontal);sim::World world(permissions,sim::RulesProfile::CityV16,3);
    const auto axis=[&](sim::Cell p){return horizontal?sim::Cell{p.y,p.x}:p;};
    const auto put=[&](sim::CommandType type,sim::Cell cell){const auto result=world.execute({type,axis(cell)});check(result.accepted,"normal paid Health fixture command");return *world.building_owner_at(axis(cell));};
    const auto healthy=put(sim::CommandType::PlaceHousehold,{7,12});
    const auto sick=put(sim::CommandType::PlaceHousehold,{14,12});
    put(sim::CommandType::PlaceWell,{6,14});const auto post=put(sim::CommandType::PlaceHealthPost,{4,4});
    check(sim::building_footprint(world.profile(),world.rule_version(),sim::Object::HealthPost)==sim::BuildingFootprint{2,2}&&
          world.workforce_required(post)==2,"Health visuals changed rule3 Post geometry/workers");
    check(world.execute(sim::set_building_operation(post,false)).accepted,"ordinary initial Health pause");
    for(int x=4;x<=7;++x)check(world.execute({sim::CommandType::PlaceRoad,axis({x,6})}).accepted,"paid Post entrance road");
    for(int y=7;y<=11;++y)check(world.execute({sim::CommandType::PlaceRoad,axis({7,y})}).accepted,"paid gate corridor road");
    for(int x=8;x<=14;++x)check(world.execute({sim::CommandType::PlaceRoad,axis({x,11})}).accepted,"paid two-target curved road");
    sim::CourierId id{};for(const auto& c:world.couriers())if(c.owner==post&&c.role==sim::CourierRole::HealthWorker)id=c.id;
    check(static_cast<unsigned>(id)!=0,"existing HealthWorker missing");
    auto control=sim::World::restore(world.snapshot(),permissions);std::array<bool,4> seen_directions{};bool gate=false;
    const auto identical=[&]{check(world.snapshot()==control.snapshot()&&world.map_permissions()==permissions&&control.map_permissions()==permissions&&
        world.navigation_valid()&&world.health_state_valid()&&world.fire_state_valid()&&world.service_state_valid()&&world.city_economy_valid(),
        "Health visuals changed complete policy-bound World or economy");};
    const auto query=[&]{const auto before=world.snapshot();const auto& c=world.courier(id);perf::set_enabled(true);perf::reset();
        const auto pose=oe::walker_pose(c,world.ticks(),profile);const bool live=oe::walker_live_visible(c);zero_counters();perf::set_enabled(false);
        check(live==(c.phase!=sim::CourierPhase::IdleAtWorkshop)&&pose.role==Role::HealthWorker&&pose.frame&&!pose.loaded&&c.cargo==0&&c.reserved==0,
            "actual Health trip visibility/cargo/pose changed");check(world.snapshot()==before,"Health pose changed complete state");
        if(pose.moving){seen_directions[assets::direction_index(*pose.direction)]=true;if(c.path_vertex<c.path.size()&&world.fixed_passage(c.path[c.path_vertex]))gate=true;}
    };
    const auto step=[&]{world.tick();control.tick();identical();query();};
    query();check(!oe::walker_live_visible(world.courier(id))&&!world.household_health_protected(healthy)&&!world.household_sick(sick),"initial Health ghost or free protection");
    while(world.ticks()<3400)step();
    check(!world.household_sick(healthy)&&world.household_health_risk(healthy)==34&&world.household_sick(sick)&&
        world.building(sick).sick_until_tick==4600&&world.workers_assigned(post)==0,"natural wet/dry illness or paused staffing control differs");
    const auto command=[&](sim::Command cmd){check(world.execute(cmd).accepted&&control.execute(cmd).accepted,"ordinary Health control command");identical();};
    command(sim::set_building_operation(post,true));step();
    check(world.workers_assigned(post)==2&&world.courier(id).target==sick&&world.courier(id).phase==sim::CourierPhase::ToWarehouse&&
        world.household_sick(sick)&&!world.household_health_protected(sick)&&!world.household_health_protected(healthy),
        "Health failed actual2/2 sick-first dispatch or treated remotely");
    command(sim::set_building_operation(post,false));
    int budget=200;while(budget--&&!(world.courier(id).path[world.courier(id).path_vertex]==axis({7,7})&&world.courier(id).edge_progress==1))step();check(budget>=0,"Health never entered real gate");
    const auto begun=world.courier_position(id);command({sim::CommandType::RemoveRoad,axis({7,10})});
    check(world.courier_position(id)==begun&&world.courier(id).route_pending&&oe::walker_pose(world.courier(id),world.ticks(),profile).moving,
        "Health road cut discarded begun-edge position/figure");
    budget=30;while(budget--&&!(world.courier(id).route_pending&&world.courier(id).edge_progress==0))step();check(budget>=0,"Health never waited at interrupted edge");
    const auto fixed=world.courier_position(id);const auto frame=oe::walker_pose(world.courier(id),world.ticks(),profile).frame;
    auto restored=sim::World::restore(world.snapshot(),permissions);
    for(int n=0;n<20;++n){step();restored.tick();check(restored.snapshot()==world.snapshot()&&restored.map_permissions()==permissions&&
        world.courier_position(id)==fixed&&oe::walker_pose(world.courier(id),world.ticks(),profile).frame==frame&&
        !oe::walker_pose(world.courier(id),world.ticks(),profile).moving&&world.household_sick(sick)&&!world.household_health_protected(sick),
        "waiting paused Health moved, animated or healed before arrival");}
    command({sim::CommandType::PlaceRoad,axis({7,10})});
    budget=200;while(budget--&&world.household_sick(sick)){const auto state=world.snapshot();auto moving=sim::World::restore(state,permissions);
        check(moving.snapshot()==state&&oe::walker_pose(moving.courier(id),moving.ticks(),profile).frame==oe::walker_pose(world.courier(id),world.ticks(),profile).frame,
            "moving Health full-policy restore changed visible phase");step();moving.tick();check(moving.snapshot()==world.snapshot(),"Health restored continuation diverged");}
    const auto cure=world.ticks();check(budget>=0&&world.building(sick).sick_until_tick==cure&&world.household_health_protected(sick)&&
        world.building(sick).health_protection_until_tick==cure+2400&&world.household_health_risk(sick)==0&&
        world.courier(id).phase==sim::CourierPhase::Returning&&oe::walker_live_visible(world.courier(id)),"only actual Health arrival should cure/protect and return visibly");
    budget=200;while(budget--&&world.courier(id).phase!=sim::CourierPhase::IdleAtWorkshop)step();check(budget>=0&&!oe::walker_live_visible(world.courier(id)),"Health home return failed to hide");
    check(sim::World::restore(world.snapshot(),permissions).snapshot()==world.snapshot(),"Health idle full-policy restore changed state");
    command(sim::set_building_operation(post,true));step();check(world.courier(id).target==healthy&&!world.household_health_protected(healthy)&&
        !world.household_sick(healthy),"healthy unprotected target failed existing risk priority");command(sim::set_building_operation(post,false));
    budget=200;while(budget--&&!world.household_health_protected(healthy))step();check(budget>=0&&
        world.building(healthy).health_protection_until_tick==world.ticks()+2400&&world.household_health_risk(healthy)==0&&
        !world.household_sick(healthy),"healthy prevention granted outside actual arrival");
    budget=200;while(budget--&&world.courier(id).phase!=sim::CourierPhase::IdleAtWorkshop)step();check(budget>=0&&gate&&
        std::all_of(seen_directions.begin(),seen_directions.end(),[](bool value){return value;}),"Health gate/curve four directions or final return incomplete");
    // A separately paid second Post has its own existing logical courier and
    // independent trip; no visual layer creates a new patrol or shared cargo.
    const auto second=put(sim::CommandType::PlaceHealthPost,{1,4});const auto second_control=control.execute({sim::CommandType::PlaceHealthPost,axis({1,4})});
    check(second_control.accepted,"paid second Health Post control failed");
    for(int x=1;x<=3;++x)command({sim::CommandType::PlaceRoad,axis({x,6})});
    command(sim::set_building_operation(post,true));step();
    std::size_t active=0;std::array<sim::CourierId,2> ids{};std::size_t at=0;
    for(const auto& c:world.couriers())if(c.role==sim::CourierRole::HealthWorker){check(c.cargo==0&&c.reserved==0,"second Health courier gained cargo");ids.at(at++)=c.id;active+=c.phase!=sim::CourierPhase::IdleAtWorkshop;}
    check(at==2&&ids[0]!=ids[1]&&active==2&&world.workers_assigned(post)==2&&world.workers_assigned(second)==2,
        "two paid staffed Posts did not dispatch independent actual workers");identical();
    std::cout<<"ordinary Health gate "<<(horizontal?"horizontal":"vertical")<<" natural_illness=3400 cure="<<cure<<" final_tick="<<world.ticks()<<" full_controls_equal=1\n";
}
void unstaffed_health(const assets::WalkerVisualProfile& profile) {
    const auto permissions=gate_permissions(false);sim::World world(permissions,sim::RulesProfile::CityV16,3);
    check(world.execute({sim::CommandType::PlaceHealthPost,{1,1}}).accepted,"paid unstaffed Health Post placement");
    sim::CourierId id{};for(const auto& c:world.couriers())if(c.role==sim::CourierRole::HealthWorker)id=c.id;
    const auto owner=world.courier(id).owner;check(world.workers_assigned(owner)==0,"empty city invented Health staff");
    for(int i=0;i<32;++i){world.tick();const auto& c=world.courier(id);check(c.phase==sim::CourierPhase::IdleAtWorkshop&&!oe::walker_live_visible(c)&&
        !oe::walker_pose(c,world.ticks(),profile).moving&&c.cargo==0&&c.reserved==0,"unstaffed Post invented a visible trip");}
}

}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* target=nullptr;
    try {
        Fixture f;schema_checks(f);append_checks(f);const auto profile=f.profile();pose_checks(profile);unstaffed_health(profile);actual_health(profile,false);actual_health(profile,true);
        const bool metal=argc>1&&std::string_view(argv[1])=="metal";check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal?"cocoa":"dummy")&&SDL_Init(SDL_INIT_VIDEO),"SDL HealthWorker fixture init");
        window=SDL_CreateWindow("Authored HealthWorker frames",256,256,SDL_WINDOW_HIDDEN);check(window!=nullptr,"owned HealthWorker window");renderer=SDL_CreateRenderer(window,metal?"metal":"software");check(renderer!=nullptr,"actual requested HealthWorker renderer");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal?"metal":"software"),"actual HealthWorker backend differs from request");
        std::cout<<"Actual backend="<<SDL_GetRendererName(renderer)<<"; owned hidden 256x256 target/pre-Present authored pixels.\n";
        target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,256,256);check(target&&SDL_SetRenderTarget(renderer,target),"owned HealthWorker target");pixel_checks(renderer,f);
        SDL_DestroyTexture(target);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"HealthWorker schema/atomic append/pose/real gate/direct native-flip pixels PASS\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";if(target)SDL_DestroyTexture(target);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();return 1;}
}
