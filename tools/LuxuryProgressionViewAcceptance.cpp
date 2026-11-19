// Explicit supplied-checkpoint SDL acceptance. Scripted production handlers
// are separate from physical OS input and unassisted human acceptance.
#include "FoodNativePlaytest.h"
#include "core/PerformanceDiagnostics.h"

#include <iterator>
#include <set>

namespace {
namespace p=oe_food_playtest;
namespace perf=oe::performance;
namespace save=oe::persistence;
using p::require;
struct Options {fs::path data,resources,input,output;std::string renderer="software";};
Options options(int argc,char** argv) {
    Options result;std::set<std::string> seen;
    for(int i=1;i<argc;++i) {
        const std::string name=argv[i];require(i+1<argc,"Missing value for "+name);const std::string value=argv[++i];
        require(seen.insert(name).second,"Duplicate option "+name);
        if(name=="--data")result.data=value;
        else if(name=="--resources")result.resources=value;
        else if(name=="--input")result.input=value;
        else if(name=="--output")result.output=value;
        else if(name=="--renderer")result.renderer=value;
        else throw std::runtime_error("Unknown option "+name);
    }
    require(result.data.is_absolute() && result.resources.is_absolute() && result.input.is_absolute() && result.output.is_absolute(),
        "Usage: openemperor-luxury-progression-view-acceptance --data ABS --resources ABS --input ABS --output NEW_ABS_.local/agri-mac04/SUBDIR [--renderer software|metal]");
    require(result.renderer=="software" || result.renderer=="metal","Renderer must be software or metal");
    result.data=fs::canonical(result.data);result.resources=fs::canonical(result.resources);result.input=fs::canonical(result.input);return result;
}
std::string bytes(const fs::path& path) {
    require(fs::is_regular_file(path) && fs::file_size(path)<=64U*1024U*1024U,"Protected input exceeds the private verification size bound");
    std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot read protected input "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
struct InputGuard {
    fs::path path;std::string literal;fs::perms permissions;fs::file_time_type modified;
    explicit InputGuard(fs::path file):path(std::move(file)),literal(bytes(path)),permissions(fs::status(path).permissions()),modified(fs::last_write_time(path)){}
    void unchanged()const {
        require(bytes(path)==literal && fs::status(path).permissions()==permissions && fs::last_write_time(path)==modified,
            "Protected input bytes, permissions or modification time changed: "+path.string());
    }
};
void event(oe::SandboxView& view,SDL_Event value) {bool running=true;view.handle_event(value,running);require(running,"Scripted event closed the view");}
void user_takeover() {
    SDL_Event value{};
    while(SDL_PollEvent(&value))
        if(value.type==SDL_EVENT_QUIT || value.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED || value.type==SDL_EVENT_KEY_DOWN ||
           value.type==SDL_EVENT_MOUSE_BUTTON_DOWN || value.type==SDL_EVENT_MOUSE_WHEEL)
            throw std::runtime_error("Stopped scripted acceptance because native user input or window closure was observed");
}
void click(oe::SandboxView& view,SDL_Renderer* renderer,oe::scene::Point screen) {
    float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(screen.x),static_cast<float>(screen.y),&x,&y),SDL_GetError());
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.down=true;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;value.button.down=false;event(view,value);
}
sim::BuildingId house(const sim::World& world) {
    require(world.original_economy_state() && !world.original_economy_state()->houses.empty(),"Checkpoint has no paid House");
    const sim::OriginalHouseState* best=nullptr;
    for(const auto& candidate:world.original_economy_state()->houses)if(candidate.house.occupants>0 &&
        (!best || candidate.house.type>best->house.type))best=&candidate;
    require(best,"Checkpoint has no actual occupied House");return best->building_id;
}
void select_house(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id,std::uint32_t border) {
    view.set_tool(5);const auto& body=view.world().building(id);
    const auto front=sim::building_front_cell(view.world().profile(),view.world().rule_version(),body.kind,body.cell);
    auto target=oe::maps::terrain_ground({static_cast<unsigned>(front.x),static_cast<unsigned>(front.y)},border);
    target.y-=20;p::center(view,target);require(view.render(),SDL_GetError());
    bool selected=false;
    for(const auto cell:view.world().building_cells(id)) {
        auto ground=oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border);ground.y-=20;
        const auto center=view.camera().world_to_screen(ground);
        for(int dy=-8;dy<=8 && !selected;dy+=2)for(int dx=-8;dx<=8 && !selected;dx+=2) {
            const oe::scene::Point point{center.x+dx,center.y+dy};
            if(!view.layout().map.contains(point.x,point.y))continue;
            const auto picked=view.pick(point);if(!picked || view.world().building_owner_at(*picked)!=id)continue;
            click(view,renderer,point);selected=view.selected_building()==id && view.original_house_inspector_active();
        }
        if(selected)break;
    }
    require(selected,"Actual House could not be selected through production picking");
    const auto tab=view.original_house_section_rect(oe::SandboxView::OriginalHouseSection::Goods);
    require(tab.w>0 && tab.h>0,"Actual House Goods tab is absent");click(view,renderer,{tab.x+tab.w*.5,tab.y+tab.h*.5});
    require(view.original_house_section()==oe::SandboxView::OriginalHouseSection::Goods,"House Goods tab did not select through production input");
}
Json cargo(const sim::World& world) {
    Json result=Json::array();bool moving=false;
    for(const auto& item:world.original_goods_state()->cargo)if(item.amount>0) {
        const auto& actor=world.courier(item.transfer.courier_id);
        moving|=actor.edge_progress>0;
        result.push_back({{"courier",static_cast<unsigned>(actor.id)},{"good",static_cast<unsigned>(item.good)},
            {"amount",item.amount},{"edge_progress",actor.edge_progress}});
    }
    require(!result.empty() && moving,"Supplied checkpoint has no moving physical goods cargo");return result;
}
Json goods(const sim::World& world,sim::BuildingId id) {
    const auto actual=oe::original_house_ui::inspect_house(world,id,oe::original_house_ui::Section::Goods);
    require(actual.applicable && actual.hunting_food.has_value(),"Selected House inspector lost Rule29's sole seven-kind Food authority");
    Json result=Json::array();
    for(const auto& row:actual.goods) {
        require(row.stock==orig::goods_stock(actual.housing.actual,row.good),"Inspector stock differs from sole actual House stock");
        result.push_back({{"good",static_cast<unsigned>(row.good)},{"stock",row.stock},{"consumed",row.consumed},
            {"incoming_reserved",row.incoming_reserved},{"in_transit",row.in_transit},{"delivered",row.delivered}});
    }
    require(result.size()==orig::goods_count,"Inspector omitted a household good");return result;
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    require(SDL_GetRenderTarget(renderer)==nullptr,"Capture must begin at the ordinary window target");
    int width=0,height=0;
    require(SDL_GetCurrentRenderOutputSize(renderer,&width,&height) && width>0 && height>0,"Cannot read actual capture dimensions");
    const auto before=view.world().snapshot();const auto camera=view.camera();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    require(bool(target),"Cannot create owned RGBA32 capture target");
    struct RestoreTarget {
        SDL_Renderer* renderer;bool needed=true;
        ~RestoreTarget(){if(needed)SDL_SetRenderTarget(renderer,nullptr);}
    } restore{renderer};
    require(SDL_SetRenderTarget(renderer,target.get()),"Cannot activate owned capture target");
    // SandboxView skips Present while an explicit texture target is active.
    // Read its completed production image before returning to the window.
    require(view.render() && SDL_GetRenderTarget(renderer)==target.get(),"Production render changed the owned capture target");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    require(bool(raw),"Cannot read production pixels before Present");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> rgba(
        SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    require(rgba && rgba->w==width && rgba->h==height,"Capture dimensions or format differ from the window output");
    require(!fs::exists(path) && SDL_SavePNG(rgba.get(),path.string().c_str()),"Cannot write fresh private capture PNG");
    require(SDL_SetRenderTarget(renderer,nullptr) && SDL_GetRenderTarget(renderer)==nullptr,"Cannot restore the ordinary window target");
    restore.needed=false;
    const auto after=view.camera();
    require(view.world().snapshot()==before && after.zoom==camera.zoom && after.offset.x==camera.offset.x &&
        after.offset.y==camera.offset.y && after.viewport_width==camera.viewport_width && after.viewport_height==camera.viewport_height,
        "Capture changed World authority or the camera/viewport");
}
void write_json(const fs::path& path,const Json& value) {std::ofstream file(path);file<<value.dump(2)<<'\n';require(bool(file),"Cannot write private acceptance JSON");}
}

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;std::unique_ptr<oe::SandboxView> view;fs::path output;
    try {
        const auto selected=options(argc,argv);const InputGuard input(selected.input),model(selected.data/"Model/EmperorBuildingModels.txt");
        const auto document=save::read_save(selected.input);
        require(document.source_schema_version==45 && document.world.rule_version==29 && document.world.original_economy &&
            document.world.original_economy->model_source,"Input must be an exact paid Rule29/schema45 checkpoint");
        require(document.world.original_economy->model_source->sha256=="7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6",
            "Unchanged supplied-model acceptance requires the original supported model fingerprint");
        const InputGuard map(selected.data/document.map_relative);output=p::output_root(selected.output,selected.data,".local/agri-mac04");
        const bool native=selected.renderer=="metal";
        if(!native)require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#if defined(__APPLE__)
        else require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"cocoa"),SDL_GetError());
