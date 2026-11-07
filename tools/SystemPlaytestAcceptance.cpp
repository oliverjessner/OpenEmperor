// OE41/OE42/OE50 QA only. Reuse the shipped event/dialog harness; all gameplay enters
// MenuSession and SandboxView through ordinary SDL events.
#define main oe41_reused_original_menu_acceptance_main
#include "OriginalStarterMenuAcceptance.cpp"
#undef main
#include <map>
#include <regex>
#include <set>

namespace oe41 {
using A=oe::sandbox_ui::Action;
constexpr const char* help="openemperor-system-playtest-acceptance --check-paths|--inventory|--create|--resume|--map|--inspect-paid|--inspector-layout DATA RESOURCES FRESH_APP_ROOT FRESH_OUTPUT [SAVE_OR_MAP]";
const fs::path repo=fs::absolute(__FILE__).parent_path().parent_path();
const fs::path private_root=repo/".local/oe41";
const fs::path layout_root=repo/".local/oe42";
const fs::path playtest_root=repo/".local/oe43";
const fs::path current_playtest_root=repo/".local/oe50";
Json menu_rows=Json::array(),toolbar_rows=Json::array(),events=Json::array(),scenario_results=Json::array();
std::map<int,std::string> action_names;

void paths(const fs::path& data,const fs::path& resources,const fs::path& app,const fs::path& out) {
    for(const auto& target:{app,out}) {
        no_symlinks(target);const auto p=fs::weakly_canonical(target);
        require(((contains(private_root,p)&&p!=private_root)||(contains(layout_root,p)&&p!=layout_root)||
            (contains(playtest_root,p)&&p!=playtest_root)||(contains(current_playtest_root,p)&&p!=current_playtest_root))&&!fs::exists(p),
            "Use fresh ignored OE41/OE42/OE43/OE50 app/output directories");
        for(const auto& source:{fs::canonical(data),fs::canonical(resources)})
            require(!contains(p,source)&&!contains(source,p),"OE41 outputs overlap read-only inputs");
    }
    require(!contains(app,out)&&!contains(out,app),"App root and evidence output must be disjoint");
}
std::optional<std::uint64_t> current_earned_tick(const fs::path& resolved) {
    // Exact supplied earned checkpoints, not a broad original-economy directory
    // admission. MenuSession still validates every saved/source identity.
    const auto root=repo/".local/original-economy/oe48/oe49-final-create-01";
    for(const auto& [name,tick]:std::array<std::pair<const char*,std::uint64_t>,3>{{
            {"seed.save.json",0},{"first-success.save.json",5395},{"result.save.json",25795}}})
        if(resolved==fs::weakly_canonical(root/name))return tick;
    return std::nullopt;
}
bool current_playtest_input(const fs::path& resolved) {
    return (contains(current_playtest_root,resolved)&&resolved!=current_playtest_root)||
        current_earned_tick(resolved).has_value();
}
void current_checkpoint_identity(const fs::path& resolved,const oe::persistence::SaveDocument& document) {
    if(!current_playtest_input(resolved))return;
    require(document.world.profile==sim::RulesProfile::CityV16&&
        document.world.rule_version==sim::kOriginalDeadlineFoodDispatchRuleVersion&&
        document.source_schema_version==34&&document.map_permissions_policy_version==3&&
        document.map_relative==fs::path("Cities/Xia.map"),
        "OE50 checkpoint must retain its earned Rule18/schema34/policy3 Xia identity");
    if(const auto tick=current_earned_tick(resolved))
        require(document.world.ticks==*tick,"OE50 earned checkpoint tick differs from its supplied identity");
}
void names() {
    const auto source=bytes(repo/"src/app/SandboxUiLayout.h");
    const auto begin=source.find("enum class Action {");const auto end=source.find("};",begin);
    require(begin!=std::string::npos&&end!=std::string::npos,"Actual toolbar registration missing");
    const auto body=source.substr(begin+19,end-begin-19);const std::regex word("[A-Za-z][A-Za-z0-9_]*");
    int value=0;for(auto i=std::sregex_iterator(body.begin(),body.end(),word);i!=std::sregex_iterator();++i)action_names.emplace(value++,i->str());
}
std::vector<std::string> menu_labels() {
    // Candidate labels come from current registration, then actual diagnostics
    // confirm visibility, enabled state and action identity. Dynamic rows below
    // are independently discovered by bounded hit sampling.
    const auto source=bytes(repo/"src/app/MenuSession.cpp");const std::regex literal("\"([^\"\\n]*)\"");
    std::set<std::string> labels;for(auto i=std::sregex_iterator(source.begin(),source.end(),literal);i!=std::sregex_iterator();++i)labels.insert((*i)[1]);
    for(const auto* d:{"Very Easy","Easy","Normal","Hard","Very Hard"})labels.insert(std::string("D Difficulty: ")+d);
    for(const auto* s:{"Automatic","Off","Earthen","Stone","Brick"})labels.insert(std::string("G Great Wall: ")+s);
    return {labels.begin(),labels.end()};
}
struct Journey:Harness {
    using Harness::Harness;
    void reset_menu(const fs::path& explicit_data,const fs::path& root) {
        menu->shutdown();menu.reset();auto adapter=std::make_unique<FileDialog>();dialog=adapter.get();
        menu=std::make_unique<Menu>(explicit_data,root,std::move(adapter),resources);menu->initialize(window,renderer);
    }
    Json state() {
        if(menu->state()!=Menu::State::Playing)return {{"menu_state",static_cast<unsigned>(menu->state())},{"message",menu->message()}};
        const auto& w=view().world();const auto doc=view().capture_save_document();
        return {{"profile",sim::rules_profile_name(w.profile())},{"rule",w.rule_version()},
            {"schema_read",doc.source_schema_version?Json(doc.source_schema_version):Json(nullptr)},
            {"policy",doc.map_permissions_policy_version},{"tick",w.ticks()},{"funds",w.treasury()},
            {"construction",w.construction_spent_total()},{"tax",w.taxes_collected_total()},{"upkeep",w.maintenance_spent_total()},
            {"population",w.total_population()},{"buildings",w.buildings().size()},{"couriers",w.couriers().size()},
            {"command_sequence",w.command_sequence()},{"paused",view().paused()},{"tool",view().tool()},
            {"map",doc.map_relative.generic_string()},{"map_sha256",doc.map_sha256},{"hud",view().hud_status_line()}};
    }
    void note(const std::string& id,const std::string& observation) {
        events.push_back({{"id",id},{"observation",observation},{"state",state()},{"status","PASS"}});
    }
    void menu_click(const std::string& label) {
        const auto before=menu->state();Harness::menu_click(label);
        events.push_back({{"id","menu."+label},{"label",label},{"before_state",static_cast<unsigned>(before)},
            {"after_state",static_cast<unsigned>(menu->state())},{"status","PASS"},{"scope","actual mouse activation"}});
        for(auto& row:menu_rows)if(row["label"]==label&&row["menu_state"]==static_cast<unsigned>(before)) {
            row["executed"]=true;row["result"]="PASS";
        }
    }
    void capture(const std::string& name) {
        require(menu->render(),"Actual Menu/SandboxView render failed");
        std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> image(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
        const auto file=out/(name+".png");require(image&&!fs::exists(file)&&SDL_SavePNG(image.get(),file.string().c_str()),"Cannot capture actual frame");
        captures.push_back({{"file",file.filename().string()},{"width",image->w},{"height",image->h},{"state",state()},
            {"visually_inspected",false},{"runtime","scripted SDL dummy/software"}});
        if(menu->state()==Menu::State::Playing){const auto road=view().road_display_stats();const auto buildings=view().building_display_stats();const auto houses=view().original_house_display_stats();
            captures.back()["render_counters"]={{"original_road_ground_items",view().original_road_ground_items()},
                {"road_draws",road.draws},{"road_fallback_draws",road.fallback_draws},{"road_texture_uploads",road.texture_uploads},
                {"building_draws",buildings.drawn_instances},{"building_fallbacks",buildings.placeholder_fallbacks},
                {"house_draws",houses.drawn_instances},{"house_fallbacks",houses.authored_fallbacks},
                {"walker_draws",view().last_courier_draws()},{"building_textures",view().building_texture_count()},{"walker_textures",view().walker_texture_count()}};}
    }
    void menu_inventory(const std::string& context) {
        require(menu->render(),"Menu registration render failed");
        std::map<int,Json> found;
        for(const auto& label:menu_labels())if(const auto r=menu->diagnostic_button_rect(label)) {
            const double scale=1.5*std::max(1.0F,SDL_GetWindowPixelDensity(window));
            const auto p=window_point({(r->x+r->w*.5)*scale,(r->y+r->h*.5)*scale});
            if(const auto hit=menu->diagnostic_button_hit(float(p.x),float(p.y)))found[hit->action]=
                {{"action",hit->action},{"label",label},{"enabled",hit->enabled},{"label_provenance","registration candidate confirmed by actual diagnostic"}};
        }
        int width=0,height=0;require(SDL_GetWindowSize(window,&width,&height),SDL_GetError());
        for(int y=4;y<height;y+=8)for(int x=4;x<width;x+=8)if(const auto hit=menu->diagnostic_button_hit(float(x),float(y)))
            if(!found.contains(hit->action))found[hit->action]={{"action",hit->action},{"label","dynamic row or computed label"},
                {"enabled",hit->enabled},{"label_provenance","actual bounded hit sampling; label unavailable through public API"}};
        for(auto& [id,row]:found){(void)id;row["context"]=context;row["menu_state"]=static_cast<unsigned>(menu->state());
            row["available"]=true;row["visible"]=true;row["executed"]=false;row["result"]="NOT_RUN";menu_rows.push_back(row);}
        std::ofstream progress(out/"inventory-progress.json");progress<<Json{{"menu_controls",menu_rows},{"toolbar_controls",toolbar_rows},{"events",events}}.dump(2)<<'\n';
    }
    void wait_map() {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(menu->map_rules_result().status==oe::maps::MapRulesStatus::Checking&&std::chrono::steady_clock::now()<deadline){menu->advance();std::this_thread::yield();}
        require(menu->map_rules_result().status!=oe::maps::MapRulesStatus::Checking,"Actual map preflight deadline");
    }
    void choose_map(const std::string& name) {
        const auto catalog=oe::menu::validate_data_root(data);const fs::path target="Cities/"+name+".map";
        const auto wanted=std::find_if(catalog.entries.begin(),catalog.entries.end(),[&](const auto& e){return e.relative_path==target;});
        require(wanted!=catalog.entries.end(),"Requested original map absent: "+target.string());
        for(std::size_t i=0;i<catalog.entries.size();++i)key(SDLK_UP);
        for(auto i=catalog.entries.begin();i!=wanted;++i)key(SDLK_DOWN);
        wait_map();require(menu->map_rules_result().request.map_relative==target,"Actual menu selected another map");
        const oe::maps::MapGeometry geometry(*wanted->declared_size);require(geometry.supported,"Map geometry unsupported");border=geometry.border;
    }
    bool start() {
        wait_map();if(!menu->map_rules_can_start()){note("map.preflight","Actual selected map refused: "+menu->map_rules_result().reason);return false;}
        menu_click("Start sandbox");
        if(menu->state()==Menu::State::ConfirmLeave){menu_inventory("confirm-replace");menu_click("Discard Changes");}
        require(menu->state()==Menu::State::Loading,"Start did not defer actual load");
        require(menu->render(),"Actual Loading render failed");menu->advance();
        if(menu->state()==Menu::State::ConfirmLeave){menu_inventory("confirm-prepared-replace");menu_click("Discard Changes");}
        (void)view();focus();
        if(view().world().original_production_enabled())require(view().paused(),"New original production view must be paused");
        else paused(true);
        return true;
    }
    void back() {if(menu->state()==Menu::State::Playing)key(SDLK_ESCAPE);if(menu->state()!=Menu::State::MainMenu)key(SDLK_ESCAPE);require(menu->state()==Menu::State::MainMenu,"Cannot return to actual MainMenu");}
    void toolbar(const std::string& context,bool exercise) {
        require(menu->render(),"Toolbar render failed");const auto buttons=view().layout().buttons;
        for(const auto& button:buttons){const int id=static_cast<int>(button.action);const auto name=action_names.at(id);
            Json row{{"action",id},{"name",name},{"context",context},{"identity",state()},{"available",true},{"visible",true},
                {"layout_enabled",button.enabled},{"enabled",nullptr},{"executed",false},{"result","NOT_RUN"},
                {"source","src/app/SandboxUiLayout.cpp; src/app/SandboxView.cpp::action_enabled"}};
            // Exercise every construction selector without granting a command.
            const bool selector=button.action<=A::Market||button.action==A::FireWatch||button.action==A::Well||button.action==A::HealthPost||button.action==A::Bridge||button.action==A::Ramp||
                (button.action>=A::OriginalHempFarm&&button.action!=A::ToggleIndustryPage);
            if(exercise&&selector){ui(A::Select);const auto before=view().capture_save_document();const auto old=view().tool();
                ui(button.action);same_document(before,view().capture_save_document());
                row["executed"]=true;row["tool_before"]=old;row["tool_after"]=view().tool();row["message"]=view().last_message();
                row["enabled"]=view().tool()!=old||button.action==A::Select;row["result"]="PASS";row["scope"]="selector event only; purchase separately reported";}
            toolbar_rows.push_back(row);
        }ui(A::Select);
    }
    void zoom_images(const std::string& prefix) {
        const auto map=view().layout().map;Point anchor{map.x+map.w*.5,map.y+map.h*.5};
        for(const auto& b:view().world().buildings())if(b.placed&&sim::original_house_kind(b.kind))if(const auto p=ground(b.cell)){anchor=*p;break;}
        for(int z:{1,2,4}){zoom(z,anchor);const auto before=view().capture_save_document();
            for(int i=0;i<3;++i){SDL_Event e{};e.type=SDL_EVENT_MOUSE_MOTION;const auto raw=window_point(anchor);e.motion.x=float(raw.x);e.motion.y=float(raw.y);event(e);menu->update(0);require(menu->render(),"Purity frame failed");}
            same_document(before,view().capture_save_document());capture(prefix+"-"+std::to_string(z)+"x");}
        zoom(1,anchor);note("B03.B17.purity","Actual hover/update(0)/render at 1/2/4x retained complete SaveDocument authority");
    }
    void window_zoom_images(const std::string& prefix) {
        const auto before=view().capture_save_document();
        for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,900},{1920,1080}}}) {
            require(SDL_SetWindowSize(window,size[0],size[1]),SDL_GetError());SDL_Event e{};
            e.type=SDL_EVENT_WINDOW_RESIZED;e.window.data1=size[0];e.window.data2=size[1];event(e);focus();
            require(menu->render(),"Actual resized playtest render failed");
            int width=0,height=0;
            require(SDL_GetRenderOutputSize(renderer,&width,&height)&&width==size[0]&&height==size[1],
                "Actual playtest resize output differs from requested aperture");
            zoom_images(prefix+"-"+std::to_string(width)+"x"+std::to_string(height));
            same_document(before,view().capture_save_document());
        }
    }
    void center_world(Point world) {
        const auto map=view().layout().map;const Point middle{map.x+map.w*.5,map.y+map.h*.5};zoom(1,middle);
        // Two normal wheel gestures at different anchors translate the camera.
        // No camera setter or injected held-device state is used.
        for(int n=0;n<48;++n){const auto p=view().camera().world_to_screen(world);const double dx=middle.x-p.x,dy=middle.y-p.y;
            if(std::abs(dx)<.01&&std::abs(dy)<.01)return;
            const double x=std::clamp(dx,-map.w*.3,map.w*.3),y=std::clamp(dy,-map.h*.3,map.h*.3);
            zoom(2,{middle.x-x,middle.y-y});zoom(1,{middle.x+x,middle.y+y});}
        throw std::runtime_error("Bounded actual wheel centering did not converge");
    }
    void center_cell(sim::Cell cell) {
        const auto& w=view().world();const double h=w.map_permissions()?w.map_permissions()->cell_height(cell):0;
        center_world({(cell.x-cell.y)*40.0,(cell.x+cell.y-2.0*border)*20+40-h*40});
    }
    bool select_body(sim::BuildingId id) {
        ui(A::Select);if(view().debug_diagnostics())key(SDLK_F1);const auto& b=view().world().building(id);center_cell(b.cell);
        for(const auto cell:sim::building_footprint_cells(view().world().profile(),view().world().rule_version(),b.kind,b.cell))
            if(const auto p=ground(cell)){click(*p);if(view().selected_building()==id)return true;}
        return false;
    }
    void operations() {
        for(const auto& body:view().world().buildings())if(body.placed&&body.kind==sim::Object::Market&&select_body(body.id)) {
            const auto layout=view().inspector_layout();const auto rect=layout.operation_toggle;
            const auto before=view().world().snapshot();const auto required=view().world().workforce_required(body.id);
            click({rect.x+rect.w*.5,rect.y+rect.h*.5});require(!view().world().building(body.id).operating_enabled,"Actual operation pause failed");
            require(view().world().maintenance_spent_total()==before.maintenance_spent_total&&view().world().workforce_required(body.id)==required,"Pause waived installed workforce/upkeep authority");
            click({rect.x+rect.w*.5,rect.y+rect.h*.5});require(view().world().building(body.id).operating_enabled,"Actual operation resume failed");
            for(int index:{0,2,1}) {
                const auto priority=layout.operation_priorities[std::size_t(index)];
                require(priority.w>0&&priority.h>0,"Actual priority control is clipped");
                click({priority.x+priority.w*.5,priority.y+priority.h*.5});
                require(view().world().building(body.id).workforce_priority==static_cast<sim::WorkforcePriority>(index),
                    "Actual priority click did not select its displayed priority");
            }
            require(view().world().building(body.id).workforce_priority==sim::WorkforcePriority::Normal,"Actual priority controls failed");
            events.push_back({{"id","B08.operation"},{"status","PASS"},{"owner",static_cast<unsigned>(body.id)},{"scope","actual inspector pause/resume and High/Low/Normal; installed worker requirement retained"},{"state",state()}});return;
        }
        events.push_back({{"id","B08.operation"},{"status","NOT_RUN"},{"reason","No paid Market selectable through current UI"}});
    }
    void protected_refusal() {
        const auto& w=view().world();if(!w.map_permissions())return;
        for(int y=0;y<w.height();++y)for(int x=0;x<w.width();++x){const sim::Cell cell{x,y};
            if(!w.map_permissions()->protected_original(cell))continue;const auto decision=w.validate({sim::CommandType::PlaceRoad,cell});
            if(decision.accepted)continue;center_cell(cell);ui(A::Road);const auto p=ground(cell);if(!p)continue;
            const auto before=w.snapshot();click(*p);require(w.snapshot()==before,"Protected original-cell UI placement changed World");
            events.push_back({{"id","B04.protected-source"},{"status","PASS"},{"cell",{x,y}},{"validator_reason",decision.reason},
                {"actual_ui_message",view().last_message()},{"state",state()}});ui(A::Select);return;}
        events.push_back({{"id","B04.protected-source"},{"status","NOT_RUN"},{"reason","No protected original cell with a selectable current ground point"}});
    }
    bool buy(A action,sim::CommandType command,const std::string& id) {
        ui(action);const auto& w=view().world();
        for(int y=0;y<w.height();++y)for(int x=0;x<w.width();++x){const sim::Cell c{x,y};const auto valid=w.validate({command,c});
            if(!valid.accepted||!valid.changed)continue;const auto point=ground(c);if(!point)continue;
            const auto before=w.snapshot();const auto cost=w.construction_cost(command);click(*point);
            if(view().budget_warning_pending()){key(SDLK_ESCAPE);continue;}
            if(w.snapshot()==before)continue;
            require(w.command_sequence()==before.command_sequence+1&&w.treasury()==before.treasury-cost&&w.construction_spent_total()==before.construction_spent_total+static_cast<std::uint64_t>(cost),"UI purchase bypassed normal paid ledger");
            events.push_back({{"id",id},{"status","PASS"},{"cell",{x,y}},{"quote",cost},{"owner",w.building_owner_at(c)?Json(static_cast<unsigned>(*w.building_owner_at(c))):Json(nullptr)},{"state",state()}});
            const auto after=w.snapshot();click(*point);if(view().budget_warning_pending())key(SDLK_ESCAPE);
            require(w.snapshot()==after,"Duplicate footprint refusal changed authority");note(id+".duplicate","Repeated occupied-cell placement left exact World unchanged");ui(A::Select);return true;
        }
        ui(A::Select);events.push_back({{"id",id},{"status","NOT_RUN"},{"reason","No visible affordable legal cell; no placement bypass"},{"state",state()}});return false;
    }
    void road_cancellations() {
        paused(true);if(view().debug_diagnostics())key(SDLK_F1);ui(A::Road);
        std::optional<std::pair<Point,Point>> endpoints;
        const auto& world=view().world();
        for(int y=0;y<world.height()&&!endpoints;++y)for(int x=0;x<world.width()&&!endpoints;++x) {
            const sim::Cell cell{x,y};const auto first=world.validate({sim::CommandType::PlaceRoad,cell});
            if(!first.accepted||!first.changed)continue;const auto from=ground(cell);if(!from)continue;
            for(const sim::Cell delta:std::array<sim::Cell,2>{{{1,0},{0,1}}}) {
                const sim::Cell next{x+delta.x,y+delta.y};const auto last=world.validate({sim::CommandType::PlaceRoad,next});
                if(!last.accepted||!last.changed)continue;
                if(const auto to=ground(next)){endpoints=std::pair{*from,*to};break;}
            }
        }
        require(endpoints.has_value(),"No actual visible legal two-cell drag for cancellation checks");
        const auto initial=view().capture_save_document();
        const auto mouse=[&](Uint32 type,Point point,Uint8 button=SDL_BUTTON_LEFT) {
            const auto raw=window_point(point);SDL_Event event{};event.type=type;
            if(type==SDL_EVENT_MOUSE_MOTION){event.motion.x=float(raw.x);event.motion.y=float(raw.y);}
            else {event.button.button=button;event.button.x=float(raw.x);event.button.y=float(raw.y);}
            this->event(event);
        };
        for(const auto* cancellation:{"escape","right-click","focus-loss","resize","ui-release"}) {
            focus();ui(A::Road);mouse(SDL_EVENT_MOUSE_MOTION,endpoints->first);
            mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,endpoints->first);mouse(SDL_EVENT_MOUSE_MOTION,endpoints->second);
            require(view().input_diagnostic_state().road_drag,"Actual press/motion did not begin its road gesture");
            Point release=endpoints->second;
            if(std::string_view(cancellation)=="escape")key(SDLK_ESCAPE);
            else if(std::string_view(cancellation)=="right-click") {
                mouse(SDL_EVENT_MOUSE_BUTTON_DOWN,release,SDL_BUTTON_RIGHT);mouse(SDL_EVENT_MOUSE_BUTTON_UP,release,SDL_BUTTON_RIGHT);
            } else if(std::string_view(cancellation)=="focus-loss") {
                SDL_Event event{};event.type=SDL_EVENT_WINDOW_FOCUS_LOST;this->event(event);focus();
            } else if(std::string_view(cancellation)=="resize") {
                require(SDL_SetWindowSize(window,1600,900),SDL_GetError());SDL_Event event{};
                event.type=SDL_EVENT_WINDOW_RESIZED;event.window.data1=1600;event.window.data2=900;this->event(event);
            } else {
                const auto toolbar=view().layout().toolbar;
                release={toolbar.x+toolbar.w*.5,toolbar.y+toolbar.h*.5};
            }
            mouse(SDL_EVENT_MOUSE_BUTTON_UP,release);menu->advance();
            require(menu->state()==Menu::State::Playing&&!view().input_diagnostic_state().road_drag,
                "Cancellation left a road gesture or exited its city");
            same_document(initial,view().capture_save_document());
            if(std::string_view(cancellation)=="resize") {
                require(SDL_SetWindowSize(window,1280,800),SDL_GetError());SDL_Event event{};
                event.type=SDL_EVENT_WINDOW_RESIZED;event.window.data1=1280;event.window.data2=800;this->event(event);
                require(menu->render(),"Render after actual cancellation resize failed");
            }
            note(std::string("B04.road-cancel.")+cancellation,"Actual SDL press/motion/cancel/release retained complete SaveDocument; no road or charge");
        }
        ui(A::Select);
    }
    void controls() {
        paused(true);const auto before=view().world().ticks();ui(A::Step);require(view().world().ticks()==before+1,"Actual Step did not advance once");
        ui(A::Speed2);ui(A::Speed4);ui(A::Speed1);note("B03.clock","Clicked Step, Speed2, Speed4, Speed1 through the real toolbar");
        const auto state_before=view().capture_save_document();
        for(const auto keycode:{SDLK_F1,SDLK_G,SDLK_F4,SDLK_F6,SDLK_F7,SDLK_F8,SDLK_F10}){key(keycode);require(menu->render(),"Debug/overlay render failed");
            same_document(state_before,view().capture_save_document());note("B17.key."+std::to_string(keycode),"Actual diagnostic/overlay key routed; authority unchanged");}
        key(SDLK_F1);key(SDLK_H);capture("help");key(SDLK_ESCAPE);ui(A::ToggleIncome);require(menu->render(),"Income render failed");ui(A::ToggleIncome);
        ui(A::TogglePanel);ui(A::TogglePanel);same_document(state_before,view().capture_save_document());
        SDL_Event e{};e.type=SDL_EVENT_WINDOW_FOCUS_LOST;event(e);e.type=SDL_EVENT_WINDOW_FOCUS_GAINED;event(e);
        same_document(state_before,view().capture_save_document());note("B03.focus","Actual focus loss/gain retained full authority");
    }
    void save_checkpoint(const std::string& label) {
        key(SDLK_F5);require(!menu->settings().last_save.empty(),"Actual F5 did not publish manual save");
        const auto path=menu->settings().last_save;const auto input=bytes(path);const auto saved=oe::persistence::read_save(path);
        require(saved.world==view().world().snapshot(),"Actual F5 snapshot differs");
        fs::copy_file(path,out/(label+".save.json"));key(SDLK_PERIOD);key(SDLK_F9);
        require(view().paused()&&view().world().snapshot()==saved.world&&bytes(path)==input,"Actual F9 changed complete saved authority/bytes");
        key(SDLK_F5);require(bytes(path)==input,"Actual F5 canonical complete document differs after F9");note("B18.manual","Actual F5/Step/F9/F5 complete snapshot and canonical document bytes equal");
    }
};

