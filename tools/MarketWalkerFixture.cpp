// Technical working supply fixture: the production paid starter and ordinary ticks.
#include "app/MenuStorage.h"
#include "app/ResourceLocator.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;namespace oe=openemperor;namespace sim=oe::simulation;
using Json=nlohmann::json;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const fs::path& file,const Json& value){std::ofstream out(file);out<<value.dump(2)<<'\n';check(bool(out),"Private fixture report write failed.");}
void configure(oe::SandboxView& view,const oe::VisualSelection& selected){
    view.set_walker_visuals(selected.walker,selected.walker_source);
    view.set_building_visuals(selected.building,selected.building_source);
    view.set_road_visuals(selected.road,selected.road_source);
    view.set_fire_visuals(selected.fire,selected.fire_source,selected.fire_fallback_reason);
    view.set_fire_inspector_visuals(selected.fire_inspector,selected.fire_inspector_source,selected.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(selected.market_walker,selected.market_walker_source,selected.market_walker_fallback_reason);
}
constexpr std::array roles{sim::CourierRole::MarketFoodInbound,sim::CourierRole::MarketPotteryInbound,
    sim::CourierRole::MarketFoodDistribution,sim::CourierRole::MarketPotteryDistribution};
constexpr std::array<const char*,4> names{"food-inbound","pottery-inbound","food-distribution","pottery-distribution"};
Json facts(const sim::World& world){
    Json result={{"tick",world.ticks()},{"funds",world.treasury()},{"taxes",world.taxes_collected_total()},
        {"maintenance",world.maintenance_spent_total()},{"population",world.workforce_supply()},
        {"construction_spent",world.construction_spent_total()},{"canonical_complete_world",world.canonical_state()},
        {"transports",Json::array()}};
    for(const auto& c:world.couriers())if(std::find(roles.begin(),roles.end(),c.role)!=roles.end()){
        Json transport={{"courier",static_cast<unsigned>(c.id)},{"role",static_cast<unsigned>(c.role)},
            {"owner",static_cast<unsigned>(c.owner)},{"target",static_cast<unsigned>(c.target)},
            {"phase",static_cast<unsigned>(c.phase)},{"cargo",c.cargo},{"route_pending",c.route_pending},
            {"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},{"path",Json::array()}};
        for(const auto p:c.path)transport["path"].push_back({p.x,p.y});
        if(const auto position=world.courier_position(c.id))transport["position"]={position->x,position->y};
        result["transports"].push_back(std::move(transport));
    }
    return result;
}
}
int main(int argc,char** argv){SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
try{
    check(argc==3||argc==4,"Usage: openemperor-market-walker-fixture <original-data> <fresh-app-root> [resource-root]");
    const auto data=fs::canonical(argv[1]),root=fs::canonical(argv[2]);
    check(fs::is_directory(root)&&fs::is_empty(root),"Fresh empty isolated app root required.");
    const auto resource=argc==4?fs::canonical(argv[3]):oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
    const auto selected=oe::detect_and_select_visual_profiles(data,resource);
    check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
    check(SDL_CreateWindowAndRenderer("Technical paid supply fixture",1100,700,SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
    auto stored=oe::maps::load_stored_map_session(data,"Cities/Xia.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
        oe::maps::StoredGraphicsProfile::Slot8);
    const oe::maps::MapGeometry geometry(stored.map.declared_map_size);
    oe::SandboxView view(std::move(stored),true,sim::RulesProfile::CityV16,3);
    fs::create_directory(root/"saves");view.configure_save(data,"Cities/Xia.map",root/"saves/working-supply.json");
    configure(view,selected);view.initialize(window,renderer);
    check(view.world().ticks()==0&&view.world().construction_spent_total()==1280&&view.world().treasury()==20&&
        view.world().workforce_supply()==24&&view.world().workforce_used()==24,
        "Production paid1280/1300 and24/24 starter changed.");
    const auto save=[&](const fs::path& path){oe::persistence::write_save(path,view.capture_save_document(),data,view.buildable_mask());};
    save(root/"saves/paid-starter.json");
    Json report={{"kind","isolated technical ordinary paid Xia supply fixture"},{"rules","sandbox-city-v16"},
        {"rule_version",3},{"map_policy",1},{"paid_starter",facts(view.world())},
        {"source","unchanged normal production prepared starter; no added funds, stock or workforce"}};
    const auto camera=view.camera();report["camera"]={{"offset",{camera.offset.x,camera.offset.y}},
        {"zoom",camera.zoom},{"border",geometry.border},{"world_height",view.world().map_permissions()->cell_height({108,115})}};
    Json trace=Json::array({facts(view.world())});std::array<bool,4> outbound{},returned{};
    bool working=false;std::uint64_t working_tick=0;
    for(int i=1;i<=1600;++i){view.tick_once();trace.push_back(facts(view.world()));bool food=false,pottery=false;
        for(const auto& c:view.world().couriers())for(std::size_t r=0;r<roles.size();++r)if(c.role==roles[r]){
            const auto owner=view.world().building(c.owner).kind;
            check(owner==(r==0?sim::Object::Farm:r==1?sim::Object::Warehouse:sim::Object::Market),"Actual transport owner differs.");
            if(c.phase==sim::CourierPhase::ToWarehouse&&c.cargo>0){
                check(view.world().building(c.target).kind==(r<2?sim::Object::Market:sim::Object::Household),
                    "Actual transport target differs.");
                if(!outbound[r]){save(root/"saves"/(std::string(names[r])+".json"));report[names[r]]=facts(view.world());outbound[r]=true;}
                if(r==2)food=true;if(r==3)pottery=true;
            }
            if(c.phase==sim::CourierPhase::Returning){check(c.cargo==0,"Return retained invented cargo.");
                if(!returned[r]){save(root/"saves"/(std::string(names[r])+"-empty-return.json"));returned[r]=true;}}
        }
        if(!working&&food&&pottery&&view.world().taxes_collected_total()>0){
            save(root/"saves/working-supply.json");working=true;working_tick=view.world().ticks();report["working_supply"]=facts(view.world());}
        if(i==400||i==800||i==1200||i==1600)save(root/("expected-tick-"+std::to_string(i)+".json"));
    }
    check(working&&std::all_of(outbound.begin(),outbound.end(),[](bool x){return x;})&&
        std::all_of(returned.begin(),returned.end(),[](bool x){return x;}),"Paid ordinary supply did not demonstrate four roles/emptyreturns/simultaneous distributors and actualtax.");
    write(root/"complete-world-trace.json",trace);report["working_tick"]=working_tick;
    report["actual_tick1600"]=facts(view.world());write(root/"fixture-report.json",report);
    oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/Xia.map";
    settings.profile=sim::RulesProfile::CityV16;settings.prepared_starter=false;settings.autosave_enabled=true;
    settings.last_save=root/"saves/working-supply.json";oe::menu::write_settings(root,settings);
    view.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
    std::cout<<"TECHNICAL PAID SUPPLY:1280/1300,24/24, no free goods/workers. Working Xia save tick"<<working_tick
        <<"; real Food/Pottery inbounds, simultaneous distributors and emptyreturns observed.\n";return 0;
}catch(const std::exception& e){if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();
    std::cerr<<"Market walker fixture: "<<e.what()<<'\n';return 1;}}
