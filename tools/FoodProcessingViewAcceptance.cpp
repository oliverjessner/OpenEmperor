// Explicit paid schema50 processing checkpoints through production controls and inspectors.
// Scripted SDL evidence remains separate from physical input and human acceptance.
#include "FoodNativePlaytest.h"
#include "app/MenuSession.h"
#include "core/PerformanceDiagnostics.h"

#include <iterator>
#include <charconv>
#include <set>

namespace {
namespace p=oe_food_playtest;
namespace perf=oe::performance;
namespace save=oe::persistence;
using p::require;
#if defined(OPENEMPEROR_AGRICULTURE_RECONFIGURATION_ACCEPTANCE)
constexpr bool reconfiguration_mode=true;
constexpr bool warehouse_mode=true;
constexpr const char* usage="openemperor-agriculture-reconfiguration-view-acceptance --data ABS --resources ABS --input EXACT52_CHECKPOINT --output NEW_ABS_.local/agri-mac11/SUBDIR --target PAID_ID [--renderer software|metal] [--phase checkpoint|drainable|draining|ready|blocked|retired|rebuilt] [--width 900..1920] [--height 600..1280]";
#elif defined(OPENEMPEROR_FOOD_WAREHOUSES_ACCEPTANCE)
constexpr bool reconfiguration_mode=false;
constexpr bool warehouse_mode=true;
constexpr const char* usage="openemperor-food-warehouses-view-acceptance --data ABS --resources ABS --input EXACT51_CHECKPOINT --output NEW_ABS_.local/agri-mac10/SUBDIR [--renderer software|metal] [--phase checkpoint|warehouse-stock|warehouse-inbound|warehouse-outbound|warehouse-returning|warehouse-returned|warehouse-market|warehouse-mill|warehouse-shop|warehouse-raw-consumed|processing|ingredient-cargo|meal-cargo|consumed|paused|fire|unstaffed]";
#else
constexpr bool reconfiguration_mode=false;
constexpr bool warehouse_mode=false;
constexpr const char* usage="openemperor-food-processing-view-acceptance --data ABS --resources ABS --input EXACT50_CHECKPOINT --output NEW_ABS_.local/agri-mac09/SUBDIR [--renderer software|metal] [--phase checkpoint|processing|ingredient-cargo|meal-cargo|consumed|paused|fire|unstaffed]";
#endif
constexpr auto selected_rule=reconfiguration_mode ? sim::kAgricultureReconfigurationRuleVersion:warehouse_mode ? sim::kFoodWarehousesRuleVersion:sim::kFoodProcessingRuleVersion;
constexpr unsigned selected_schema=reconfiguration_mode ? 52:warehouse_mode ? 51:50;
constexpr const char* mode_selector=reconfiguration_mode ? "R Reconfigure (36)":warehouse_mode ? "V Food warehouses (35)":"Q Food processing (34)";
bool watch_native=false;
void no_symlinks(const fs::path& path) {
    fs::path cursor;
    for(const auto& part:fs::absolute(path)) {
        cursor/=part;std::error_code error;const auto status=fs::symlink_status(cursor,error);
        require(error==std::errc::no_such_file_or_directory||!error,"Cannot inspect input ancestry");
        require(error||!fs::is_symlink(status),"Acceptance inputs must not traverse symlinks");
    }
}
struct Options {fs::path data,resources,input,output;std::string renderer="software",phase="checkpoint";std::optional<sim::BuildingId> target;int width=1440,height=1000;};
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
        else if(name=="--target"&&reconfiguration_mode) {
            std::uint32_t target=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),target);
            require(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&target>0,"Target must be a positive paid ID");
            result.target=static_cast<sim::BuildingId>(target);
        }
        else if((name=="--width"||name=="--height")&&reconfiguration_mode) {
            int dimension=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),dimension);
            const bool width=name=="--width";const int minimum=width?900:600,maximum=width?1920:1280;
            require(parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size()&&dimension>=minimum&&dimension<=maximum,
                name+" is outside its bounded native viewport range");
            (width?result.width:result.height)=dimension;
        }
        else throw std::runtime_error("Unknown option "+name);
    }
    require(result.data.is_absolute()&&result.resources.is_absolute()&&result.input.is_absolute()&&result.output.is_absolute(),usage);
    require(result.renderer=="software"||result.renderer=="metal","Renderer must be software or metal");
    require(!reconfiguration_mode||result.target.has_value(),"Reconfiguration requires an explicit paid target ID");
    const bool warehouse_phase=warehouse_mode&&std::set<std::string>{"warehouse-stock","warehouse-inbound","warehouse-outbound","warehouse-returning","warehouse-returned","warehouse-market","warehouse-mill","warehouse-shop","warehouse-raw-consumed"}.contains(result.phase);
    const bool reconfiguration_phase=reconfiguration_mode&&std::set<std::string>{"checkpoint","drainable","draining","ready","blocked","retired","rebuilt"}.contains(result.phase);
    require(reconfiguration_mode ? reconfiguration_phase:warehouse_phase||std::set<std::string>{"checkpoint","processing","ingredient-cargo","meal-cargo","consumed","paused","fire","unstaffed"}.contains(result.phase),"Unknown processing checkpoint phase");
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
Json warehouse_raw_house(const sim::World& world) {
    for (const auto& stock:world.processed_food_state()->stocks) {
        if (!sim::original_house_kind(world.building(stock.building_id).kind)||!stock.quality_witness) continue;
        const auto& witness=*stock.quality_witness;const auto& actor=world.courier(witness.transfer.courier_id);
        if (world.building(actor.owner).kind!=sim::Object::Market) continue;
        const auto& histories=world.food_warehouses_state()->markets;
        const auto market=std::find_if(histories.begin(),histories.end(),[&](const auto& row){return row.market_id==actor.owner;});
        if (market==histories.end()) continue;
        for (std::size_t kind=0;kind<sim::processed_food_first_meal;++kind) {
            const auto& receipt=market->first_received[kind];
            if (!witness.accepted[kind]||stock.consumed[kind]<=witness.consumed_before[kind]||!receipt||receipt->tick>witness.tick||
                world.building(receipt->source).kind!=sim::Object::Warehouse) continue;
            return {{"id",static_cast<unsigned>(stock.building_id)},{"kind",kind},{"market",static_cast<unsigned>(actor.owner)},
                {"warehouse",static_cast<unsigned>(receipt->source)},{"warehouse_to_market",first_arrival(receipt)},
                {"house_receipt_tick",witness.tick},{"house_receipt_courier",static_cast<unsigned>(witness.transfer.courier_id)},
                {"house_receipt_sequence",witness.transfer.sequence},{"accepted",witness.accepted},
                {"consumed_before",witness.consumed_before},{"consumed_now",stock.consumed}};
        }
    }
    return nullptr;
}
Json actual_warehouses(const sim::World& world,const std::string& phase) {
    require(world.food_warehouses_enabled()&&world.food_warehouses_state()&&world.processed_food_state(),"Warehouse checkpoint lacks its sole Food authority");
    bool three_kinds=false,inbound=false,outbound=false,returning=false,returned=false,market=false,mill=false,shop=false;
    Json warehouses=Json::array(),markets=Json::array();
    for (const auto& history:world.food_warehouses_state()->warehouses) {
        const auto status=world.food_warehouse_status(history.warehouse_id);
        const auto own=std::find_if(world.processed_food_state()->stocks.begin(),world.processed_food_state()->stocks.end(),
            [&](const auto& stock){return stock.building_id==history.warehouse_id;});
        require(own!=world.processed_food_state()->stocks.end()&&!own->quality_witness,"Warehouse stored Food became a House quality witness");
        returned|=std::any_of(own->returned.begin(),own->returned.end(),[](auto amount){return amount>0;});
        const auto kinds=std::count_if(status.amounts.begin(),status.amounts.end(),[](auto amount){return amount>0;});three_kinds|=kinds>=3;
        Json received=Json::array(),delivered=Json::array();
        for (std::size_t kind=0;kind<sim::processed_food_count;++kind) {
            received.push_back(first_arrival(history.first_received[kind]));
            const auto& first=history.first_delivered[kind];
            if (first) {
                const auto target=world.building(first->target).kind;
                market|=target==sim::Object::Market;mill|=target==sim::Object::OriginalMill53;shop|=target==sim::Object::OriginalFoodShop66;
                delivered.push_back({{"courier",static_cast<unsigned>(first->transfer.courier_id)},{"sequence",first->transfer.sequence},
                    {"target",static_cast<unsigned>(first->target)},{"tick",first->tick},{"amount",first->amount}});
            } else delivered.push_back(nullptr);
        }
        warehouses.push_back({{"id",static_cast<unsigned>(history.warehouse_id)},{"amounts",status.amounts},{"capacity",status.capacity},
            {"incoming",status.incoming},{"return_reserved",status.return_reserved},{"blocker",status.blocker},
            {"first_received",received},{"first_delivered",delivered}});
    }
    for (const auto& history:world.food_warehouses_state()->markets) {
        Json received=Json::array();for (const auto& first:history.first_received) received.push_back(first_arrival(first));
        markets.push_back({{"id",static_cast<unsigned>(history.market_id)},{"first_received",received}});
    }
    for (const auto& actor:world.couriers()) if (world.original_food_cargo(actor.id)>0) {
        inbound|=actor.phase==sim::CourierPhase::ToWarehouse&&world.building(actor.target).kind==sim::Object::Warehouse;
        outbound|=actor.phase==sim::CourierPhase::ToWarehouse&&actor.role==sim::CourierRole::OriginalWarehouseFood;
        returning|=actor.phase==sim::CourierPhase::Returning&&actor.role==sim::CourierRole::OriginalWarehouseFood;
    }
    const auto raw_house=warehouse_raw_house(world);
    require(!phase.starts_with("warehouse-")||(phase=="warehouse-stock"&&three_kinds)||(phase=="warehouse-inbound"&&inbound)||
        (phase=="warehouse-outbound"&&outbound)||(phase=="warehouse-market"&&market)||(phase=="warehouse-mill"&&mill)||
        (phase=="warehouse-shop"&&shop)||(phase=="warehouse-returning"&&returning)||(phase=="warehouse-returned"&&returned)||
        (phase=="warehouse-raw-consumed"&&!raw_house.is_null()),"Supplied paid checkpoint does not establish the requested Warehouse phase");
    return {{"three_kinds_held",three_kinds},{"loaded_inbound",inbound},{"loaded_outbound",outbound},
        {"loaded_returning",returning},{"real_return_settled",returned},{"raw_house",raw_house},
        {"actual_market_receipt",market},{"actual_mill_receipt",mill},{"actual_shop_receipt",shop},{"warehouses",warehouses},{"markets",markets}};
}
Json actual_reconfiguration(const sim::World& world,sim::BuildingId target,const std::string& phase) {
    require(world.agriculture_reconfiguration_enabled()&&world.agriculture_reconfiguration_state(),"Checkpoint lacks exact reconfiguration authority");
    const auto& body=world.building(target);const bool retired=world.agriculture_retired(target);
    const auto status=world.demolition_status(target);
    std::optional<sim::BuildingId> predecessor;
    if (body.placed) for (const auto& previous:world.buildings())
        if (previous.cell==body.cell&&previous.id<target&&world.agriculture_retired(previous.id)&&previous.placed_tick<=body.placed_tick)
            predecessor=previous.id;
    require(phase=="checkpoint"||(phase=="drainable"&&body.placed&&!status.allowed&&status.drain_supported&&!status.draining)||
        (phase=="draining"&&body.placed&&status.draining)||(phase=="ready"&&body.placed&&status.allowed)||
        (phase=="blocked"&&body.placed&&!status.allowed&&!status.drain_supported)||(phase=="retired"&&!body.placed&&retired)||
        (phase=="rebuilt"&&body.placed&&predecessor.has_value()),"Paid checkpoint does not establish its requested reconfiguration phase");
    Json journal=Json::array();for (const auto& entry:world.agriculture_reconfiguration_state()->entries)
        journal.push_back({{"id",static_cast<unsigned>(entry.building_id)},{"started_tick",entry.started_tick},{"started_sequence",entry.started_sequence},
            {"removed_tick",entry.removed_tick ? Json(*entry.removed_tick):Json(nullptr)},
            {"removed_sequence",entry.removed_sequence ? Json(*entry.removed_sequence):Json(nullptr)}});
    Json workers=Json::array();
    for (const auto& farm:world.physical_food_growers_state()->farms) if (farm.latest_trip&&!farm.latest_trip->returned_tick&&
        (farm.farm_id==target||farm.latest_trip->field_id==target)) {
        const auto& actor=world.courier(farm.courier_id);
        workers.push_back({{"kind","FoodGrower"},{"id",farm.latest_trip->grower_id.value},{"courier",static_cast<unsigned>(actor.id)},
            {"field",static_cast<unsigned>(farm.latest_trip->field_id)},{"phase",static_cast<unsigned>(farm.latest_trip->phase)},
            {"cargo",world.physical_food_grower_cargo(actor.id)},{"edge_progress",actor.edge_progress},{"route_pending",actor.route_pending}});
    }
    for (const auto& boat:world.fishing_state()->boats) if (boat.quay_id==target)
        workers.push_back({{"kind","FishingBoat"},{"id",boat.id.value},{"phase",static_cast<unsigned>(boat.phase)},
            {"cargo",boat.cargo},{"reserved",boat.reserved},{"edge_progress",boat.edge_progress},{"return_requested",boat.return_requested}});
    for (const auto& hunter:world.hunting_state()->hunters) if (hunter.tent_id==target)
        workers.push_back({{"kind","Hunter"},{"id",hunter.id.value},{"phase",static_cast<unsigned>(hunter.phase)},
            {"cargo",hunter.cargo},{"reserved",hunter.reserved},{"edge_progress",hunter.edge_progress},{"return_requested",hunter.return_requested}});
    return {{"requested",phase},{"target",static_cast<unsigned>(target)},{"placed",body.placed},{"retired",retired},
        {"allowed",status.allowed},{"drain_supported",status.drain_supported},{"draining",status.draining},
        {"reason",status.reason},{"drain_reason",status.drain_reason},{"active_channels",status.active_couriers},
        {"incoming_deliveries",status.incoming_deliveries},{"stored_goods",status.stored_goods_summary},
        {"predecessor",predecessor ? Json(static_cast<unsigned>(*predecessor)):Json(nullptr)},{"journal",journal},{"actual_workers",workers}};
}
Json regular_q(const fs::path& data,const fs::path& resources,const fs::path& output,SDL_Window* window,SDL_Renderer* renderer) {
    oe::menu::MenuSession menu(data,output/"menu",std::make_unique<oe::menu::NativeDialog>(),resources);menu.initialize(window,renderer);
    require(menu.state()==oe::menu::MenuSession::State::MainMenu&&!menu.food_processing_setup(),"General menu default changed");
    const auto button=menu.diagnostic_button_rect(mode_selector);require(button.has_value(),"Regular main menu omits the selected Food mode");
    require(menu.render(),SDL_GetError());SDL_Event value{};value.type=SDL_EVENT_KEY_DOWN;value.key.key=reconfiguration_mode ? SDLK_R:warehouse_mode ? SDLK_V:SDLK_Q;
    user_takeover();menu.handle_event(value);menu.advance();user_takeover();
    require(menu.state()==oe::menu::MenuSession::State::NewSandbox&&(reconfiguration_mode ? menu.agriculture_reconfiguration_setup():warehouse_mode ? menu.food_warehouses_setup():menu.food_processing_setup())&&menu.original_agriculture_setup(),"Food menu key did not select its exact rule");
    require(menu.render(),SDL_GetError());menu.shutdown();return {{"selector",mode_selector},{"regular_main_menu",true},
        {reconfiguration_mode ? "scripted_key_rule36":warehouse_mode ? "scripted_key_rule35":"scripted_key_rule34",true},{"general_default_unchanged",true}};
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
Json warehouse_inspector(oe::SandboxView& view,sim::BuildingId id) {
    require(!view.debug_diagnostics()&&view.selected_building()==id,"Normal Warehouse inspector selected another owner");
    const auto status=view.world().food_warehouse_status(id);const auto lines=view.inspection_lines();const auto text=combined(lines);
    for (const auto* wanted:{"Food Warehouse #","Food stock ","Status: ","Incoming ","return room ","Goods and Auxiliary","Hemp stock","Wood stock"})
        require(text.find(wanted)!=std::string::npos,"Normal Warehouse inspector omitted "+std::string(wanted));
    constexpr std::array names{"Wheat","Millet","Rice","Cabbage","Bean Curd (Soy)","Fish","Meat",
        "Dinners grade1","Dinners grade2","Dinners grade3","Dinners grade4","Dinners grade5 (locked)"};
    for (std::size_t kind=0;kind<status.amounts.size();++kind)
        require(text.find(std::string(names[kind])+" stock "+std::to_string(status.amounts[kind])+"/4")!=std::string::npos,"Warehouse inspector differs from sole Food stock");
    return {{"lines",lines},{"amounts",status.amounts},{"capacity",status.capacity},{"incoming",status.incoming},{"return_reserved",status.return_reserved},{"blocker",status.blocker}};
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
Json exercise_reconfiguration(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId target,
                             std::uint32_t border,const fs::path& output,Json& captures) {
    const auto before=view.world().snapshot();const auto body=view.world().building(target);
    const auto status=view.world().demolition_status(target);
    const auto record=[&](const char* suffix) {
        const auto name="reconfiguration-target-"+std::to_string(static_cast<unsigned>(target))+"-"+suffix+".png";
        const auto purity=pure_frames(view);capture(view,renderer,output/name);
        captures.push_back({{"kind",std::string("reconfiguration_")+suffix},{"path",name},{"id",static_cast<unsigned>(target)},
            {"lines",view.inspection_lines()},{"purity",purity}});
    };
    if (!body.placed) {
        view.set_tool(5);p::center(view,oe::maps::terrain_ground({static_cast<unsigned>(body.cell.x),static_cast<unsigned>(body.cell.y)},border));
        require(view.render(),SDL_GetError());
        for (const auto cell:sim::building_footprint_cells(view.world().profile(),view.world().rule_version(),body.kind,body.cell))
            require(view.world().building_owner_at(cell)!=target,"Retired footprint retains its paid owner");
        record("retired");return {{"retired_footprint_released",true},{"normal_action","none"}};
    }
    select(view,renderer,target,border);require(view.inspector_layout().demolition.h>0,"Reconfiguration target lacks shared footer action");record("before");
    const auto press=[&]{const auto rect=view.inspector_layout().demolition;click(view,renderer,{rect.x+rect.w*.5,rect.y+rect.h*.5});};
    if (status.allowed) {
        press();require(view.demolition_pending()&&!view.recovery_safe_point(),"Ready footer did not open existing blocking confirmation");
        view.tick_once();require(view.world().snapshot()==before,"Pending removal confirmation advanced World");
        key(view,SDLK_ESCAPE);require(view.world().snapshot()==before&&!view.demolition_pending(),"Cancelled removal changed paid authority");
        press();key(view,SDLK_RETURN);
        require(!view.world().building(target).placed&&view.world().agriculture_retired(target)&&view.world().ticks()==before.ticks&&
            view.world().treasury()==before.treasury&&view.world().snapshot().next_building_id==before.next_building_id,
            "Confirmed removal lost its tombstone, refunded money or reused IDs");
        require(view.render(),SDL_GetError());record("removed");key(view,SDLK_F9);
        require(view.world().snapshot()==before,"F9 failed to restore exact ready checkpoint after private removal");
        return {{"normal_action","confirm_remove"},{"cancel_atomic",true},{"modal_blocks_ticks_and_recovery",true},{"no_refund",true},{"f9_restored",true}};
    }
    press();
    if (status.drain_supported&&!status.draining) {
        require(!view.demolition_pending()&&(view.world().agriculture_draining(target)||view.world().demolition_status(target).allowed),
            "Drain footer did not start authoritative physical drainage");
        require(view.world().ticks()==before.ticks&&view.world().treasury()==before.treasury,"Drain granted time or refund");
        require(view.render(),SDL_GetError());record("drain-started");key(view,SDLK_F9);
        require(view.world().snapshot()==before,"F9 failed to restore exact checkpoint after private drain request");
        return {{"normal_action","begin_drain"},{"no_automatic_removal",true},{"f9_restored",true}};
    }
    require(view.world().snapshot()==before&&!view.demolition_pending()&&view.last_message()==status.reason,
        "Blocked or pending drain footer changed World or opened removal confirmation");
    return {{"normal_action","refused"},{"atomic",true},{"reason",status.reason}};
}
} // namespace
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;std::unique_ptr<oe::SandboxView> view;fs::path output;
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<usage<<'\n';return 0;}
        const auto selected=options(argc,argv);const InputGuard input(selected.input);const auto document=save::read_save(selected.input);
        require(document.source_schema_version==selected_schema&&document.world.rule_version==selected_rule&&document.world.processed_food&&document.world.food_processing&&
            (!warehouse_mode||document.world.food_warehouses)&&(!reconfiguration_mode||document.world.agriculture_reconfiguration)&&
            !document.world.hunting_food&&document.world.original_economy&&document.world.original_economy->model_source,
            "Input must be an exact paid checkpoint for the selected Food rule/schema");
        const SourceGuard sources(selected.data,document.map_relative);output=p::output_root(selected.output,selected.data,
            reconfiguration_mode ? ".local/agri-mac11":warehouse_mode ? ".local/agri-mac10":".local/agri-mac09");
        watch_native=selected.renderer=="metal";if(!watch_native)require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy"),SDL_GetError());
