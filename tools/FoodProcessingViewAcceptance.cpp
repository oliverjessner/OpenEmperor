// Explicit paid schema50 processing checkpoints through production controls and inspectors.
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
constexpr const char* usage="openemperor-food-processing-view-acceptance --data ABS --resources ABS --input EXACT50_CHECKPOINT --output NEW_ABS_.local/agri-mac09/SUBDIR [--renderer software|metal] [--phase checkpoint|processing|ingredient-cargo|meal-cargo|consumed|paused|fire|unstaffed]";
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
    require(std::set<std::string>{"checkpoint","processing","ingredient-cargo","meal-cargo","consumed","paused","fire","unstaffed"}.contains(result.phase),"Unknown processing checkpoint phase");
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
    require(selected,"Paid body could not be selected through production picking");require(view.render(),SDL_GetError());
}

bool select_grower(oe::SandboxView& view,SDL_Renderer* renderer,sim::CourierId channel) {
    const auto ground=view.courier_ground_position(channel);require(ground.has_value(),"Actual processing carrier has no position");
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
Json first_arrival(const std::optional<sim::FoodProcessingArrival>& receipt) {
    if (!receipt) return nullptr;
    return {{"courier",static_cast<unsigned>(receipt->transfer.courier_id)},{"sequence",receipt->transfer.sequence},
        {"source",static_cast<unsigned>(receipt->source)},{"tick",receipt->tick},{"amount",receipt->amount}};
}
Json actual_processing(const sim::World& world,const std::string& phase) {
    require(world.food_processing_enabled()&&world.processed_food_state()&&world.food_processing_state()&&!world.hunting_food_state(),"Processing checkpoint duplicated Food authority");
    bool processing=false,ingredient_cargo=false,meal_cargo=false,consumed=false,paused=false,fire=false,unstaffed=false;
    Json mills=Json::array(),shops=Json::array(),trips=Json::array(),houses=Json::array();
    for(const auto& mill:world.food_processing_state()->mills) {
        const auto status=world.food_processing_status(mill.mill_id);const auto& body=world.building(mill.mill_id);
        processing|=mill.active&&mill.active->work>0;paused|=!body.operating_enabled;fire|=world.building_on_fire(body.id);unstaffed|=world.workers_assigned(body.id)<world.workforce_required(body.id);
        Json first_inputs=Json::array(),first_recipes=Json::array();
        for (const auto& receipt:mill.first_ingredients) first_inputs.push_back(first_arrival(receipt));
        for (const auto& receipt:mill.first_completions)
            first_recipes.push_back(receipt ? Json{{"sequence",receipt->batch.sequence},{"grade",receipt->batch.grade},
                {"started_tick",receipt->batch.started_tick},{"completed_tick",receipt->completed_tick},
                {"ingredients",receipt->batch.ingredients},{"work",receipt->batch.work}}:Json(nullptr));
        mills.push_back({{"id",static_cast<unsigned>(body.id)},{"requested_grade",mill.requested_grade},{"active_grade",status.active_grade},
            {"work",status.work},{"required_work",status.required_work},{"inputs",status.ingredients},{"meals",status.meals},
            {"recipes_started",mill.started},{"recipes_completed",mill.completed},{"completed_recipes_by_grade",mill.completed_by_grade},
            {"consumed_ingredients",mill.ingredients_consumed},{"blocker",status.blocker},{"incoming",status.incoming},
            {"return_reserved",status.return_reserved},{"processing_reserved",status.processing_reserved},
            {"first_ingredients",first_inputs},{"first_completions",first_recipes}});
    }
    for(const auto& shop:world.food_processing_state()->shops) {
        const auto status=world.food_processing_status(shop.shop_id);
        const auto& body=world.building(shop.shop_id);paused|=!body.operating_enabled;
        fire|=world.building_on_fire(body.id);unstaffed|=world.workers_assigned(body.id)<world.workforce_required(body.id);
        Json first_meals=Json::array();for (const auto& receipt:shop.first_meals) first_meals.push_back(first_arrival(receipt));
        shops.push_back({{"id",static_cast<unsigned>(shop.shop_id)},{"market",static_cast<unsigned>(shop.market_id)},
            {"meals",status.meals},{"blocker",status.blocker},{"incoming",status.incoming},{"return_reserved",status.return_reserved},{"first_meals",first_meals}});
    }
    for(const auto& trip:world.processed_food_state()->trips) {
        int raw=0,meals=0;for(std::size_t resource=0;resource<12;++resource) {
            const auto cargo=trip.loaded[resource]-trip.delivered[resource]-trip.returned[resource];require(cargo>=0,"Negative processing cargo");
            if(resource<7)raw+=cargo;else meals+=cargo;
        }
        ingredient_cargo|=raw>0&&world.building(trip.target).kind==sim::Object::OriginalMill53;meal_cargo|=meals>0;
        if(raw||meals)trips.push_back({{"courier",static_cast<unsigned>(trip.transfer.courier_id)},{"sequence",trip.transfer.sequence},
            {"source",static_cast<unsigned>(trip.source)},{"target",static_cast<unsigned>(trip.target)},{"raw_cargo",raw},{"meal_cargo",meals}});
    }
    for(const auto& house:world.food_processing_state()->houses) {
        consumed|=house.first_consumption.has_value();
        Json first_grades=Json::array();for (const auto& receipt:house.first_by_grade) first_grades.push_back(first_arrival(receipt));
        houses.push_back({{"id",static_cast<unsigned>(house.house_id)},{"first_receipt_tick",house.first_receipt.tick},
            {"accepted",house.first_receipt.accepted},{"first_consumption_tick",house.first_consumption?Json(house.first_consumption->tick):Json(nullptr)},
            {"first_consumption_grade",house.first_consumption?Json(house.first_consumption->grade):Json(nullptr)},
            {"first_by_grade",first_grades}});
    }
    require(phase=="checkpoint"||(phase=="processing"&&processing)||(phase=="ingredient-cargo"&&ingredient_cargo)||
        (phase=="meal-cargo"&&meal_cargo)||(phase=="consumed"&&consumed)||(phase=="paused"&&paused)||(phase=="fire"&&fire)||
        (phase=="unstaffed"&&unstaffed),"Supplied paid checkpoint does not establish requested processing phase");
    return {{"requested",phase},{"processing",processing},{"ingredient_cargo",ingredient_cargo},{"meal_cargo",meal_cargo},
        {"consumed",consumed},{"paused",paused},{"fire",fire},{"unstaffed",unstaffed},{"mills",mills},{"shops",shops},{"live_trips",trips},{"houses",houses}};
}
Json regular_q(const fs::path& data,const fs::path& resources,const fs::path& output,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,output/"menu",std::make_unique<oe::menu::NativeDialog>(),resources);menu.initialize(window,renderer);
    require(menu.state()==oe::menu::MenuSession::State::MainMenu&&!menu.food_processing_setup(),"General menu default changed");
    const auto button=menu.diagnostic_button_rect("Q Food processing (34)");require(button.has_value(),"Regular main menu omits Q Food processing");
    require(menu.render(),SDL_GetError());SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=SDLK_Q;
    user_takeover();menu.handle_event(value);menu.advance();user_takeover();
    require(menu.state()==oe::menu::MenuSession::State::NewSandbox&&menu.food_processing_setup()&&menu.original_agriculture_setup(),"Q did not select Rule34");
    require(menu.render(),SDL_GetError());menu.shutdown();return {{"selector","Q Food processing (34)"},{"regular_main_menu",true},{"scripted_key_rule34",true},{"general_default_unchanged",true}};
}
Json processing_inspector(oe::SandboxView& view,sim::BuildingId id) {
    require(!view.debug_diagnostics()&&view.selected_building()==id,"Normal processing inspector selected another owner");
    const auto lines=view.inspection_lines();const auto text=combined(lines);const auto status=view.world().food_processing_status(id);
    for(const auto& wanted:{std::string("AUTHORED recipes"),std::string("Status: "),std::string("Workers "),std::string("Grade5 unavailable"),std::string("Food courier:")})
        require(text.find(wanted)!=std::string::npos,"Normal processing inspector omitted "+wanted);
    for(std::size_t grade=0;grade<5;++grade)require(text.find("Dinners28 grade "+std::to_string(grade+1)+" stock "+std::to_string(status.meals[grade]))!=std::string::npos,"Inspector grade differs from sole meal inventory");
    if(view.world().building(id).kind==sim::Object::OriginalMill53)
        require(text.find("Requested recipe grade "+std::to_string(status.requested_grade))!=std::string::npos,"Mill recipe control differs from actual request");
    else require(status.parent_market&&text.find("Bound paid Market #"+std::to_string(static_cast<unsigned>(*status.parent_market)))!=std::string::npos,"Food Shop inspector lost its stable actual Market binding");
    return {{"lines",lines},{"requested_grade",status.requested_grade},{"active_grade",status.active_grade},{"work",status.work},{"required_work",status.required_work},{"ingredients",status.ingredients},{"meals",status.meals},{"blocker",status.blocker}};
}
void house_goods(oe::SandboxView& view,SDL_Renderer* renderer,const std::string& wanted) {
    for(unsigned step=0;step<4&&view.original_house_section()!=oe::SandboxView::OriginalHouseSection::Goods;++step)key(view,SDLK_PAGEDOWN);
    require(view.original_house_section()==oe::SandboxView::OriginalHouseSection::Goods,"House Goods section unavailable");
    const auto lines=view.original_house_section_lines();const auto found=std::find_if(lines.begin(),lines.end(),[&](const auto& line){return line.find(wanted)!=std::string::npos;});
    require(found!=lines.end(),"Actual House Goods inspector omitted "+wanted);const auto index=static_cast<std::size_t>(found-lines.begin());
    const auto shown=std::min<std::size_t>(3,lines.size()-index);
    for(unsigned step=0;step<512;++step) {
        const auto content=view.original_house_content_rect();const auto row=content.y+static_cast<int>(index)*17*view.layout().scale-view.original_house_scroll();
        if(row>=content.y&&row+static_cast<int>(shown)*17*view.layout().scale<=content.y+content.h)break;
        float x=0,y=0;require(SDL_RenderCoordinatesToWindow(renderer,float(content.x+content.w/2),float(content.y+content.h/2),&x,&y),SDL_GetError());
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=row<content.y?1:-1;wheel.wheel.mouse_x=x;wheel.wheel.mouse_y=y;event(view,wheel);
    }
    const auto content=view.original_house_content_rect();const auto row=content.y+static_cast<int>(index)*17*view.layout().scale-view.original_house_scroll();
    require(row>=content.y&&row+static_cast<int>(shown)*17*view.layout().scale<=content.y+content.h,"Complete House meal witness cannot be reached by normal scrolling");require(view.render(),SDL_GetError());
}
} // namespace
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;std::unique_ptr<oe::SandboxView> view;fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);const InputGuard input(selected.input);const auto document=save::read_save(selected.input);
        require(document.source_schema_version==50&&document.world.rule_version==34&&document.world.processed_food&&document.world.food_processing&&
            !document.world.hunting_food&&document.world.original_economy&&document.world.original_economy->model_source,
            "Input must be an exact paid Rule34/schema50 checkpoint");
        const SourceGuard sources(selected.data,document.map_relative);output=p::output_root(selected.output,selected.data,".local/agri-mac09");
        watch_native=selected.renderer=="metal";if(!watch_native)require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#if defined(__APPLE__)
        else require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"cocoa"),SDL_GetError());
