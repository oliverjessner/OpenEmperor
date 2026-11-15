// Paid synthetic SDL checks. No Original execution, pixels or human acceptance.
#include "EconomyStartViewFixture.h"
#include "PlantationsFixture.h"
#include "core/PerformanceDiagnostics.h"

#include <cmath>
#include <memory>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace q=oe_plantations_fixture;
namespace f=oe::testing::economy;
namespace fs=std::filesystem;
using f::check;
using Point=oe::scene::Point;

void event(oe::SandboxView& view,SDL_Event value) {
    bool running=true;view.handle_event(value,running);
    check(running,"Calendar UI input unexpectedly closed the session");
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=code;event(view,value);
}
Point window_point(SDL_Renderer* renderer,Point point) {
    float x=0,y=0;
    check(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),
          "Calendar UI inverse coordinate conversion failed");
    return {x,y};
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,Point point) {
    const auto p=window_point(renderer,point);
    SDL_Event move{};move.type=SDL_EVENT_MOUSE_MOTION;
    move.motion.x=static_cast<float>(p.x);move.motion.y=static_cast<float>(p.y);event(view,move);view.update(0,{});
    for(const auto type:{SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP}) {
        SDL_Event value{};value.type=type;value.button.button=SDL_BUTTON_LEFT;
        value.button.x=static_cast<float>(p.x);value.button.y=static_cast<float>(p.y);event(view,value);
    }
    view.update(0,{});
}
Point ground(sim::Cell cell) {
    return {double(cell.x-cell.y)*40,double(cell.x+cell.y-144)*20+20};
}
void center(oe::SandboxView& view,Point point) {
    const auto map=view.layout().map;const auto z=view.camera().zoom;
    const Point target{map.x+map.w*.5-point.x*z,map.y+map.h*.5-point.y*z};
    for(unsigned step=0;step<1000;++step) {
        const auto current=view.camera().offset;const auto dx=target.x-current.x,dy=target.y-current.y;
        if(std::abs(dx)<1e-6 && std::abs(dy)<1e-6)return;
        oe::SandboxView::PanKeyState held{};
        if(std::abs(dx)>1e-6) {held.left=dx>0;held.right=dx<0;view.update(std::min(.05,std::abs(dx)/400),held);}
        else {held.up=dy>0;held.down=dy<0;view.update(std::min(.05,std::abs(dy)/400),held);}
    }
    throw std::runtime_error("Paused calendar inspection camera did not converge");
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id) {
    view.set_tool(5);const auto& body=view.world().building(id);
    center(view,ground(body.cell));check(view.render(),"Calendar selection preparation frame failed");
    const auto p=view.camera().world_to_screen(ground(body.cell));std::optional<Point> point;
    for(int y=-30;y<=20 && !point;y+=2)for(int x=-24;x<=24 && !point;x+=2) {
        const Point candidate{p.x+x,p.y+y};
        if(!view.layout().map.contains(candidate.x,candidate.y))continue;
        const auto cell=view.pick(candidate);
        if(cell && view.world().building_owner_at(*cell)==id)point=candidate;
    }
    check(point.has_value(),"Calendar building has no visible selectable owned cell");
    click(view,renderer,*point);check(view.selected_building()==id,"Calendar mouse selection missed its paid building");
}
std::string joined(const std::vector<std::string>& lines) {
    std::string text;for(const auto& line:lines){text+=line;text+=' ';}return text;
}
bool contains(const oe::SandboxView& view,std::string_view value) {
    return joined(view.inspection_lines()).find(value)!=std::string::npos;
}
void calendar_matches_world(const oe::SandboxView& view) {
    const auto tick=view.world().ticks();
    const auto text=joined(view.inspection_lines());
    check(text.find("Elapsed cycle "+std::to_string(tick/9792))!=std::string::npos,
          "Inspector elapsed cycle diverges from sole World ticks");
    check(text.find("Outer slot "+std::to_string((tick/816)%12))!=std::string::npos,
          "Inspector outer slot is stale at a boundary");
    check(text.find("Subslot "+std::to_string((tick/51)%16))!=std::string::npos,
          "Inspector subslot is stale at a boundary");
    check(text.find("Phase "+std::to_string(tick%51))!=std::string::npos,
          "Inspector phase is stale at a boundary");
    check(text.find("AUTHORED")!=std::string::npos && text.find("Reference")!=std::string::npos,
          "Inspector loses the reference-versus-authored time boundary");
    for(const auto name:{"January","February","March","April","May","June","July","August",
                         "September","October","November","December"})
        check(text.find(name)==std::string::npos,"Unproved named month entered the calendar inspector");
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    int width=0,height=0;check(SDL_GetCurrentRenderOutputSize(renderer,&width,&height),"Calendar capture dimensions");
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    check(target && SDL_SetRenderTarget(renderer,target.get()),"Calendar capture target");
    check(view.render(),"Calendar capture render failed");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> surface(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(surface && SDL_SavePNG(surface.get(),path.string().c_str()),"Calendar PNG write failed");
    check(SDL_SetRenderTarget(renderer,nullptr),"Calendar output target restore failed");
}
void pure_frames(oe::SandboxView& view) {
    const auto snapshot=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();
    oe::performance::set_enabled(true);oe::performance::reset();
    for(unsigned frame=0;frame<12;++frame)check(view.render(),"Calendar pure frame failed");
    for(std::size_t index=0;index<static_cast<std::size_t>(oe::performance::Counter::Count);++index)
        check(!oe::performance::counter(static_cast<oe::performance::Counter>(index)),
              "Calendar render performed World/resource/navigation work");
    oe::performance::set_enabled(false);
    check(view.world().snapshot()==snapshot && view.agriculture_inspection_prepare_count()==prepared,
          "Calendar frames changed World or rebuilt prepared Agriculture diagnosis");
}
}

