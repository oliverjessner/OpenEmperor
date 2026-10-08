#include "app/SandboxView.h"
#include "app/WalkerPose.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include "renderer/FireSpriteSet.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "renderer/WalkerSpriteSet.h"
#include "EconomyStartViewFixture.h"
#include "FireInspectorWalkerFixture.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

// Shared production painter and paid ordinary HealthWorker visits. Pixel data is
// independently authored; these are direct View tests, not original/human QA.
namespace {
namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace assets=oe::assets;
namespace perf=oe::performance;
namespace authored=oe::testing::inspector;
using Json=nlohmann::json;
using View=oe::SandboxView;
using Pixel=std::array<std::uint8_t,4>;
using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
void check(bool yes,const char* why) { if (!yes) throw std::runtime_error(why); }
void key(View& view,SDL_Keycode code) {
    SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;bool running=true;
    view.handle_event(e,running);check(running,"Unexpected test quit.");
}
void click(View& view,oe::scene::Point point) {
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=static_cast<float>(point.x);e.button.y=static_cast<float>(point.y);bool running=true;
    view.handle_event(e,running);e.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(e,running);
    check(running,"Unexpected click quit.");
}
Surface read(SDL_Renderer* renderer) {
    Surface s(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);check(bool(s),"Private readback failed.");return s;
}
Pixel pixel(SDL_Surface* image,int x,int y) {
    Pixel p{};check(SDL_ReadSurfacePixel(image,x,y,&p[0],&p[1],&p[2],&p[3]),"Private pixel read failed.");return p;
}
void pure_render(View& view) {
    const auto state=view.world().snapshot();perf::set_enabled(true);perf::reset();check(view.render(),"Production render failed.");
    for (std::size_t i=0;i<static_cast<std::size_t>(perf::Counter::Count);++i)
        check(perf::counter(static_cast<perf::Counter>(i))==0,"HealthWorker render performed asset/World/navigation work.");
    perf::set_enabled(false);check(state==view.world().snapshot(),"Render changed complete World snapshot.");
}
oe::scene::Point ground(const View& view,sim::CourierId id) {
    const auto p=view.world().courier_position(id);check(p.has_value(),"Real HealthWorker position missing.");
    return view.camera().world_to_screen({40*(p->x-p->y),20*(p->x+p->y-144)+20});
}
void zoom_to(View& view,sim::CourierId id,double zoom) {
    const auto p=ground(view,id);SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=static_cast<float>(p.x);
    e.wheel.mouse_y=static_cast<float>(p.y);e.wheel.y=static_cast<float>(std::log(zoom/view.camera().zoom)/std::log(1.15));
    bool running=true;view.handle_event(e,running);check(std::abs(view.camera().zoom-zoom)<0.00001,"Wheel zoom mismatch.");
}
struct Fixture {
    oe::testing::economy::Temp map;
    authored::Fixture images;
    fs::path data,profile;
    Fixture() {
        map.root=fs::canonical(map.root);data=map.root/"data";profile=map.root/"health-worker.json";
        fs::copy_file(images.data/"DATA/walker.sg3",data/"DATA/walker.sg3");
        fs::copy_file(images.data/"DATA/walker.555",data/"DATA/walker.555");
        auto full=authored::Fixture::legacy();full["schema_version"]=6;
        auto role=authored::Fixture::inspector()["roles"]["fire_inspector"];role["clip_id"]="authored-health-worker";
        // Explicit display-only flips: existing native asymmetric gait and
        // shadow pixels are shared without any reflected RGBA allocations.
        for (auto& f:role["frames"]) {
            const auto alias=f["alias"].get<std::string>();const int direction=alias[1]-'0',phase=alias[3]-'0';
            const bool flip=direction==1||direction==2;f["flip_x"]=flip;
            if (flip) f["foot_anchor"][0]=(10+2*phase)-(5+phase);
        }
        full["roles"]["health_worker"]=role;authored::Fixture::save(profile,full);
    }
    oe::maps::StoredMapSession session() const {
        return oe::maps::load_stored_map_session(data,"Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8);
    }
    void configure(View& v,bool sprites,const char* name) const {
        v.configure_save(data,"Cities/A.map",map.root/name);
        if (sprites) v.set_walker_visuals(profile,oe::VisualProfileSource::Custom);
    }
};
sim::Cell connected_purchase(const sim::World& world,sim::CommandType type) {
    std::vector<sim::Cell> candidates;
    for(int y=0;y<world.height();++y)for(int x=0;x<world.width();++x)candidates.push_back({x,y});
    const auto seed=world.building(static_cast<sim::BuildingId>(7)).cell;
    std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){
        const int da=std::abs(a.x-seed.x-4)+std::abs(a.y-seed.y-1),db=std::abs(b.x-seed.x-4)+std::abs(b.y-seed.y-1);
        return da!=db?da<db:a.y!=b.y?a.y<b.y:a.x<b.x;
    });
    const auto kind=type==sim::CommandType::PlaceHousehold?sim::Object::Household:sim::Object::HealthPost;
    for(auto cell:candidates)if(world.validate({type,cell}).accepted)
        for(auto f:sim::building_footprint_cells(world.profile(),world.rule_version(),kind,cell))
            for(auto d:{sim::Cell{1,0},sim::Cell{-1,0},sim::Cell{0,1},sim::Cell{0,-1}}){
                const sim::Cell road{f.x+d.x,f.y+d.y};
                if(world.object_at(road)==sim::Object::Road&&world.map_permissions()->cell_height(road)==world.map_permissions()->cell_height(f))return cell;
            }
    throw std::runtime_error("Xia has no normal affordable connected House/Post candidate.");
}

