#include "EconomyStartViewFixture.h"
#include "OriginalInfluenceFixture.h"
#include "OriginalProductionFixture.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <tuple>

namespace {
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace orig=oe::original;
namespace f=oe_influence_fixture;
namespace fs=std::filesystem;
using oe::testing::economy::check;
struct PaidCheckpoint {
    std::int64_t funds;
    std::uint64_t tax,upkeep,construction;
};
PaidCheckpoint paid_checkpoint(const sim::World& world) {
    return {world.treasury(),world.taxes_collected_total(),world.maintenance_spent_total(),world.construction_spent_total()};
}
bool paid_delta(const sim::World& world,PaidCheckpoint before,std::uint64_t cost) {
    const auto taxes=world.taxes_collected_total()-before.tax;
    const auto upkeep=world.maintenance_spent_total()-before.upkeep;
    check(taxes<1000000 && upkeep<1000000 && cost<1000000,"Paid UI example exceeded its explicit small arithmetic domain");
    return world.construction_spent_total()-before.construction==cost &&
        world.treasury()==before.funds+static_cast<std::int64_t>(taxes)-static_cast<std::int64_t>(upkeep)-static_cast<std::int64_t>(cost);
}
void key(oe::SandboxView& view,SDL_Keycode code) {
    SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=code;
    bool running=true;view.handle_event(event,running);
}
void action_click(oe::SandboxView& view,oe::sandbox_ui::Action action) {
    const auto& buttons=view.layout().buttons;
    const auto button=std::find_if(buttons.begin(),buttons.end(),[&](const auto& value){return value.action==action;});
    check(button!=buttons.end(),"Requested production toolbar action is missing");
    const auto rect=button->rect;bool running=true;SDL_Event event{};
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=static_cast<float>(rect.x+rect.w/2);event.button.y=static_cast<float>(rect.y+rect.h/2);
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(event,running);
}
void click(oe::SandboxView& view,sim::Cell cell) {
    double sx=0,sy=0;unsigned count=0;
    for(int y=80;y<660;y+=4)for(int x=8;x<1090;x+=4)
        if(view.pick({double(x),double(y)})==cell){sx+=x;sy+=y;++count;}
    if (!count) std::cerr<<std::setprecision(17)<<"Production cell not visible: cell=("<<cell.x<<','<<cell.y<<
        ") ticks="<<view.world().ticks()<<" tool="<<view.tool()<<" zoom="<<view.camera().zoom<<
        " offset=("<<view.camera().offset.x<<','<<view.camera().offset.y<<") industry="<<view.industry_page()<<'\n';
    check(count>0,"Production cell not visible");
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=static_cast<float>(sx/count);event.motion.y=static_cast<float>(sy/count);
    bool running=true;view.handle_event(event,running);view.update(0);
    event={};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=static_cast<float>(sx/count);event.button.y=static_cast<float>(sy/count);
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(event,running);
}
void verify_storage_inspector(oe::SandboxView& view,sim::Cell cell) {
    const auto before=view.world().snapshot();
    const auto owner=view.world().building_owner_at(cell);
    check(owner.has_value(),"Storage inspector scenario lost its paid building");
    const auto id=*owner;
    const bool warehouse=view.world().building(id).kind==sim::Object::Warehouse;
    const auto capacity=warehouse ? 32:16;
    view.set_tool(5);click(view,cell);
    check(view.selected_building()==id,"Storage selection lost its actual BuildingId");
    const auto lines=view.inspection_lines();
    const auto layout=view.inspector_layout();
    check(layout.detail_y<layout.list_y && layout.scroll==0,
          "Selected storage inventory remains behind the full building list");
    const auto footprint=std::find_if(lines.begin(),lines.end(),[](const auto& line) {
        return line.starts_with("Footprint ");
    });
    const auto maintenance=std::find_if(lines.begin(),lines.end(),[](const auto& line) {
        return line.starts_with("Maintenance:");
    });
    check(footprint!=lines.end() && maintenance!=lines.end(),"Storage inspector lost existing building details");
    const auto assert_stock=[&](std::string_view name,std::int32_t amount) {
        const auto expected=std::string(name)+" stock "+std::to_string(amount)+"/"+std::to_string(capacity);
        const auto row=std::find(lines.begin(),lines.end(),expected);
        check(row!=lines.end(),"Storage inspector omitted or misreported a local physical stock");
        check(row<footprint && row<maintenance,"Stock is buried after technical building details");
        return static_cast<std::size_t>(std::distance(lines.begin(),row));
    };
    const auto& goods=view.world().original_goods_state()->stocks;
    const auto account=std::find_if(goods.begin(),goods.end(),[&](const auto& row){return row.building_id==id;});
    check(account!=goods.end(),"Paid storage has no authoritative six-Goods account");
    constexpr std::array goods_names{"Hemp","Ceramics","Tea","Silk","Bronzeware","Lacquerware"};
    std::size_t first_stock=lines.size();
    for(std::size_t good=0;good<goods_names.size();++good)
        first_stock=std::min(first_stock,assert_stock(goods_names[good],account->amounts[good]));
    check(layout.detail_row_rect(first_stock).h>0,"First stock row is absent from the initial storage viewport");
    constexpr std::array material_names{"Wood","Bronze","RawSilk","Lacquer","Paper"};
    if(warehouse) {
        const auto& auxiliary=view.world().original_production_state()->industry.auxiliary;
        const auto materials=std::find_if(auxiliary.begin(),auxiliary.end(),[&](const auto& row){return row.building_id==id;});
        check(materials!=auxiliary.end(),"Paid Warehouse has no authoritative material account");
        for(std::size_t material=0;material<material_names.size();++material)
            assert_stock(material_names[material],materials->amounts[material]);
        check(std::none_of(lines.begin(),lines.end(),[](const auto& line){return line.starts_with("Food ");}),
              "Warehouse inspector invented an unsupported Food account");
    } else {
        for(const auto name:material_names)
            check(std::none_of(lines.begin(),lines.end(),[&](const auto& line) {
                return line.starts_with(std::string(name)+" stock ");
            }),"Market inspector invented a material stock account");
        const auto& food=view.world().original_production_state()->food.stocks;
        const auto food_account=std::find_if(food.begin(),food.end(),[&](const auto& row){return row.building_id==id;});
        check(food_account!=food.end(),"Paid Market has no authoritative Food account");
        std::string details;
        for(const auto& line:lines) details+=line+' ';
        const auto expected="Food Wheat "+std::to_string(food_account->amounts[0])+
            " / Millet "+std::to_string(food_account->amounts[1])+" (total cap16)";
        check(details.find(expected)!=std::string::npos,"Market inspector lost its actual typed Food amounts/capacity");
    }
    check(view.world().snapshot()==before,"Storage selection or inventory inspection mutated World authority");
}
void hover(oe::SandboxView& view,sim::Cell cell) {
    double sx=0,sy=0;unsigned count=0;
    for(int y=80;y<660;y+=4)for(int x=8;x<1090;x+=4)
        if(view.pick({double(x),double(y)})==cell){sx+=x;sy+=y;++count;}
    check(count>0,"Production hover cell not visible");
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=static_cast<float>(sx/count);event.motion.y=static_cast<float>(sy/count);
    bool running=true;view.handle_event(event,running);view.update(0);
}
void click_screen(oe::SandboxView& view,oe::scene::Point screen) {
    SDL_Event event{};event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=static_cast<float>(screen.x);event.motion.y=static_cast<float>(screen.y);
    bool running=true;view.handle_event(event,running);view.update(0);
    event={};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=static_cast<float>(screen.x);event.button.y=static_cast<float>(screen.y);
    view.handle_event(event,running);event.type=SDL_EVENT_MOUSE_BUTTON_UP;view.handle_event(event,running);
}
void pan_to_offset(oe::SandboxView& view,oe::scene::Point offset) {
    // Exercise the ordinary held-key input path while the simulation is paused.
    SDL_Event focused{};focused.type=SDL_EVENT_WINDOW_FOCUS_GAINED;bool running=true;
    view.handle_event(focused,running);
    for (unsigned step=0;step<1000;++step) {
        const auto current=view.camera().offset;
        const double dx=offset.x-current.x,dy=offset.y-current.y;
        if (std::abs(dx)<1e-6 && std::abs(dy)<1e-6) return;
        oe::SandboxView::PanKeyState keys{};
        if (std::abs(dx)>1e-6) {
            keys.left=dx>0;keys.right=dx<0;
            view.update(std::min(0.05,std::abs(dx)/400.0),keys);
        } else {
            keys.up=dy>0;keys.down=dy<0;
            view.update(std::min(0.05,std::abs(dy)/400.0),keys);
        }
    }
    check(false,"Could not position actual View camera through ordinary held-key pan");
}
void zoom_at(oe::SandboxView& view,oe::scene::Point screen,double zoom) {
    check(zoom>=0.5 && zoom<=4.0,"Requested camera zoom is outside ordinary wheel limits");
    const double initial_zoom=view.camera().zoom;
    for (unsigned step=0;step<3;++step) {
        if (std::abs(view.camera().zoom-zoom)<1e-9) return;
        SDL_Event wheel{};wheel.type=SDL_EVENT_MOUSE_WHEEL;
        // SDL carries float wheel deltas. A second real residual event removes
        // that rounding without editing Camera2D or relaxing the target zoom.
        wheel.wheel.y=static_cast<float>(std::log(zoom/view.camera().zoom)/std::log(1.15));
        wheel.wheel.mouse_x=static_cast<float>(screen.x);wheel.wheel.mouse_y=static_cast<float>(screen.y);
        bool running=true;view.handle_event(wheel,running);view.update(0);
    }
    if (std::abs(view.camera().zoom-zoom)>=1e-9) {
        const auto input=view.input_diagnostic_state(screen);const auto& map=view.layout().map;
        std::cerr<<std::setprecision(17)<<"Wheel zoom initial="<<initial_zoom<<" target="<<zoom<<
            " observed="<<view.camera().zoom<<" raw=("<<screen.x<<','<<screen.y<<") map=("<<
            map.x<<','<<map.y<<','<<map.w<<','<<map.h<<") help="<<view.help_open()<<
            " budget="<<view.budget_warning_pending()<<" demolition="<<view.demolition_pending()<<
            " road_drag="<<input.road_drag<<" ui="<<input.ui<<'\n';
        if (input.render_position) std::cerr<<"Transformed wheel point=("<<
            input.render_position->x<<','<<input.render_position->y<<")\n";
    }
    check(std::abs(view.camera().zoom-zoom)<1e-9,"Actual View wheel zoom disagrees with requested test zoom");
}
fs::path authored_walker_profile(const fs::path& data) {
    using namespace oe::testing::economy;
    Bytes archive(40680U+2U*72U,0);
    u32(archive,0,static_cast<std::uint32_t>(archive.size()));u32(archive,4,214);
    u32(archive,12,2);u32(archive,16,2);u32(archive,20,1);
    const std::string name="authored-clip.bmp";
    std::copy(name.begin(),name.end(),archive.begin()+680);u32(archive,680+124,2);
    const auto at=40680U+72U;u32(archive,at,4);u32(archive,at+4,10);
    u16(archive,at+20,2);u16(archive,at+22,2);u16(archive,at+50,256);
    write(data/"DATA/culture-input.sg3",archive);
    write(data/"DATA/culture-input.555",{0,0,0,0,2,0,0x7c,0,0x7c,2,0,0x7c,0,0x7c});
    const auto path=data/"culture-input.json";
    f::write_text(path,R"({"schema_version":1,"mode":"curated_walker_preview","role":"clay",
"ticks_per_frame":1,"evidence":"Authored2x2clip solely to exercise real F3 input gating",
"frames":[{"alias":"dot","archive":"DATA/culture-input.sg3","image_index":1,"foot_anchor":[1,1]}],
"clips":{"pos_x":["dot"],"neg_x":["dot"],"pos_y":["dot"],"neg_y":["dot"]},"idle":"dot"})");
    return path;
}
void verify_materialized_elite_fire(SDL_Window* window,SDL_Renderer* renderer,
    const fs::path& data,const fs::path& save) {
    auto view=oe::SandboxView::create_original_production_save_fixture(
        oe::maps::load_stored_map_session(data,"Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8));
    view->configure_save(data,"Cities/A.map",save);view->initialize(window,renderer);
    // Select the existing authored flame fallback, independent of optional art.
    view->set_fire_visuals({});
    const auto funds=view->world().treasury();
    const auto road=view->execute({sim::CommandType::PlaceRoad,{118,91}});
    const auto plot=view->execute({sim::CommandType::PlaceOriginalElitePlot,{118,92}});
    check(road.accepted && road.changed && plot.accepted && plot.changed &&
          view->world().treasury()==funds-82,"Small fire regression did not pay for its Plot and ordinary road");
    const auto id=*view->world().building_owner_at({118,92});
    for (int tick=0;tick<119;++tick) view->tick_once();
    check(view->world().building(id).kind==sim::Object::OriginalElitePlot &&
          view->world().household_population(id)==0 && !view->world().building_owner_at({121,95}),
          "Small fire regression granted geometry or residents before actual immigration");
    view->tick_once();
    check(view->world().ticks()==120 && view->world().building(id).kind==sim::Object::OriginalEliteHouse &&
          view->world().household_population(id)==2 && view->world().building_owner_at({121,95})==id,
          "Small fire regression did not materialize the paid4x4 House at the actual first arrival");
    constexpr auto ignition_tick=sim::Rules::fire_risk_step_ticks*sim::Rules::fire_risk_threshold;
    static_assert(ignition_tick==2000);
    for (std::uint64_t tick=view->world().ticks();tick+1<ignition_tick;++tick) view->tick_once();
    check(view->world().ticks()==ignition_tick-1 && !view->world().building_on_fire(id) &&
          view->world().building(id).fire_risk==sim::Rules::fire_risk_threshold-1,
          "Natural House fire risk did not reach its real placement-relative threshold");
    key(*view,SDLK_R);view->set_tool(5);
    const auto& map=view->layout().map;
    const oe::scene::Point map_center{map.x+map.w*.5,map.y+map.h*.5};
    zoom_at(*view,map_center,1.0);
    const auto geometry_front=sim::building_front_cell(view->world().profile(),view->world().rule_version(),
        view->world().building(id).kind,view->world().building(id).cell);
    check(geometry_front==sim::Cell{121,95},"Natural-fire fixture did not retain the actual4x4 front cell");
    const oe::scene::Point world_ground{double(geometry_front.x-geometry_front.y)*40.0,
        double(geometry_front.x+geometry_front.y-144)*20.0+20.0};
    pan_to_offset(*view,{map_center.x-world_ground.x*view->camera().zoom,
        map_center.y-(world_ground.y-60)*view->camera().zoom});
    view->update(0);check(view->render(),"Unlit4x4 House regression frame");
    check(view->fire_display_stats().fallback_draws==0,"Unlit materialized House acquired flame pixels");
    view->tick_once();
    check(view->world().ticks()==ignition_tick && view->world().building(id).kind==sim::Object::OriginalEliteHouse &&
          view->world().building_on_fire(id) && view->world().fire_remaining(id)==sim::Rules::fire_incident_ticks,
          "Materialized House did not ignite through ordinary unprotected risk ticks");
    const auto authority=view->world().snapshot();key(*view,SDLK_F5);
    const auto house_tax=view->world().original_house_tax_status(id);
    check(house_tax.burning && !house_tax.eligible && house_tax.amount_if_assessed_now==0 &&
          house_tax.paid_total>0 && house_tax.last_assessment &&
          house_tax.last_assessment->tick==2*sim::original_tax_interval_ticks &&
          house_tax.paid_total==view->world().taxes_collected_total(),
          "Burning Elite tax diagnosis lost actual prior payments or incorrectly taxes the current Fire state");
    check(view->world().water_covered_households()==0 &&
          std::none_of(view->world().buildings().begin(),view->world().buildings().end(),
              [](const auto& building){return building.kind==sim::Object::Household;}) &&
          view->hud_status_line().find("Your city is supplied. Add water")==std::string::npos,
          "Actual taxed Elite-only Original city displayed the vacuous legacy supplied-city finance hint");
    view->tick_once();key(*view,SDLK_F9);
    check(view->world().snapshot()==authority && view->world().building_on_fire(id),
          "Actual burning4x4 House F5/F9 changed sole identity, geometry or fire authority");
    view->update(0);check(view->render(),"Burning materialized4x4 House regression frame");
    check(!view->fire_display_stats().animated && view->fire_display_stats().fallback_draws==1 &&
          view->fire_display_stats().draws==0 && view->world().burning_buildings()==1,
          "Materialized4x4 fallback bypassed its actual single-House flame overlay");
    // Independent flat authored-map projection, not renderer-generated bounds.
    const auto front=sim::building_front_cell(view->world().profile(),view->world().rule_version(),
        view->world().building(id).kind,view->world().building(id).cell);
    if (front!=sim::Cell{121,95} || std::abs(view->camera().zoom-1.0)>=1e-9)
        std::cerr<<std::setprecision(17)<<"Burning House observed front=("<<front.x<<','<<front.y<<
            ") zoom="<<view->camera().zoom<<" offset=("<<view->camera().offset.x<<','<<view->camera().offset.y<<")\n";
    check(front==sim::Cell{121,95} && std::abs(view->camera().zoom-1.0)<1e-9,
          "Burning House regression lost actual4x4 front geometry");
    const auto ground=view->camera().world_to_screen({double(front.x-front.y)*40.0,
        double(front.x+front.y-144)*20.0+20.0});
    std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(
        SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
    check(frame!=nullptr,"Burning House pixel readback");
    unsigned orange=0;
    for (int y=static_cast<int>(ground.y)-54;y<=static_cast<int>(ground.y)-15;++y)
        for (int x=static_cast<int>(ground.x)-16;x<=static_cast<int>(ground.x)+16;++x) {
            check(view->layout().map.contains(x,y),"Burning House witness ROI escaped the actual map aperture");
            std::uint8_t r=0,g=0,b=0,a=0;
            check(SDL_ReadSurfacePixel(frame.get(),x,y,&r,&g,&b,&a),"Burning House pixel read");
            if (r>180 && g>60 && g<210 && b<100 && r>g+20) ++orange;
        }
    check(orange>40,"Actual materialized House flame had no substantial orange pixels at its authoritative front foot");
    check(view->world().snapshot()==authority,"Burning House drawing/readback mutated World authority");
    view->shutdown();
    std::cout<<"Paid Elite4x4 natural fire at2000, actual flame pixels and F5/F9 PASS\n";
}
void verify_difficulty_views(SDL_Window* window,SDL_Renderer* renderer,const fs::path& data,const fs::path& saves) {
    constexpr std::array names{"Very easy","Easy","Normal","Hard","Very hard"};
    for (std::size_t n=0;n<orig::difficulty_count;++n) {
        const auto difficulty=static_cast<orig::Difficulty>(n);
        const auto make_session=[&]{return oe::maps::load_stored_map_session(data,"Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);};
        const auto target=saves/("difficulty-"+std::to_string(n)+".json");
        auto view=oe::SandboxView::create_original_production_save_fixture(make_session(),difficulty);
        view->configure_save(data,"Cities/A.map",target);view->initialize(window,renderer);
        const auto road=view->execute({sim::CommandType::PlaceRoad,{104,103}});
        const auto home=view->execute({sim::CommandType::PlaceHousehold,{104,101}});
        check(road.accepted && home.accepted && view->world().treasury()==11918,
              "Typed-difficulty UI start did not pay its actual House and road");
        for (int tick=0;tick<120;++tick) view->tick_once();
        check(view->world().original_rules_model()->difficulty()==difficulty &&
              view->world().original_economy_state()->model_source->difficulty==difficulty &&
              view->world().total_population()==2,"Typed difficulty was ignored by actual model/immigration authority");
        const auto summary=view->income_summary_lines();
        check(summary.size()==3 && std::any_of(summary.begin(),summary.end(),[&](const auto& line){
            return line.find(std::string("Difficulty ")+names[n])!=std::string::npos;}),
            "Typed difficulty was missing from the compact actual3-row HUD");
        key(*view,SDLK_F5);const auto saved=view->world().snapshot();
        const auto document=oe::persistence::read_save(target);
        check(document.world.original_economy->model_source->difficulty==difficulty,
              "Paid difficulty start saved a different selected model");
        view->shutdown();view.reset();
        auto loaded=oe::SandboxView::create_original_production_save_fixture(make_session());
        loaded->configure_save(data,"Cities/A.map",target,document);loaded->initialize(window,renderer);
        check(loaded->world().snapshot()==saved && loaded->world().original_rules_model()->difficulty()==difficulty,
              "Unspecified initial load silently replaced the saved difficulty withNormal");
        loaded->shutdown();loaded.reset();
        auto wrong=oe::SandboxView::create_original_production_save_fixture(make_session(),
            static_cast<orig::Difficulty>((n+1)%orig::difficulty_count));
        bool refused=false;
        try { wrong->configure_save(data,"Cities/A.map",target,document); }
        catch (const std::invalid_argument&) { refused=true; }
        check(refused,"Explicit mismatched difficulty was accepted before initial load");
    }
    std::cout<<"Five paid typed-difficulty View starts, HUD/source/save and strict initial-load selection PASS\n";
}
void verify_live_finance_goal(SDL_Window* window,SDL_Renderer* renderer,const std::string& source,
    const fs::path& capture) {
    oe::testing::economy::Temp temp;const auto data=fs::canonical(temp.root)/"data";
    auto model=source;
    // This isolated goal-predicate scene uses explicitly authored zero needs.
    // The separate main View still proves the full paid goods/service chain.
    for (std::size_t row=0;row<3;++row) {
        for (unsigned field=2;field<=16;++field) oe_production_fixture::set_house_field(model,row,field,0);
        for (unsigned field=21;field<=23;++field) oe_production_fixture::set_house_field(model,row,field,0);
        oe_production_fixture::set_house_field(model,row,1,row<2 ? 0:100);
    }
    fs::create_directories(data/"Model");f::write_text(data/"Model/EmperorBuildingModels.txt",model);
    auto view=oe::SandboxView::create_original_production_save_fixture(
        oe::maps::load_stored_map_session(data,"Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8),orig::Difficulty::Normal);
    const auto save=fs::canonical(temp.root)/"saves/goal.json";
    view->configure_save(data,"Cities/A.map",save);view->initialize(window,renderer);
    const auto buy=[&](sim::CommandType type,sim::Cell cell) {
        const auto result=view->execute({type,cell});check(result.accepted && result.changed,result.reason.c_str());
    };
    for (int x=104;x<=120;++x) buy(sim::CommandType::PlaceRoad,{x,103});
    for (int x=104;x<=119;x+=3) buy(sim::CommandType::PlaceHousehold,{x,101});
    check(view->world().construction_spent_total()==514 && view->world().treasury()==11486,
          "Live goal scene did not independently pay six80-Funds Houses and seventeen2-Funds roads");
    for (std::uint64_t tick=0;tick+1<sim::original_tax_interval_ticks;++tick) view->tick_once();
    const auto pending=view->world().original_goal_status();
    check(!pending.reached && pending.inhabited_houses==6 && pending.common_houses==6 &&
          pending.population>=50 && !pending.positive_tax && pending.nonnegative_funds,
          "Authored live goal ignored its still-unpaid positive-tax condition");
    const auto before_tax=view->world().snapshot();key(*view,SDLK_F5);view->tick_once();
    const auto reached=view->world().snapshot();
    check(view->world().original_goal_status().reached && view->world().taxes_collected_total()>0,
          "First actual monthly household tax did not unlock the simultaneously satisfied live mission");
    key(*view,SDLK_F9);check(view->world().snapshot()==before_tax,"F9 before monthly tax changed a cycle or cumulative authority");
    view->tick_once();check(view->world().snapshot()==reached,"Actual816 boundary afterF9 did not replay identical finance and live goal");
    key(*view,SDLK_F5);const auto bytes=f::read_text(save);
    key(*view,SDLK_T);check(view->income_open(),"Actual T key did not open Original finance/mission window");
    const auto lines=view->income_lines();std::string joined;
    for (const auto& line:lines) { if (!joined.empty()) joined+=' ';joined+=line; }
    for (const std::string_view expected:{"Difficulty: Normal","Debt: 0","Taxes actually received:","Upkeep actually paid:",
            "Construction spent: 514","Next tax tick: 1632","Authored housing mission: MET",
            "Nonburning occupied homes: 6 /6","Nonburning Common Plain+ homes: 6 /1",
            "Positive tax received: yes","Funds >=0: yes","No bonus or win latch"})
        check(joined.find(expected)!=std::string::npos,"Actual T panel hid a signed finance fact or current authored goal condition");
    const auto authority=view->world().snapshot();
    if (!capture.empty()) {
        const auto write_capture=[&](std::string_view name,int notches) {
            view->update(0);check(view->render(),"Actual Original finance/goal capture frame");
            const auto output=capture.parent_path()/std::string(name);
            std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> frame(SDL_RenderReadPixels(renderer,nullptr),SDL_DestroySurface);
            check(frame!=nullptr && SDL_SavePNG(frame.get(),output.string().c_str()),"Actual finance/goal PNG output");
            const auto goal=view->world().original_goal_status();
            nlohmann::json observed{{"capture",name},{"development_revision",view->world().original_production_state()->development_revision},
                {"ticks",view->world().ticks()},{"funds",view->world().treasury()},
                {"model_sha256",view->world().original_rules_model()->provenance().sha256},
                {"difficulty",static_cast<unsigned>(view->world().original_rules_model()->difficulty())},
                {"taxes",view->world().taxes_collected_total()},{"upkeep",view->world().maintenance_spent_total()},
                {"construction",view->world().construction_spent_total()},{"goal_reached",goal.reached},
                {"inhabited_houses",goal.inhabited_houses},{"common_plain_or_higher",goal.common_houses},
                {"population",goal.population},{"income_lines",lines},{"actual_panel_wheel_notches",notches},
                {"authored_zero_need_goal_fixture",true},{"original_map",false},{"original_art_restored",false},
                {"world_unchanged",view->world().snapshot()==authority},{"human_acceptance_claimed",false}};
            auto report=output;report.replace_extension(".observed.json");f::write_text(report,observed.dump(2)+"\n");
            auto saved=output;saved.replace_extension(".save.json");f::write_text(saved,bytes);
        };
        write_capture("production.finance.png",0);
        const auto mission=std::find_if(lines.begin(),lines.end(),[](const auto& line){return line.find("Authored housing mission:")!=std::string::npos;});
        check(mission!=lines.end(),"Mission header absent from actual wrapped finance rows");
        const auto row=static_cast<int>(std::distance(lines.begin(),mission));
        const int notches=std::max(0,(row*17)/24);
        SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;scroll.wheel.y=static_cast<float>(-notches);
        scroll.wheel.mouse_x=static_cast<float>(view->layout().panel.x+12);
        scroll.wheel.mouse_y=static_cast<float>(view->layout().panel.y+50);bool running=true;view->handle_event(scroll,running);
        write_capture("production.mission.png",-notches);
    }
    check(view->world().snapshot()==authority,"Finance and goal inspection mutated authoritative simulation state");
    key(*view,SDLK_T);check(!view->income_open(),"T did not restore ordinary House inspection");
    while (view->world().ticks()<2000) view->tick_once();
    const auto lost=view->world().original_goal_status();
    check(!lost.reached && lost.inhabited_houses==0 && lost.common_houses==0 &&
          lost.positive_tax && lost.nonnegative_funds && view->world().taxes_collected_total()>0,
          "Actual natural fires did not reverse the current mission or erased its true historical payments");
    check(view->world().treasury()==sim::original_production_starting_treasury-514+
        static_cast<std::int64_t>(view->world().taxes_collected_total()),"Mission toggling added a hidden bonus or lost signed finance accounting");
    view->shutdown();
    std::cout<<"Actual paid six-House authored mission,816tax/F5F9/T and natural-fire loss without bonus PASS\n";
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc<=2,"Expected at most capture output path");
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") &&
            SDL_Init(SDL_INIT_VIDEO),"Production SDL init");
        check(SDL_CreateWindowAndRenderer("Original production",1100,700,SDL_WINDOW_HIDDEN,
            &window,&renderer),"Production SDL window");
        oe::testing::economy::Temp temp;const auto root=fs::canonical(temp.root),data=root/"data";
        auto model=oe_production_fixture::authored_model_text(37);
        for (std::size_t row=0;row<orig::house_row_count;++row)
            oe_production_fixture::set_house_field(model,row,static_cast<unsigned>(orig::HouseField::TaxRate),3);
        fs::create_directories(data/"Model");f::write_text(data/"Model/EmperorBuildingModels.txt",model);
        const auto session=[&]{return oe::maps::load_stored_map_session(data,"Cities/A.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);};
        const auto target=root/"saves/production.json";
        verify_difficulty_views(window,renderer,data,root/"saves");
        verify_materialized_elite_fire(window,renderer,data,root/"saves/elite-fire.json");
        verify_live_finance_goal(window,renderer,model,argc==2 ? fs::path(argv[1]):fs::path{});
        auto view=oe::SandboxView::create_original_production_save_fixture(session());
        view->configure_save(data,"Cities/A.map",target);view->initialize(window,renderer);
        check(view->world().rule_version()==10 && view->world().treasury()==12000 && view->paused(),"Production identity");
        check(!view->industry_page(),"Industry page was not view-local defaultOFF");
        const auto& buttons=view->layout().buttons;
        check(view->world().bridges_supported() && view->world().ramps_supported() && buttons.size()==39 &&
              std::count_if(buttons.begin(),buttons.end(),[](const auto& b){return b.action==oe::sandbox_ui::Action::Bridge;})==1 &&
              std::count_if(buttons.begin(),buttons.end(),[](const auto& b){return b.action==oe::sandbox_ui::Action::Ramp;})==1,
              "Initial production toolbar disagrees with the actual inherited Bridge/Ramp capabilities");
        for(const auto action:{oe::sandbox_ui::Action::OriginalHempFarm,oe::sandbox_ui::Action::OriginalHempField,
                oe::sandbox_ui::Action::OriginalFoodFarm,oe::sandbox_ui::Action::OriginalWheatField,oe::sandbox_ui::Action::OriginalMilletField,
                oe::sandbox_ui::Action::HealthPost,oe::sandbox_ui::Action::ServicePost,
                oe::sandbox_ui::Action::OriginalAncestorShrine,oe::sandbox_ui::Action::OriginalWaterWell,
                oe::sandbox_ui::Action::OriginalDaoistShrine,oe::sandbox_ui::Action::OriginalBuddhistShrine,
                oe::sandbox_ui::Action::OriginalConfucianAcademy,oe::sandbox_ui::Action::OriginalMusicSchool,
                oe::sandbox_ui::Action::OriginalAcrobatSchool,oe::sandbox_ui::Action::OriginalDramaSchool}) {
            const auto button=std::find_if(buttons.begin(),buttons.end(),[&](const auto& value){return value.action==action;});
            check(button!=buttons.end() && view->layout().button_at(button->rect.x+2,button->rect.y+2)==action &&
                !view->layout().map.contains(button->rect.x+2,button->rect.y+2),"Hemp action was hidden or overlapped playable map input");
        }
        check(std::string(SDL_GetWindowTitle(window)).find("save schema 26")!=std::string::npos,"Production title");
        check(view->recovery_safe_point(),"Active original production did not expose its safe recovery point");
        const auto buy=[&](sim::CommandType type,sim::Cell cell) {
            const auto result=view->execute({type,cell});check(result.accepted && result.changed,result.reason.c_str());
        };
        for(int x=104;x<=120;++x)buy(sim::CommandType::PlaceRoad,{x,103});
        for(int x:{106,109,112,115})buy(sim::CommandType::PlaceHousehold,{x,101});
        buy(sim::CommandType::PlaceClaySource,{106,104});
        buy(sim::CommandType::PlacePottery,{110,104});
        buy(sim::CommandType::PlaceWarehouse,{114,104});
        check(view->world().total_population()==0 && view->world().original_goods_balance(orig::Goods::Ceramics).stock==0,
            "Construction injected population or goods");
        bool travelling=false;
        for(int tick=0;tick<2000 && !travelling;++tick) {
            view->tick_once();
            travelling=std::any_of(view->world().couriers().begin(),view->world().couriers().end(),[](const auto& c){
                return c.role==sim::CourierRole::Pottery && c.phase==sim::CourierPhase::ToWarehouse && c.cargo>0;});
        }
        check(travelling && view->world().total_population()>0,"Real staffed Pottery courier never departed");
        const auto saved=view->world().snapshot();key(*view,SDLK_F5);
        check(oe::persistence::read_save(target).source_schema_version==26,"Actual production F5 schema");
        for(int i=0;i<100;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==saved && view->paused(),"Actual production F9 midcourier replay");
        for(int i=0;i<800;++i)view->tick_once();
        const auto balance=view->world().original_goods_balance(orig::Goods::Ceramics);
        check(balance.supported && balance.conserved && balance.totals.produced>0 && balance.stock>0,
            "Actual warehouse arrival did not produce typed Ceramics");
        verify_storage_inspector(*view,{114,104});
        key(*view,SDLK_0);check(view->tool()==0,"Market keyboard selection unavailable");
        click(*view,{118,104});
        check(view->world().object_at({118,104})==sim::Object::Market,"Paid Market mouse placement failed");
        bool typed_trip=false;
        for(int i=0;i<5000 && !typed_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) if(c.role==sim::CourierRole::MarketPotteryDistribution &&
                view->world().original_courier_cargo(c.id)>0 && c.edge_progress>0) {
                check(c.cargo==0 && c.reserved==0,"Typed delivery mirrored cargo in legacy movement fields");
                const auto rows=view->walker_diagnostics();
                const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==c.id;});
                check(row!=rows.end() && row->cargo==view->world().original_courier_cargo(c.id),
                    "Rendered walker diagnostics hid actual typed cargo");
                typed_trip=true;break;
            }
        }
        check(typed_trip,"No actual typed Market-to-House courier departed");
        const auto typed_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==typed_saved,"Actual typed midcourier F5/F9 diverged");
        bool delivered=false;
        for(int i=0;i<2500 && !delivered;++i) {
            view->tick_once();
            for(const auto& house:view->world().original_economy_state()->houses) delivered|=house.house.ceramics>0;
        }
        check(delivered,"Actual Market arrival never updated the authoritative House stock");
        verify_storage_inspector(*view,{118,104});
        verify_storage_inspector(*view,{114,104});
        const auto funds_before=paid_checkpoint(view->world());
        for(const sim::Cell road:std::array{sim::Cell{120,104},sim::Cell{120,105},sim::Cell{120,106},
                sim::Cell{119,106},sim::Cell{118,106}}) buy(sim::CommandType::PlaceRoad,road);
        key(*view,SDLK_N);check(view->tool()==16,"Hemp Farm keyboard tool unavailable");
        click(*view,{118,107});
        check(view->world().object_at({118,107})==sim::Object::OriginalHempFarm,"Paid Hemp Farm mouse placement failed");
        const auto farm=view->world().building_owner_at({118,107});
        check(farm && view->selected_building()==farm,"Farm purchase did not retain explicit selection for paid Field");
        for(int i=0;i<100;++i)view->tick_once();
        check(view->world().original_goods_balance(orig::Goods::Hemp).totals.produced==0,
            "Farm without a separately paid Field produced free Hemp");
        key(*view,SDLK_M);check(view->tool()==17,"Hemp Field keyboard tool unavailable");
        click(*view,{120,107});
        check(view->world().object_at({120,107})==sim::Object::OriginalHempField &&
            paid_delta(view->world(),funds_before,54),"Field/Farm cost or actual tax/upkeep reconciliation changed through normal UI commands");
        view->set_tool(5);click(*view,{118,107});
        const auto farm_authority=view->world().snapshot();
        const auto farm_lines=view->inspection_lines();std::string farm_details;
        for (const auto& line:farm_lines) { if (!farm_details.empty()) farm_details+=' ';farm_details+=line; }
        check(view->selected_building()==farm && view->world().workforce_required(*farm)==18 &&
              farm_details.find("Maintenance: 36 every 400 ticks")!=std::string::npos,
              "Actual Hemp Farm inspector did not quote its18 required workers at2 Funds each per400t");
        key(*view,SDLK_9);check(view->tool()==9,"Acupuncture hover tool unavailable");
        hover(*view,{120,107});
        const auto prospective=view->hud_status_line();
        check(prospective.find("upkeep 2/required worker/400t; T: installed")!=std::string::npos &&
              prospective.find("upkeep $4/400t")==std::string::npos,
              "Original provider hover showed a legacy fixed upkeep quote instead of the actual installed-worker rule");
        check(view->world().snapshot()==farm_authority,"Actual inspector/hover finance queries changed simulation authority");
        view->set_tool(5);
        bool hemp_trip=false;
        for(int i=0;i<5000 && !hemp_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) if(c.role==sim::CourierRole::OriginalHempToWarehouse &&
                view->world().original_courier_cargo(c.id)>0 && c.edge_progress>0) {
                check(view->world().original_courier_good(c.id)==orig::Goods::Hemp,"Actual Farm cargo lost its Hemp identity");
                hemp_trip=true;break;
            }
        }
        check(hemp_trip,"Paid staffed Farm never dispatched physical Hemp");
        const auto hemp_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<100;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==hemp_saved,"Typed Hemp midcourier F5/F9 diverged");
        bool both_goods=false;
        for(int i=0;i<8000 && !both_goods;++i) {
            view->tick_once();
            for(const auto& house:view->world().original_economy_state()->houses)
                both_goods|=house.house.hemp>0 && house.house.ceramics>0;
        }
        check(both_goods,"No authoritative House received two independently transported original goods");
        const auto food_funds=view->world().treasury();
        for(int x=104;x<=117;++x) buy(sim::CommandType::PlaceRoad,{x,106});
        for(int y:{104,105})buy(sim::CommandType::PlaceRoad,{104,y});
        key(*view,SDLK_O);check(view->tool()==18,"Food Farm keyboard tool unavailable");
        click(*view,{104,107});
        check(view->world().object_at({106,109})==sim::Object::OriginalFoodFarm &&
            view->selected_building()==view->world().building_owner_at({104,107}),"Paid Food Farm lost3x3 occupancy or explicit selection");
        key(*view,SDLK_W);check(view->tool()==19,"Wheat Field keyboard tool unavailable");
        click(*view,{107,107});check(view->world().object_at({107,107})==sim::Object::OriginalWheatField,"Paid Wheat Field mouse command failed");
        key(*view,SDLK_O);click(*view,{110,107});
        check(view->world().object_at({112,109})==sim::Object::OriginalFoodFarm,"Second paid Food Farm footprint failed");
        key(*view,SDLK_L);check(view->tool()==20,"Millet Field keyboard tool unavailable");
        click(*view,{113,107});check(view->world().object_at({113,107})==sim::Object::OriginalMilletField,"Paid Millet Field mouse command failed");
        check(food_funds-view->world().treasury()==200,"Food Farms, Fields and access roads were not separately paid");
        bool food_trip=false;
        for(int i=0;i<10000 && !food_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) if(c.role==sim::CourierRole::OriginalFoodToHouse &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0 && view->world().original_food_cargo(c.id)>0) {
                const auto rows=view->walker_diagnostics();
                const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==c.id;});
                check(c.cargo==0 && c.reserved==0 && row!=rows.end() && row->cargo==view->world().original_food_cargo(c.id),
                    "Actual Food cargo hidden by movement or visual diagnostics");
                food_trip=true;break;
            }
        }
        check(food_trip,"Paid Food chain never dispatched an actual House courier");
        const auto food_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==food_saved,"Food midcourier F5/F9 changed sole crop or quality authority");
        bool real_quality=false;
        for(int i=0;i<10000 && !real_quality;++i) {
            view->tick_once();
            for(const auto& house:view->world().original_economy_state()->houses)
                real_quality|=house.house.food_quality>0 && house.house.hemp>0 && house.house.ceramics>0;
        }
        check(real_quality,"Actual Food House arrival never applied quality");
        const auto medical_funds=view->world().treasury();
        key(*view,SDLK_J);check(view->tool()==13,"Herbalist keyboard tool unavailable");
        click(*view,{108,104});
        key(*view,SDLK_9);check(view->tool()==9,"Acupuncture keyboard tool unavailable");
        click(*view,{112,104});
        check(view->world().object_at({109,105})==sim::Object::HealthPost &&
            view->world().object_at({113,105})==sim::Object::ServicePost &&
            view->world().treasury()==medical_funds-80,"Medical UI purchases lost2x2 ownership or prepared30/50 prices");
        check(view->world().original_production_state()->services.witnesses.empty(),"Provider purchase granted visits");
        bool medical_trip=false;
        for(int i=0;i<1000 && !medical_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers())
                medical_trip|=(c.role==sim::CourierRole::HealthWorker || c.role==sim::CourierRole::Service) &&
                    c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0;
        }
        check(medical_trip,"Paid medical sources never dispatched through ordinary movement");
        const auto medical_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==medical_saved,"Medical midtrip F5/F9 changed sole visit authority");
        bool medical_visits=false;
        for(int i=0;i<4000 && !medical_visits;++i) {
            view->tick_once();
            for(const auto& house:view->world().original_economy_state()->houses)
                medical_visits|=house.house.herbalist>0 && house.house.acupuncture>0 &&
                    house.house.hemp>0 && house.house.ceramics>0 && house.house.food_quality>0;
        }
        check(medical_visits,"Actual same House never received both paid medical visits and physical goods/Food");
        const auto ancestor_funds=view->world().treasury();
        const auto previous_tool=view->tool();key(*view,SDLK_H);key(*view,SDLK_Y);
        check(view->tool()==previous_tool,"Ancestor keyboard tool escaped the help modal");
        key(*view,SDLK_H);
        key(*view,SDLK_Y);check(view->tool()==21,"Ancestor keyboard tool unavailable");
        click(*view,{116,104});
        check(view->world().object_at({117,105})==sim::Object::OriginalAncestorShrine214 &&
            view->world().treasury()==ancestor_funds-40,"Ancestor UI purchase lost2x2 ownership or prepared price");
        const auto shrine=*view->world().building_owner_at({116,104});
        check(view->world().workforce_required(shrine)==4,"Ancestor UI source lost its prepared worker requirement");
        for(const auto& h:view->world().original_economy_state()->houses)
            check(h.house.ancestor==0,"Ancestor purchase granted free access");
        bool ancestor_trip=false;
        for(int i=0;i<1000 && !ancestor_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) if(c.role==sim::CourierRole::OriginalAncestor &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0) ancestor_trip=true;
        }
        check(ancestor_trip,"Paid Ancestor source never dispatched an actual visitor");
        const auto ancestor_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();
        key(*view,SDLK_F9);check(view->world().snapshot()==ancestor_saved,"Ancestor midtrip F5/F9 changed its sole visit authority");
        bool ancestor_visit=false;
        for(int i=0;i<4000 && !ancestor_visit;++i) {
            view->tick_once();
            for(const auto& h:view->world().original_economy_state()->houses)
                ancestor_visit|=h.house.ancestor>0 && h.house.herbalist>0 && h.house.acupuncture>0 &&
                    h.house.hemp>0 && h.house.ceramics>0 && h.house.food_quality>0;
        }
        check(ancestor_visit,"No actual House received Ancestor plus medical visits and physical goods/Food");
        const auto water_funds=view->world().treasury();
        const auto old_tool=view->tool();key(*view,SDLK_H);key(*view,SDLK_Q);
        check(view->tool()==old_tool,"Water keyboard escaped Help modal");key(*view,SDLK_H);
        key(*view,SDLK_Q);check(view->tool()==22,"Water keyboard tool unavailable");
        click(*view,{116,107});
        check(view->world().object_at({117,108})==sim::Object::OriginalWaterWell72 &&
            view->world().treasury()==water_funds-25,"Water Well UI did not pay its2x2model source");
        bool water_trip=false;
        for(int i=0;i<1000 && !water_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) water_trip|=c.role==sim::CourierRole::OriginalWater &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0;
        }
        check(water_trip,"Paid Water Well did not start real movement");
        const auto water_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==water_saved,"Water midtrip F5/F9 changed authority");
        bool plain=false;
        for(int i=0;i<4000 && !plain;++i) {
            view->tick_once();
            for(const auto& h:view->world().original_economy_state()->houses)
                plain|=h.house.type==orig::HouseType::PlainCottage && h.house.water && h.house.ancestor>0 &&
                    h.house.food_quality>=20 && h.house.hemp>0 && h.house.ceramics>0 && h.house.herbalist>0 && h.house.acupuncture>0;
        }
        check(plain,"Actual UI city never reached paid Water/Food/Ancestor-backed Plain Cottage");
        const auto religion_funds=view->world().treasury();
        for(int x=104;x<=120;++x)buy(sim::CommandType::PlaceRoad,{x,100});
        for(int y:{101,102})buy(sim::CommandType::PlaceRoad,{104,y});
        for(const auto [code,tool,origin,kind,price]:std::array{
                std::tuple{SDLK_U,23,sim::Cell{106,98},sim::Object::OriginalDaoistShrine215,45},
                std::tuple{SDLK_K,24,sim::Cell{110,98},sim::Object::OriginalBuddhistShrine217,45},
                std::tuple{SDLK_V,25,sim::Cell{114,97},sim::Object::OriginalConfucianAcademy219,80}}) {
            const auto old=view->tool();key(*view,SDLK_H);key(*view,code);
            check(view->tool()==old,"Religion key escaped Help modal");key(*view,SDLK_H);
            key(*view,code);check(view->tool()==tool,"Paid religion keyboard tool unavailable");
            const auto funds=view->world().treasury();click(*view,origin);
            const auto side=kind==sim::Object::OriginalConfucianAcademy219 ? 3:2;
            check(view->world().object_at({origin.x+side-1,origin.y+side-1})==kind &&
                view->world().treasury()==funds-price,"Religion UI purchase lost actual footprint or model price");
        }
        check(view->world().treasury()==religion_funds-208,"Religion sources and access roads were not separately paid");
        using A=oe::sandbox_ui::Action;
        const auto page_before=view->world().snapshot();
        const auto main_layout_before=view->layout();
        key(*view,SDLK_H);action_click(*view,A::ToggleIndustryPage);
        check(!view->industry_page() && view->world().snapshot()==page_before,"Industry page escaped Help modal or changed World");key(*view,SDLK_H);
        action_click(*view,A::ToggleIndustryPage);
        check(view->industry_page() && view->world().snapshot()==page_before && view->layout().buttons.size()<=31,
              "Industry page changed authority or exceeded20 construction tools");
        for (const auto action:{A::OriginalLoggingShed,A::OriginalBronzeSmelter,A::OriginalBronzewareMaker,A::OriginalLacquerwareMaker,
                A::OriginalPaperMaker,A::OriginalWeaver,A::OriginalTeaCuringShed,A::OriginalLacquerRefinery,A::OriginalSilkwormShed,
                A::OriginalTeaBush,A::OriginalLacquerTree,A::OriginalMulberryTree})
            check(std::count_if(view->layout().buttons.begin(),view->layout().buttons.end(),[&](const auto& b){return b.action==action;})==1,
                  "Industry page lost one of its12 actual paid construction tools");
        const auto industry_funds=view->world().treasury();
        for (int x=104;x<=120;++x) buy(sim::CommandType::PlaceRoad,{x,91});
        for (int y=92;y<100;++y) buy(sim::CommandType::PlaceRoad,{104,y});
        action_click(*view,A::OriginalPaperMaker);check(view->tool()==33,"Paper Maker industry tool is unavailable");
        click(*view,{110,89});
        check(view->world().object_at({111,90})==sim::Object::OriginalPaperMaker45,"Paper mouse purchase lost actual2x2 body");
        const auto paper_id=*view->world().building_owner_at({110,89});
        action_click(*view,A::OriginalTeaCuringShed);check(view->tool()==35,"Tea Shed industry tool is unavailable");click(*view,{118,89});
        const auto tea_id=*view->world().building_owner_at({118,89});
        action_click(*view,A::OriginalTeaBush);check(view->tool()==38,"Paid Tea Bush tool did not find its free purchased source");click(*view,{120,89});
        const auto plant=*view->world().building_owner_at({120,89});
        const auto& sources=view->world().original_production_state()->industry.sources;
        const auto tea=std::find_if(sources.begin(),sources.end(),[&](const auto& row){return row.building_id==tea_id;});
        check(tea!=sources.end() && tea->plant_id==plant && view->world().object_at({120,89})==sim::Object::OriginalTeaBush26 &&
              view->world().treasury()==industry_funds-166,"Plant mouse placement lost unique adjacent ownership or separate50/70/40/6 costs");
        const auto pause_tea=view->execute(sim::set_building_operation(tea_id,false));check(pause_tea.accepted && pause_tea.changed,"Tea did not pause through ordinary operation control");
        const auto elite_funds=view->world().treasury();
        const auto elite_old_tool=view->tool();key(*view,SDLK_H);action_click(*view,A::OriginalElitePlot);
        check(view->tool()==elite_old_tool,"Elite tool escaped the Help modal");key(*view,SDLK_H);
        action_click(*view,A::OriginalElitePlot);check(view->tool()==41,"Paid Elite plot tool unavailable");
        click(*view,{118,92});
        const auto elite_id=*view->world().building_owner_at({118,92});
        check(view->world().object_at({119,93})==sim::Object::OriginalElitePlot &&
              view->world().treasury()==elite_funds-80 && view->world().household_population(elite_id)==0,
              "Elite mouse purchase lost authored2x2 ownership, exact80 price or granted residents");
        const auto empty_diagnosis=view->world().original_housing_diagnosis(elite_id);
        check(empty_diagnosis.actual.type==orig::HouseType::EliteUnoccupied && empty_diagnosis.awaiting_admission &&
              empty_diagnosis.elite_family && empty_diagnosis.geometry_authored_2x2 && empty_diagnosis.geometry_deferred &&
              empty_diagnosis.actual_footprint_side==2 && empty_diagnosis.materialization_pending &&
              empty_diagnosis.materialization_ready &&
              empty_diagnosis.original_required_side==4 && empty_diagnosis.pure_result_type==orig::HouseType::EliteUnoccupied,
              "Empty Elite diagnosis invented an Original footprint or pure first occupancy");
        const auto empty_matrix=view->world().original_housing_decision_matrix(elite_id);
        check(empty_matrix.size()==15 && empty_matrix[8].row_capacity==0 && empty_matrix[8].same_family &&
              !empty_matrix[0].same_family && !empty_matrix[10].needs_met,
              "Hypothetical15-row decision matrix granted an unlocked type or missing actual goods");
        const auto elite_saved=view->world().snapshot();key(*view,SDLK_F5);key(*view,SDLK_F9);
        check(view->world().snapshot()==elite_saved,"Empty paid Elite plot F5/F9 changed family or bootstrap state");
        buy(sim::CommandType::PlaceRoad,{120,94});
        const auto blocked=view->world().original_housing_diagnosis(elite_id);
        check(blocked.materialization_pending && !blocked.materialization_ready &&
              blocked.materialization_diagnostic.blocker==sim::BuildBlocker::SandboxOccupied &&
              blocked.materialization_diagnostic.cell==sim::Cell{120,94} &&
              view->world().building(elite_id).kind==sim::Object::OriginalElitePlot &&
              !view->world().building_owner_at({121,95}),
              "Paid future-growth road did not expose an exact blocker without claiming future cells");
        key(*view,SDLK_F5);const auto blocked_saved=view->world().snapshot();key(*view,SDLK_F9);
        check(view->world().snapshot()==blocked_saved,"Blocked2x2 Plot F5/F9 manufactured partial4x4 claims");
        view->set_tool(5);click(*view,{118,92});
        const auto blocked_lines=view->inspection_lines();
        for (const std::string_view expected:{"plot2x2","Growth4x4 unreserved","Growth blocked:","Growth cell 120,94"}) {
            const bool present=std::any_of(blocked_lines.begin(),blocked_lines.end(),
                [&](const auto& line){return line.find(expected)!=std::string::npos;});
            if (!present) {
                std::cerr<<"Missing blocked Plot inspector label: "<<expected<<'\n';
                for (const auto& line:blocked_lines) std::cerr<<'['<<line<<"]\n";
            }
            check(present,"Blocked Plot inspector hid actual size or concrete unreserved-growth conflict");
        }
        if (argc==2) {
            SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;scroll.wheel.y=-12.0F;
            scroll.wheel.mouse_x=static_cast<float>(view->layout().panel.x+12);
            scroll.wheel.mouse_y=static_cast<float>(view->layout().panel.y+50);
            bool running=true;view->handle_event(scroll,running);
            view->update(0);check(view->render(),"Blocked Plot capture frame");
            auto output=fs::path(argv[1]).parent_path()/"production.elite-blocked.png";
            auto* frame=SDL_RenderReadPixels(renderer,nullptr);check(frame!=nullptr,"Blocked Plot capture pixels");
            const bool okay=SDL_SavePNG(frame,output.string().c_str());SDL_DestroySurface(frame);check(okay,"Blocked Plot capture output");
            nlohmann::json observed{{"capture","actual-paid-road-blocked-empty-elite-plot"},
                {"ticks",view->world().ticks()},{"funds",view->world().treasury()},
                {"house_id",static_cast<unsigned>(elite_id)},{"actual_footprint_side",2},
                {"future_footprint_side",4},{"future_cells_reserved",false},
                {"actual_panel_wheel_notches",-12},
                {"blocker",sim::build_blocker_name(blocked.materialization_diagnostic.blocker)},
                {"blocker_cell",nlohmann::json::array({120,94})},{"inspection_lines",blocked_lines},
                {"original_art_restored",false},{"human_acceptance_claimed",false}};
            auto report=output;report.replace_extension(".observed.json");f::write_text(report,observed.dump(2)+"\n");
            auto save=output;save.replace_extension(".save.json");f::write_text(save,f::read_text(target));
        }
        buy(sim::CommandType::RemoveRoad,{120,94});
        check(view->world().original_housing_diagnosis(elite_id).materialization_ready &&
              view->world().building(elite_id).kind==sim::Object::OriginalElitePlot,
              "Real paid road repair did not unlock future growth while retaining empty Plot");
        view->set_tool(5);click(*view,{110,89});
        const auto paper_details=view->inspection_lines();
        check(view->industry_inspector_selected() && paper_details.size()<=10,
              "Actual sidebar did not select the compact visible Industry inspector");
        for (const std::string_view expected:{"Authored managed recipes","Hemp -> Paper","80 productive ticks","Hemp input","Art: authored fallback"})
            check(std::any_of(paper_details.begin(),paper_details.end(),[&](const auto& line){return line.find(expected)!=std::string::npos;}),
                  "Paper inspector hid actual input, authored recipe or the unconfigured-art fallback");
        const auto paper_art=view->original_building_visual_diagnostic(paper_id);
        check(paper_art.applicable && !paper_art.configured && !paper_art.active && !paper_art.asset &&
              std::none_of(paper_details.begin(),paper_details.end(),[](const auto& line){
                  return line.find("Marker art; original pending")!=std::string::npos;
              }),"Synthetic Paper fixture invented native art or retained a stale pending-art claim");
        const auto page_saved=view->world().snapshot();key(*view,SDLK_F5);
        if (argc==2) {
            check(view->industry_page() && view->selected_building()==paper_id,"Industry capture lost its actual Paper inspector selection");
            view->update(0);check(view->render(),"Industry page actual capture frame");
            auto output=fs::path(argv[1]).parent_path()/"production.industry.png";
            auto* frame=SDL_RenderReadPixels(renderer,nullptr);check(frame!=nullptr,"Industry page capture pixels");
            const bool okay=SDL_SavePNG(frame,output.string().c_str());SDL_DestroySurface(frame);check(okay,"Industry page capture output");
            nlohmann::json tools=nlohmann::json::array();
            for (const auto& button:view->layout().buttons)
                tools.push_back({{"action",static_cast<unsigned>(button.action)},
                    {"rect",nlohmann::json::array({button.rect.x,button.rect.y,button.rect.w,button.rect.h})}});
            nlohmann::json observed{{"capture","paid-industry-page-paper-inspector"},
                {"development_revision",view->world().original_production_state()->development_revision},
                {"ticks",view->world().ticks()},{"funds",view->world().treasury()},
                {"population",view->world().total_population()},{"industry_page",view->industry_page()},
                {"industry_inspector",view->industry_inspector_selected()},
                {"selected_building",static_cast<unsigned>(*view->selected_building())},
                {"paper_id",static_cast<unsigned>(paper_id)},{"tea_id",static_cast<unsigned>(tea_id)},
                {"plant_id",static_cast<unsigned>(plant)},{"tools",tools},{"inspection_lines",paper_details},
                {"original_art_restored",false},{"human_acceptance_claimed",false}};
            auto observed_output=output;observed_output.replace_extension(".observed.json");f::write_text(observed_output,observed.dump(2)+"\n");
            auto saved_output=output;saved_output.replace_extension(".save.json");f::write_text(saved_output,f::read_text(target));
            check(view->world().snapshot()==page_saved,"Industry screenshot preparation mutated paid World authority");
        }
        for (int i=0;i<3;++i) view->tick_once();key(*view,SDLK_F9);
        check(view->industry_page() && view->world().snapshot()==page_saved,"Industry F5/F9 changed new body/plant binding or saved page authority");
        action_click(*view,A::ToggleIndustryPage);check(!view->industry_page(),"Industry Back action did not restore city tools");
        const auto& main_layout=view->layout();
        check(main_layout.buttons.size()==39 && main_layout.buttons.size()==main_layout_before.buttons.size() &&
              std::equal(main_layout.buttons.begin(),main_layout.buttons.end(),main_layout_before.buttons.begin(),[](const auto& a,const auto& b){
                  return a.action==b.action && a.rect.x==b.rect.x && a.rect.y==b.rect.y && a.rect.w==b.rect.w && a.rect.h==b.rect.h;
              }),"Industry Back changed the exact27 historical tools, page toggle or11 existing controls");
        check(main_layout.buttons[28].action==A::Pause &&
              main_layout.buttons[28].rect.y==main_layout.toolbar.y+6*36*main_layout.scale &&
              main_layout.toolbar.h==7*36*main_layout.scale,
              "Main page exceeded its actual six construction rows plus one control row");
        bool real_paper_trip=false;
        for (int i=0;i<3000 && !real_paper_trip;++i) {
            view->tick_once();
            for (const auto& trip:view->world().original_production_state()->industry.aux_trips)
                real_paper_trip|=trip.material==sim::OriginalAuxMaterial::Paper && trip.phase==sim::OriginalGoodsTransferPhase::Loaded &&
                    view->world().courier(trip.transfer.courier_id).role==sim::CourierRole::OriginalAuxSupplier;
        }
        check(real_paper_trip && view->world().workers_assigned(paper_id)==10,"Actual paid Paper chain did not supply a real Academy material courier");
        const auto walkers=view->walker_diagnostics();
        check(std::any_of(walkers.begin(),walkers.end(),[](const auto& row){return row.role==sim::CourierRole::OriginalAuxSupplier && row.cargo>0 && row.live_visible;}),
              "Actual material cargo was hidden by marker or diagnostic presentation");
        const auto paper_trip_saved=view->world().snapshot();key(*view,SDLK_F5);for (int i=0;i<40;++i) view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==paper_trip_saved,"Material midcourier F5/F9 changed sole cargo or recipe sinks");
        for (int i=0;i<800 && view->world().household_population(elite_id)==0;++i) view->tick_once();
        const auto moved=view->world().original_housing_diagnosis(elite_id);
        const auto& histories=view->world().original_production_state()->housing.houses;
        const auto admitted=std::find_if(histories.begin(),histories.end(),[&](const auto& row){return row.building_id==elite_id;});
        check(moved.actual.type==orig::HouseType::ModestSiheyuan && moved.actual.occupants>0 &&
              moved.actual_footprint_side==4 && !moved.geometry_authored_2x2 && !moved.geometry_deferred &&
              !moved.materialization_pending && view->world().building(elite_id).kind==sim::Object::OriginalEliteHouse &&
              admitted!=histories.end() && admitted->first_transitions[10] &&
              admitted->first_transitions[10]->kind==sim::OriginalHousingTransitionKind::Admission &&
              admitted->first_transitions[10]->evaluation.input.type==orig::HouseType::EliteUnoccupied &&
              admitted->first_transitions[10]->admitted>0 && admitted->first_transitions[10]->admitted<=2,
              "Elite first occupancy did not record the actual11→13 paid immigration event");
        for (int dy=0;dy<4;++dy) for (int dx=0;dx<4;++dx)
            check(view->world().object_at({118+dx,92+dy})==sim::Object::OriginalEliteHouse &&
                  view->world().building_owner_at({118+dx,92+dy})==elite_id,
                  "Materialized View Elite did not own exactly its actual4x4 body");
        const auto materialized=view->world().snapshot();key(*view,SDLK_F5);key(*view,SDLK_F9);
        check(view->world().snapshot()==materialized,"Materialized4x4 Elite F5/F9 changed identity, stock, routes or geometry");
        bool religion_trip=false;
        for(int i=0;i<1200 && !religion_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) religion_trip|=c.role==sim::CourierRole::OriginalConfucian &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0;
        }
        check(religion_trip,"Paid Confucian3x3 source never dispatched through shared movement");
        const auto religion_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==religion_saved,"Religion midtrip F5/F9 changed actual trip/clock authority");
        bool all_religions=false;
        for(int i=0;i<6000 && !all_religions;++i) {
            view->tick_once();
            for(const auto& h:view->world().original_economy_state()->houses)
                all_religions|=h.building_id==elite_id && orig::is_elite(h.house.type) && h.house.confucian>0 &&
                    h.house.daoist>0 && h.house.buddhist>0 && h.house.ancestor>0 && h.house.water &&
                    h.house.herbalist>0 && h.house.acupuncture>0 && h.house.hemp>0 && h.house.ceramics>0 && h.house.food_quality>0;
        }
        check(all_religions,"No actual occupied Elite received its Paper-backed Confucian visit and five other service categories");
        for (const auto& h:view->world().original_economy_state()->houses)
            if (!orig::is_elite(h.house.type)) check(h.house.confucian==0,"Common House received forbidden Elite-only Confucian effect");
        const auto culture_funds=view->world().treasury();
        for(int x=104;x<=120;++x)if(!view->world().road_cell({x,96}))buy(sim::CommandType::PlaceRoad,{x,96});
        for(int y:{97,98,99})if(!view->world().road_cell({104,y}))buy(sim::CommandType::PlaceRoad,{104,y});
        view->set_walker_visuals(authored_walker_profile(data));
        check(view->walker_display_stats().schema_version==1,"Authored profile did not enable actual F3 input gate");
        const auto diagnostic_world=view->world().snapshot();key(*view,SDLK_F3);action_click(*view,A::ToggleIndustryPage);
        check(!view->industry_page() && view->world().snapshot()==diagnostic_world,"Industry page escaped F3 diagnostics or changed authority");key(*view,SDLK_F3);
        for(const auto [code,tool,origin,kind,command,side]:std::array{
                std::tuple{SDLK_E,26,sim::Cell{106,94},sim::Object::OriginalMusicSchool211,sim::CommandType::PlaceOriginalMusicSchool,2},
                std::tuple{SDLK_C,27,sim::Cell{110,93},sim::Object::OriginalAcrobatSchool212,sim::CommandType::PlaceOriginalAcrobatSchool,3},
                std::tuple{SDLK_A,28,sim::Cell{114,93},sim::Object::OriginalDramaSchool213,sim::CommandType::PlaceOriginalDramaSchool,3}}) {
            const auto old=view->tool();key(*view,SDLK_H);key(*view,code);
            check(view->tool()==old,"Culture key escaped Help modal");key(*view,SDLK_H);
            key(*view,SDLK_F3);key(*view,code);
            check(view->tool()==old,"Culture key changed the tool during walker diagnostics");key(*view,SDLK_F3);
            key(*view,code);check(view->tool()==tool,"Paid school keyboard tool unavailable");
            const auto funds=view->world().treasury(),price=view->world().construction_cost(command);
            click(*view,origin);
            check(view->world().object_at({origin.x+side-1,origin.y+side-1})==kind &&
                view->world().treasury()==funds-price,"School UI purchase lost its evidenced footprint or prepared model price");
            check(view->world().workers_assigned(*view->world().building_owner_at(origin))==view->world().workforce_required(*view->world().building_owner_at(origin)),
                "Paid culture view scene did not supply actual full workers");
            for(const auto& h:view->world().original_economy_state()->houses)
                check(h.house.music==0 && h.house.acrobat==0 && h.house.drama==0,"School purchase granted a free culture visit");
        }
        view->set_walker_visuals({});
        check(view->world().treasury()==culture_funds-212,"Schools and16 additional physical access roads were not separately paid");
        bool culture_trip=false;
        for(int i=0;i<2000 && !culture_trip;++i) {
            view->tick_once();
            for(const auto& c:view->world().couriers()) culture_trip|=c.role==sim::CourierRole::OriginalDrama &&
                c.phase==sim::CourierPhase::ToWarehouse && c.edge_progress>0;
        }
        check(culture_trip,"Paid Drama3x3 source never dispatched through shared movement");
        view->set_tool(5);click(*view,{114,93});
        const auto source_details=view->inspection_lines();
        for(const std::string_view expected:{"Experimental school-to-House","Target timer:","Available ","No future visit promised"})
            check(std::any_of(source_details.begin(),source_details.end(),[&](const auto& line){return line.find(expected)!=std::string::npos;}),
                "School inspector hid its experimental adapter or derived actual-target delivery status");
        const auto culture_saved=view->world().snapshot();key(*view,SDLK_F5);
        for(int i=0;i<80;++i)view->tick_once();key(*view,SDLK_F9);
        check(view->world().snapshot()==culture_saved,"Culture midtrip F5/F9 changed real trip/clock authority");
        bool all_services=false;
        for(int i=0;i<6000 && !all_services;++i) {
            view->tick_once();
            for(const auto& h:view->world().original_economy_state()->houses)
                all_services|=h.building_id==elite_id && orig::is_elite(h.house.type) && h.house.music>0 && h.house.acrobat>0 && h.house.drama>0 &&
                    h.house.confucian>0 && h.house.daoist>0 && h.house.buddhist>0 && h.house.ancestor>0 && h.house.water &&
                    h.house.herbalist>0 && h.house.acupuncture>0 && h.house.hemp>0 && h.house.ceramics>0 && h.house.food_quality>0;
        }
        check(all_services,"No actual House simultaneously received nine separate services and paid water/goods/Food");
        bool common_services=false;
        for (const auto& h:view->world().original_economy_state()->houses) if (!orig::is_elite(h.house.type)) {
            check(h.house.confucian==0,"New Elite-only religion leaked to a Common House");
            common_services|=h.house.music>0 && h.house.acrobat>0 && h.house.drama>0 && h.house.daoist>0 &&
                h.house.buddhist>0 && h.house.ancestor>0 && h.house.herbalist>0 && h.house.acupuncture>0;
        }
        check(common_services,"No actual Common House retained its eight independently delivered permitted services");
        key(*view,SDLK_F5);const auto current=f::read_text(target);
        const auto before=view->world().snapshot();
        const auto model_before=view->world().original_rules_model();
        auto bad=nlohmann::json::parse(current);
        bad["world"]["original_production"]["development_revision"]=sim::kOriginalProductionDevelopmentRevision-1;
        f::write_text(target,bad.dump());key(*view,SDLK_F9);
        check(view->world().snapshot()==before && view->world().original_rules_model()==model_before,
            "Bad development revision published through F9");
        f::write_text(target,current);key(*view,SDLK_F9);
        const auto same_difficulty_model=view->world().original_rules_model();
        bad=nlohmann::json::parse(current);
        bad["world"]["original_economy"]["model_source"]["difficulty"]=static_cast<unsigned>(orig::Difficulty::Hard);
        f::write_text(target,bad.dump());key(*view,SDLK_F9);
        check(view->world().snapshot()==before && view->world().original_rules_model()==same_difficulty_model &&
              view->last_message().find("difficulty")!=std::string::npos,
              "F9 silently changed the immutable session difficulty or partially replaced World authority");
        f::write_text(target,current);key(*view,SDLK_F9);
        std::optional<sim::Cell> supplied;
        for(const auto& house:view->world().original_economy_state()->houses)
            if(house.building_id==elite_id && orig::is_elite(house.house.type) && house.house.water && house.house.hemp>0 && house.house.ceramics>0 && house.house.food_quality>0 &&
                house.house.herbalist>0 && house.house.acupuncture>0 && house.house.ancestor>0 &&
                house.house.confucian>0 && house.house.daoist>0 && house.house.buddhist>0 &&
                house.house.music>0 && house.house.acrobat>0 && house.house.drama>0) {supplied=view->world().building(house.building_id).cell;break;}
        check(supplied.has_value(),"No actual two-goods House available for inspection");
        view->set_tool(5);click(*view,*supplied);
        const auto details=view->inspection_lines();
        for (const std::string_view expected:{"4x4","Elite4x4; authored courtyard","Current row","Next row",
                "Bronze OR Lacquer","Daoist OR Buddhist","need","Original housing art pending","Tax rate",
                "Tax paid total","Next tax tick","Last tax tick"})
            check(std::any_of(details.begin(),details.end(),[&](const auto& line){return line.find(expected)!=std::string::npos;}),
                  "House inspector hid a real condition, alternative or actual4x4 authored geometry boundary");
        check(std::count_if(details.begin(),details.end(),[](const auto& line) {
            return line.find("Herbalist")!=std::string::npos && line.find("Acupuncture")!=std::string::npos;
        })==1,"Medical values wrapped and displaced the default House inspector");
        check(std::count_if(details.begin(),details.end(),[](const auto& line) {
            return line.find("Music")!=std::string::npos && line.find("Acrobat")!=std::string::npos && line.find("Drama")!=std::string::npos;
        })==1,"Three culture timers did not fit on their own visible inspector line");
        check(std::count_if(details.begin(),details.end(),[](const auto& line) {
            return line.find("Food:")!=std::string::npos && line.find("Quality:")!=std::string::npos &&
                line.find(" W")!=std::string::npos && line.find(" M")!=std::string::npos;
        })==1,"Food stock, quality and actual crop amounts did not fit on one inspector line");
        const auto misses=std::find_if(details.begin(),details.end(),[](const auto& line) {
            return line.find("Food demand misses:")!=std::string::npos;
        });
        check(misses!=details.end() && std::distance(details.begin(),misses)<=10,
              "Ancestor display displaced Food demand misses from the visible default inspector");
        // The initial whole-map fit permits zoom below0.5, which ordinary wheel
        // input cannot restore. Establish a reachable1x baseline through the
        // actual wheel path; resetting the camera does not promise a fixed zoom.
        key(*view,SDLK_R);
        const auto& map_rect=view->layout().map;
        const oe::scene::Point map_center{map_rect.x+map_rect.w*.5,map_rect.y+map_rect.h*.5};
        zoom_at(*view,map_center,1.0);
        // Independent authored-map oracle: all terrain is height0, border72.
        const auto front=sim::building_front_cell(view->world().profile(),view->world().rule_version(),
            view->world().building(elite_id).kind,view->world().building(elite_id).cell);
        check(front==sim::Cell{121,95},"Actual4x4 painter did not use unchanged logical front-cell convention");
        const oe::scene::Point elite_ground{double(front.x-front.y)*40.0,double(front.x+front.y-144)*20.0+20.0};
        // Preserve a reachable view of the selected House, not the old whole-map
        // fit magnified about its distant center. Ordinary pan owns both moves.
        pan_to_offset(*view,{map_center.x-elite_ground.x*view->camera().zoom,
            map_center.y-(elite_ground.y-60)*view->camera().zoom});
        const auto original_camera=view->camera();
        const bool debug_was_open=view->debug_diagnostics();if (!debug_was_open) key(*view,SDLK_F1);
        nlohmann::json visual_checks=nlohmann::json::array();
        for (const double zoom:{0.5,1.0,2.0}) {
            zoom_at(*view,map_center,zoom);
            pan_to_offset(*view,{map_center.x-elite_ground.x*view->camera().zoom,
                map_center.y-(elite_ground.y-60)*view->camera().zoom});
            view->set_tool(5);view->update(0);check(view->render(),"Actual4x4 outer-extent picking frame");
            for (const oe::scene::Point relative:std::array<oe::scene::Point,4>{{
                    {-150,-60},{150,-60},{0,-130},{0,10}}}) {
                const auto screen=view->camera().world_to_screen({elite_ground.x+relative.x,elite_ground.y+relative.y});
                check(map_rect.contains(screen.x,screen.y),"Required outer4x4 visual sample was clipped out of playable map");
                click_screen(*view,screen);
                check(view->selected_building()==elite_id && !view->input_diagnostic_state().selected_landscape,
                    "Outer4x4 rendered footprint did not select its existing BuildingId through shared visual hit geometry");
            }
            unsigned observed_feet=0;
            for (const auto& row:view->walker_diagnostics()) {
                const auto position=view->world().courier_position(row.id);
                if (!row.live_visible || !position) continue;
                const auto expected=view->camera().world_to_screen({(position->x-position->y)*40.0,
                    (position->x+position->y-144)*20.0+20.0});
                check(std::abs(row.screen_ground.x-expected.x)<1e-6 && std::abs(row.screen_ground.y-expected.y)<1e-6,
                    "Actual shared courier feet moved away from authoritative saved path position after4x4 materialization");
                ++observed_feet;
            }
            check(observed_feet>0,"No actual courier feet available during materialized4x4 View checks");
            visual_checks.push_back({{"zoom",view->camera().zoom},{"outer_samples",4},
                {"same_building_selected",true},{"actual_courier_feet_checked",observed_feet}});
            if (argc==2) {
                check(view->render(),"Actual4x4 zoom capture frame");
                const std::string label=zoom==0.5 ? "05":zoom==1 ? "1":"2";
                auto output=fs::path(argv[1]).parent_path()/("production.elite4x4.zoom"+label+".png");
                auto* frame=SDL_RenderReadPixels(renderer,nullptr);check(frame!=nullptr,"4x4zoom capture pixels");
                const bool okay=SDL_SavePNG(frame,output.string().c_str());SDL_DestroySurface(frame);check(okay,"4x4zoom capture output");
                nlohmann::json observed{{"capture","actual-paid-materialized-elite4x4"},
                    {"ticks",view->world().ticks()},{"house_id",static_cast<unsigned>(elite_id)},
                    {"actual_footprint_side",4},{"front",nlohmann::json::array({front.x,front.y})},
                    {"visual_checks",visual_checks},{"painter","unchanged logical front-cell order"},
                    {"world_unchanged",view->world().snapshot()==before},
                    {"original_art_restored",false},{"human_acceptance_claimed",false}};
                auto report=output;report.replace_extension(".observed.json");f::write_text(report,observed.dump(2)+"\n");
                auto save=output;save.replace_extension(".save.json");f::write_text(save,current);
            }
        }
        zoom_at(*view,map_center,original_camera.zoom);pan_to_offset(*view,original_camera.offset);
        if (!debug_was_open) key(*view,SDLK_F1);
        view->set_tool(5);click(*view,*supplied);
        check(view->world().snapshot()==before,"Camera, outer picking or4x4 diagnostics changed paid World authority");
        oe::performance::set_enabled(true);oe::performance::reset();
        for(int i=0;i<100;++i){view->update(0);check(view->render(),"Production idle frame");}
        check(view->world().snapshot()==before,"Production frame changed World");
        for(const auto counter:{oe::performance::Counter::WorldCopies,oe::performance::Counter::WorldExecutes,
            oe::performance::Counter::BfsCalls,oe::performance::Counter::AssetDecodes,oe::performance::Counter::TextureUploads,
            oe::performance::Counter::FileReads,oe::performance::Counter::FileWrites})
            check(oe::performance::counter(counter)==0,"Production frame prepared or mutated state");
        oe::performance::set_enabled(false);
        if(argc==2){auto* frame=SDL_RenderReadPixels(renderer,nullptr);check(frame!=nullptr,"Capture pixels");
            const bool okay=SDL_SavePNG(frame,argv[1]);SDL_DestroySurface(frame);check(okay,"Capture output");
            const auto id=*view->selected_building();
            const auto& houses=view->world().original_economy_state()->houses;
            const auto h=std::find_if(houses.begin(),houses.end(),[&](const auto& value){return value.building_id==id;});
            check(h!=houses.end() && h->house.food_quality>0,"Captured House lost its actual Food quality");
            nlohmann::json observed{{"development_revision",view->world().original_production_state()->development_revision},
                {"ticks",view->world().ticks()},{"funds",view->world().treasury()},
                {"population",view->world().total_population()},
                {"selected_house",static_cast<unsigned>(id)},
                {"selected_house_food_quality",h->house.food_quality},
                {"selected_house_hemp",h->house.hemp},{"selected_house_ceramics",h->house.ceramics},
                {"selected_house_herbalist",h->house.herbalist},{"selected_house_acupuncture",h->house.acupuncture},
                {"selected_house_ancestor",h->house.ancestor},{"selected_house_water",h->house.water},
                {"selected_house_confucian",h->house.confucian},{"selected_house_daoist",h->house.daoist},
                {"selected_house_buddhist",h->house.buddhist},
                {"selected_house_music",h->house.music},{"selected_house_acrobat",h->house.acrobat},
                {"selected_house_drama",h->house.drama},
                {"selected_house_type",static_cast<unsigned>(h->house.type)},
                {"selected_house_devolve_counter",h->house.devolve_counter},
                {"model_sha256",view->world().original_rules_model()->provenance().sha256},
                {"synthetic_model",true},{"inspection_lines",details},{"idle_frames",100},{"idle_world_unchanged",true}};
            observed["elite4x4_visual_checks"]=visual_checks;
            for(const auto& row:view->world().original_production_state()->food.stocks) if(row.building_id==id)
                observed["selected_house_food"]=row.amounts;
            auto output=fs::path(argv[1]);output.replace_extension(".observed.json");f::write_text(output,observed.dump(2));
            output=fs::path(argv[1]);output.replace_extension(".save.json");f::write_text(output,current);
            SDL_Event scroll{};scroll.type=SDL_EVENT_MOUSE_WHEEL;scroll.wheel.y=-13.0F;
            scroll.wheel.mouse_x=static_cast<float>(view->layout().panel.x+12);
            scroll.wheel.mouse_y=static_cast<float>(view->layout().panel.y+50);
            bool running=true;view->handle_event(scroll,running);view->update(0);
            check(view->render(),"Actual housing diagnosis scroll frame");
            output=fs::path(argv[1]).parent_path()/"production.housing.png";
            auto* diagnosis_frame=SDL_RenderReadPixels(renderer,nullptr);check(diagnosis_frame!=nullptr,"Housing diagnosis capture pixels");
            const bool diagnosis_okay=SDL_SavePNG(diagnosis_frame,output.string().c_str());SDL_DestroySurface(diagnosis_frame);
            check(diagnosis_okay,"Actual housing diagnosis capture output");
            const auto diagnosis=view->world().original_housing_diagnosis(id);
            const auto row_json=[](const sim::OriginalHousingRowDiagnosis& row) {
                nlohmann::json needs=nlohmann::json::array();
                for (const auto& need:row.requirements) needs.push_back({{"field",static_cast<unsigned>(need.field)},
                    {"required",need.required},{"actual",need.actual},{"alternative",need.alternative},
                    {"presence",need.presence},{"met",need.met}});
                return nlohmann::json{{"row",static_cast<unsigned>(row.row)},{"low",row.low},{"high",row.high},
                    {"row_capacity",row.row_capacity},{"needs_met",row.needs_met},{"requirements",needs}};
            };
            nlohmann::json housing_observed{{"capture","actual-paid-elite-housing-diagnosis"},
                {"development_revision",view->world().original_production_state()->development_revision},
                {"ticks",view->world().ticks()},{"selected_house",static_cast<unsigned>(id)},
                {"selected_house_type",static_cast<unsigned>(diagnosis.actual.type)},
                {"occupants",diagnosis.actual.occupants},{"elite_family",diagnosis.elite_family},
                {"actual_cached_desirability",diagnosis.cached_desirability},{"counter",diagnosis.actual.devolve_counter},
                {"counter_limit",diagnosis.devolve_limit},{"admission_capacity",diagnosis.admission_capacity},
                {"census_capacity",diagnosis.census_capacity},{"current",row_json(diagnosis.current)},{"next",row_json(diagnosis.next)},
                {"authored_footprint_side",diagnosis.actual_footprint_side},{"original_required_side",diagnosis.original_required_side},
                {"geometry_deferred",diagnosis.geometry_deferred},{"hypothetical_matrix_rows",15},
                {"inspection_lines",view->inspection_lines()},{"actual_panel_wheel_notches",-13},
                {"world_unchanged",view->world().snapshot()==before},{"original_art_restored",false}};
            auto housing_observed_output=output;housing_observed_output.replace_extension(".observed.json");
            f::write_text(housing_observed_output,housing_observed.dump(2)+"\n");
            auto housing_saved_output=output;housing_saved_output.replace_extension(".save.json");f::write_text(housing_saved_output,current);
            check(view->selected_building()==id && view->world().snapshot()==before,
                  "Housing scroll or diagnostic screenshot changed paid World authority");
            // Move through the real preceding service rows to the actual
            // food/goods requirements. Wrapped OR labels also occupy rows.
            scroll.wheel.y=-13.0F;view->handle_event(scroll,running);view->update(0);
            check(view->render(),"Actual housing requirements scroll frame");
            output=fs::path(argv[1]).parent_path()/"production.requirements.png";
            auto* requirements_frame=SDL_RenderReadPixels(renderer,nullptr);check(requirements_frame!=nullptr,"Housing requirements capture pixels");
            const bool requirements_okay=SDL_SavePNG(requirements_frame,output.string().c_str());SDL_DestroySurface(requirements_frame);
            check(requirements_okay,"Actual housing requirements capture output");
            auto requirements_observed=housing_observed;
            requirements_observed["capture"]="actual-paid-elite-housing-requirements";
            requirements_observed["actual_panel_wheel_notches"]=-26;
            requirements_observed["additional_panel_wheel_notches"]=-13;
            requirements_observed["inspection_lines"]=view->inspection_lines();
            requirements_observed["world_unchanged"]=view->world().snapshot()==before;
            auto requirements_observed_output=output;requirements_observed_output.replace_extension(".observed.json");
            f::write_text(requirements_observed_output,requirements_observed.dump(2)+"\n");
            auto requirements_saved_output=output;requirements_saved_output.replace_extension(".save.json");f::write_text(requirements_saved_output,current);
            check(view->selected_building()==id && view->world().snapshot()==before,
                  "Requirements scroll or screenshot changed paid World authority");
        }
        view->shutdown();view.reset();
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"Original production SDL paid goods/Food/nine services, actual House timers/quality, midcourier F5/F9/100 idle frames PASS\n";return 0;
    }catch(const std::exception& error){
        std::cerr<<error.what()<<'\n';SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 1;
    }
}
