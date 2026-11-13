// Scripted isolated SDL checks. No native or human acceptance claim.
#include "EconomyStartViewFixture.h"
#include "PlantationsFixture.h"
#include "app/WalkerPose.h"
#include "assets/PlantationVisualProfile.h"
#include "core/PerformanceDiagnostics.h"
#include <cmath>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace q=oe_plantations_fixture;
namespace f=oe::testing::economy;
namespace fs=std::filesystem;
using f::check;
using Point=oe::scene::Point;
void event(oe::SandboxView& view,SDL_Event value) {bool running=true;view.handle_event(value,running);check(running,"Input unexpectedly closed view");}
void key(oe::SandboxView& view,SDL_Keycode code) {SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);}
void center(oe::SandboxView& view,Point world) {
    const auto map=view.layout().map;const auto scale=view.camera().zoom;
    const Point target{map.x+map.w*.5-world.x*scale,map.y+map.h*.5-world.y*scale};
    for(unsigned step=0;step<1000;++step) {
        const auto current=view.camera().offset;const auto dx=target.x-current.x,dy=target.y-current.y;
        if(std::abs(dx)<1e-6 && std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6){held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused camera did not converge");
}
void zoom(oe::SandboxView& view,SDL_Renderer* renderer,double wanted) {
    const auto map=view.layout().map;float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(map.x+map.w*.5),static_cast<float>(map.y+map.h*.5),&x,&y),"Zoom input transform");
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_WHEEL;value.wheel.mouse_x=x;value.wheel.mouse_y=y;
    value.wheel.y=static_cast<float>(std::log(wanted/view.camera().zoom)/std::log(1.15));event(view,value);view.update(0);
    check(std::abs(view.camera().zoom-wanted)<1e-6,"Requested2x zoom not reached");
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point screen) {
    float x=0,y=0;check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(screen.x),static_cast<float>(screen.y),&x,&y),"Input transform");
    SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;move.motion.x=x;move.motion.y=y;event(view,move);view.update(0);
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
        SDL_Event value{};value.type=type;value.button.button=SDL_BUTTON_LEFT;value.button.x=x;value.button.y=y;event(view,value);
    }
    view.update(0);
}
bool contains(const std::vector<std::string>& lines,std::string_view text) {
    return std::any_of(lines.begin(),lines.end(),[&](const auto& line){return line.find(text)!=std::string::npos;});
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Output dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Capture target");check(view.render(),"Scripted plantation frame");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Write private capture");check(SDL_SetRenderTarget(renderer,nullptr),"Restore output target");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==1 || (argc==3 && std::string_view(argv[1])=="--real"),"Expected optional --real ORIGINAL_DATA_ROOT");
        f::Temp temp;temp.root=fs::canonical(temp.root);f::write(temp.root/"data/Cities/A.map",f::map_file(true,true,true));
        fs::create_directories(temp.root/"data/Model");q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",q::authored_model());
        const auto repo=fs::canonical(fs::path(__FILE__).parent_path().parent_path());
        const auto output=repo/".local/agri-03/plantation-presentation"/(argc==3 ? "real":"synthetic");fs::create_directories(output);
        if(argc==3)for(const auto name:{"China_General.sg3","China_General.555"})
            fs::copy_file(fs::canonical(argv[2])/"DATA"/name,temp.root/"data/DATA"/name);
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");check(SDL_Init(SDL_INIT_VIDEO),"Initialize isolated SDL");
        check(SDL_CreateWindowAndRenderer("Plantation scripted presentation",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),"Create scripted renderer");
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto owner=oe::SandboxView::create_original_plantations(std::move(session),std::nullopt,false);auto& view=*owner;
        view.configure_save(temp.root/"data","Cities/A.map",temp.root/"plantations.save.json");
        if(argc==3) {
            const auto pack=repo/"resources/compatibility/gog-derived-2.0.0.2-en-assetset-1";
            view.set_building_visuals(pack/"buildings.json",oe::VisualProfileSource::Builtin);
            view.set_original_building_visuals(pack/"original-buildings.json",oe::VisualProfileSource::Builtin);
        }
        view.initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focus);
        check(view.world().original_plantations_enabled() && view.world().rule_version()==25 && view.paused(),"Wrong explicit Agriculture generation/start state");
        check(view.last_message().find("AUTHORED Wheat/Millet; Rice/Cabbage/Soy locked")!=std::string::npos,
            "Rule25 UI hides its authored-food/locked-crops boundary");
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view.execute({type,cell});
            if (!result.accepted || !result.changed) throw std::runtime_error("Paid construction "+
                std::to_string(static_cast<unsigned>(type))+" at "+std::to_string(cell.x)+","+std::to_string(cell.y)+": "+result.reason);
            return view.world().building_owner_at(cell).value_or(sim::BuildingId{});
        };
        for(int x=87;x<=143;++x)if(!view.world().road_cell({x,110}))buy(sim::CommandType::PlaceRoad,{x,110});
        for(int x=87;x<=143;x+=3)buy(sim::CommandType::PlaceHousehold,{x,108});
        std::array<sim::BuildingId,3> sources{},plants{};
        constexpr std::array source_commands{sim::CommandType::PlaceOriginalTeaCuringShed,sim::CommandType::PlaceOriginalLacquerRefinery,sim::CommandType::PlaceOriginalSilkwormShed};
        for(std::size_t i=0;i<3;++i) {
            const int x=106+static_cast<int>(i)*10;sources[i]=buy(source_commands[i],{x,111});
            const sim::Cell cell{x+4,115};view.set_tool(38+static_cast<int>(i));
            check(view.tool()==38+static_cast<int>(i) && view.preview(cell).accepted,"Plant tool failed unambiguous ranged parent selection");
            sim::Command command{static_cast<sim::CommandType>(static_cast<unsigned>(sim::CommandType::PlaceOriginalTeaBush)+i),cell};command.building_id=sources[i];
            const auto result=view.execute(command);check(result.accepted && result.changed,"Paid ranged plant refused");plants[i]=*view.world().building_owner_at(cell);
            if(i<2)buy(sim::CommandType::PlaceFireWatch,{x,114});
            const auto art=view.plantation_visual_diagnostic(plants[i]);check(art.applicable && art.source==sources[i] && art.growth==0,"Plant diagnostic detached from sole World state");
            if(argc==3)check(art.active && art.asset,"Verified native plant bank did not prepare");
        }
        view.set_tool(5);center(view,oe::maps::terrain_ground({120,115},72));check(view.render(),"Initial plants frame");
        click(view,renderer,view.camera().world_to_screen(oe::maps::terrain_ground({120,115},72)));
        check(view.selected_building()==plants[1] && contains(view.inspection_lines(),"growth 0/80") &&
            contains(view.inspection_lines(),"Bound source"),"Actual plant inspector lacks authoritative growth/parent");
        capture(view,renderer,output/"plantations-initial.png");
        bool returned=false,live=false,captured_growth=false;
        for(unsigned step=0;step<3000 && !returned;++step) {
            view.tick_once();
            if(!captured_growth && q::plant(view.world(),plants[0]).growth>=60 && q::plant(view.world(),plants[0]).growth<80) {
                captured_growth=true;zoom(view,renderer,2);
                for(std::size_t index=0;index<plants.size();++index) {
                    const auto cell=view.world().building(plants[index]).cell;
                    center(view,oe::maps::terrain_ground({static_cast<std::uint32_t>(cell.x),static_cast<std::uint32_t>(cell.y)},72));
                    capture(view,renderer,output/("plantation-"+std::to_string(26+index)+"-growth-2x.png"));
                }
            }
            for(const auto& source:view.world().plantations_state()->sources)if(source.latest_trip) {
                const auto& actor=view.world().courier(source.courier_id);live|=oe::walker_live_visible(actor);
                check(!oe::walker_visual_role(actor.role),"Grower borrowed an unqualified native family");
                returned|=source.latest_trip->returned_tick.has_value();
            }
        }
        check(returned && live && captured_growth,"Paid plant never produced a physical visible growth/harvest/return");
        for(const auto id:plants) {
            const auto art=view.plantation_visual_diagnostic(id);check(art.growth==q::plant(view.world(),id).growth &&
                art.stage==oe::assets::plantation_growth_visual_stage(art.model_type,art.growth),"Plant image does not follow actual saved growth");
        }
        capture(view,renderer,output/"plantations-work.png");
        const auto before=view.world().snapshot();oe::performance::set_enabled(true);oe::performance::reset();
        for(int frame=0;frame<20;++frame)check(view.render(),"Pure plantation frame");
        for(std::size_t i=0;i<static_cast<std::size_t>(oe::performance::Counter::Count);++i)
            check(!oe::performance::counter(static_cast<oe::performance::Counter>(i)),"Plantation render performed World/resource/route work");
        oe::performance::set_enabled(false);check(view.world().snapshot()==before,"Plantation render mutated World");
        view.save_now();check(oe::persistence::read_save(view.save_path()).source_schema_version==41,"Wrong plantation save generation");
        view.tick_once();view.load_now();check(view.world().snapshot()==before,"Plantation F9 changed physical state");
        key(view,SDLK_F4);check(!view.plantation_visual_diagnostic(plants[0]).active,"F4 failed labelled fallback comparison");capture(view,renderer,output/"plantations-f4.png");
        owner.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Plantation paid tools, ranged binding, inspectors, actual Grower visibility/return, growth visuals, F4/F5/F9 and pure frames PASS\n";return 0;
    } catch(const std::exception& error) {
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