void inventory(Journey& h) {
    h.reset_menu({},h.app/"inventory-selection");h.menu_inventory("data-setup");h.menu_click("Choose Game Data Folder");h.dialog->select(h.data);h.menu->advance();
    h.menu_inventory("main");h.menu_click("N New Sandbox");h.menu_inventory("ordinary-new-default");
    require(h.menu->settings().profile==sim::RulesProfile::CityV11,"General default changed");
    for(int i=0;i<15;++i)h.menu_click("Previous rules");
    h.choose_map("Xia");
    for(int p=0;p<16;++p){h.menu_inventory("profile-"+std::to_string(p));
        if(h.start()){h.toolbar("profile-"+std::to_string(p),true);if(p==10){h.key(SDLK_F5);fs::copy_file(h.menu->settings().last_save,h.out/"city11.save.json");}h.back();h.menu_click("N New Sandbox");}
        if(p<15)h.menu_click("Next rules");}
    h.menu_click("Advanced...");h.menu_inventory("advanced-visuals");h.key(SDLK_ESCAPE);
    const std::array<const char*,10> entries{"Play Original economy","O Original economy","F Original fire protection","S Sustainable economy","M Needs-aware logistics","W Cardinal-water economy",
        "I Inspector coordination","D Deadline-aware fire dispatch","T Deadline-aware services","U Deadline-aware food"};
    for(const auto* label:entries){h.menu_click(label);h.choose_map("Xia");
        for(int d=0;d<5;++d){h.menu_inventory(std::string(label)+"-difficulty-"+std::to_string(d));h.key(SDLK_D);}
        h.key(SDLK_P);h.menu_inventory(std::string(label)+"-empty");h.key(SDLK_P);
        if(h.start()){h.toolbar(label,true);h.ui(A::ToggleIndustryPage);h.toolbar(std::string(label)+"-industry",true);h.ui(A::ToggleIndustryPage);
            h.capture(std::string_view(label)=="Play Original economy" ? "recommended-rule18":"rule-"+std::to_string(h.view().world().rule_version()));h.key(SDLK_F5);}
        h.back();}
    h.menu_click("L Load Save");h.menu_inventory("manual-load");h.menu_click("Show recovery history");h.menu_inventory("recovery-load");
    if(h.menu->diagnostic_button_rect("Delete history...")){h.menu_click("Delete history...");h.menu_inventory("confirm-delete");h.menu_click("Cancel");}
    h.key(SDLK_ESCAPE);
    const auto upgrade=h.menu->app_root()/"saves/OE41-City11.json";fs::copy_file(h.out/"city11.save.json",upgrade);
    h.menu_click("L Load Save");if(h.menu->diagnostic_button_rect("Show manual saves"))h.menu_click("Show manual saves");
    const auto saves=oe::menu::list_saves(h.menu->app_root());bool found=false;
    for(const auto& entry:saves.entries){if(entry.path==upgrade){found=true;break;}h.menu_click("Next");}
    require(found,"Actual paid City11 save missing from manual list");h.menu_inventory("upgrade-eligible");
    h.menu_click("Enable demolition in a copy");h.menu_inventory("confirm-upgrade");h.menu_click("Cancel");h.key(SDLK_ESCAPE);
    h.note("B02.inventory","All16 ordinary profile choices, recommended entry and nine original generations/difficulties, both original tool pages, manual/recovery and confirmation controls visited");
    const auto bad=h.app/"invalid-preferences";fs::create_directories(bad);{std::ofstream file(bad/"settings.json");file<<"{invalid";}
    h.reset_menu({},bad);h.menu_inventory("invalid-preferences-data-setup");h.menu_click("Reset preferences");
}
void create(Journey& h) {
    h.reset_menu({},h.app/"data-selection");require(h.menu->state()==Menu::State::DataSetup,"Fresh data-selection entry missing");h.menu_inventory("data-setup");
    h.menu_click("Choose Game Data Folder");h.dialog->select(h.out/"invalid-data");h.menu->advance();
    require(h.menu->state()==Menu::State::DataSetup,"Invalid data root accepted");h.capture("data-refused");
    h.menu_click("Choose Game Data Folder");h.dialog->select(h.data);h.menu->advance();require(h.menu->state()==Menu::State::MainMenu,"Supplied original data rejected");h.note("B01","Invalid root refused then explicit read-only supplied root selected");
    h.menu_click("Play Original economy");h.choose_map("Xia");require(h.start(),"Recommended Rule18 paid start refused");
    const auto initial=h.view().capture_save_document();
    require(initial.world.rule_version==sim::kOriginalDeadlineFoodDispatchRuleVersion&&initial.map_permissions_policy_version==3&&
        h.view().world().original_rules_model()->difficulty()==oe::original::Difficulty::Normal,
        "Recommended menu did not select the existing Normal Rule18/policy3 contract");
    require(h.view().world().ticks()==0&&h.view().world().construction_spent_total()>0,"Paid menu starter did not begin normally");
    h.toolbar("rule18-main",true);h.ui(A::ToggleIndustryPage);h.toolbar("rule18-industry",true);h.ui(A::ToggleIndustryPage);
    h.controls();h.road_cancellations();h.buy(A::Road,sim::CommandType::PlaceRoad,"B04.road");h.buy(A::Household,sim::CommandType::PlaceHousehold,"B04.house");h.operations();
    h.ticks(900);h.note("B08.staffing","900 real TickDriver ticks after paid starter and extra charged UI placements");
    h.window_zoom_images("paid-city");h.save_checkpoint("journey");
    const auto saved=h.view().world().snapshot();h.back();h.menu_inventory("main-with-session");h.menu_click("Autosave: ON");h.menu_click("Autosave: OFF");
    h.menu_click("L Load Save");h.menu_click("Show recovery history");h.menu_inventory("paid-recovery");h.menu_click("Open selected");
    if(h.menu->state()==Menu::State::ConfirmLeave)h.menu_click("Discard Changes");
    require(h.menu->state()==Menu::State::Loading,"Recovery did not enter Loading");require(h.menu->render(),"Recovery loading render failed");h.menu->advance();h.focus();
    require(h.view().world().snapshot()==saved,"Freshly published actual Recovery differs from manual state");h.note("B18.recovery","Actual menu recovery entry restored complete paid snapshot");
    const auto previous_map=h.view().capture_save_document().map_sha256;
    h.back();h.menu_click("N New Sandbox");h.choose_map("Handan");if(h.start()){
        const auto document=h.view().capture_save_document();require(h.view().world().profile()==sim::RulesProfile::CityV11&&h.view().world().rule_version()==3&&
            document.map_relative==fs::path("Cities/Handan.map")&&document.map_sha256!=previous_map,"Second menu session leaked old profile/map authority");
        h.note("B20.second-session","Second actual ordinary City11/rule3 Handan replaced Rule18 Xia through menu, with a distinct map SHA");h.capture("second-session");
    }else scenario_results.push_back({{"id","B20.second-session"},{"status","BLOCKED"},{"reason",h.menu->map_rules_result().reason}});
}
void resume(Journey& h,const fs::path& input) {
    no_symlinks(input);const auto resolved=fs::canonical(input);
    require(contains(private_root,resolved)||contains(playtest_root,resolved)||current_playtest_input(resolved),
        "Only explicit OE41/OE43/OE50 earned save accepted");
    const auto original=bytes(input);const auto document=oe::persistence::read_save(input);current_checkpoint_identity(resolved,document);
    const auto working=h.out/"working-input.save.json";fs::copy_file(input,working);
    h.menu_click("L Load Save");h.menu_click("Open save file...");h.dialog->select(working);h.menu->advance();
    require(h.menu->state()==Menu::State::Loading,"Independent actual menu load not deferred");require(h.menu->render(),"Independent loading render failed");h.menu->advance();h.focus();
    require(h.view().paused(),"Independent actual menu load is not paused");same_document(document,h.view().capture_save_document());
    h.key(SDLK_F5);require(bytes(h.menu->settings().last_save)==original,"Independent full canonical document bytes differ");h.note("B18.new-process","Independent process actual Menu load and F5 retain complete original document bytes");
    h.ticks(160);h.save_checkpoint("continued");require(bytes(input)==original,"Independent continuation altered protected input");h.window_zoom_images("resumed-city");
}
void map_journey(Journey& h,const std::string& map) {
    h.menu_click("N New Sandbox");while(h.menu->settings().profile!=sim::RulesProfile::CityV16)h.menu_click("Next rules");
    h.menu_click("Prepared starter settlement");h.choose_map(map);h.menu_inventory("map-"+map);h.capture("map-choice");
    if(!h.start()){scenario_results.push_back({{"id","B16"},{"map",map},{"status","BLOCKED"},{"reason",h.menu->map_rules_result().reason}});return;}
    require(h.view().world().rule_version()==4&&h.view().world().buildings().empty(),"Map view was not ordinary Rule4 Empty");
    h.zoom_images("map-"+map);h.buy(A::Road,sim::CommandType::PlaceRoad,"B04.map-road");h.protected_refusal();
    h.note("B16","Actual original map Menu→Rule4 Empty→SandboxView, 1/2/4x captures and paid Road attempt");h.save_checkpoint("map");
    scenario_results.push_back({{"id","B16"},{"map",map},{"status","PASS"},{"scope","scripted Menu/SandboxView and captures; visual inspection recorded separately"}});
}
void inspect_paid(Journey& h,const fs::path& input) {
    no_symlinks(input);const auto resolved=fs::canonical(input);
    require(contains(repo/".local/original-economy/oe40",resolved)||contains(repo/".local/original-economy/oe39",resolved)||contains(private_root,resolved)||
        current_playtest_input(resolved),"Only explicit earned task save accepted");
    const auto original=bytes(input);const auto document=oe::persistence::read_save(input);current_checkpoint_identity(resolved,document);
    const auto working=h.out/"working-input.save.json";fs::copy_file(input,working);
    h.menu_click("L Load Save");h.menu_click("Open save file...");h.dialog->select(working);h.menu->advance();
    require(h.menu->state()==Menu::State::Loading,"Paid inspection load did not defer");require(h.menu->render(),"Paid Loading render failed");h.menu->advance();h.focus();
    require(h.view().paused()&&h.view().world().snapshot()==document.world,"Paid inspection load altered actual World");const auto snapshot=h.view().world().snapshot();
    Json bodies=Json::array(),actors=Json::array(),tabs=Json::array();
    for(const auto& body:h.view().world().buildings())if(body.placed){const bool selected=h.select_body(body.id);
        bodies.push_back({{"id",static_cast<unsigned>(body.id)},{"object",static_cast<unsigned>(body.kind)},{"selected",selected},{"status",selected?"PASS":"NOT_RUN"},{"lines",selected?h.view().inspection_lines():std::vector<std::string>{}}});
        if(selected&&sim::original_house_kind(body.kind)){require(h.view().original_house_inspector_active(),"Actual House selection lacks inspector");
            for(unsigned n=0;n<4;++n){const auto section=static_cast<oe::SandboxView::OriginalHouseSection>(n);const auto r=h.view().original_house_section_rect(section);h.click({r.x+r.w*.5,r.y+r.h*.5});
                require(h.view().original_house_section()==section,"Actual House tab did not select");tabs.push_back({{"house",static_cast<unsigned>(body.id)},{"section",n},{"lines",h.view().original_house_section_lines()},{"status","PASS"}});}
            const auto r=h.view().original_house_overlay_rect();for(int n=0;n<4;++n)h.click({r.x+r.w*.5,r.y+r.h*.5});}
        same_document(document,h.view().capture_save_document());}
    h.ui(A::Select);if(!h.view().debug_diagnostics())h.key(SDLK_F1);require(h.menu->render(),"Actor frame render failed");
    for(const auto& courier:h.view().world().couriers()){
        auto diagnostics=h.view().walker_diagnostics();const auto first=std::find_if(diagnostics.begin(),diagnostics.end(),[&](const auto& d){return d.id==courier.id;});
        require(first!=diagnostics.end(),"Actual courier has no bounded diagnostic row");const bool live=first->live_visible;
        bool selected=false;if(live){const auto world=h.view().camera().screen_to_world(first->screen_ground);h.center_world(world);require(h.menu->render(),"Centered actor render failed");
            diagnostics=h.view().walker_diagnostics();const auto d=std::find_if(diagnostics.begin(),diagnostics.end(),[&](const auto& row){return row.id==courier.id;});
            if(d!=diagnostics.end()&&d->submitted)for(int y=0;y<int(d->image_height)&&!selected;y+=4)for(int x=0;x<int(d->image_width)&&!selected;x+=4){
                const Point point{d->image_origin.x+x,d->image_origin.y+y};if(!h.view().layout().map.contains(point.x,point.y))continue;
                h.click(point);selected=h.view().input_diagnostic_state().selected_walker==static_cast<std::uint32_t>(courier.id);}}
        actors.push_back({{"id",static_cast<unsigned>(courier.id)},{"owner",static_cast<unsigned>(courier.owner)},{"role",static_cast<unsigned>(courier.role)},
            {"phase",static_cast<unsigned>(courier.phase)},{"live_visible",live},{"selected",selected},{"status",selected?"PASS":"NOT_RUN"},{"reason",selected?"actual alpha-hit selection":live?"No selectable visible pixel found":"idle/non-live actor has no pickable frame"}});
        same_document(document,h.view().capture_save_document());}
    h.key(SDLK_F1);h.window_zoom_images("earned-city");h.key(SDLK_F5);require(bytes(h.menu->settings().last_save)==original,"Paid inspection F5 full bytes differ");h.key(SDLK_F9);
    require(h.view().world().snapshot()==snapshot&&bytes(input)==original,"Paid inspection F9 or protected source changed");
    json_file(h.out/"inspection.json",{{"bodies",bodies},{"actors",actors},{"house_tabs",tabs},{"complete_world_unchanged",true},{"complete_f5_f9_bytes_equal",true}});
    h.note("B17.paid-inspection","Actual earned-city building, actor, House tab/overlay selection; individual inaccessible rows remain NOT_RUN");
}