void paid_health_setup(View& view,View& plain) {
    check(view.world().construction_spent_total()==1280&&view.world().treasury()==20&&view.world().workforce_supply()==24&&view.world().workforce_used()==24,"Old paid starter changed.");
    for(int tick=1;tick<=3400;++tick){
        view.tick_once();plain.tick_once();
        check(view.world().snapshot()==plain.world().snapshot(),"Health visuals changed full pre-illness production/economy/Health state.");
        if(tick==1600||tick==2000){const auto cell=connected_purchase(view.world(),sim::CommandType::PlaceHousehold);
            check(view.execute({sim::CommandType::PlaceHousehold,cell}).accepted&&plain.execute({sim::CommandType::PlaceHousehold,cell}).accepted,"Normally earned extra House purchase rejected.");}
    }
    for(const auto& b:view.world().buildings())if(b.placed&&b.kind==sim::Object::Household&&unsigned(b.id)<=10)
        check(view.world().household_sick(b.id)&&b.sick_until_tick==4600,"Expected natural dry sickness did not occur at3400.");
    const auto cell=connected_purchase(view.world(),sim::CommandType::PlaceHealthPost);
    check(view.execute({sim::CommandType::PlaceHealthPost,cell}).accepted&&plain.execute({sim::CommandType::PlaceHealthPost,cell}).accepted,"Normally earned paid2x2 Health Post rejected.");
    check(view.world().snapshot()==plain.world().snapshot(),"Paid Health Post changed complete visual control.");
}
sim::CourierId health_id(const View& view) {
    for (const auto& c:view.world().couriers()) if (c.role==sim::CourierRole::HealthWorker) return c.id;
    throw std::runtime_error("Paid city has no HealthWorker courier.");
}
void equal_documents(View& a,View& b,const Fixture& f,const char* tag) {
    const auto left=f.map.root/(std::string(tag)+"-drawn.json"),right=f.map.root/(std::string(tag)+"-plain.json");
    oe::persistence::write_save(left,a.capture_save_document(),f.data,a.buildable_mask());
    oe::persistence::write_save(right,b.capture_save_document(),f.data,b.buildable_mask());
    Json x,y;std::ifstream l(left),r(right);l>>x;r>>y;check(x==y,"Complete SaveDocuments differ by HealthWorker presentation.");
    check(x.at("schema_version")==19,"HealthWorker fixture changed schema19.");
}
void home_hidden(View& view,sim::CourierId id) {
    check(view.world().courier(id).phase==sim::CourierPhase::IdleAtWorkshop,"Home check is not actually idle.");
    for (int pass=0;pass<2;++pass) {
        pure_render(view);const auto rows=view.walker_diagnostics();
        const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& r){return r.id==id;});
        check(row!=rows.end()&&!row->live_visible&&!row->submitted&&!row->sprite_drawn,"Idle HealthWorker submits a body/marker.");
        const auto at=ground(view,id);click(view,{at.x,at.y-8});
        check(view.input_diagnostic_state().selected_walker!=static_cast<unsigned>(id),"Idle HealthWorker retains an exterior hit.");
        key(view,SDLK_F2);
    }
}
Pixel color(assets::StorageDirection d) {
    constexpr std::array<Pixel,4> p{{{0,0,255,255},{0,255,0,255},{255,255,0,255},{0,255,255,255}}};
    return p[assets::direction_index(d)];
}
void ordinary_visits(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture f;const auto profile=assets::load_walker_visual_profile(f.data,f.profile);
    View view(f.session(),true,sim::RulesProfile::CityV16,3),plain(f.session(),true,sim::RulesProfile::CityV16,3);
    f.configure(view,true,"visits.json");f.configure(plain,false,"visits-control.json");view.initialize(window,renderer);plain.initialize(window,renderer);
    paid_health_setup(view,plain);const auto id=health_id(view);check(view.world().workers_assigned(view.world().courier(id).owner)==2,"Normal Post not2/2 staffed.");
    check(view.walker_texture_count()==8,"Role aliases did not globally deduplicate native images.");
    zoom_to(view,id,1.0);key(view,SDLK_5);home_hidden(view,id);
    std::array<bool,4> directions{};std::array<bool,2> gait{};bool outgoing=false,returning=false,arrival=false,picked=false;
    bool hole=false,zoomed=false,saved=false,home=false,flipped=false;
    for (int tick=3401;tick<=4000;++tick) {
        const auto before=view.world().snapshot();view.tick_once();plain.tick_once();
        check(view.world().snapshot()==plain.world().snapshot(),"HealthWorker visuals changed dispatch/path/coverage/economy/population full state.");
        const auto& c=view.world().courier(id);check(c.cargo==0&&c.reserved==0,"HealthWorker gained cargo/reservation.");
        for (const auto& b:view.world().buildings()) if (b.placed&&b.kind==sim::Object::Household) {
            const auto old=std::find_if(before.buildings.begin(),before.buildings.end(),[&](const auto& p){return p.id==b.id;});
            check(old!=before.buildings.end(),"Complete snapshot lost House.");
            if (old->health_protection_until_tick!=b.health_protection_until_tick) {
                check(c.phase==sim::CourierPhase::Returning&&c.target==b.id&&
                    b.health_protection_until_tick==view.world().ticks()+2400&&b.health_risk==0,"Health protection changed before actual arrival.");
                check(old->sick_until_tick<=view.world().ticks()||b.sick_until_tick==view.world().ticks(),"Actual Health arrival did not cure natural sickness.");arrival=true;
            }
        }
        outgoing=outgoing||c.phase==sim::CourierPhase::ToWarehouse;
        returning=returning||c.phase==sim::CourierPhase::Returning;
        if (returning&&c.phase==sim::CourierPhase::IdleAtWorkshop) { home_hidden(view,id);home=true; }
        if (tick%2==0) {
            pure_render(view);const auto pose=oe::walker_pose(c,view.world().ticks(),profile);
            if (!pose.moving||!pose.direction||!pose.frame) continue;
            const auto at=ground(view,id);const auto& area=view.layout().map;
            if (at.x<area.x+10||at.x>=area.x+area.w-10||at.y<area.y+25||at.y>=area.y+area.h-5) continue;
            const auto output=read(renderer);const auto expected=color(*pose.direction);
            if (pixel(output.get(),static_cast<int>(at.x),static_cast<int>(at.y-8))!=expected) continue;
            directions[assets::direction_index(*pose.direction)]=true;
            const auto& frame=profile.find(assets::WalkerVisualRole::HealthWorker)->frames[*pose.frame];
            gait[static_cast<std::size_t>((frame.id.image_index-1)%4/2)]=true;flipped=flipped||frame.flip_x;
            key(view,SDLK_F1);pure_render(view);click(view,{at.x,at.y-8});
            picked=picked||view.input_diagnostic_state().selected_walker==static_cast<unsigned>(id);
            pure_render(view);click(view,{at.x-5,at.y-15});
            hole=hole||view.input_diagnostic_state().selected_walker!=static_cast<unsigned>(id);key(view,SDLK_F1);
            if (!zoomed&&picked) {
                for (const double z:{1.15,2.0,4.0,1.0}) {
                    zoom_to(view,id,z);pure_render(view);const auto scaled=read(renderer);const auto p=ground(view,id);
                    check(pixel(scaled.get(),static_cast<int>(p.x),static_cast<int>(p.y-8*z))==expected,"HealthWorker torso foot reference changed under zoom.");
                }
                pure_render(view);const auto a=read(renderer);view.update(0);pure_render(view);const auto b=read(renderer);const auto p=ground(view,id);
                for (int y=-13;y<0;++y) for (int x=-4;x<4;++x)
                    check(pixel(a.get(),static_cast<int>(p.x)+x,static_cast<int>(p.y)+y)==
                        pixel(b.get(),static_cast<int>(p.x)+x,static_cast<int>(p.y)+y),"Paused HealthWorker gait changed.");
                zoomed=true;
            }
            if (!saved&&c.phase==sim::CourierPhase::ToWarehouse) {
                const auto state=view.world().snapshot();const auto frame_index=pose.frame;key(view,SDLK_F5);key(plain,SDLK_F5);
                equal_documents(view,plain,f,"travelling");key(view,SDLK_F9);key(plain,SDLK_F9);
                check(state==view.world().snapshot()&&oe::walker_pose(view.world().courier(id),view.world().ticks(),profile).frame==frame_index,
                    "Active F5/F9 changed complete HealthWorker state or frame.");saved=true;
            }
        }
        if (tick==3600||tick==3800||tick==4000) equal_documents(view,plain,f,"boundary");
    }
    check(outgoing&&returning&&arrival&&home&&picked&&hole&&zoomed&&saved&&flipped&&gait[0]&&gait[1]&&
        std::all_of(directions.begin(),directions.end(),[](bool b){return b;}),"Ordinary HealthWorker body/direction/gait/arrival/return/picking/save acceptance incomplete.");
    equal_documents(view,plain,f,"visits-final");view.shutdown();plain.shutdown();
    check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,"HealthWorker visits leaked textures.");
}
void interrupted_visits(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture f;const auto profile=assets::load_walker_visual_profile(f.data,f.profile);
    View view(f.session(),true,sim::RulesProfile::CityV16,3),plain(f.session(),true,sim::RulesProfile::CityV16,3);
    f.configure(view,true,"waiting.json");f.configure(plain,false,"waiting-control.json");view.initialize(window,renderer);plain.initialize(window,renderer);
    paid_health_setup(view,plain);const auto id=health_id(view);const auto owner=view.world().courier(id).owner;zoom_to(view,id,1.0);
    const auto command=[&](sim::Command input) {
        const auto a=view.execute(input),b=plain.execute(input);check(a.accepted&&b.accepted&&a.changed==b.changed&&
            view.world().snapshot()==plain.world().snapshot(),"Ordinary HealthWorker pause/cut/paidrepair command changed complete control.");
    };
    bool paused=false,begun=false,wait=false,repaired=false,arrival=false,returned=false;std::optional<sim::Cell> removed;
    std::optional<std::vector<Pixel>> waiting_pixels;int waiting_ticks=0;
    for (int tick=0;tick<350;++tick) {
        view.tick_once();plain.tick_once();check(view.world().snapshot()==plain.world().snapshot(),"Interrupted HealthWorker tick changed full control.");
        const auto current=view.world().courier(id);
        if (!paused&&current.phase==sim::CourierPhase::ToWarehouse&&current.edge_progress==1&&current.path_vertex>=1) {
            for (std::size_t i=current.path_vertex+2;i<current.path.size();++i) {
                if (!view.world().validate({sim::CommandType::RemoveRoad,current.path[i]}).accepted) continue;
                command(sim::set_building_operation(owner,false));paused=true;
                check(oe::walker_live_visible(view.world().courier(id)),"Paused Post hid real begun HealthWorker visit.");
                const auto position=view.world().courier_position(id);removed=current.path[i];command({sim::CommandType::RemoveRoad,*removed});
                const auto& c=view.world().courier(id);check(c.route_pending&&c.edge_progress==1&&oe::walker_live_visible(c)&&
                    oe::walker_pose(c,view.world().ticks(),profile).moving&&view.world().courier_position(id)==position,
                    "Road cut discarded HealthWorker begun-edge position/movement.");begun=true;break;
            }
        }
        pure_render(view);const auto c=view.world().courier(id);
        if (begun&&!repaired&&c.route_pending&&c.edge_progress==0) {
            const auto pose=oe::walker_pose(c,view.world().ticks(),profile);
            check(!pose.moving&&pose.frame&&oe::walker_live_visible(c),"Real interrupted HealthWorker vanished or walked in place.");
            const auto p=ground(view,id);const auto output=read(renderer);std::vector<Pixel> crop;
            for (int y=-12;y<-5;++y) for (int x=-3;x<3;++x) crop.push_back(pixel(output.get(),static_cast<int>(p.x)+x,static_cast<int>(p.y)+y));
            if (!waiting_pixels) waiting_pixels=crop;else check(*waiting_pixels==crop,"Waiting HealthWorker gait changed across World ticks.");
            wait=true;++waiting_ticks;
            if (waiting_ticks==1) {
                const auto state=view.world().snapshot();key(view,SDLK_F5);key(plain,SDLK_F5);equal_documents(view,plain,f,"waiting");
                key(view,SDLK_F9);key(plain,SDLK_F9);check(view.world().snapshot()==state,"Waiting HealthWorker F5/F9 changed full state.");
            }
            if (waiting_ticks==8) {command({sim::CommandType::PlaceRoad,*removed});repaired=true;}
        }
        arrival=arrival||(repaired&&c.phase==sim::CourierPhase::Returning);
        if (arrival&&c.phase==sim::CourierPhase::IdleAtWorkshop) {home_hidden(view,id);returned=true;break;}
    }
    check(paused&&begun&&wait&&repaired&&arrival&&returned&&view.world().workers_assigned(owner)==0,
        "Actual HealthWorker paused-owner/cut/wait/save/paidrepair/arrival/return acceptance incomplete.");
    equal_documents(view,plain,f,"waiting-final");view.shutdown();plain.shutdown();
    check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::FireSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,
        "Interrupted HealthWorker session leaked textures.");
}
void unstaffed_and_two_posts(SDL_Window* window,SDL_Renderer* renderer) {
    Fixture f;View view(f.session(),true,sim::RulesProfile::CityV16,3),plain(f.session(),true,sim::RulesProfile::CityV16,3);
    f.configure(view,true,"staffing.json");f.configure(plain,false,"staffing-control.json");view.initialize(window,renderer);plain.initialize(window,renderer);
    for(int i=0;i<1600;++i){view.tick_once();plain.tick_once();check(view.world().snapshot()==plain.world().snapshot(),"Negative staffing control differed.");}
    const auto post_cell=connected_purchase(view.world(),sim::CommandType::PlaceHealthPost);
    check(view.execute({sim::CommandType::PlaceHealthPost,post_cell}).accepted&&plain.execute({sim::CommandType::PlaceHealthPost,post_cell}).accepted,"Negative paidPost rejected.");
    const auto first=health_id(view);const auto owner=view.world().courier(first).owner;
    check(view.world().workers_assigned(owner)==0,"Negative HealthPost unexpectedly staffed.");home_hidden(view,first);
    for(int i=0;i<20;++i){view.tick_once();plain.tick_once();home_hidden(view,first);check(view.world().snapshot()==plain.world().snapshot(),"Zero-worker Post invented Health effects.");}
    const auto buy=[&](sim::CommandType type){const auto cell=connected_purchase(view.world(),type);check(view.execute({type,cell}).accepted&&plain.execute({type,cell}).accepted,"Normally earned staffing/multiple-Post purchase rejected.");};
    // The first House requires actually earned funds; this independent branch
    // waits for the unchanged economy rather than granting funds/workforce.
    while(view.world().treasury()<80){view.tick_once();plain.tick_once();}
    buy(sim::CommandType::PlaceHousehold);check(view.world().workers_assigned(owner)==2,"Paid House did not supply actualHealth crew.");
    while(view.world().treasury()<120){view.tick_once();plain.tick_once();}
    buy(sim::CommandType::PlaceHealthPost);
    std::vector<sim::CourierId> ids;for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::HealthWorker)ids.push_back(c.id);
    check(ids.size()==2&&view.walker_texture_count()==8,"Two Posts did not share native prepared textures.");
    std::array<bool,2> dispatched{},arrived{};
    for(int i=0;i<400;++i){view.tick_once();plain.tick_once();check(view.world().snapshot()==plain.world().snapshot(),"Independent twoHealth patrols changed full control.");pure_render(view);
        for(std::size_t n=0;n<ids.size();++n){const auto& c=view.world().courier(ids[n]);check(view.world().workers_assigned(c.owner)==2&&c.cargo==0&&c.reserved==0,"Independent HealthPost crew/cargo changed.");dispatched[n]=dispatched[n]||c.phase==sim::CourierPhase::ToWarehouse;arrived[n]=arrived[n]||c.phase==sim::CourierPhase::Returning;}}
    check(dispatched[0]&&dispatched[1]&&arrived[0]&&arrived[1],"Two genuine independent Health patrols incomplete.");
    equal_documents(view,plain,f,"two-posts");view.shutdown();plain.shutdown();
    check(oe::WalkerSpriteSet::live_texture_count()==0&&oe::StoredGraphicsRenderer::live_texture_count()==0,"Health staffing sessions leaked textures.");
}

}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        const bool metal=argc==2&&std::string_view(argv[1])=="metal";check(argc==1||metal,"Usage: HealthWorker view tests [metal]");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,metal?"cocoa":"dummy")&&SDL_SetHint(SDL_HINT_RENDER_DRIVER,metal?"metal":"software")&&
            SDL_Init(SDL_INIT_VIDEO),"SDL init failed.");
        check(SDL_CreateWindowAndRenderer("Authored HealthWorker production pixels",1280,720,SDL_WINDOW_HIDDEN,&window,&renderer),"SDL renderer failed.");
        check(std::string_view(SDL_GetRendererName(renderer))==(metal?"metal":"software"),"Actual backend mismatch.");
        std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
            SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,720),SDL_DestroyTexture);
        check(bool(target)&&SDL_SetRenderTarget(renderer,target.get()),"Owned pre-Present target failed.");
        ordinary_visits(window,renderer);interrupted_visits(window,renderer);unstaffed_and_two_posts(window,renderer);
        check(SDL_SetRenderTarget(renderer,nullptr),"Target release failed.");target.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"HealthWorker ordinary paid visits, actual arrival/return, shared alpha/flip/zoom, paused owner and waiting SaveDocument neutrality PASS\n";return 0;
    } catch (const std::exception& e) {
        std::cerr<<e.what()<<'\n';if (renderer) SDL_DestroyRenderer(renderer);if (window) SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
