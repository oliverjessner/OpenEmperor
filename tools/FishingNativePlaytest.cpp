// Shared paid-city preparation, capture and exact F5/F9 support.
#include "FoodNativePlaytest.h"

using namespace oe_food_playtest;

int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;fs::path output;
    try {
        require(argc==5,"Usage: openemperor-fishing-native-playtest DATA RESOURCE_ROOT MAP FRESH_.local/agri-mac02/OUT");
        const auto data=fs::canonical(argv[1]),resources=fs::canonical(argv[2]);const fs::path map=argv[3];
        output=output_root(argv[4],data);
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("Scripted actual-data Fishing playtest",1280,900,SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto session=oe::maps::load_stored_map_session(data,map,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        const auto border=session.plan.border;
        auto view=oe::SandboxView::create_playable_fishing(std::move(session),orig::Difficulty::Normal,false);
        view->configure_save(data,map,output/"saves/initial.save.json");configure(*view,oe::detect_and_select_visual_profiles(data,resources));
        view->initialize(window,renderer);view->set_tool(5);if(!view->paused())key(*view,SDLK_SPACE);
        SDL_Event focused{};focused.type=SDL_EVENT_WINDOW_FOCUS_GAINED;bool running=true;view->handle_event(focused,running);
        const auto before=view->capture_save_document();diagnose_permissions(view->world(),output);
        const auto plan=choose_plan(view->world());
        for(const auto command:plan.commands) {
            const auto result=view->execute(command);require(result.accepted,"Normal paid view construction failed: "+result.reason);
        }
        require(view->world().total_population()==0 && view->world().ticks()==0 &&
            view->world().construction_spent_total()>0 && view->world().fishing_state()->boats.empty(),"Paid setup granted time, people or an unearned boat");
        for(const auto& stock:view->world().fishing_food_state()->stocks)
            for(const auto amount:stock.amounts)require(amount==0,"Paid setup granted Food");
        Json checkpoints=Json::array();checkpoints.push_back(checkpoint(*view,data,map,output,"paid-empty"));
        Json witnesses=Json::object();bool saved_boat=false,saved_courier=false;
        for(int i=0;i<30000;++i) {
            view->tick_once();const auto& world=view->world();const auto observation=observe(world);
            const auto witness=[&](const std::string& name,bool actual) {if(actual && !witnesses.contains(name))witnesses[name]=observation;};
            witness("full_staffing",world.workers_assigned(plan.quay)==world.workforce_required(plan.quay));
            bool loaded_boat=false,travelling=false,working=false,actual_return=false;
            for(const auto& boat:world.fishing_state()->boats) {
                travelling|=boat.edge_progress>0;working|=boat.phase==sim::FishingBoatPhase::Fishing && boat.work_progress>0;
                loaded_boat|=boat.cargo>0;actual_return|=boat.phase==sim::FishingBoatPhase::Unloading && boat.cargo>0;
            }
            witness("physical_boat_movement",travelling);witness("actual_site_work",working);witness("actual_boat_catch",loaded_boat);
            witness("actual_dock_return",actual_return);witness("quay_unload",observation["quay_unloaded"].get<std::uint64_t>()>0);
            witness("market_arrival",observation["market_received"].get<std::uint64_t>()>0);
            witness("house_arrival",observation["house_received"].get<std::uint64_t>()>0);
            witness("fish_consumption",observation["fish_consumed"].get<std::uint64_t>()>0);
            witness("millet_consumption",observation["millet_consumed"].get<std::uint64_t>()>0);
            if(!saved_boat && loaded_boat && travelling) {checkpoints.push_back(checkpoint(*view,data,map,output,"live-loaded-boat"));saved_boat=true;continue;}
            bool loaded_courier=false;
            for(const auto& trip:world.fishing_food_state()->trips) if(trip.loaded[5]>0 && trip.phase==sim::OriginalGoodsTransferPhase::Loaded &&
                world.courier(trip.transfer.courier_id).edge_progress>0)loaded_courier=true;
            if(!saved_courier && loaded_courier) {checkpoints.push_back(checkpoint(*view,data,map,output,"live-fish-courier"));saved_courier=true;continue;}
            if(saved_boat && saved_courier && witnesses.contains("fish_consumption") && witnesses.contains("millet_consumption"))break;
        }
        require(witnesses.contains("full_staffing") && witnesses.contains("physical_boat_movement") && witnesses.contains("actual_site_work") &&
            witnesses.contains("actual_boat_catch") && witnesses.contains("actual_dock_return") && witnesses.contains("quay_unload") &&
            witnesses.contains("market_arrival") && witnesses.contains("house_arrival") && witnesses.contains("fish_consumption") &&
            witnesses.contains("millet_consumption") && saved_boat && saved_courier,
            "Actual-data paid city did not complete Fishing and Millet consumption within30000 ticks: "+observe(view->world()).dump());
        invariant(view->world());
        checkpoints.push_back(checkpoint(*view,data,map,output,"earned-consumption"));
        for(int i=0;i<3000;++i) {
            bool visible=false;for(const auto& boat:view->world().fishing_state()->boats)visible|=boat.edge_progress>0;
            if(visible)break;view->tick_once();
        }
        checkpoints.push_back(checkpoint(*view,data,map,output,"manual"));
        const auto focus=oe::maps::terrain_ground({static_cast<unsigned>(plan.quay_cell.x),static_cast<unsigned>(plan.quay_cell.y)},border);
        Json captures=Json::array();bool inspector=false;
        for(int zoom=0;zoom<3;++zoom) {
            center(*view,focus);inspector|=select_quay(*view,renderer,plan.quay,border);
            const auto path=output/("quay-zoom-"+std::to_string(zoom)+".png");capture(*view,renderer,path);
            captures.push_back({{"path",path.filename().string()},{"zoom",view->camera().zoom}});if(zoom<2)key(*view,SDLK_Z);
        }
        const auto final=view->capture_save_document();require(final.map_sha256==before.map_sha256,"Original map changed during scripted playtest");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map=map;settings.last_save=output/"saves/manual.save.json";
        settings.profile=sim::RulesProfile::CityV16;settings.prepared_starter=false;oe::menu::write_settings(output,settings);
        Json report={{"status","PASS"},{"scope","scripted SDL software with actual supplied map/model; not native-window or human acceptance"},
            {"native_window_acceptance","NOT_RUN"},{"human_acceptance","NOT_RUN"},{"original_model_modified",false},{"snapshot_injection",false},
            {"original_map",map.generic_string()},{"map_sha256",before.map_sha256},{"model_sha256",view->world().original_economy_state()->model_source->sha256},
            {"rule_version",28},{"save_schema",44},{"city_origin",{plan.origin.x,plan.origin.y}},{"city_layout",plan.layout},{"planning_statistics",planning_statistics},
            {"quay_origin",{plan.quay_cell.x,plan.quay_cell.y}},{"quay_orientation",plan.orientation},
            {"quay_id",static_cast<unsigned>(plan.quay)},{"farm_id",static_cast<unsigned>(plan.farm)},{"market_id",static_cast<unsigned>(plan.market)},
            {"construction_spent",view->world().construction_spent_total()},{"witnesses",witnesses},{"checkpoints",checkpoints},
            {"captures",captures},{"quay_inspector_selected",inspector},{"final",observe(view->world())},
            {"native_load_arguments",Json::array({"--data",data.string(),"--load-sandbox",(output/"saves/manual.save.json").string()})}};
        std::ofstream report_file(output/"report.json");report_file<<report.dump(2)<<'\n';report_file.close();
        require(bool(report_file),"Could not publish private actual-data playtest report");
        view->shutdown();view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS scripted actual-data Fishing/Millet city; native/human NOT_RUN; report "<<(output/"report.json")<<'\n';return 0;
    } catch(const std::exception& error) {
        if(!output.empty())std::ofstream(output/"report.json")<<Json{{"status","FAIL"},{"reason",error.what()},{"planning_statistics",planning_statistics},
            {"scope","scripted SDL software only"},{"native_window_acceptance","NOT_RUN"},{"human_acceptance","NOT_RUN"}}.dump(2)<<'\n';
        if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
        std::cerr<<"Fishing actual-data playtest FAIL: "<<error.what()<<'\n';return 1;
    }
}