// OE42: the real paid OE41/OE40 checkpoints, actual window resizes, normal view
// input and production drawing. No authored World or edited save substitutes.
void inspector_layout(Journey& h,const fs::path& input) {
    no_symlinks(input);
    const auto resolved=fs::canonical(input);
    const bool paid901=resolved==fs::weakly_canonical(private_root/"run-01/journey-create-02/journey.save.json");
    const auto earned=repo/".local/original-economy/oe40/run-01/gate-c-release-lean-med-02/first-success.save.json";
    const bool current=current_playtest_input(resolved);
    require(paid901||resolved==fs::weakly_canonical(earned)||current,"Inspector layout requires an explicit OE41/OE40/OE50 earned checkpoint");
    const auto original=bytes(input);const auto document=oe::persistence::read_save(input);
    if(current)current_checkpoint_identity(resolved,document);
    else require(document.world.ticks==(paid901?901:7333)&&document.world.rule_version==14&&document.map_relative==fs::path("Cities/Xia.map"),
        "OE42 checkpoint identity is not the supplied Rule14/Xia paid generation");
    const auto working=h.out/"working-input.save.json";fs::copy_file(input,working);
    h.menu_click("L Load Save");h.menu_click("Open save file...");h.dialog->select(working);h.menu->advance();
    require(h.menu->state()==Menu::State::Loading,"OE42 menu load was not deferred");
    require(h.menu->render(),"OE42 loading render failed");h.menu->advance();h.focus();
    require(h.view().paused()&&h.view().world().snapshot()==document.world,"OE42 menu restore changed complete World");
    const auto initial=h.view().world().snapshot();Json checks=Json::array();
    const auto pure=[&] {same_document(document,h.view().capture_save_document());};
    const auto middle=[](oe::sandbox_ui::Rect r) {return Point{r.x+r.w*.5,r.y+r.h*.5};};
    const auto wheel=[&](Point point,float amount) {
        const auto raw=h.window_point(point);SDL_Event e{};e.type=SDL_EVENT_MOUSE_WHEEL;
        e.wheel.mouse_x=float(raw.x);e.wheel.mouse_y=float(raw.y);e.wheel.y=amount;h.event(e);h.menu->update(0);
    };
    const auto render=[&] {require(h.menu->render(),"OE42 actual inspector render failed");pure();};
    const auto geometry=[&] {
        const auto layout=h.view().inspector_layout();const auto panel=h.view().layout().panel;
        std::vector<oe::sandbox_ui::Rect> fixed{layout.operation_toggle,layout.demolition,layout.hints};
        fixed.insert(fixed.end(),layout.operation_priorities.begin(),layout.operation_priorities.end());
        const auto overlaps=[](auto a,auto b) {return a.w>0&&a.h>0&&b.w>0&&b.h>0&&
            a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;};
        for(std::size_t i=0;i<fixed.size();++i)if(fixed[i].w>0&&fixed[i].h>0) {
            require(panel.contains(fixed[i].x,fixed[i].y)&&panel.contains(fixed[i].x+fixed[i].w-1,fixed[i].y+fixed[i].h-1),
                "Inspector fixed control escaped its panel");
            require(!overlaps(layout.content,fixed[i]),"Inspector content overlaps fixed controls/hints");
            for(std::size_t j=0;j<i;++j)require(!overlaps(fixed[i],fixed[j]),"Inspector fixed regions overlap");
        }
        return layout;
    };
    const auto scroll_document=[&](const std::string& capture_prefix) {
        bool detail_captured=false;
        auto layout=geometry();wheel(middle(layout.content),1000000);render();layout=geometry();
        require(h.view().inspector_scroll()==0,"Inspector content cannot return to its beginning");
        std::vector<bool> list_seen(layout.list_rows),detail_seen(layout.detail_rows);
        const auto collect=[&](const auto& current) {
            for(std::size_t i=0;i<list_seen.size();++i)if(current.list_row_rect(i).h>0)list_seen[i]=true;
            for(std::size_t i=0;i<detail_seen.size();++i)if(current.detail_row_rect(i).h>0)detail_seen[i]=true;
        };
        for(int n=0;;++n) {
            layout=geometry();collect(layout);
            if(!capture_prefix.empty()&&!detail_captured&&layout.detail_row_rect(0).h>0&&
                layout.detail_row_rect(0).y<=layout.content.y+24*layout.scale) {
                h.capture(capture_prefix+"-details");detail_captured=true;
            }
            require(n<1024,"Inspector content failed bounded scroll traversal");
            if(layout.scroll==layout.scroll_limit)break;
            const auto before=layout.scroll;wheel(middle(layout.content),-1);render();
            require(h.view().inspector_scroll()>before,"Inspector content cannot reach its final rows");
        }
        require(std::all_of(list_seen.begin(),list_seen.end(),[](bool v){return v;})&&
            std::all_of(detail_seen.begin(),detail_seen.end(),[](bool v){return v;}),
            "At least one building/detail row is unreachable through actual scrolling");
        wheel(middle(layout.content),-1000000);render();
        require(h.view().inspector_scroll()==layout.scroll_limit,"Inspector content overscroll escaped its limit");
        if(layout.hints.h>0) {
            wheel(middle(layout.hints),1000000);render();std::vector<bool> seen(layout.hint_rows);
            for(int n=0;;++n) {
                const auto current=geometry();for(std::size_t i=0;i<seen.size();++i)if(current.hint_row_rect(i).h>0)seen[i]=true;
                require(n<128,"Demolition hints failed bounded scroll traversal");
                if(current.hint_scroll==current.hint_scroll_limit)break;
                wheel(middle(current.hints),-1);render();
                require(h.view().inspector_hint_scroll()>current.hint_scroll,"Demolition hint scroll stalled");
            }
            require(std::all_of(seen.begin(),seen.end(),[](bool v){return v;}),"At least one demolition hint is unreachable");
        }
        return std::pair{list_seen.size(),detail_seen.size()};
    };
    const auto inert_edges=[&] {
        const auto layout=geometry();const auto panel=h.view().layout().panel;const auto selected=h.view().selected_building();
        for(const Point p:{Point{panel.x+panel.w*.5,double(panel.y+2)},
                           Point{panel.x+panel.w*.5,double(panel.y+panel.h-2)}}) {
            h.click(p);require(h.view().selected_building()==selected,"Header/footer selected an invisible building row");pure();
        }
        // A partial line at either content edge is absent from both drawing
        // and selection. Probe its visible fragment through real press/release.
        for(std::size_t i=0;i<layout.list_rows;++i)if(layout.list_row_rect(i).h==0) {
            const int y=layout.list_y+int(i)*18*layout.scale-layout.scroll;
            const int a=std::max(y-2*layout.scale,layout.content.y);
            const int b=std::min(y+14*layout.scale,layout.content.y+layout.content.h);
            if(a>=b)continue;const Point p{layout.content.x+layout.content.w*.5,(a+b)*.5};
            require(!layout.list_row_at(p.x,p.y),"Clipped row has a live layout hitbox");h.click(p);
            require(h.view().selected_building()==selected,"Clipped invisible row remained clickable");pure();
        }
    };
    std::array<sim::BuildingId,4> owners{};
    for(const auto& body:h.view().world().buildings())if(body.placed) {
        if(body.kind==sim::Object::Market&&static_cast<unsigned>(owners[0])==0&&(!paid901||static_cast<unsigned>(body.id)==7))owners[0]=body.id;
        if(body.kind==sim::Object::Warehouse)owners[1]=body.id;
        if(body.kind==sim::Object::FireWatch)owners[2]=body.id;
        if(sim::original_house_kind(body.kind)&&static_cast<unsigned>(owners[3])==0)owners[3]=body.id;
    }
    require(static_cast<unsigned>(owners[0])>0&&static_cast<unsigned>(owners[1])>0&&static_cast<unsigned>(owners[3])>0&&
        (paid901||(current&&document.world.ticks==0)||static_cast<unsigned>(owners[2])>0),"Paid checkpoint lacks its required Market, Warehouse, FireWatch or House");
    const std::array<std::string,4> names{"market"+std::to_string(static_cast<unsigned>(owners[0])),"warehouse","firewatch","house"};
    if(static_cast<unsigned>(owners[2])==0)
        scenario_results.push_back({{"id","inspector.FireWatch"},{"checkpoint_tick",document.world.ticks},{"status","NOT_RUN"},
            {"reason","This earned checkpoint has no FireWatch; no building is granted for inspection"}});
    for(const auto size:std::array<std::array<int,2>,3>{{{1280,800},{1600,900},{1920,1080}}}) {
        require(SDL_SetWindowSize(h.window,size[0],size[1]),SDL_GetError());SDL_Event resized{};
        resized.type=SDL_EVENT_WINDOW_RESIZED;resized.window.data1=size[0];resized.window.data2=size[1];h.event(resized);h.focus();render();
        int width=0,height=0;require(SDL_GetRenderOutputSize(h.renderer,&width,&height)&&width==size[0]&&height==size[1],"Actual resize output differs from requested size");
        const auto dimensions=std::to_string(width)+"x"+std::to_string(height);
        for(std::size_t kind=0;kind<owners.size();++kind) {
            if(static_cast<unsigned>(owners[kind])==0)continue;
            require(h.select_body(owners[kind]),std::string("Cannot select actual paid ")+names[kind]);render();
            const auto map=h.view().layout().map;const Point anchor=middle(map);
            for(int zoom:{1,2,4}) {
                h.zoom(zoom,anchor);render();const auto prefix=dimensions+"-"+names[kind]+"-"+std::to_string(zoom)+"x";
                if(kind==3) {
                    for(unsigned section=0;section<4;++section) {
                        const auto tab=static_cast<oe::SandboxView::OriginalHouseSection>(section);
                        h.click(middle(h.view().original_house_section_rect(tab)));render();
                        require(h.view().original_house_section()==tab,"Actual House section click failed");
                        const auto content=h.view().original_house_content_rect();const auto rows=h.view().original_house_section_lines().size();
                        wheel(middle(content),1000000);render();require(h.view().original_house_scroll()==0,"House section cannot return to beginning");
                        std::vector<bool> seen(rows);for(int n=0;;++n) {
                            const int scroll=h.view().original_house_scroll();const int scale=h.view().layout().scale;
                            for(std::size_t i=0;i<rows;++i)if(oe::sandbox_ui::visible_text_row(content.y+int(i)*17*scale-scroll,12*scale,content).h>0)seen[i]=true;
                            require(n<512,"House section failed bounded traversal");wheel(middle(content),-1);render();
                            if(h.view().original_house_scroll()==scroll)break;
                        }
                        require(std::all_of(seen.begin(),seen.end(),[](bool v){return v;}),"House section has unreachable lines");
                        if(zoom==1&&(section==2||width==1280))h.capture(prefix+"-section"+std::to_string(section)+"-end");
                    }
                    checks.push_back({{"size",dimensions},{"owner",static_cast<unsigned>(owners[kind])},{"zoom",zoom},{"house_sections",4},{"status","PASS"}});
                } else {
                    const auto layout=geometry();wheel(middle(layout.content),1000000);if(layout.hints.h>0)wheel(middle(layout.hints),1000000);render();
                    const bool capture=kind==0;
                    if(capture||(zoom==1&&kind>0))h.capture(prefix+"-top");
                    const auto rows=scroll_document(zoom==1 ? prefix:std::string{});inert_edges();
                    if(capture||(zoom==1&&kind>0))h.capture(prefix+"-end");
                    auto click_layout=geometry();wheel(middle(click_layout.content),1000000);render();bool clicked=false;
                    for(int n=0;n<1024&&!clicked;++n) {
                        click_layout=geometry();std::size_t row=0;
                        for(const auto& body:h.view().world().buildings())if(body.placed) {
                            const auto rect=click_layout.list_row_rect(row++);if(rect.h<=0)continue;
                            h.click(middle(rect));require(h.view().selected_building()==body.id,"Visible building row selected a different owner");
                            pure();clicked=true;break;
                        }
                        if(!clicked) {require(click_layout.scroll<click_layout.scroll_limit,"No visible clickable building list row");wheel(middle(click_layout.content),-1);render();}
                    }
                    require(clicked,"Visible list activation exceeded bounded traversal");
                    require(h.select_body(owners[kind]),"Inspector could not return to its previous owner");render();
                    checks.push_back({{"size",dimensions},{"owner",static_cast<unsigned>(owners[kind])},{"zoom",zoom},
                        {"list_rows_reached",rows.first},{"detail_rows_reached",rows.second},{"status","PASS"}});
                }
            }
        }
    }
    // At a small actual resize the toggle may no longer fit below the header,
    // while the independent priority controls still fit and must still draw.
    const auto resize=[&](int width,int height) {
        require(SDL_SetWindowSize(h.window,width,height),SDL_GetError());SDL_Event e{};
        e.type=SDL_EVENT_WINDOW_RESIZED;e.window.data1=width;e.window.data2=height;h.event(e);h.focus();render();
        int actual_width=0,actual_height=0;
        require(SDL_GetRenderOutputSize(h.renderer,&actual_width,&actual_height)&&actual_width==width&&actual_height==height,
            "Small resize output differs from actual requested window");
    };
    resize(1280,800);
    const auto full_layout=h.view().layout();
    const int small_height=full_layout.top.h+full_layout.toolbar.h+full_layout.status.h+180;
    resize(1280,small_height);
    require(h.select_body(owners[0]),"Market not selectable after small actual resize");render();
    const auto small=geometry();require(small.operation_toggle.h==0,"Small resize did not isolate the clipped operation toggle");
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> pixels(SDL_RenderReadPixels(h.renderer,nullptr),SDL_DestroySurface);
    require(bool(pixels),"Cannot inspect actual small-resize priority pixels");
    for(std::size_t index=0;index<small.operation_priorities.size();++index) {
        const auto rect=small.operation_priorities[index];require(rect.w>2&&rect.h>2,"Priority control unexpectedly disappeared at small resize");
        Uint8 red=0,green=0,blue=0,alpha=0;
        require(SDL_ReadSurfacePixel(pixels.get(),rect.x+1,rect.y+1,&red,&green,&blue,&alpha),SDL_GetError());
        const bool active=static_cast<std::size_t>(h.view().world().building(owners[0]).workforce_priority)==index;
        require(red==38&&green==(active?100:58)&&blue==(active?125:77)&&alpha==255,
            "Visible priority hitbox lacks its actual rendered button at small resize");
    }
    h.capture("1280x"+std::to_string(small_height)+"-"+names[0]+"-small-resize");pure();
    checks.push_back({{"size","1280x"+std::to_string(small_height)},{"owner",static_cast<unsigned>(owners[0])},
        {"scope","small resize: clipped toggle, independently rendered priorities, exact pixels and complete World"},{"status","PASS"}});
    resize(1280,800);
    // Commands deliberately alter authority only in this separate phase. The
    // reference executes the same ordinary commands, never a view shortcut.
    Json operations=Json::array();
    for(std::size_t kind=0;kind<3;++kind) {
        if(static_cast<unsigned>(owners[kind])==0)continue;
        require(h.select_body(owners[kind]),"Operation owner no longer selectable");render();
        if(!sim::World::operation_controllable(h.view().world().building(owners[kind]).kind))continue;
        auto expected=h.view().world();const auto layout=h.view().inspector_layout();
        const auto apply=[&](sim::Command command,oe::sandbox_ui::Rect rect) {
            require(rect.w>0&&rect.h>0,"Operation control is not visible");const auto result=expected.execute(command);
            require(result.accepted,"Reference ordinary operation command refused");h.click(middle(rect));
            require(h.view().world().snapshot()==expected.snapshot(),"Inspector operation differs from ordinary World command");
        };
        const bool operating=expected.building(owners[kind]).operating_enabled;
        apply(sim::set_building_operation(owners[kind],!operating),layout.operation_toggle);
        apply(sim::set_building_operation(owners[kind],operating),layout.operation_toggle);
        for(int priority:{0,2,1})apply(sim::set_building_workforce_priority(owners[kind],static_cast<sim::WorkforcePriority>(priority)),layout.operation_priorities[std::size_t(priority)]);
        operations.push_back({{"owner",static_cast<unsigned>(owners[kind])},{"exact_world_commands_equal",true}});
        h.key(SDLK_F9);require(h.view().world().snapshot()==initial,"F9 after operation checks failed complete restoration");
    }
    h.key(SDLK_F5);require(bytes(h.menu->settings().last_save)==original,"Inspector F5 changed canonical checkpoint bytes");
    h.key(SDLK_PERIOD);h.key(SDLK_F9);require(h.view().paused()&&h.view().world().snapshot()==initial,"Inspector F5/Step/F9 lost complete World");
    h.key(SDLK_F5);require(bytes(h.menu->settings().last_save)==original&&bytes(input)==original,"Inspector F5/F9 altered complete document or protected input");
    json_file(h.out/"inspector-layout.json",{{"checkpoint_tick",document.world.ticks},{"checks",checks},{"operations",operations},{"complete_world_unchanged",true},
        {"complete_canonical_f5_f9_equal",true},{"protected_input_bytes_equal",true},{"native_gui","NOT_RUN"},{"human","NOT_RUN"}});
    h.note("OE42.inspector-layout","Actual paid"+std::to_string(document.world.ticks)+" inspector, complete row reachability, clipped-hit refusal, resize/zoom, ordinary operation commands and canonical F5/F9 checked");
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2&&std::string_view(argv[1])=="--help"){std::cout<<oe41::help<<'\n';return 0;}
        require(argc==6||argc==7,oe41::help);const std::string mode=argv[1];
        require(mode=="--check-paths"||mode=="--inventory"||mode=="--create"||mode=="--resume"||mode=="--map"||mode=="--inspect-paid"||mode=="--inspector-layout",oe41::help);
        require((mode=="--resume"||mode=="--map"||mode=="--inspect-paid"||mode=="--inspector-layout")==bool(argc==7),oe41::help);
        const fs::path data=fs::canonical(argv[2]),resources=fs::canonical(argv[3]);
        const auto app=fs::absolute(argv[4]).lexically_normal(),out=fs::absolute(argv[5]).lexically_normal();
        oe41::paths(data,resources,app,out);
        if(mode=="--check-paths") {std::cout<<"System playtest path guards PASS; no output created\n";return 0;}
        oe41::names();oe41::Journey h(data,resources,app,out);
        if(mode=="--inventory")oe41::inventory(h);else if(mode=="--create")oe41::create(h);
        else if(mode=="--resume")oe41::resume(h,fs::absolute(argv[6]).lexically_normal());else if(mode=="--inspect-paid")oe41::inspect_paid(h,fs::absolute(argv[6]).lexically_normal());
        else if(mode=="--inspector-layout")oe41::inspector_layout(h,fs::absolute(argv[6]).lexically_normal());else oe41::map_journey(h,argv[6]);
        json_file(out/"report.json",{{"format","openemperor-system-playtest-v1"},{"mode",mode},{"technical_status","PASS"},{"scenario_results",oe41::scenario_results},
            {"renderer",SDL_GetRendererName(h.renderer)},{"video_driver",SDL_GetCurrentVideoDriver()},{"sdl_runtime",SDL_GetVersion()},
            {"menu_controls",oe41::menu_rows},{"toolbar_controls",oe41::toolbar_rows},{"events",oe41::events},{"captures",h.captures},
            {"final_state",h.state()},{"native_gui","NOT_RUN"},{"human","NOT_RUN"},
            {"scope","Actual scripted MenuSession/SandboxView SDL events; toolbar selector PASS is distinct from purchase/supply; unexecuted menu rows remain NOT_RUN"}});
        std::cout<<"System playtest PASS: "<<out<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<"System playtest FAIL: "<<e.what()<<'\n';return 1;}
}
