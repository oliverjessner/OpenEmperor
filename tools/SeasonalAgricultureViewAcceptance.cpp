// Explicit paid schema48 seasonal checkpoint verification through production handlers.
// Scripted SDL evidence remains separate from physical input and human acceptance.
#include "FoodNativePlaytest.h"
#include "app/MenuSession.h"
#include "core/PerformanceDiagnostics.h"

#include <iterator>
#include <set>

namespace {
namespace p=oe_food_playtest;
namespace perf=oe::performance;
namespace save=oe::persistence;
using p::require;
constexpr const char* usage="openemperor-seasonal-agriculture-view-acceptance --data ABS --resources ABS --input EXACT48_CHECKPOINT --output NEW_ABS_.local/agri-mac07/SUBDIR [--renderer software|metal] [--phase checkpoint|outward|waiting|working|food-loaded|hemp-loaded|plant-loaded|ready|expiry|boundary]";
bool watch_native=false;
void no_symlinks(const fs::path& path) {
    fs::path cursor;
    for(const auto& part:fs::absolute(path)) {
        cursor/=part;std::error_code error;const auto status=fs::symlink_status(cursor,error);
        require(error==std::errc::no_such_file_or_directory||!error,"Cannot inspect input ancestry");
        require(error||!fs::is_symlink(status),"Acceptance inputs must not traverse symlinks");
    }
}
struct Options {fs::path data,resources,input,output;std::string renderer="software",phase="checkpoint";};
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
    require(std::set<std::string>{"checkpoint","outward","waiting","working","food-loaded","hemp-loaded","plant-loaded","ready","expiry","boundary"}.contains(result.phase),"Unknown seasonal checkpoint phase");
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
void show_inspector_row(oe::SandboxView& view,SDL_Renderer* renderer,const std::string& wanted) {
    const auto lines=view.inspection_lines();const auto found=std::find_if(lines.begin(),lines.end(),
        [&](const auto& line){return line.find(wanted)!=std::string::npos;});
    require(found!=lines.end(),"Actual cargo row is absent from its inspector");
    const auto index=static_cast<std::size_t>(found-lines.begin());
    for(unsigned step=0;step<512&&view.inspector_layout().detail_row_rect(index).h<=0;++step) {
        const auto content=view.inspector_layout().content;float x=0,y=0;
        require(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&x,&y),SDL_GetError());
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=-1;wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;
        event(view,wheel);
    }
    require(view.inspector_layout().detail_row_rect(index).h>0,"Actual cargo row cannot be reached through normal inspector scrolling");
    require(view.render(),SDL_GetError());
}
std::string combined(const std::vector<std::string>& lines) {
    std::string result;for(const auto& line:lines){if(!result.empty())result+=' ';result+=line;}return result;
}
std::array<sim::BuildingId,9> subjects(const sim::World& world) {
    std::array<sim::BuildingId,9> ids{};
    for(const auto& field:world.hunting_food_state()->fields)if(static_cast<unsigned>(field.crop)<5 && !static_cast<unsigned>(ids[static_cast<unsigned>(field.crop)]))
        ids[static_cast<unsigned>(field.crop)]=field.field_id;
    for(const auto& farm:world.original_production_state()->hemp_farms)if(farm.field_id && !static_cast<unsigned>(ids[5]))ids[5]=*farm.field_id;
    for(const auto& plant:world.plantations_state()->plants) {
        const auto kind=world.building(plant.plant_id).kind;
        const unsigned index=kind==sim::Object::OriginalTeaBush26?6:kind==sim::Object::OriginalLacquerTree27?7:8;
        if(!static_cast<unsigned>(ids[index]))ids[index]=plant.plant_id;
    }
    require(std::all_of(ids.begin(),ids.end(),[](auto id){return static_cast<unsigned>(id)>0;}),"Checkpoint must contain all nine actual paid Food/Hemp/Tea/Lacquer/Mulberry kinds");
    return ids;
}
Json seasonal_observation(const sim::World& world,sim::BuildingId id) {
    const auto status=world.agriculture_season_status(id);require(status&&status->seasonal_production,"Paid subject lost seasonal status");
    const auto& s=*status;
    return {{"subject",static_cast<unsigned>(id)},{"producer",static_cast<unsigned>(s.producer)},{"model",static_cast<unsigned>(s.model)},
        {"tick",world.ticks()},{"elapsed_year",s.calendar.position.year},{"slot",s.calendar.position.month},
        {"subslot",s.calendar.position.step},{"phase",s.calendar.position.phase},{"growing",s.growing},{"harvest_allowed",s.harvest_allowed},
        {"progress",s.productive_progress},{"ready",s.harvest_ready},{"harvest_progress",s.harvest_progress},
        {"wasted_work",s.wasted_work},{"expired_ready",s.expired_ready},{"harvested",s.harvested},{"returned",s.returned},
        {"ready_expiry_tick",s.ready_expiry_tick?Json(*s.ready_expiry_tick):Json(nullptr)},
        {"next_opening_tick",s.next_opening_tick?Json(*s.next_opening_tick):Json(nullptr)},
        {"water_available",s.water_available},{"yield_effect_active",s.yield_effect_active}};
}
Json checkpoint_evidence(const sim::World& world,const std::string& requested) {
    bool outward=false,waiting=false,working=false,food_loaded=false,hemp_loaded=false,plant_loaded=false,ready=false,expired=false;
    Json live=Json::array(),observations=Json::array();
    for(const auto id:subjects(world)) {const auto row=seasonal_observation(world,id);ready|=row.at("ready").get<bool>();expired|=row.at("expired_ready").get<std::uint64_t>()>0;observations.push_back(row);}
    for(const auto& farm:world.physical_food_growers_state()->farms)if(farm.latest_trip&&!farm.latest_trip->returned_tick) {
        const auto& t=*farm.latest_trip;const auto& actor=world.courier(farm.courier_id);
        outward|=t.phase==sim::PhysicalFoodGrowerPhase::Outward&&actor.edge_progress>0;
        waiting|=t.phase==sim::PhysicalFoodGrowerPhase::Waiting&&t.wait_left>0;
        working|=t.phase==sim::PhysicalFoodGrowerPhase::Working&&t.work_ticks>0;
        food_loaded|=world.physical_food_grower_cargo(actor.id)==1;
        require(!actor.cargo&&!actor.reserved,"Physical Food Grower duplicated legacy cargo");
        live.push_back({{"kind","food"},{"courier",static_cast<unsigned>(actor.id)},{"grower",t.grower_id.value},
            {"phase",static_cast<unsigned>(t.phase)},{"cargo",world.physical_food_grower_cargo(actor.id)},{"edge_progress",actor.edge_progress}});
    }
    for(const auto& farm:world.seasonal_agriculture_state()->hemp_farms)if(farm.latest_trip&&!farm.latest_trip->returned_tick) {
        const auto& t=*farm.latest_trip;const auto& actor=world.courier(farm.courier_id);hemp_loaded|=world.seasonal_hemp_grower_cargo(actor.id)==1;
        require(!actor.cargo&&!actor.reserved,"Physical Hemp Grower duplicated legacy cargo");
        live.push_back({{"kind","hemp"},{"courier",static_cast<unsigned>(actor.id)},{"grower",t.grower_id.value},
            {"cargo",world.seasonal_hemp_grower_cargo(actor.id)},{"ready_tick",t.ready_tick},{"edge_progress",actor.edge_progress}});
    }
    for(const auto& source:world.plantations_state()->sources)if(source.latest_trip&&!source.latest_trip->returned_tick) {
        const auto& t=*source.latest_trip;const auto& actor=world.courier(source.courier_id);plant_loaded|=world.plantation_grower_cargo(actor.id)==1;
        live.push_back({{"kind","plantation"},{"courier",static_cast<unsigned>(actor.id)},{"grower",static_cast<std::uint64_t>(t.grower_id)},
            {"cargo",world.plantation_grower_cargo(actor.id)},{"ready_tick",t.ready_tick},{"edge_progress",actor.edge_progress}});
    }
    const auto remainder=world.ticks()%sim::seasonal_month_ticks;
    const bool boundary=remainder==0||remainder==sim::seasonal_month_ticks-1;
    require(requested=="checkpoint"||(requested=="outward"&&outward)||(requested=="waiting"&&waiting)||(requested=="working"&&working)||
        (requested=="food-loaded"&&food_loaded)||(requested=="hemp-loaded"&&hemp_loaded)||(requested=="plant-loaded"&&plant_loaded)||
        (requested=="ready"&&ready)||(requested=="expiry"&&expired)||(requested=="boundary"&&boundary),"Actual supplied checkpoint does not match requested seasonal phase");
    return {{"requested",requested},{"outward",outward},{"waiting",waiting},{"working",working},{"food_loaded",food_loaded},
        {"hemp_loaded",hemp_loaded},{"plantation_loaded",plant_loaded},{"ready",ready},{"expired_ready_observed",expired},
        {"boundary",boundary},{"live_cargo",live},{"subjects",observations}};
}
Json inspector(oe::SandboxView& view,sim::BuildingId id) {
    require(!view.debug_diagnostics()&&view.selected_building()==id,"Normal seasonal inspector selection changed");
    const auto lines=view.inspection_lines();const auto text=combined(lines);const auto status=view.world().agriculture_season_status(id);
    require(status&&status->seasonal_production,"Selected paid subject lost seasonal authority");
    const auto tick=view.world().ticks();
    for(const auto& expected:{"Elapsed cycle "+std::to_string(tick/9792),"Outer slot "+std::to_string((tick/816)%12),
            "Subslot "+std::to_string((tick/51)%16),"Phase "+std::to_string(tick%51),std::string("Growing slots"),
            std::string("Next growing opening"),std::string("Wasted work"),std::string("expired ready"),std::string("yield inactive"),std::string("AUTHORED")})
        require(text.find(expected)!=std::string::npos,"Normal seasonal inspector omitted "+expected);
    require(text.find("Actual work "+std::to_string(status->productive_progress)+"/80")!=std::string::npos,"Inspector work differs from actual field/plant/source progress");
    if(status->ready_expiry_tick)require(text.find("Ready crop expires before tick "+std::to_string(*status->ready_expiry_tick))!=std::string::npos,"Inspector ready expiry differs from actual cycle");
    require(text.find("Original period query is not its quantity callback veto")!=std::string::npos,"Inspector conflates source query and authored work gate");
    return {{"lines",lines},{"actual",seasonal_observation(view.world(),id)}};
}
Json regular_z(const fs::path& data,const fs::path& resources,const fs::path& output,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,output/"menu",std::make_unique<oe::menu::NativeDialog>(),resources);menu.initialize(window,renderer);
    require(menu.state()==oe::menu::MenuSession::State::MainMenu&&!menu.seasonal_agriculture_setup(),"General menu default changed");
    const auto button=menu.diagnostic_button_rect("Z Seasons (32)");require(button.has_value(),"Regular main menu omits Z Seasons");
    require(menu.render(),SDL_GetError());SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=SDLK_Z;
    user_takeover();menu.handle_event(value);menu.advance();user_takeover();
    require(menu.state()==oe::menu::MenuSession::State::NewSandbox&&menu.seasonal_agriculture_setup()&&menu.original_agriculture_setup(),"Z did not select explicit Rule32 setup");
    require(menu.render(),SDL_GetError());menu.shutdown();
    return {{"selector","Z Seasons (32)"},{"regular_main_menu",true},{"scripted_key_rule32",true},{"general_default_unchanged",true}};
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
        require(document.source_schema_version==48&&document.world.rule_version==32&&document.world.seasonal_agriculture&&
            document.world.multi_field_agriculture&&document.world.physical_food_growers&&document.world.hunting_food&&
            document.world.original_economy&&document.world.original_economy->model_source,
            "Input must be an exact paid Rule32/schema48 checkpoint");
        const SourceGuard sources(selected.data,document.map_relative);output=p::output_root(selected.output,selected.data,".local/agri-mac07");
        watch_native=selected.renderer=="metal";
        if(!watch_native)require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#if defined(__APPLE__)
        else require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"cocoa"),SDL_GetError());
