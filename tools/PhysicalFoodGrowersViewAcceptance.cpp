// Explicit paid schema47 checkpoint verification through production handlers.
// Scripted SDL evidence remains separate from physical input and human acceptance.
#include "FoodNativePlaytest.h"
#include "core/PerformanceDiagnostics.h"

#include <iterator>
#include <set>

namespace {
namespace p=oe_food_playtest;
namespace perf=oe::performance;
namespace save=oe::persistence;
using p::require;
constexpr const char* usage="openemperor-physical-food-growers-view-acceptance --data ABS --resources ABS --input EXACT47_GROWER_PHASE --output NEW_ABS_.local/agri-mac06/SUBDIR [--renderer software|metal] [--phase outward|waiting|working|loaded]";
bool watch_native=false;
void no_symlinks(const fs::path& path) {
    fs::path cursor;
    for(const auto& part:fs::absolute(path)) {
        cursor/=part;std::error_code error;const auto status=fs::symlink_status(cursor,error);
        require(error==std::errc::no_such_file_or_directory||!error,"Cannot inspect input ancestry");
        require(error||!fs::is_symlink(status),"Acceptance inputs must not traverse symlinks");
    }
}
struct Options {fs::path data,resources,input,output;std::string renderer="software",phase="loaded";};
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
        else if(name=="--phase")result.phase=value;
        else throw std::runtime_error("Unknown option "+name);
    }
    require(result.data.is_absolute()&&result.resources.is_absolute()&&result.input.is_absolute()&&result.output.is_absolute(),usage);
    require(result.renderer=="software"||result.renderer=="metal","Renderer must be software or metal");
    require(result.phase=="outward"||result.phase=="waiting"||result.phase=="working"||result.phase=="loaded","Unknown Grower phase");
    no_symlinks(result.data);no_symlinks(result.resources);no_symlinks(result.input);
    result.data=fs::canonical(result.data);result.resources=fs::canonical(result.resources);result.input=fs::canonical(result.input);
    require(!p::below(result.input,result.data),"Explicit checkpoint must be outside Original data");return result;
}
std::string bytes(const fs::path& path,std::uintmax_t maximum) {
    no_symlinks(path);
    require(fs::is_regular_file(path)&&fs::file_size(path)>0&&fs::file_size(path)<=maximum,"Protected input exceeds its size bound");
    std::ifstream file(path,std::ios::binary);require(bool(file),"Cannot read protected input "+path.string());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
struct InputGuard {
    fs::path path;std::string literal;fs::perms permissions;fs::file_time_type modified;std::uintmax_t maximum;
    explicit InputGuard(fs::path file,std::uintmax_t bound=8U*1024U*1024U):path(std::move(file)),literal(bytes(path,bound)),
        permissions(fs::status(path).permissions()),modified(fs::last_write_time(path)),maximum(bound) {}
    void unchanged()const {
        require(bytes(path,maximum)==literal&&fs::status(path).permissions()==permissions&&fs::last_write_time(path)==modified,
            "Protected input bytes, permissions or modification time changed: "+path.string());
    }
};
struct SourceGuard {
    InputGuard model,map;
    SourceGuard(const fs::path& data,const fs::path& relative):model(data/"Model/EmperorBuildingModels.txt",64U*1024U*1024U),
        map(data/relative,64U*1024U*1024U) {no_symlinks(model.path);no_symlinks(map.path);}
    void unchanged()const {model.unchanged();map.unchanged();}
};
void user_takeover() {
    SDL_Event value{};
    while(SDL_PollEvent(&value)) {
        const bool motion=value.type==SDL_EVENT_MOUSE_MOTION&&(value.motion.xrel!=0||value.motion.yrel!=0);
        const bool input=value.type==SDL_EVENT_KEY_DOWN||value.type==SDL_EVENT_KEY_UP||value.type==SDL_EVENT_TEXT_INPUT||
            value.type==SDL_EVENT_MOUSE_BUTTON_DOWN||value.type==SDL_EVENT_MOUSE_BUTTON_UP||value.type==SDL_EVENT_MOUSE_WHEEL||motion||
            value.type==SDL_EVENT_FINGER_DOWN||value.type==SDL_EVENT_WINDOW_FOCUS_LOST;
        if(value.type==SDL_EVENT_QUIT||value.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED||(watch_native&&input))
            throw std::runtime_error("Stopped scripted verification because native user input, focus takeover or window closure was observed");
    }
}
void event(oe::SandboxView& view,SDL_Event value) {
    user_takeover();bool running=true;view.handle_event(value,running);require(running,"Scripted event closed the view");user_takeover();
}
void key(oe::SandboxView& view,SDL_Keycode code) {user_takeover();p::key(view,code);user_takeover();}
void click(oe::SandboxView& view,SDL_Renderer* renderer,oe::scene::Point point) {
    float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y),SDL_GetError());
    SDL_Event value{};value.type=SDL_EVENT_MOUSE_MOTION;value.motion.x=x;value.motion.y=y;event(view,value);view.update(0);
    value={};value.type=SDL_EVENT_MOUSE_BUTTON_DOWN;value.button.button=SDL_BUTTON_LEFT;value.button.down=true;value.button.x=x;value.button.y=y;event(view,value);
    value.type=SDL_EVENT_MOUSE_BUTTON_UP;value.button.down=false;event(view,value);view.update(0);
}
void select(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId id,std::uint32_t border) {
    user_takeover();view.set_tool(5);const auto& body=view.world().building(id);
    const auto front=sim::building_front_cell(view.world().profile(),view.world().rule_version(),body.kind,body.cell);
    auto target=oe::maps::terrain_ground({static_cast<unsigned>(front.x),static_cast<unsigned>(front.y)},border);
    target.y-=16;p::center(view,target);user_takeover();require(view.render(),SDL_GetError());
    bool selected=false;
    for(const auto cell:view.world().building_cells(id)) {
        const auto ground=oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border);
        const auto center=view.camera().world_to_screen(ground);
        for(const int dy:{0,-8,-16,-24,-32,-48,-64,8,16}) {
            user_takeover();
            for(const int dx:{0,-4,4,-8,8}) {
                const oe::scene::Point point{center.x+dx*view.camera().zoom,center.y+dy*view.camera().zoom};
                if(!view.layout().map.contains(point.x,point.y))continue;
                const auto picked=view.pick(point);if(!picked||view.world().building_owner_at(*picked)!=id)continue;
                click(view,renderer,point);selected=view.selected_building()==id;if(selected)break;
            }
            if(selected)break;
        }
        if(selected)break;
    }
    require(selected,"Paid Farm/Field could not be selected through production picking");require(view.render(),SDL_GetError());
}
struct Targets {sim::BuildingId farm{},field{};sim::CourierId courier{};};
bool matches(const sim::PhysicalFoodGrowerTrip& trip,const sim::CourierState& actor,const std::string& phase) {
    if(trip.returned_tick)return false;
    if(phase=="outward")return trip.phase==sim::PhysicalFoodGrowerPhase::Outward && actor.edge_progress>0;
    if(phase=="waiting")return trip.phase==sim::PhysicalFoodGrowerPhase::Waiting && trip.wait_left>0;
    if(phase=="working")return trip.phase==sim::PhysicalFoodGrowerPhase::Working && trip.work_ticks>0;
    return trip.phase==sim::PhysicalFoodGrowerPhase::Returning && sim::physical_food_trip_cargo(trip)==1 && actor.edge_progress>0;
}
Targets targets(const sim::World& world,const std::string& phase) {
    for(const auto& farm:world.physical_food_growers_state()->farms)if(farm.latest_trip && matches(*farm.latest_trip,world.courier(farm.courier_id),phase)) {
        std::set<sim::AgricultureCrop> crops;for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm.farm_id)crops.insert(field.crop);
        if(crops.size()>=3)return {farm.farm_id,farm.latest_trip->field_id,farm.courier_id};
    }
    throw std::runtime_error("Checkpoint requires one paid three-crop Farm with actual requested Grower phase");
}
Json cargo(const sim::World& world,const Targets& chosen) {
    for(const auto& farm:world.physical_food_growers_state()->farms)if(farm.farm_id==chosen.farm) {
        const auto& trip=*farm.latest_trip;const auto& actor=world.courier(farm.courier_id);
        require(!actor.cargo&&!actor.reserved,"Grower duplicated legacy cargo or reservation");
        require(world.physical_food_grower_cargo(actor.id)==sim::physical_food_trip_cargo(trip),"Grower cargo query differs from actual harvest/return");
        return {{"grower_id",trip.grower_id.value},{"courier",static_cast<unsigned>(actor.id)},{"crop",static_cast<unsigned>(trip.crop)},
            {"phase",static_cast<unsigned>(trip.phase)},{"wait_left",trip.wait_left},{"work_ticks",trip.work_ticks},
            {"cargo",world.physical_food_grower_cargo(actor.id)},{"return_reserved",sim::physical_food_grower_reservations(&*world.physical_food_growers_state(),chosen.farm)},
            {"edge_progress",actor.edge_progress},{"route_pending",actor.route_pending}};
    }
    throw std::runtime_error("Physical Farm disappeared");
}
bool select_grower(oe::SandboxView& view,SDL_Renderer* renderer,sim::CourierId channel) {
    const auto ground=view.courier_ground_position(channel);require(ground.has_value(),"Actual Grower has no position");
    view.set_tool(5);p::center(view,*ground);require(view.render(),SDL_GetError());const auto center=view.camera().world_to_screen(*ground);
    bool picked=false;
    for(const int dy:{0,-2,2,-18,-15,-12}) {
        if(picked)break;
        for(const int dx:{0,-2,2,8,16,24,32,40,48}) {
            const oe::scene::Point point{center.x+dx,center.y+dy};if(!view.layout().map.contains(point.x,point.y))continue;
            click(view,renderer,point);picked=view.input_diagnostic_state().selected_walker==static_cast<unsigned>(channel);
            if(picked)break;
        }
    }
    require(view.render(),SDL_GetError());return picked;
}
std::string combined(const std::vector<std::string>& lines) {
    std::string result;for(const auto& line:lines){if(!result.empty())result+=' ';result+=line;}return result;
}
Json inspector(oe::SandboxView& view,sim::BuildingId farm,std::optional<sim::BuildingId> selected_field={}) {
    const auto lines=view.inspection_lines();const auto text=combined(lines);const auto& world=view.world();
    require(view.selected_building()==selected_field.value_or(farm),"Inspector selection changed");
    for(const auto* expected:{"Farm workers","Work total","Actually harvested","Bound fields","Bound Farmhouse #","Food courier:","Grower lifetime","Actually Farm-returned"})
        require(text.find(expected)!=std::string::npos,std::string("Inspector omitted ")+expected);
    Json fields=Json::array();constexpr std::array names{"Wheat","Millet","Rice","Cabbage","Bean Curd (Soy)"};
    for(const auto& field:world.hunting_food_state()->fields)if(field.farm_id==farm&&(!selected_field||*selected_field==field.field_id)) {
        const auto& rows=world.multi_field_agriculture_state()->fields;
        const auto work=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.field_id==field.field_id;});
        require(work!=rows.end(),"Selected Field lost actual work authority");
        const auto progress=world.agriculture_field_progress(field.field_id);
        const auto label="Field #"+std::to_string(static_cast<unsigned>(field.field_id))+" "+names.at(static_cast<unsigned>(field.crop))+" | progress "+std::to_string(progress)+"/80";
        require(text.find(label)!=std::string::npos&&text.find("Work total "+std::to_string(work->productive_ticks))!=std::string::npos&&
            text.find("Actually harvested "+std::to_string(field.harvested))!=std::string::npos,
            "Inspector disagrees with sole per-field work/progress");
        fields.push_back({{"field",static_cast<unsigned>(field.field_id)},{"crop",static_cast<unsigned>(field.crop)},
            {"progress",progress},{"productive_ticks",work->productive_ticks},{"harvested",field.harvested}});
    }
    require(!fields.empty(),"Inspector omitted all paid fields");return {{"lines",lines},{"fields",fields}};
}
Json pure_frames(oe::SandboxView& view) {
    const auto before=view.world().snapshot();const auto prepared=view.agriculture_inspection_prepare_count();
    perf::set_enabled(true);perf::reset();
    for(unsigned frame=0;frame<3;++frame) {user_takeover();view.update(0);require(view.render(),SDL_GetError());(void)view.inspection_lines();}
    Json counters=Json::object();
    for(std::size_t i=0;i<perf::counter_names.size();++i) {
        const auto count=perf::counter(static_cast<perf::Counter>(i));counters[std::string(perf::counter_names[i])]=count;
        require(!count,"Paused inspector/render performed "+std::string(perf::counter_names[i]));
    }
    perf::set_enabled(false);require(view.world().snapshot()==before&&view.agriculture_inspection_prepare_count()==prepared,
        "Paused frames changed World or rebuilt inspection preparation");return {{"frames",3},{"counters",counters},{"inspection_prepare_unchanged",true}};
}
void capture(oe::SandboxView& view,SDL_Renderer* renderer,const fs::path& path) {
    user_takeover();require(SDL_GetRenderTarget(renderer)==nullptr,"Capture must begin at the ordinary window target");
    int width=0,height=0;require(SDL_GetCurrentRenderOutputSize(renderer,&width,&height)&&width>0&&height>0,"Cannot read capture dimensions");
    const auto before=view.world().snapshot();const auto camera=view.camera();
    std::unique_ptr<SDL_Texture,decltype(&SDL_DestroyTexture)> target(
        SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_TARGET,width,height),SDL_DestroyTexture);
    require(bool(target),"Cannot create owned capture target");
    struct Restore {SDL_Renderer* renderer;bool needed=true;~Restore(){if(needed)SDL_SetRenderTarget(renderer,nullptr);}} restore{renderer};
    require(SDL_SetRenderTarget(renderer,target.get()),"Cannot activate capture target");
    require(view.render()&&SDL_GetRenderTarget(renderer)==target.get(),"Production render changed capture target");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> raw(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    require(bool(raw),"Cannot read completed production capture");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> rgba(SDL_ConvertSurface(raw.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    require(rgba&&rgba->w==width&&rgba->h==height,"Capture dimensions or format differ");
    require(!fs::exists(path)&&SDL_SavePNG(rgba.get(),path.string().c_str()),"Cannot write fresh private capture PNG");
    require(SDL_SetRenderTarget(renderer,nullptr),"Cannot restore window target");restore.needed=false;
    const auto after=view.camera();require(view.world().snapshot()==before&&after.zoom==camera.zoom&&
        after.offset.x==camera.offset.x&&after.offset.y==camera.offset.y&&after.viewport_width==camera.viewport_width&&after.viewport_height==camera.viewport_height,
        "Capture changed World or camera authority");user_takeover();
}
void write_json(const fs::path& path,const Json& value) {
    require(!fs::exists(path),"Preserve existing acceptance output");std::ofstream file(path);file<<value.dump(2)<<'\n';require(bool(file),"Cannot publish private acceptance JSON");
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;std::unique_ptr<oe::SandboxView> view;fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);const InputGuard input(selected.input);const auto document=save::read_save(selected.input);
        require(document.source_schema_version==47&&document.world.rule_version==31&&document.world.multi_field_agriculture&&
            document.world.physical_food_growers&&document.world.hunting_food&&document.world.original_economy&&document.world.original_economy->model_source,
            "Input must be an exact paid Rule31/schema47 checkpoint");
        require(document.world.original_economy->model_source->sha256=="7e51df8cd49ab39fea7397b7c1a9e7edefdc98a8f09b5e2139987346b4b972e6",
            "Acceptance requires the unchanged supplied model fingerprint");
        const SourceGuard sources(selected.data,document.map_relative);output=p::output_root(selected.output,selected.data,".local/agri-mac06");
        watch_native=selected.renderer=="metal";
        if(!watch_native)require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#if defined(__APPLE__)
        else require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"cocoa"),SDL_GetError());