#if defined(__APPLE__)
        else require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"cocoa"),SDL_GetError());
#else
        require(!watch_native,"Metal acceptance requires native macOS");
#endif
        require(SDL_SetHint(SDL_HINT_RENDER_DRIVER,selected.renderer.c_str())&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer(reconfiguration_mode ? "AGRI-MAC11 scripted paid reconfiguration verification":warehouse_mode ? "AGRI-MAC10 scripted paid Food Warehouse verification":"AGRI-MAC09 scripted paid Food processing verification",selected.width,selected.height,
            watch_native?SDL_WINDOW_RESIZABLE:SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const std::string renderer_name=SDL_GetRendererName(renderer)?SDL_GetRendererName(renderer):"";require(renderer_name==selected.renderer,"Requested renderer was not selected");user_takeover();
        int output_width=0,output_height=0;require(SDL_GetRenderOutputSize(renderer,&output_width,&output_height)&&output_width>0&&output_height>0,SDL_GetError());
        const auto menu=regular_q(selected.data,selected.resources,output,window,renderer);
        auto session=oe::maps::load_stored_map_session(selected.data,document.map_relative,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);const auto border=session.plan.border;
        view=reconfiguration_mode ? oe::SandboxView::create_agriculture_reconfiguration(std::move(session),document.world.original_economy->model_source->difficulty,false):
            warehouse_mode ? oe::SandboxView::create_food_warehouses(std::move(session),document.world.original_economy->model_source->difficulty,false):
            oe::SandboxView::create_food_processing(std::move(session),document.world.original_economy->model_source->difficulty,false);
        view->configure_save(selected.data,document.map_relative,selected.input);p::configure(*view,oe::detect_and_select_visual_profiles(selected.data,selected.resources));
        view->initialize(window,renderer);SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(*view,focus);
        const auto generation=view->load_generation();key(*view,SDLK_F9);require(view->load_generation()==generation+1&&view->paused()&&view->world().snapshot()==document.world,"F9 did not restore exact supplied processing checkpoint: "+view->last_message());
        const auto evidence=actual_processing(view->world(),reconfiguration_mode||selected.phase.starts_with("warehouse-") ? "checkpoint":selected.phase);
        const auto warehouse_evidence=warehouse_mode ? actual_warehouses(view->world(),reconfiguration_mode ? "checkpoint":selected.phase):Json(nullptr);
        const auto reconfiguration_evidence=reconfiguration_mode ? actual_reconfiguration(view->world(),*selected.target,selected.phase):Json(nullptr);
        input.unchanged();sources.unchanged();
        const auto manual=output/"saves/manual.save.json";view->configure_save(selected.data,document.map_relative,manual);
        const auto saved=view->save_generation();key(*view,SDLK_F5);const auto private_document=save::read_save(manual);
        require(view->save_generation()==saved+1&&private_document.source_schema_version==selected_schema&&private_document.world==document.world,"Private F5 changed processing inputs/grade/work/cargo");
        key(*view,SDLK_PERIOD);const auto stepped=view->world().snapshot();require(stepped.ticks==document.world.ticks+1,"Single Step advanced another clock");
        key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"F9 changed exact active processing checkpoint");key(*view,SDLK_PERIOD);
        require(view->world().snapshot()==stepped,"Post-F9 processing continuation differs");key(*view,SDLK_F9);require(view->world().snapshot()==document.world,"Second F9 changed paid processing authority");
        key(*view,SDLK_SPACE);require(!view->paused(),"Space did not resume");key(*view,SDLK_SPACE);require(view->paused(),"Space did not pause");
        for(unsigned i=0;i<3&&view->camera().zoom!=1.;++i)key(*view,SDLK_Z);
        Json captures=Json::array();
        const auto reconfiguration_action=reconfiguration_mode ?
            exercise_reconfiguration(*view,renderer,*selected.target,border,output,captures):Json(nullptr);
        require(view->world().snapshot()==document.world,"Reconfiguration UI probe failed to restore its exact protected checkpoint");
        if (warehouse_mode) {
            for (const auto& body:view->world().buildings()) if (body.kind==sim::Object::Warehouse) {
                for (unsigned zoom=0;zoom<3;++zoom) {
                    select(*view,renderer,body.id,border);const auto observed=warehouse_inspector(*view,body.id);
                    show_inspector_row(*view,renderer,"Food stock ");const auto purity=pure_frames(*view);
                    const auto name="warehouse-"+std::to_string(static_cast<unsigned>(body.id))+"-"+std::to_string(int(view->camera().zoom))+"x.png";
                    capture(*view,renderer,output/name);captures.push_back({{"kind","warehouse"},{"path",name},{"id",static_cast<unsigned>(body.id)},{"inspector",observed},{"purity",purity}});
                    key(*view,SDLK_Z);
                }
                show_inspector_row(*view,renderer,"Hemp stock");const auto purity=pure_frames(*view);
                const auto name="warehouse-"+std::to_string(static_cast<unsigned>(body.id))+"-goods-aux.png";
                capture(*view,renderer,output/name);captures.push_back({{"kind","warehouse_goods_aux"},{"path",name},{"id",static_cast<unsigned>(body.id)},{"purity",purity}});
                show_inspector_row(*view,renderer,"Paper stock ");const auto deep_purity=pure_frames(*view);
                const auto deep_name="warehouse-"+std::to_string(static_cast<unsigned>(body.id))+"-deep-goods-aux.png";
                capture(*view,renderer,output/deep_name);captures.push_back({{"kind","warehouse_goods_aux_deep"},{"path",deep_name},
                    {"id",static_cast<unsigned>(body.id)},{"lines",view->inspection_lines()},{"purity",deep_purity}});
            }
        }
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
        if (warehouse_mode&&!warehouse_evidence.at("raw_house").is_null()) {
            const auto& raw=warehouse_evidence.at("raw_house");
            const auto house=static_cast<sim::BuildingId>(raw.at("id").get<unsigned>());
            constexpr std::array raw_names{"Wheat","Millet","Rice","Cabbage","Bean Curd (Soy)","Fish","Meat"};
            select(*view,renderer,house,border);
            house_goods(*view,renderer,std::string(raw_names.at(raw.at("kind").get<std::size_t>()))+" House stock");
            const auto consumed_purity=pure_frames(*view);const auto consumed_name="warehouse-house-"+std::to_string(static_cast<unsigned>(house))+"-raw-consumed.png";
            capture(*view,renderer,output/consumed_name);captures.push_back({{"kind","warehouse_raw_house_consumed"},{"path",consumed_name},{"id",static_cast<unsigned>(house)},
                {"lines",view->original_house_section_lines()},{"proof",raw},{"purity",consumed_purity}});
            house_goods(*view,renderer,"Quality arrival witness");const auto receipt_purity=pure_frames(*view);
            const auto receipt_name="warehouse-house-"+std::to_string(static_cast<unsigned>(house))+"-raw-receipt.png";
            capture(*view,renderer,output/receipt_name);captures.push_back({{"kind","warehouse_raw_house_receipt"},{"path",receipt_name},{"id",static_cast<unsigned>(house)},{"purity",receipt_purity}});
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
        for(const auto& actor:view->world().couriers()) {
            const int physical=reconfiguration_mode ? actor.role==sim::CourierRole::OriginalFoodGrower ? view->world().physical_food_grower_cargo(actor.id):
                actor.role==sim::CourierRole::OriginalSeasonalHempGrower ? view->world().seasonal_hemp_grower_cargo(actor.id):
                actor.role==sim::CourierRole::OriginalPlantationGrower ? view->world().plantation_grower_cargo(actor.id):0:0;
            const int actual_cargo=std::max(physical,view->world().original_food_cargo(actor.id));
            if (!actual_cargo) continue;
            bool relevant=physical>0;for(const auto& trip:view->world().processed_food_state()->trips)if(trip.transfer.courier_id==actor.id)
                relevant|=view->world().building(trip.target).kind==sim::Object::OriginalMill53||actor.role==sim::CourierRole::OriginalMealsToShop||actor.role==sim::CourierRole::OriginalMealsToHouse||
                    (warehouse_mode&&(view->world().building(trip.target).kind==sim::Object::Warehouse||actor.role==sim::CourierRole::OriginalWarehouseFood));
            if(!relevant)continue;const bool picked=select_grower(*view,renderer,actor.id);require(picked,"Loaded processing courier could not be picked through submitted marker pixels");
            const auto lines=view->inspection_lines();require(combined(lines).find("cargo")!=std::string::npos||
                (reconfiguration_mode&&combined(lines).find("Cargo")!=std::string::npos),"Selected physical processing carrier hides actual cargo");
            if(physical>0) {
                const auto add_section=[&](const std::string& needle,const char* suffix) {
                    show_inspector_row(*view,renderer,needle);const auto section_purity=pure_frames(*view);
                    const auto name="physical-worker-"+std::to_string(static_cast<unsigned>(actor.id))+"-"+suffix+".png";
                    capture(*view,renderer,output/name);captures.push_back({{"kind","physical_worker_witness"},{"path",name},
                        {"courier",static_cast<unsigned>(actor.id)},{"visible_row",needle},{"actual_cargo",physical},{"purity",section_purity}});
                };
                if(actor.role==sim::CourierRole::OriginalFoodGrower) {
                    const auto& farms=view->world().physical_food_growers_state()->farms;
                    const auto farm=std::find_if(farms.begin(),farms.end(),[&](const auto& row){return row.courier_id==actor.id;});
                    require(farm!=farms.end()&&farm->latest_trip,"Selected Food Grower lacks its actual latest lifetime");
                    const auto& trip=*farm->latest_trip;
                    constexpr std::array crop_names{"Wheat","Millet","Rice","Cabbage","Bean Curd (Soy)"};
                    const auto expected="Grower crop cargo "+std::to_string(physical)+"/1";
                    const auto reserved="Farm return reserved "+std::to_string(sim::physical_food_grower_reservations(&*view->world().physical_food_growers_state(),actor.owner));
                    require(combined(lines).find(expected)!=std::string::npos&&
                        combined(lines).find("Grower channel #"+std::to_string(static_cast<unsigned>(actor.id)))!=std::string::npos&&
                        combined(lines).find("Grower lifetime #"+std::to_string(trip.grower_id.value))!=std::string::npos&&
                        combined(lines).find("Assigned field #"+std::to_string(static_cast<unsigned>(trip.field_id)))!=std::string::npos&&
                        combined(lines).find(std::string("Crop: ")+crop_names.at(static_cast<std::size_t>(trip.crop)))!=std::string::npos&&
                        combined(lines).find(reserved)!=std::string::npos,
                        "Selected Food Grower inspector differs from its actual channel, lifetime, Field, crop or cargo");
                    add_section("Assigned field #","identity");add_section(expected,"crop-cargo");
                    show_inspector_row(*view,renderer,reserved);
                } else if(actor.role==sim::CourierRole::OriginalPlantationGrower) {
                    require(combined(lines).find("Grower channel #"+std::to_string(static_cast<unsigned>(actor.id)))!=std::string::npos&&
                        combined(lines).find("cargo "+std::to_string(physical))!=std::string::npos,
                        "Selected plantation Grower inspector differs from its real channel or cargo");
                    add_section("Grower channel #","identity");show_inspector_row(*view,renderer,"Grower state:");
                } else show_inspector_row(*view,renderer,"Hemp cargo ");
            }
            const auto purity=pure_frames(*view);const auto name="processing-courier-"+std::to_string(static_cast<unsigned>(actor.id))+"-cargo.png";capture(*view,renderer,output/name);
            cargos.push_back({{"courier",static_cast<unsigned>(actor.id)},{"actual_cargo",actual_cargo},{"pixel_pick","PASS"},{"path",name},{"lines",lines},{"purity",purity}});cargo_checked=true;
        }
        require((selected.phase!="ingredient-cargo"&&selected.phase!="meal-cargo"&&selected.phase!="warehouse-inbound"&&selected.phase!="warehouse-outbound"&&selected.phase!="warehouse-returning")||cargo_checked,"Requested cargo phase lacks a real selected carrier");
        input.unchanged();sources.unchanged();require(view->world().snapshot()==document.world,"Scripted inspectors changed paid processing authority");
        const Json report{{"format",reconfiguration_mode ? "openemperor-agriculture-reconfiguration-view-v1":warehouse_mode ? "openemperor-food-warehouses-view-v1":"openemperor-food-processing-view-v1"},{"status","PASS"},{"rule",selected_rule},{"schema",selected_schema},{"renderer",renderer_name},{"video_driver",SDL_GetCurrentVideoDriver()},
            {"tier",watch_native?"NATIVE_METAL_SCRIPTED_SDL":"HIDDEN_SOFTWARE_SCRIPTED_SDL"},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"original_runtime_parity","NOT_CLAIMED"},
            {"user_takeover_stops_automation",true},{"input",selected.input.string()},{"input_preserved",true},{"source_bytes_mode_mtime_unchanged",true},{"tick",document.world.ticks},
            {"supplied_model_sha256",document.world.original_economy->model_source->sha256},{reconfiguration_mode ? "regular_r_menu":warehouse_mode ? "regular_v_menu":"regular_q_menu",menu},{"checkpoint",evidence},{"warehouse_checkpoint",warehouse_evidence},
            {"reconfiguration_checkpoint",reconfiguration_evidence},{"reconfiguration_ui_action",reconfiguration_action},
            {"input_f9_exact",true},{"private_f5_f9_exact",true},{"step_reload_replay_exact",true},{"pause_resume",true},{"captures",captures},{"cargo_captures",cargos}};
        auto final_report=report;if(reconfiguration_mode)final_report["viewport"]={{"requested_width",selected.width},{"requested_height",selected.height},{"drawable_width",output_width},{"drawable_height",output_height}};
        write_json(output/"report.json",final_report);view->shutdown();view.reset();SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;SDL_Quit();std::cout<<final_report.dump(2)<<'\n';return 0;
    }catch(const std::exception& error){perf::set_enabled(false);if(view)view->shutdown();view.reset();if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Food processing View acceptance FAIL: "<<error.what()<<'\n';if(!output.empty())try{write_json(output/"failure.json",{{"status","FAIL"},{"reason",error.what()},{"native_os_input","NOT_RUN"},{"human_acceptance","NOT_RUN"}});}catch(...){}return 1;}
}