int main() {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        f::Temp temp;temp.root=fs::canonical(temp.root);
        f::write(temp.root/"data/Cities/A.map",f::map_file(true,true,true));
        fs::create_directories(temp.root/"data/Model");
        q::f::write_text(temp.root/"data/Model/EmperorBuildingModels.txt",q::authored_model());
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software");
        check(SDL_Init(SDL_INIT_VIDEO),"Initialize isolated calendar SDL");
        check(SDL_CreateWindowAndRenderer("Agriculture calendar presentation",1280,800,SDL_WINDOW_RESIZABLE,&window,&renderer),
              "Create isolated calendar renderer");
        auto session=oe::maps::load_stored_map_session(temp.root/"data","Cities/A.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        auto owner=oe::SandboxView::create_original_plantations(std::move(session),std::nullopt,false);auto& view=*owner;
        view.configure_save(temp.root/"data","Cities/A.map",temp.root/"calendar.save.json");view.initialize(window,renderer);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(view,focus);
        check(view.paused() && view.world().rule_version()==25,"Calendar presentation changed explicit historical generation");
        const auto buy=[&](sim::CommandType type,sim::Cell cell,std::optional<sim::BuildingId> parent={}) {
            sim::Command command{type,cell};if(parent)command.building_id=*parent;
            const auto result=view.execute(command);
            if(!result.accepted || !result.changed)throw std::runtime_error("Calendar paid construction "+
                std::to_string(static_cast<unsigned>(type))+" at "+std::to_string(cell.x)+","+
                std::to_string(cell.y)+": "+result.reason);
            return *view.world().building_owner_at(cell);
        };
        for(int x=87;x<=151;++x)if(!view.world().road_cell({x,110}))buy(sim::CommandType::PlaceRoad,{x,110});
        for(int x=87;x<=138;x+=3)buy(sim::CommandType::PlaceHousehold,{x,108});
        const auto food=buy(sim::CommandType::PlaceOriginalFoodFarm,{102,111});
        const auto wheat=buy(sim::CommandType::PlaceOriginalWheatField,{105,111},food);
        const auto millet=buy(sim::CommandType::PlaceOriginalFoodFarm,{96,111});
        buy(sim::CommandType::PlaceOriginalMilletField,{95,111},millet);
        const auto locked_food=buy(sim::CommandType::PlaceOriginalFoodFarm,{112,111});
        const auto rice=buy(sim::CommandType::PlaceOriginalRiceField,{115,111},locked_food);
        buy(sim::CommandType::PlaceOriginalCabbageField,{116,112},locked_food);
        buy(sim::CommandType::PlaceOriginalSoybeanField,{117,113},locked_food);
        const auto hemp=buy(sim::CommandType::PlaceOriginalHempFarm,{122,111});
        const auto hemp_field=buy(sim::CommandType::PlaceOriginalHempField,{124,111},hemp);
        std::array<sim::BuildingId,3> sources{},plants{};
        constexpr std::array source_commands{sim::CommandType::PlaceOriginalTeaCuringShed,
            sim::CommandType::PlaceOriginalLacquerRefinery,sim::CommandType::PlaceOriginalSilkwormShed};
        constexpr std::array plant_commands{sim::CommandType::PlaceOriginalTeaBush,
            sim::CommandType::PlaceOriginalLacquerTree,sim::CommandType::PlaceOriginalMulberryTree};
        for(std::size_t index=0;index<3;++index) {
            const int x=126+static_cast<int>(index)*10;
            sources[index]=buy(source_commands[index],{x,111});plants[index]=buy(plant_commands[index],{x+4,115},sources[index]);
        }
        buy(sim::CommandType::PlaceFireWatch,{110,115});buy(sim::CommandType::PlaceFireWatch,{128,115});
        check(!view.world().total_population(),"Paid calendar construction granted workers");
        for(const auto id:{wheat,rice,hemp,hemp_field,plants[0],plants[1],plants[2],sources[0]}) {
            select(view,renderer,id);calendar_matches_world(view);
            const auto layout=view.inspector_layout();
            check(layout.detail_y<=layout.list_y,"Agriculture seasonal status is hidden below the building list");
            pure_frames(view);
        }
        select(view,renderer,rice);
        check(contains(view,"locked") || contains(view,"Locked"),"Rice inspector hides inactive production");
        const auto start=view.world().snapshot();view.update(.25,{});
        check(view.world().snapshot()==start,"Paused calendar progressed during an ordinary frame update");
        key(view,SDLK_SPACE);view.update(.05,{});key(view,SDLK_SPACE);
        check(view.world().ticks()==1,"Normal1x update did not advance exactly one shared tick");
        key(view,SDLK_PLUS);key(view,SDLK_SPACE);view.update(.05,{});key(view,SDLK_SPACE);
        check(view.world().ticks()==3,"Normal2x update did not advance the same calendar twice");
        key(view,SDLK_PLUS);key(view,SDLK_SPACE);view.update(.05,{});key(view,SDLK_SPACE);
        check(view.world().ticks()==7,"Normal4x update did not advance the same calendar four times");
        for(const std::uint64_t boundary:{19,20,26,27,50,51,815,816,9791,9792}) {
            while(view.world().ticks()<boundary)view.tick_once();calendar_matches_world(view);
            if(boundary==50 || boundary==815 || boundary==9791) {
                const auto before=view.world().snapshot();key(view,SDLK_F5);key(view,SDLK_PERIOD);
                check(view.world().ticks()==boundary+1,"Ordinary Step failed at calendar boundary");calendar_matches_world(view);
                key(view,SDLK_F9);check(view.world().snapshot()==before,"F5/F9 changed calendar or paid production state");
                calendar_matches_world(view);
            }
        }
        const auto output=fs::canonical(fs::path(__FILE__).parent_path().parent_path())/".local/agri-06/calendar-presentation";
        fs::create_directories(output);
        for(const auto size:{sim::Cell{1280,800},sim::Cell{1600,900},sim::Cell{1920,1080}}) {
            check(SDL_SetWindowSize(window,size.x,size.y),"Calendar test resize failed");
            SDL_Event resized{};resized.type=SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED;event(view,resized);view.update(0,{});
            for(const auto id:{rice,hemp_field,plants[1]}) {
                select(view,renderer,id);calendar_matches_world(view);pure_frames(view);
                capture(view,renderer,output/(std::to_string(size.x)+"-"+std::to_string(static_cast<unsigned>(id))+".png"));
            }
        }
        owner.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Agriculture calendar inspectors, paid plants/Hemp/crops, boundary Step/F5/F9, pause/speed and pure frames PASS\n";
        return 0;
    } catch(const std::exception& error) {
        oe::performance::set_enabled(false);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