#else
        require(!watch_native,"Metal acceptance requires native macOS");
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,selected.renderer.c_str())&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("AGRI-MAC09 scripted paid Food processing verification",1440,1000,
            watch_native?SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const std::string renderer_name=SDL_GetRendererName(renderer)?SDL_GetRendererName(renderer):"";require(renderer_name==selected.renderer,"Requested renderer was not selected");user_takeover();
        const auto menu=regular_q(selected.data,selected.resources,output,window,renderer);
        auto session=oe::maps::load_stored_map_session(selected.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
        view=oe::SandboxView::create_food_processing(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(selected.data,document.map_relative,selected.input);p::configure(*view,oe::detect_and_select_visual_profiles(selected.data,selected.resources));
        view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto generation=view->load_generation();key(*view,SDLK_F9);require(view->load_generation()==generation+1&&view->paused()&&view->world().snapshot()==document.world,"F9 did not restore exact supplied processing checkpoint: "+view->last_message());
        const auto evidence=actual_processing(view->world(),selected.phase);input.unchanged();sources.unchanged();
        const auto manual=output/"saves/manual.save.json";view->configure_save(selected.data,document.map_relative,manual);
        const auto saved=view->save_generation();key(*view,SDLK_F5);const auto private_document=save::read_save(manual);
        require(view->save_generation()==saved+1&&private_document.source_schema_version==50&&private_document.world==document.world,"Private F5 changed processing inputs/grade/work/cargo");
        key(*view,SDLK_PERIOD);const auto stepped=view->world().snapshot();require(stepped.ticks==document.world.ticks+1,"Single Step advanced another clock");
        key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"F9 changed exact active processing checkpoint");key(*view,SDLK_PERIOD);
        require(view->world().snapshot()==stepped,"Post-F9 processing continuation differs");key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"Second F9 changed paid processing authority");
        key(*view,SDLK_SPACE);require(!view->paused(),"Space did not resume");key(*view,SDLK_SPACE);require(view->paused(),"Space did not pause");
        for(unsigned i=0;i<3&&view->camera().zoom!=1.;++i)key(*view,SDLK_Z);
        Json captures=Json::array();
        std::vector<sim::BuildingId> processing_owners;
        for(const auto& body:view->world().buildings())if(body.kind==sim::Object::OriginalMill53||body.kind==sim::Object::OriginalFoodShop66)processing_owners.push_back(body.id);
        for(const auto id:processing_owners) {
            const auto body=view->world().building(id);
            select(*view,renderer,id,border);const auto observed=processing_inspector(*view,id);
            const bool mill=body.kind==sim::Object::OriginalMill53;show_inspector_row(*view,renderer,mill?"Requested recipe grade":"Bound paid Market");
            const auto purity=pure_frames(*view);const auto name=std::string(mill?"processing-mill-":"processing-shop-")+std::to_string(static_cast<unsigned>(body.id))+"-1x.png";
            capture(*view,renderer,output/name);captures.push_back({{"kind",mill?"mill":"shop"},{"path",name},{"id",static_cast<unsigned>(body.id)},{"inspector",observed},{"purity",purity}});
            if(mill) {
                const auto before=view->world().food_processing_status(body.id).requested_grade;SDL_Event cycle{};cycle.type=SDL_EVENT_KEY_DOWN;cycle.key.key=SDLK_G;cycle.key.mod=SDL_KMOD_SHIFT;event(*view,cycle);
                require(view->world().food_processing_status(body.id).requested_grade==before%4+1,"Normal recipe cycle escaped grade1..4");key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"F9 failed to restore after recipe selection");
            }
        }
        std::vector<sim::BuildingId> house_subjects;
        const auto& meal_houses=view->world().food_processing_state()->houses;
        if(!meal_houses.empty())house_subjects.push_back(meal_houses.front().house_id);
        const auto consumed_house=std::find_if(meal_houses.begin(),meal_houses.end(),[](const auto& row){return row.first_consumption.has_value();});
        if(consumed_house!=meal_houses.end()&&std::find(house_subjects.begin(),house_subjects.end(),consumed_house->house_id)==house_subjects.end())house_subjects.push_back(consumed_house->house_id);
        for(const auto house:house_subjects) {
            const auto history=*std::find_if(meal_houses.begin(),meal_houses.end(),[&](const auto& row){return row.house_id==house;});
            select(*view,renderer,history.house_id,border);house_goods(*view,renderer,"First physical Dinners");const auto purity=pure_frames(*view);
            const auto name="processing-house-"+std::to_string(static_cast<unsigned>(history.house_id))+"-receipt.png";capture(*view,renderer,output/name);
            captures.push_back({{"kind","house_receipt"},{"path",name},{"id",static_cast<unsigned>(history.house_id)},{"lines",view->original_house_section_lines()},{"purity",purity}});
            if(history.first_consumption) {
                house_goods(*view,renderer,"First actual Dinners");const auto name2="processing-house-"+std::to_string(static_cast<unsigned>(history.house_id))+"-consumed.png";
                const auto pure2=pure_frames(*view);capture(*view,renderer,output/name2);captures.push_back({{"kind","house_consumed"},{"path",name2},{"id",static_cast<unsigned>(history.house_id)},{"purity",pure2}});
            }
        }
        Json cargos=Json::array();bool cargo_checked=false;
        for(const auto& actor:view->world().couriers())if(view->world().original_food_cargo(actor.id)>0) {
            bool relevant=false;for(const auto& trip:view->world().processed_food_state()->trips)if(trip.transfer.courier_id==actor.id)
                relevant|=view->world().building(trip.target).kind==sim::Object::OriginalMill53||actor.role==sim::CourierRole::OriginalMealsToShop||actor.role==sim::CourierRole::OriginalMealsToHouse;
            if(!relevant)continue;const bool picked=select_grower(*view,renderer,actor.id);require(picked,"Loaded processing courier could not be picked through submitted marker pixels");
            const auto lines=view->inspection_lines();require(combined(lines).find("cargo")!=std::string::npos,"Selected physical processing carrier hides actual cargo");
            const auto purity=pure_frames(*view);const auto name="processing-courier-"+std::to_string(static_cast<unsigned>(actor.id))+"-cargo.png";capture(*view,renderer,output/name);
            cargos.push_back({{"courier",static_cast<unsigned>(actor.id)},{"actual_cargo",view->world().original_food_cargo(actor.id)},{"pixel_pick","PASS"},{"path",name},{"lines",lines},{"purity",purity}});cargo_checked=true;
        }
        require((selected.phase!="ingredient-cargo"&&selected.phase!="meal-cargo")||cargo_checked,"Requested cargo phase lacks a real selected carrier");
        input.unchanged();sources.unchanged();require(view->world().snapshot()==document.world,"Scripted inspectors changed paid processing authority");
        const Json report{{"format","openemperor-food-processing-view-v1"},{"status","PASS"},{"rule",34},{"schema",50},{"renderer",renderer_name},{"video_driver",SDL_GetCurrentVideoDriver()},
            {"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"original_runtime_parity","NOT_CLAIMED"},
            {"user_takeover_stops_automation",true},{"input",selected.input.string()},{"input_preserved",true},{"source_bytes_mode_mtime_unchanged",true},{"tick",document.world.ticks},
            {"supplied_model_sha256",document.world.original_economy->model_source->sha256},{"regular_q_menu",menu},{"checkpoint",evidence},
            {"input_f9_exact",true},{"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"pause_resume",true},{"captures",captures},{"cargo_captures",cargos}};
        write_json(output/"report.json",report);view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();std::cout<<report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Food processing View acceptance FAIL: "<<error.what()<<'\n';if(!output.empty())try{write_json(output/"failure.json",{{"status","FAIL"},{"reason",error.what()},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"}});}catch(...){}return 1;}
}