#else
        require(!watch_native,"Metal acceptance requires native macOS");
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,selected.renderer.c_str())&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("AGRI-MAC06 scripted paid Farm/Field verification",1440,1000,
            watch_native?SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const auto* renderer_name=SDL_GetRendererName(renderer);require(renderer_name&&selected.renderer==renderer_name,"Requested renderer was not selected");
        user_takeover();auto session=oe::maps::load_stored_map_session(selected.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
        view=oe::SandboxView::create_physical_food_growers(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(selected.data,document.map_relative,selected.input);p::configure(*view,oe::detect_and_select_visual_profiles(selected.data,selected.resources));
        view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto generation=view->load_generation();key(*view,SDLK_F9);
        require(view->load_generation()==generation+1&&view->paused()&&view->world().snapshot()==document.world,
            "F9 did not load exact supplied schema47 state: "+view->last_message());
        const auto chosen=targets(view->world(),selected.phase);const auto supplied_cargo=cargo(view->world(),chosen);input.unchanged();sources.unchanged();
        const auto manual=output/"saves/manual.save.json";view->configure_save(selected.data,document.map_relative,manual);
        const auto saved=view->save_generation();key(*view,SDLK_F5);const auto private_document=save::read_save(manual);
        require(view->save_generation()==saved+1&&private_document.source_schema_version==47&&private_document.world==document.world,
            "Private F5 changed schema47 work/cursor/cargo authority: "+view->last_message());
        key(*view,SDLK_PERIOD);const auto stepped=view->world().snapshot();require(stepped.ticks==document.world.ticks+1,"Paused Step did not advance exactly one tick");
        const auto reloaded=view->load_generation();key(*view,SDLK_F9);
        require(view->load_generation()==reloaded+1&&view->paused()&&view->world().snapshot()==document.world,"F9 did not restore exact work/cursor/cargo after Step");
        key(*view,SDLK_PERIOD);require(view->world().snapshot()==stepped,"Post-F9 single-Step continuation differs");
        key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"Second F9 changed checkpoint authority");
        key(*view,SDLK_SPACE);require(!view->paused(),"Space did not resume");key(*view,SDLK_SPACE);require(view->paused(),"Space did not pause");
        Json captures=Json::array(),frames=Json::array();bool visible_grower=false;
        for(unsigned attempt=0;attempt<3&&view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
        require(view->camera().zoom==1.,"Zoom action could not reach1x");
        for(const double zoom:{1.,2.,4.}) {
            require(std::abs(view->camera().zoom-zoom)<1e-9,"Zoom action missed1/2/4x");
            for(const int subject:{0,1,2}) {
                const bool field=subject==1;const auto id=field?chosen.field:chosen.farm;
                if(subject==2) {
                    if(!select_grower(*view,renderer,chosen.courier)) {
                        captures.push_back({{"zoom",zoom},{"selection","grower"},{"pixel_pick","OCCLUDED_OR_UNAVAILABLE"}});continue;
                    }
                    visible_grower=true;
                } else select(*view,renderer,id,border);
                const auto observed=inspector(*view,chosen.farm,field?std::optional(chosen.field):std::nullopt);
                const char* label=subject==2?"grower":field?"field":"farm";
                const auto purity=pure_frames(*view);const auto name=std::string("physical-grower-")+label+"-"+std::to_string(static_cast<int>(zoom))+"x.png";
                capture(*view,renderer,output/name);captures.push_back({{"path",name},{"zoom",zoom},{"selection",label},
                    {"building",static_cast<unsigned>(id)},{"actual_inspector",observed}});
                frames.push_back({{"zoom",zoom},{"selection",label},{"purity",purity}});
            }
            if(zoom<4.)key(*view,SDLK_Z);
        }
        require(visible_grower || (selected.phase!="loaded" && selected.phase!="working"),
            "Working/loaded checkpoint supplied no actual visible Grower marker pixels");
        input.unchanged();sources.unchanged();require(view->world().snapshot()==document.world,"Scripted UI verification changed paid authority");
        const Json report{{"format","openemperor-physical-food-growers-view-v1"},{"status","PASS"},{"rule",31},{"schema",47},
            {"renderer",renderer_name},{"video_driver",SDL_GetCurrentVideoDriver()},{"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},
            {"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"user_takeover_stops_automation",true},
            {"input",selected.input.string()},{"input_preserved",true},{"source_bytes_mode_mtime_unchanged",true},{"tick",document.world.ticks},
            {"supplied_physical_grower",supplied_cargo},{"requested_phase",selected.phase},{"grower_pixel_picked",visible_grower},{"farm",static_cast<unsigned>(chosen.farm)},{"field",static_cast<unsigned>(chosen.field)},
            {"input_f9_exact",true},{"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"pause_resume",true},
            {"captures",captures},{"pure_frames",frames}};
        write_json(output/"report.json",report);view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<report.dump(2)<<'\n';return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Physical Food Growers View acceptance FAIL: "<<error.what()<<'\n';
        if(!output.empty())try{write_json(output/"failure.json",{{"status","FAIL"},{"reason",error.what()},{"human_acceptance","NOT_RUN"}});}catch(...){}
        return 1;
    }
}
