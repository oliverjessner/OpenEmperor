// Production SandboxView on independently authored source-cell terrain.
// No original pixels/data; scripted SDL and software captures are not human QA.
#include "EconomyStartViewFixture.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/LandscapeProvenance.h"
#include "persistence/RecoveryStore.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iterator>
#include <memory>
namespace {
namespace oe=openemperor;namespace sim=oe::simulation;namespace maps=oe::maps;
namespace p=oe::persistence;namespace perf=oe::performance;namespace fs=std::filesystem;
namespace fixture=oe::testing::economy;using fixture::check;using Json=nlohmann::json;
using Point=oe::scene::Point;using Bytes=fixture::Bytes;
constexpr sim::Cell source{110,110};
std::string bytes(const fs::path& path){std::ifstream in(path,std::ios::binary);check(bool(in),"Missing private bytes");return {std::istreambuf_iterator<char>(in),{}};}
void event(oe::SandboxView& view,SDL_Event e){bool running=true;view.handle_event(e,running);check(running,"Unexpected quit");}
void key(oe::SandboxView& view,SDL_Keycode code){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=code;event(view,e);}
void center(oe::SandboxView& view,Point aim){
    for(int n=0;n<300;++n){const auto map=view.layout().map;const auto actual=view.camera().world_to_screen(aim);
        const double dx=map.x+map.w*.5-actual.x,dy=map.y+map.h*.5-actual.y;if(std::abs(dx)<1e-6&&std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else{held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}}
    throw std::runtime_error("Paused ordinary camera did not converge");
}
Point window_point(SDL_Renderer* renderer,Point p){SDL_FPoint value{};check(SDL_RenderCoordinatesToWindow(renderer,float(p.x),float(p.y),&value.x,&value.y),"Window inverse failed");return {value.x,value.y};}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted){
    const auto m=view.layout().map;const auto q=window_point(renderer,{m.x+m.w*.5,m.y+m.h*.5});
    for(int n=0;n<3&&std::abs(view.camera().zoom-wanted)>1e-9;++n){SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;e.wheel.mouse_x=float(q.x);e.wheel.mouse_y=float(q.y);e.wheel.y=float(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,e);view.update(0,{});}
    check(std::abs(view.camera().zoom-wanted)<1e-9,"Literal zoom failed");
}
void author(const fs::path& path,sim::Cell d,int base){
    auto raw=maps::EmperorContainer::open(path).read_part(0);const maps::MapGeometry geometry{84};
    for(unsigned y=0;y<228;++y)for(unsigned x=0;x<228;++x){const auto i=std::size_t(y)*228+x;
        fixture::u32(raw,std::size_t(maps::terrain_logical_offset)+4*i,geometry.contains({x,y})?0x80U:0x80000U);
        raw[std::size_t(maps::landscape_height_offset)+i]=std::uint8_t(base+((int(x)-source.x)*d.x+(int(y)-source.y)*d.y>0?1:0));}
    Bytes file{0xaa,0xba,0xdc,0xfe};
    for(std::size_t at=0;at<raw.size();at+=32768){const auto count=std::min<std::size_t>(32768,raw.size()-at);uLongf size=compressBound(static_cast<uLong>(count));Bytes part(size);
        check(compress2(part.data(),&size,raw.data()+at,static_cast<uLong>(count),6)==Z_OK,"Authored compression failed");
        const auto h=file.size();file.resize(h+12);fixture::u32(file,h,0x12345678);fixture::u32(file,h+4,std::uint32_t(size));fixture::u32(file,h+8,std::uint32_t(count));file.insert(file.end(),part.begin(),part.begin()+std::ptrdiff_t(size));}
    fixture::write(path,file);
}
sim::Cell rotate(sim::Cell c,sim::Cell d){return {source.x+(c.x-8)*d.x-(c.y-5)*d.y,source.y+(c.x-8)*d.y+(c.y-5)*d.x};}
sim::Cell origin(sim::Cell c,sim::Object kind,sim::Cell d){const auto size=sim::building_footprint(sim::RulesProfile::CityV16,20,kind);sim::Cell low{INT32_MAX,INT32_MAX};
    for(const auto corner:{c,sim::Cell{c.x+size.width-1,c.y},sim::Cell{c.x,c.y+size.height-1},sim::Cell{c.x+size.width-1,c.y+size.height-1}}){const auto t=rotate(corner,d);low.x=std::min(low.x,t.x);low.y=std::min(low.y,t.y);}return low;}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path){
    int w=0,h=0;check(SDL_GetRenderOutputSize(renderer,&w,&h),"Output size failed");
    auto* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,w,h);check(target,"Owned target failed");
    const auto snapshot=view.world().snapshot();const auto camera=view.camera();
    check(SDL_SetRenderTarget(renderer,target),"Set owned target failed");check(view.render(),"Production capture render failed");
    auto* image=SDL_RenderReadPixels(renderer,nullptr);check(image,"Pre-Present readback failed");
    check(SDL_SetRenderTarget(renderer,nullptr),"Restore default target failed");SDL_DestroyTexture(target);
    check(SDL_SavePNG(image,path.c_str()),"Private PNG write failed");SDL_DestroySurface(image);
    check(view.world().snapshot()==snapshot&&view.camera().zoom==camera.zoom&&view.camera().offset.x==camera.offset.x&&view.camera().offset.y==camera.offset.y,"Capture mutated World/camera");
}
void one(const fs::path& output,int index,sim::Cell direction,int base,SDL_Window* window,SDL_Renderer* renderer){
    fixture::Temp temp;author(temp.root/"data/Cities/A.map",direction,base);const auto map_before=bytes(temp.root/"data/Cities/A.map");
    auto session=maps::load_stored_map_session(temp.root/"data","Cities/A.map",maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8);
    const auto border=session.plan.border;oe::SandboxView view(std::move(session),false,sim::RulesProfile::CityV16,20);view.set_authored_terrain_sides(true);
    const auto manual=output/("manual-"+std::to_string(index)+".json");view.configure_save(temp.root/"data","Cities/A.map",manual);view.initialize(window,renderer);
    if(!view.paused())key(view,SDLK_SPACE);check(view.world().source_cell_ramps_supported(),"Production did not choose Rule20");
    const auto buy=[&](sim::Command c){const auto r=view.execute(c);if(!r.accepted)throw std::runtime_error(r.reason);};
    for(const auto command:std::array<sim::Command,4>{{{sim::CommandType::PlaceHousehold,{1,1}},{sim::CommandType::PlaceHousehold,{1,3}},{sim::CommandType::PlaceClaySource,{4,2}},{sim::CommandType::PlacePottery,{14,2}}}}){const auto kind=*sim::placed_object(command.type);buy({command.type,origin(command.cell,kind,direction)});}
    for(int x=4;x<=15;++x)buy({sim::CommandType::PlaceRoad,rotate({x,5},direction)});
    for(const auto c:{sim::Cell{5,4},sim::Cell{14,4}})buy({sim::CommandType::PlaceRoad,rotate(c,direction)});
    check(!view.world().find_building_route(sim::BuildingId{3},sim::BuildingId{4}),"Unbuilt step granted route");
    const auto before=view.world().snapshot();
    if(index==0) {
        const Point raw_ground{double(source.x-source.y)*40,(double(source.x+source.y)-2*border)*20+20-base*40};
        center(view,raw_ground);key(view,SDLK_P);check(view.tool()==15,"P did not select shared ramp action");
        const sim::Cell high{source.x+direction.x,source.y+direction.y};
        const auto a=view.camera().world_to_screen(raw_ground);
        const auto b=view.camera().world_to_screen({double(high.x-high.y)*40,(double(high.x+high.y)-2*border)*20+20-(base+1)*40});
        check(view.pick(a)==source&&view.pick(b)==high,"Actual source/high construction picking differs");
        const auto wa=window_point(renderer,a),wb=window_point(renderer,b);
        const auto mouse=[&](bool down,Point q){SDL_Event e{};e.type=down?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;e.button.x=float(q.x);e.button.y=float(q.y);event(view,e);};
        mouse(true,wa);key(view,SDLK_ESCAPE);mouse(false,wb);
        check(view.world().snapshot()==before,"Cancelled source-cell drag published authority");
        key(view,SDLK_P);mouse(true,wa);SDL_Event motion{};motion.type=SDL_EVENT_MOUSE_MOTION;motion.motion.x=float(wb.x);motion.motion.y=float(wb.y);event(view,motion);
        perf::set_enabled(true);perf::reset();for(int n=0;n<100;++n){event(view,motion);view.update(0,{});check(view.render(),"Source-cell preview frame failed");}
        for(std::size_t n=0;n<std::size_t(perf::Counter::Count);++n)check(perf::counter(static_cast<perf::Counter>(n))==0,"Source-cell preview performed forbidden work");perf::set_enabled(false);
        check(view.world().snapshot()==before,"Source-cell preview changed World");mouse(false,wb);
        check(view.world().source_cell_ramps().size()==1,"Actual P drag did not commit source-cell ramp");
        const auto placed=view.world().snapshot();mouse(false,wb);check(view.world().snapshot()==placed,"Duplicate source-cell release committed again");
    } else buy(sim::place_source_cell_ramp(source,{source.x+direction.x,source.y+direction.y}));
    view.set_tool(5);
    check(view.world().treasury()==before.treasury-40&&view.ramp_geometry_bytes()<=8U*1024U*1024U,"Paid geometry budget failed");
    const Point low_ground{double(source.x-source.y)*40,(double(source.x+source.y)-2*border)*20+20-base*40};
    center(view,{low_ground.x,low_ground.y-20});check(view.render(),"Prepared source-cell selection render failed");
    oe::SourceCellRampPresentation selected_mesh;selected_mesh.prepare(view.world());bool selected=false;
    for(int y=-60;y<=20&&!selected;y+=2)for(int x=-50;x<=50&&!selected;x+=2) {
        const Point world{low_ground.x+x+.5,low_ground.y+y+.5};
        if(!selected_mesh.hit_part(world,low_ground,sim::SourceCellRampId{1},true) &&
           !selected_mesh.hit_part(world,low_ground,sim::SourceCellRampId{1},false))continue;
        const auto screen=view.camera().world_to_screen(world);if(!view.layout().map.contains(screen.x,screen.y))continue;
        check(view.pick(screen)==source,"Prepared source-cell bank pixels picked another cell");const auto q=window_point(renderer,screen);
        SDL_Event down{};down.type=SDL_EVENT_MOUSE_BUTTON_DOWN;down.button.button=SDL_BUTTON_LEFT;down.button.x=float(q.x);down.button.y=float(q.y);event(view,down);
        down.type=SDL_EVENT_MOUSE_BUTTON_UP;event(view,down);const auto lines=view.inspection_lines();
        selected=std::any_of(lines.begin(),lines.end(),[](const auto& line){return line.find("Authored source-cell ramp #1")!=std::string::npos;});
    }
    check(selected,"Actual source-cell bank selection lost its authored ID/height Inspector");
    bool loaded=false,returned=false;Json witnesses=Json::array();
    for(int n=0;n<650;++n){view.tick_once();const auto& courier=view.world().couriers().front();const auto pos=view.world().courier_position(courier.id);
        if(pos&&courier.edge_progress>0&&view.world().source_cell_ramp_surface_height(*pos)){
            const auto actual=view.world().source_cell_ramp_surface_height(*pos);const double along=(pos->x-source.x)*direction.x+(pos->y-source.y)*direction.y;
            check(std::abs(*actual-(base+std::clamp(along+.5,0.,1.)))<1e-12,"World foot differs from source surface");
            if(courier.cargo>0&&!loaded&&std::abs(along)<=.25){loaded=true;witnesses.push_back({{"phase","loaded"},{"tick",view.world().ticks()},{"position",{pos->x,pos->y}},{"height",*actual},{"cargo",courier.cargo}});break;}
        }}
    check(loaded,"Real paid cargo did not reach source ramp");
    const auto loaded_state=view.world().snapshot();key(view,SDLK_F5);const auto full=bytes(manual);key(view,SDLK_PERIOD);key(view,SDLK_F9);
    check(view.world().snapshot()==loaded_state&&bytes(manual)==full,"F5/F9 changed complete loaded state/bytes");
    const auto doc=p::read_save(manual);auto independent=p::restore_authored_source_cell_ramps(doc,temp.root/"data",view.buildable_mask());
    check(independent.snapshot()==loaded_state,"Independent production source restore differs");
    p::RecoveryStore store(output/("app-"+std::to_string(index)),temp.root/"data");const auto history=store.create_history(view.capture_save_document(),view.buildable_mask());const auto checkpoint=store.write_checkpoint(history,view.capture_save_document(),view.buildable_mask());
    check(p::restore_authored_source_cell_ramps(p::read_save(checkpoint.path),temp.root/"data",view.buildable_mask()).snapshot()==loaded_state,"Recovery lost loaded physical state");
    const Point aim{double(source.x-source.y)*40,(double(source.x+source.y)-2*border)*20+20-(base+.5)*40};
    for(const auto size:std::array<std::array<int,2>,3>{{{900,600},{1280,720},{1600,1000}}}){
        check(SDL_SetWindowSize(window,size[0],size[1]),"Window size failed");SDL_PumpEvents();SDL_Event resize{};resize.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,resize);
        for(const double z:{1.,2.,4.}){zoom(view,renderer,z);center(view,aim);const auto state=view.world().snapshot();
            check(view.render(),"Actual production render failed");const auto position=*view.world().courier_position(view.world().couriers().front().id);
            const double height=*view.world().source_cell_ramp_surface_height(position);const auto expected=view.camera().world_to_screen({(position.x-position.y)*40,(position.x+position.y-2*border)*20+20-height*40});
            const auto diagnostics=view.walker_diagnostics();const auto actor=std::find_if(diagnostics.begin(),diagnostics.end(),[&](const auto& row){return row.id==view.world().couriers().front().id;});
            check(actor!=diagnostics.end()&&actor->submitted&&std::abs(actor->screen_ground.x-expected.x)<1e-9&&std::abs(actor->screen_ground.y-expected.y)<1e-9,"Production actor foot/culling differs from authoritative surface");
            capture(view,renderer,output/("dir-"+std::to_string(index)+"-"+std::to_string(size[0])+"-z"+std::to_string(int(z))+".png"));
            check(view.world().snapshot()==state,"Visual matrix changed World");
        }}
    const auto visual_control_state=view.world().snapshot();key(view,SDLK_F1);
    for(int mode=0;mode<8;++mode){key(view,SDLK_F8);check(view.render(),"Source-cell F8/Snapshot render failed");}
    key(view,SDLK_F1);key(view,SDLK_F7);check(view.render(),"Source-cell F7 comparison failed");key(view,SDLK_F7);
    check(view.world().snapshot()==visual_control_state,"Source-cell F8/Snapshot/F7 controls changed World");
    perf::set_enabled(true);perf::reset();for(int n=0;n<100;++n)check(view.render(),"Pure production frame failed");
    for(std::size_t i=0;i<std::size_t(perf::Counter::Count);++i)check(perf::counter(static_cast<perf::Counter>(i))==0,"Production frame performed preparation/authority work");perf::set_enabled(false);
    for(int n=0;n<700;++n){view.tick_once();independent.tick();check(independent.snapshot()==view.world().snapshot(),"Full continuation after view restore differs");const auto& actor=view.world().couriers().front();returned|=actor.phase==sim::CourierPhase::IdleAtWorkshop;}
    check(returned&&view.world().pottery_completed_total()>0&&view.world().maintenance_spent_total()>0,"Actual production arrival/return/recipe/upkeep absent");
    check(bytes(temp.root/"data/Cities/A.map")==map_before,"Authored source bytes changed during paid journey");
    std::ofstream out(output/("direction-"+std::to_string(index)+".json"));out<<Json{{"direction",{direction.x,direction.y}},{"low_height",base},{"witnesses",witnesses},{"captures",9},{"pure_frames",100},{"save_schema",36},{"geometry_bytes",view.ramp_geometry_bytes()},{"terrain_sides",view.authored_terrain_side_count()},{"continued_tick",view.world().ticks()},{"pottery_completed",view.world().pottery_completed_total()},{"upkeep",view.world().maintenance_spent_total()}}.dump(2)<<'\n';check(bool(out),"Report write failed");view.shutdown();
}
}
int main(){SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;try{
    const auto output=fs::current_path()/".local/oe53"/("view-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(output);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");check(SDL_Init(SDL_INIT_VIDEO),"SDL video init failed");window=SDL_CreateWindow("Authored source-cell production QA",1280,720,SDL_WINDOW_HIDDEN);check(window,"Window failed");renderer=SDL_CreateRenderer(window,"software");check(renderer,"Software renderer failed");
    int i=0;for(const auto d:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}){one(output,i,d,i==0?-2:i==2?2:0,window,renderer);++i;}
    check(oe::StoredGraphicsRenderer::live_texture_count()==0,"Source-cell production texture leak");SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cout<<"Authored source-cell production cargo/feet/36 captures/F5F9/Recovery/full continuation/pure frames PASS: "<<output<<'\n';return 0;
}catch(const std::exception& e){SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<e.what()<<'\n';return 1;}}