#else
        require(!native,"Metal acceptance requires native macOS");
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,selected.renderer.c_str()) && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("AGRI-MAC04 scripted checkpoint verification",1440,1000,
            native ? SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const auto* renderer_name=SDL_GetRendererName(renderer);require(renderer_name && selected.renderer==renderer_name,"Requested SDL renderer was not selected");
        auto session=oe::maps::load_stored_map_session(selected.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        const auto border=session.plan.border;
        view=oe::SandboxView::create_playable_hunting(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(selected.data,document.map_relative,selected.input);p::configure(*view,oe::detect_and_select_visual_profiles(selected.data,selected.resources));
        view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto load_generation=view->load_generation();p::key(*view,SDLK_F9);
        require(view->load_generation()==load_generation+1 && view->paused() && view->world().snapshot()==document.world,
            "Explicit input F9 did not load the exact schema45 World: "+view->last_message());
        const auto supplied_cargo=cargo(view->world());input.unchanged();model.unchanged();map.unchanged();
        // Subsequent F5 writes only the fresh guarded private output path.
        const auto manual=output/"saves/manual.save.json";view->configure_save(selected.data,document.map_relative,manual);
        const auto save_generation=view->save_generation();p::key(*view,SDLK_F5);
        require(view->save_generation()==save_generation+1 && save::read_save(manual).source_schema_version==45 &&
            save::read_save(manual).world==document.world,"F5 changed schema45/cargo authority: "+view->last_message());
        p::key(*view,SDLK_PERIOD);const auto stepped=view->world().snapshot();require(stepped.ticks==document.world.ticks+1,"Paused Step did not advance exactly one tick");
        const auto reload_generation=view->load_generation();p::key(*view,SDLK_F9);
        require(view->load_generation()==reload_generation+1 && view->paused() && view->world().snapshot()==document.world,"F9 failed exact cargo restore after Step");
        p::key(*view,SDLK_PERIOD);require(view->world().snapshot()==stepped,"Post-F9 Step replay differs from independent previous continuation");
        p::key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"Second F9 changed the earned checkpoint");
        p::key(*view,SDLK_SPACE);require(!view->paused(),"Space did not resume");p::key(*view,SDLK_SPACE);require(view->paused(),"Space did not pause");
        const auto target=house(view->world());Json captures=Json::array(),frames=Json::array();
        for(int attempt=0;attempt<3 && view->camera().zoom!=1.0;++attempt)p::key(*view,SDLK_Z);
        require(view->camera().zoom==1.0,"Zoom action could not reach1x");
        for(const double zoom:{1.,2.,4.}) {
            if(native)user_takeover();require(std::abs(view->camera().zoom-zoom)<1e-9,"Zoom action missed1/2/4x");
            select_house(*view,renderer,target,border);const auto observed=goods(view->world(),target);
            const auto before=view->world().snapshot();perf::set_enabled(true);perf::reset();
            for(int frame=0;frame<3;++frame) {view->update(0);require(view->render(),SDL_GetError());(void)view->original_house_section_lines();}
            Json counters=Json::object();
            for(std::size_t i=0;i<perf::counter_names.size();++i) {
                const auto count=perf::counter(static_cast<perf::Counter>(i));counters[std::string(perf::counter_names[i])]=count;
                require(count==0,"Pure paused inspector/render performed "+std::string(perf::counter_names[i]));
            }
            perf::set_enabled(false);require(view->world().snapshot()==before,"Inspector/render changed World authority");
            const auto path=output/("luxury-house-"+std::to_string(static_cast<int>(zoom))+"x.png");capture(*view,renderer,path);
            captures.push_back({{"path",path.filename().string()},{"zoom",zoom},{"house",static_cast<unsigned>(target)},
                {"type",static_cast<unsigned>(view->world().original_housing_diagnosis(target).actual.type)},{"goods",observed},
                {"inspector_lines",view->original_house_section_lines()}});frames.push_back({{"zoom",zoom},{"frames",3},{"counters",counters}});
            if(zoom<4.)p::key(*view,SDLK_Z);
        }
        input.unchanged();model.unchanged();map.unchanged();require(view->world().snapshot()==document.world,"Scripted UI verification changed checkpoint authority");
        const Json report{{"format","openemperor-luxury-progression-view-v1"},{"status","PASS"},{"rule",29},{"schema",45},
            {"renderer",renderer_name},{"video_driver",SDL_GetCurrentVideoDriver()},{"tier",native ? "NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},
            {"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"input",selected.input.string()},{"input_preserved",true},
            {"model_and_map_preserved",true},{"tick",document.world.ticks},{"supplied_cargo",supplied_cargo},{"input_f9_exact",true},
            {"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"pause_resume",true},{"captures",captures},{"pure_frames",frames}};
        write_json(output/"report.json",report);view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<report.dump(2)<<'\n';return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Luxury progression View acceptance FAIL: "<<error.what()<<'\n';
        if(!output.empty())try {write_json(output/"failure.json",{{"status","FAIL"},{"reason",error.what()},{"human_acceptance","NOT_RUN"}});}catch(...){}
        return 1;
    }
}