#else
        require(!watch_native,"Metal acceptance requires native macOS");
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,selected.renderer.c_str())&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("AGRI-MAC07 scripted paid seasonal verification",1440,1000,
            watch_native?SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const std::string renderer_name=SDL_GetRendererName(renderer)?SDL_GetRendererName(renderer):"";
        require(selected.renderer==renderer_name,"Requested renderer was not selected");user_takeover();
        const auto z_menu=regular_z(selected.data,selected.resources,output,window,renderer);
        auto session=oe::maps::load_stored_map_session(selected.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
        view=oe::SandboxView::create_seasonal_agriculture(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(selected.data,document.map_relative,selected.input);p::configure(*view,oe::detect_and_select_visual_profiles(selected.data,selected.resources));
        view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto generation=view->load_generation();key(*view,SDLK_F9);
        require(view->load_generation()==generation+1&&view->paused()&&view->world().snapshot()==document.world,
            "F9 did not load exact supplied schema48 state: "+view->last_message());
        const auto evidence=checkpoint_evidence(view->world(),selected.phase);input.unchanged();sources.unchanged();
        const auto manual=output/"saves/manual.save.json";view->configure_save(selected.data,document.map_relative,manual);
        const auto saved=view->save_generation();key(*view,SDLK_F5);const auto private_document=save::read_save(manual);
        require(view->save_generation()==saved+1&&private_document.source_schema_version==48&&private_document.world==document.world,
            "Private F5 changed schema48 seasonal/cargo authority: "+view->last_message());
        key(*view,SDLK_PERIOD);const auto stepped=view->world().snapshot();require(stepped.ticks==document.world.ticks+1,"Paused Step advanced another clock");
        const auto stepped_evidence=checkpoint_evidence(view->world(),"checkpoint");
        Json boundary_step_capture=nullptr;
        if(evidence.at("boundary").get<bool>()) {
            std::size_t changed=0;
            for(std::size_t i=0;i<9;++i)if(evidence.at("subjects").at(i).at("ready")!=stepped_evidence.at("subjects").at(i).at("ready") ||
                evidence.at("subjects").at(i).at("expired_ready")!=stepped_evidence.at("subjects").at(i).at("expired_ready")) {changed=i;break;}
            const auto id=subjects(view->world()).at(changed);
            for(unsigned attempt=0;attempt<3&&view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
            require(view->camera().zoom==1.,"Boundary Step zoom could not reach1x");
            select(*view,renderer,id,border);const auto observed=inspector(*view,id);const auto purity=pure_frames(*view);
            const auto name="seasonal-boundary-after-step.png";capture(*view,renderer,output/name);
            boundary_step_capture={{"path",name},{"subject",static_cast<unsigned>(id)},{"inspector",observed},{"purity",purity}};
        }
        const auto reloaded=view->load_generation();key(*view,SDLK_F9);
        require(view->load_generation()==reloaded+1&&view->paused()&&view->world().snapshot()==document.world,"F9 did not restore exact seasonal checkpoint after Step");
        key(*view,SDLK_PERIOD);require(view->world().snapshot()==stepped,"Post-F9 boundary/phase Step continuation differs");
        key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"Second F9 changed checkpoint authority");
        key(*view,SDLK_SPACE);require(!view->paused(),"Space did not resume");key(*view,SDLK_SPACE);require(view->paused(),"Space did not pause");
        Json captures=Json::array(),frames=Json::array();
        for(unsigned attempt=0;attempt<3&&view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
        require(view->camera().zoom==1.,"Zoom action could not reach1x");
        constexpr std::array names{"wheat","millet","rice","cabbage","soy","hemp","tea","lacquer","mulberry"};
        const auto paid=subjects(view->world());
        for(std::size_t i=0;i<paid.size();++i) {
            user_takeover();select(*view,renderer,paid[i],border);const auto observed=inspector(*view,paid[i]);const auto purity=pure_frames(*view);
            const auto name=std::string("seasonal-")+names[i]+"-1x.png";capture(*view,renderer,output/name);
            captures.push_back({{"path",name},{"kind",names[i]},{"subject",static_cast<unsigned>(paid[i])},{"actual_inspector",observed}});
            frames.push_back({{"kind",names[i]},{"purity",purity}});
        }
        Json cargo_captures=Json::array();bool requested_marker=false,hemp_source_cargo=false;
        for(const auto& actor:view->world().couriers()) {
            const bool food=actor.role==sim::CourierRole::OriginalFoodGrower;
            const bool hemp=actor.role==sim::CourierRole::OriginalSeasonalHempGrower;
            if(!food&&!hemp)continue;
            const auto amount=food?view->world().physical_food_grower_cargo(actor.id):view->world().seasonal_hemp_grower_cargo(actor.id);
            if(amount!=1)continue;
            for(unsigned attempt=0;attempt<3&&view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
            std::vector<int> marker_zoom_attempts{1};
            bool picked=select_grower(*view,renderer,actor.id);
            const auto kind=food?"food":"hemp";
            if(!picked&&hemp)for(unsigned zoom_attempt=0;zoom_attempt<2&&!picked;++zoom_attempt) {
                key(*view,SDLK_Z);marker_zoom_attempts.push_back(static_cast<int>(view->camera().zoom));
                picked=select_grower(*view,renderer,actor.id);
            }
            if(!picked&&!hemp){cargo_captures.push_back({{"kind",kind},{"courier",static_cast<unsigned>(actor.id)},{"pixel_pick","OCCLUDED_OR_UNAVAILABLE"}});continue;}
            if(!picked) {
                for(unsigned attempt=0;attempt<3&&view->camera().zoom!=1.;++attempt)key(*view,SDLK_Z);
                select(*view,renderer,actor.owner,border);hemp_source_cargo=true;
            }
            require(view->selected_building()==actor.owner,"Actual cargo inspection lost its paid source");
            const auto text=combined(view->inspection_lines());
            require(text.find(food?"Grower crop cargo 1/1":"Hemp cargo 1/1")!=std::string::npos,
                "Actual selected Grower inspector differs from retained physical cargo");
            show_inspector_row(*view,renderer,food?"Grower crop cargo 1/1":"Hemp cargo 1/1");
            const auto name=std::string("seasonal-")+kind+"-cargo-"+(picked?"":"source-")+
                std::to_string(static_cast<int>(view->camera().zoom))+"x.png";const auto purity=pure_frames(*view);
            capture(*view,renderer,output/name);
            cargo_captures.push_back({{"kind",kind},{"courier",static_cast<unsigned>(actor.id)},
                {"pixel_pick",picked?"PASS":"OCCLUDED_OR_UNAVAILABLE"},{"ordinary_source_selected",!picked},
                {"marker_zoom_attempts",marker_zoom_attempts},
                {"actual_cargo",amount},{"cargo_row_visible",true},{"path",name},{"lines",view->inspection_lines()},{"purity",purity}});
            requested_marker|=(selected.phase=="food-loaded"&&food)||(selected.phase=="hemp-loaded"&&hemp);
        }
        require((selected.phase!="food-loaded"&&selected.phase!="hemp-loaded")||requested_marker||
            (selected.phase=="hemp-loaded"&&hemp_source_cargo),
            "Requested loaded checkpoint lacks verified marker or separately labelled source cargo evidence");
        bool plant_cargo_visible=false;
        for(const auto& source:view->world().plantations_state()->sources)if(source.latest_trip &&
            view->world().plantation_grower_cargo(source.courier_id)==1) {
            const auto plant=source.latest_trip->plant_id;
            select(*view,renderer,plant,border);const auto text=combined(view->inspection_lines());
            require(text.find("cargo 1")!=std::string::npos,"Bound plant inspector omitted its real incoming source Grower cargo");
            show_inspector_row(*view,renderer,"cargo 1");
            const auto name="seasonal-plantation-cargo-"+std::to_string(static_cast<unsigned>(source.courier_id))+"-1x.png";
            const auto purity=pure_frames(*view);capture(*view,renderer,output/name);plant_cargo_visible=true;
            cargo_captures.push_back({{"kind","plantation"},{"courier",static_cast<unsigned>(source.courier_id)},
                {"pixel_pick","NOT_RUN_EXISTING_PLANTATION_ROLE"},{"actual_cargo",1},{"cargo_row_visible",true},
                {"path",name},{"lines",view->inspection_lines()},{"purity",purity}});
        }
        require(selected.phase!="plant-loaded"||plant_cargo_visible,"Requested plantation cargo has no visible bound-plant inspector evidence");
        input.unchanged();sources.unchanged();require(view->world().snapshot()==document.world,"Scripted UI changed paid seasonal authority");
        const Json report{{"format","openemperor-seasonal-agriculture-view-v1"},{"status","PASS"},{"rule",32},{"schema",48},
            {"renderer",renderer_name},{"video_driver",SDL_GetCurrentVideoDriver()},{"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},
            {"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"original_runtime_parity","NOT_CLAIMED"},{"user_takeover_stops_automation",true},
            {"input",selected.input.string()},{"input_preserved",true},{"source_bytes_mode_mtime_unchanged",true},{"tick",document.world.ticks},
            {"supplied_model_sha256",document.world.original_economy->model_source->sha256},{"regular_z_menu",z_menu},{"checkpoint",evidence},
            {"single_step_checkpoint",stepped_evidence},
            {"boundary_step_capture",boundary_step_capture},
            {"input_f9_exact",true},{"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"pause_resume",true},{"captures",captures},{"cargo_captures",cargo_captures},{"pure_frames",frames}};
        write_json(output/"report.json",report);view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<report.dump(2)<<'\n';return 0;
    } catch(const std::exception& error) {
        perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Seasonal Agriculture View acceptance FAIL: "<<error.what()<<'\n';
        if(!output.empty())try{write_json(output/"failure.json",{{"status","FAIL"},{"reason",error.what()},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"}});}catch(...){}
        return 1;
    }
}
