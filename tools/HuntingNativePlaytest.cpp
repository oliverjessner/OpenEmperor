// Paid supplied-data acceptance. Shared preparation uses production validators;
// no injected people, food, arrivals, animals or source-model modifications.
#include "FoodNativePlaytest.h"

using namespace oe_food_playtest;
namespace {
struct HuntingPlan {Plan coastal;sim::BuildingId tent{};sim::Cell tent_cell{};};
HuntingPlan add_hunting(sim::World& world,Plan plan) {
    struct Candidate {sim::Cell cell;int score;};std::vector<Candidate> candidates;
    std::vector<sim::Cell> nearby;
    for(const auto road:plan.roads)for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx)
        nearby.push_back({road.x+dx,road.y+dy});
    std::sort(nearby.begin(),nearby.end(),[](auto a,auto b){return a.y!=b.y ? a.y<b.y:a.x<b.x;});
    nearby.erase(std::unique(nearby.begin(),nearby.end()),nearby.end());
    for(const auto cell:nearby) {
        if(!world.validate({sim::CommandType::PlaceOriginalHuntersTent,cell}).accepted)continue;
        int score=world.width()+world.height();
        for(const auto road:plan.roads)score=std::min(score,distance(cell,road));
        if(score<=4)candidates.push_back({cell,score});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.score<b.score;});
    for(const auto option:candidates) {
        auto candidate=world;auto complete=plan;
        if(!buy(candidate,complete,{sim::CommandType::PlaceOriginalHuntersTent,option.cell}))continue;
        const auto id=*candidate.building_owner_at(option.cell);
        // Ordinary paid roads do not open unsupported hunting terrain. Reject
        // an unreachable animal context before exploring any road layout.
        bool prey_reachable=false;
        for(const auto& animal:candidate.hunting_state()->animals) {
            const auto cells=candidate.building_cells(id);
            const auto route=candidate.find_hunting_route(cells.front(),animal.current,id);
            if(route && route->size()>1) {prey_reachable=true;break;}
        }
        if(!prey_reachable)continue;
        std::vector<sim::Cell> goals;
        for(const auto cell:candidate.building_cells(id))for(const sim::Cell delta:std::array<sim::Cell,4>{{{0,-1},{1,0},{0,1},{-1,0}}}) {
            const sim::Cell c{cell.x+delta.x,cell.y+delta.y};
            if(candidate.validate({sim::CommandType::PlaceRoad,c}).accepted)goals.push_back(c);
        }
        for(const auto goal:goals)for(const auto start:plan.roads) {
            if(distance(start,goal)>8)continue;
            for(const auto& cells:road_options(start,goal)) {
                if(cells.empty() || cells.size()>20)continue;
                auto connected=candidate;auto connected_plan=complete;bool legal=true;
                for(const auto cell:cells)if(!buy(connected,connected_plan,{sim::CommandType::PlaceRoad,cell})) {legal=false;break;}
                if(!legal || !connected.find_building_route(id,plan.market))continue;
                bool reachable=false;
                for(const auto source:connected.building_cells(id))for(const auto& animal:connected.hunting_state()->animals) {
                    const auto path=connected.find_hunting_route(source,animal.current,id);
                    if(path && path->size()>1) {reachable=true;break;}
                }
                if(reachable) {world=std::move(connected);return {std::move(connected_plan),id,option.cell};}
            }
        }
    }
    throw std::runtime_error("Paid city has no legal road-connected Tent with an actual shared route to living prey");
}
HuntingPlan choose_hunting_city(const sim::World& base) {
    const auto layouts=city_blueprints();
    struct Origin {std::size_t layout;sim::Cell cell;int distance;};std::vector<Origin> origins;
    require(!base.hunting_state()->animals.empty(),"Map has no qualified living prey");
    for(std::size_t i=0;i<layouts.size();++i)for(const auto cell:clear_origins(base,layouts[i])) {
        int closest=base.width()+base.height();
        for(const auto& animal:base.hunting_state()->animals)closest=std::min(closest,distance(cell,animal.current));
        origins.push_back({i,cell,closest});
    }
    std::stable_sort(origins.begin(),origins.end(),[](auto a,auto b){return a.distance<b.distance;});
    std::size_t attempted=0;
    for(const auto origin:origins) {
        if(++attempted>128)break;
        auto candidate=base;Plan plan;
        if(!city(candidate,plan,layouts[origin.layout],origin.cell))continue;
        try {auto result=add_hunting(candidate,std::move(plan));planning_statistics["hunting_layout_attempts"]=attempted;return result;}
        catch(const std::runtime_error&) {}
    }
    throw std::runtime_error("Bounded paid Hunting layout planning found no connected city/Tent/prey combination");
}
Json hunting_observation(const sim::World& world) {
    std::uint64_t births=0,killed=0,unloaded=0,market=0,houses=0,consumed=0,millet=0,fish=0;
    for(const auto& habitat:world.hunting_state()->habitats) {births+=habitat.births;killed+=habitat.killed;}
    for(const auto& tent:world.hunting_state()->tents)unloaded+=tent.unloaded;
    for(const auto& stock:world.hunting_food_state()->stocks) {
        const auto kind=world.building(stock.building_id).kind;
        if(kind==sim::Object::Market)market+=stock.received[6];
        if(sim::original_house_kind(kind)) {houses+=stock.received[6];consumed+=stock.consumed[6];millet+=stock.consumed[1];fish+=stock.consumed[5];}
    }
    return {{"tick",world.ticks()},{"population",world.total_population()},{"funds",world.treasury()},
        {"live_animals",world.hunting_state()->animals.size()},{"births",births},{"killed",killed},
        {"tent_unloaded",unloaded},{"market_received",market},{"house_received",houses},
        {"meat_consumed",consumed},{"millet_consumed",millet},{"fish_consumed",fish},
        {"active_hunters",world.hunting_state()->hunters.size()},{"hunter_launches",world.hunting_state()->launched}};
}
Json save_hunting(oe::SandboxView& view,const fs::path& data,const fs::path& map,const fs::path& output,const std::string& name) {
    const auto path=output/"saves"/(name+".save.json");view.configure_save(data,map,path);
    const auto before=view.world().snapshot();const auto generation=view.save_generation();key(view,SDLK_F5);
    require(view.save_generation()==generation+1,"Hunting F5 failed: "+view.last_message());
    const auto saved=oe::persistence::read_save(path);
    require(saved.source_schema_version==45 && saved.world==before,"Hunting F5 changed schema45 World");
    const auto loaded=view.load_generation();key(view,SDLK_F9);
    require(view.load_generation()==loaded+1 && view.world().snapshot()==before,"Hunting F9 changed World: "+view.last_message());
    return {{"name",name},{"tick",before.ticks},{"schema",45},{"f5_f9_exact",true}};
}
bool select_tent(oe::SandboxView& view,SDL_Renderer* renderer,sim::BuildingId tent,std::uint32_t border) {
    view.set_tool(5);
    for(const auto cell:view.world().building_cells(tent)) {
        const auto point=view.camera().world_to_screen(oe::maps::terrain_ground({static_cast<unsigned>(cell.x),static_cast<unsigned>(cell.y)},border));
        float x=0,y=0;if(!SDL_RenderCoordinatesToWindow(renderer,static_cast<float>(point.x),static_cast<float>(point.y),&x,&y))continue;
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.down=true;e.button.x=x;e.button.y=y;bool running=true;
        view.handle_event(e,running);e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.down=false;view.handle_event(e,running);
        if(view.selected_building()==tent)return true;
    }
    return false;
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;fs::path output;
    try {
        require(argc==5,"Usage: openemperor-hunting-native-playtest DATA RESOURCE_ROOT MAP FRESH_.local/agri-mac03/OUT");
        const auto data=fs::canonical(argv[1]),resources=fs::canonical(argv[2]);const fs::path map=argv[3];
        output=output_root(argv[4],data,".local/agri-mac03");
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_SetHint(SDL_HINT_RENDER_DRIVER,"software") && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("Scripted supplied-data Hunting",1280,900,SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto session=oe::maps::load_stored_map_session(data,map,oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8,oe::maps::GreatWallPresentationMode::Automatic,{},true);
        const auto border=session.plan.border;
        auto view=oe::SandboxView::create_playable_hunting(std::move(session),orig::Difficulty::Normal,false);
        view->configure_save(data,map,output/"saves/initial.save.json");configure(*view,oe::detect_and_select_visual_profiles(data,resources));
        view->initialize(window,renderer);if(!view->paused())key(*view,SDLK_SPACE);
        SDL_Event focus{};focus.type=SDL_EVENT_WINDOW_FOCUS_GAINED;bool running=true;view->handle_event(focus,running);
        const auto source=view->capture_save_document();
        std::cerr<<"Preparing paid city and real prey routes\n";
        const auto plan=choose_hunting_city(view->world());
        std::cerr<<"Paid city at "<<plan.coastal.origin.x<<','<<plan.coastal.origin.y<<"\n";
        for(const auto command:plan.coastal.commands) {
            const auto result=view->execute(command);require(result.accepted,"Paid normal construction refused: "+result.reason);
        }
        require(view->world().ticks()==0 && view->world().total_population()==0 && view->world().hunting_state()->hunters.empty(),"Paid setup injected time, people or hunters");
        for(const auto& stock:view->world().hunting_food_state()->stocks)for(const auto amount:stock.amounts)require(!amount,"Setup injected Food");
        Json checkpoints=Json::array(),witnesses=Json::object();checkpoints.push_back(save_hunting(*view,data,map,output,"paid-empty"));
        bool saved_hunt=false,saved_cargo=false,saved_market=false,saved_house=false;
        for(int tick=0;tick<30000;++tick) {
            view->tick_once();const auto& world=view->world();const auto state=hunting_observation(world);
            const auto witness=[&](const char* name,bool fact){if(fact && !witnesses.contains(name))witnesses[name]=state;};
            witness("full_staffing",world.workers_assigned(plan.tent)==world.workforce_required(plan.tent));
            witness("living_prey",!world.hunting_state()->animals.empty());
            bool moving=false,hunting=false,cargo=false,returning=false;
            for(const auto& hunter:world.hunting_state()->hunters) {
                moving|=hunter.edge_progress>0;hunting|=hunter.phase==sim::HuntingHunterPhase::Hunting && hunter.work_progress>0;
                cargo|=hunter.cargo>0 && hunter.edge_progress>0;
                returning|=hunter.phase==sim::HuntingHunterPhase::Unloading && hunter.cargo>0;
            }
            witness("physical_hunter_movement",moving);witness("actual_hunt_work",hunting);witness("physical_meat_cargo",cargo);
            witness("actual_tent_return",returning);witness("successful_kill",state["killed"].get<std::uint64_t>()>0);
            witness("tent_unload",state["tent_unloaded"].get<std::uint64_t>()>0);
            witness("market_arrival",state["market_received"].get<std::uint64_t>()>0);
            witness("house_arrival",state["house_received"].get<std::uint64_t>()>0);
            witness("meat_consumption",state["meat_consumed"].get<std::uint64_t>()>0);
            witness("fish_consumption",state["fish_consumed"].get<std::uint64_t>()>0);
            witness("millet_consumption",state["millet_consumed"].get<std::uint64_t>()>0);
            bool market_cargo=false,house_cargo=false;
            for(const auto& trip:world.hunting_food_state()->trips)if(trip.loaded[6]>0 && trip.phase==sim::OriginalGoodsTransferPhase::Loaded && world.courier(trip.transfer.courier_id).edge_progress>0) {
                const bool to_market=world.courier(trip.transfer.courier_id).role==sim::CourierRole::OriginalFoodToMarket;
                market_cargo|=to_market;house_cargo|=!to_market;
            }
            // Loading replaces World authority; discard every live-state reference
            // before the next observation, including references into Food trips.
            if(hunting && !saved_hunt) {checkpoints.push_back(save_hunting(*view,data,map,output,"hunting"));saved_hunt=true;continue;}
            if(cargo && !saved_cargo) {checkpoints.push_back(save_hunting(*view,data,map,output,"loaded-hunter"));saved_cargo=true;continue;}
            if(market_cargo && !saved_market) {checkpoints.push_back(save_hunting(*view,data,map,output,"meat-to-market"));saved_market=true;continue;}
            if(house_cargo && !saved_house) {checkpoints.push_back(save_hunting(*view,data,map,output,"meat-to-house"));saved_house=true;continue;}
            if(saved_hunt && saved_cargo && saved_market && saved_house && witnesses.contains("meat_consumption") && witnesses.contains("millet_consumption"))break;
        }
        for(const char* event:{"full_staffing","living_prey","physical_hunter_movement","actual_hunt_work","physical_meat_cargo","actual_tent_return","successful_kill","tent_unload","market_arrival","house_arrival","meat_consumption","millet_consumption"})
            require(witnesses.contains(event),std::string("Missing paid actual event ")+event+": "+hunting_observation(view->world()).dump());
        require(saved_hunt && saved_cargo && saved_market && saved_house,"Missing actual movement save checkpoints");
        invariant(view->world());checkpoints.push_back(save_hunting(*view,data,map,output,"manual"));
        const auto target=oe::maps::terrain_ground({static_cast<unsigned>(plan.tent_cell.x),static_cast<unsigned>(plan.tent_cell.y)},border);
        for(int attempt=0;attempt<3 && view->camera().zoom!=1.0;++attempt)key(*view,SDLK_Z);
        require(view->camera().zoom==1.0,"Existing zoom action did not reach1x");
        Json captures=Json::array();bool inspected=false;
        for(int zoom=0;zoom<3;++zoom) {
            center(*view,target);inspected|=select_tent(*view,renderer,plan.tent,border);
            const auto path=output/("hunting-zoom-"+std::to_string(zoom)+".png");capture(*view,renderer,path);
            captures.push_back({{"path",path.filename().string()},{"zoom",view->camera().zoom}});if(zoom<2)key(*view,SDLK_Z);
        }
        require(inspected,"Paid Tent inspector not selectable");
        const auto final=view->capture_save_document();require(source.map_sha256==final.map_sha256,"Original map changed");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map=map;settings.last_save=output/"saves/manual.save.json";
        settings.profile=sim::RulesProfile::CityV16;settings.prepared_starter=false;oe::menu::write_settings(output,settings);
        const Json report={{"status","PASS"},{"scope","scripted SDL software; supplied map/model; native and human acceptance separate"},
            {"snapshot_injection",false},{"source_model_modified",false},{"rule",29},{"schema",45},{"map",map.generic_string()},
            {"map_sha256",source.map_sha256},{"model_sha256",view->world().original_economy_state()->model_source->sha256},
            {"tent_id",static_cast<unsigned>(plan.tent)},{"tent_origin",{plan.tent_cell.x,plan.tent_cell.y}},
            {"construction_spent",view->world().construction_spent_total()},
            {"source_tent_staff",view->world().workforce_required(plan.tent)},
            {"city_origin",{plan.coastal.origin.x,plan.coastal.origin.y}},{"city_layout",plan.coastal.layout},
            {"witnesses",witnesses},{"checkpoints",checkpoints},{"captures",captures},{"inspector_selected",inspected},
            {"final",hunting_observation(view->world())},{"fish_in_this_city","NOT_RUN; seven-kind chain acceptance is the dedicated paid World test"},
            {"native_acceptance","NOT_RUN"},{"human_acceptance","NOT_RUN"}};
        std::ofstream report_file(output/"report.json");report_file<<report.dump(2)<<'\n';report_file.close();
        require(bool(report_file),"Could not publish supplied-data Hunting evidence report");
        view->shutdown();view.reset();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"PASS supplied-data paid Hunting/Millet; report "<<output/"report.json"<<'\n';return 0;
    } catch(const std::exception& error) {
        if(!output.empty())std::ofstream(output/"report.json")<<Json{{"status","FAIL"},{"reason",error.what()},{"planning",planning_statistics}}.dump(2)<<'\n';
        if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();std::cerr<<error.what()<<'\n';return 1;
    }
}
