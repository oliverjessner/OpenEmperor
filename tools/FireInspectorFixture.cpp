// Local technical scene generator; ordinary paid commands and unchanged ticks.
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
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>
namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace {
void require(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);}
std::vector<sim::Cell> curve(sim::Cell o){
    std::vector<sim::Cell> cells;
    for(int x=0;x<=4;++x)cells.push_back({o.x+x,o.y+2});
    for(int x=8;x<=14;++x)cells.push_back({o.x+x,o.y+2});
    cells.push_back({o.x+4,o.y+1});cells.push_back({o.x+4,o.y});
    for(int x=5;x<=8;++x)cells.push_back({o.x+x,o.y});
    cells.push_back({o.x+8,o.y+1});
    cells.push_back({o.x+6,o.y+1});cells.push_back({o.x+6,o.y+2});
    return cells;
}
std::array<sim::Command,4> buildings(sim::Cell o){return {{
    {sim::CommandType::PlaceClaySource,{o.x,o.y+3}},
    {sim::CommandType::PlacePottery,{o.x+6,o.y+3}},
    {sim::CommandType::PlaceWarehouse,{o.x+9,o.y+3}},
    {sim::CommandType::PlaceFireWatch,{o.x+12,o.y+3}}}};}
}
int main(int argc,char** argv){
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        require(argc==3,"Usage: openemperor-fire-inspector-fixture <original-data-directory> <fresh-app-root>");
        const auto data=fs::canonical(argv[1]),root=fs::canonical(argv[2]);
        require(fs::is_directory(root)&&fs::is_empty(root),"Fixture requires an empty private app root.");
        const fs::path map="Cities/Xia.map",save=root/"saves/fire-inspector-fixture.json";
        oe::persistence::validate_save_target(save,data);
        const auto selected=oe::detect_and_select_visual_profiles(data,
            oe::locate_resource_root(fs::canonical(argv[0]).parent_path()));
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("Local Inspector presentation fixture",1100,800,
            SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto session=oe::maps::load_stored_map_session(data,map,
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);
        oe::maps::MapGeometry geometry(session.map.declared_map_size);
        const int target_x=static_cast<int>(geometry.border+session.map.declared_map_size/2)-7;
        const int target_y=static_cast<int>(geometry.border+session.map.declared_map_size/2)-2;
        oe::SandboxView view(std::move(session),false,sim::RulesProfile::CityV16,3);
        view.configure_save(data,map,save);
        view.set_walker_visuals(selected.walker,selected.walker_source);
        view.set_building_visuals(selected.building,selected.building_source);
        view.set_road_visuals(selected.road,selected.road_source);
        view.set_fire_visuals(selected.fire,selected.fire_source,selected.fire_fallback_reason);
        view.set_fire_inspector_visuals(selected.fire_inspector,selected.fire_inspector_source,
            selected.fire_inspector_fallback_reason);
        view.initialize(window,renderer);
        std::vector<sim::Cell> candidates;
        for(int y=0;y+5<view.world().height();++y)
            for(int x=0;x+15<view.world().width();++x)candidates.push_back({x,y});
        std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){
            const int da=std::abs(a.x-target_x)+std::abs(a.y-target_y);
            const int db=std::abs(b.x-target_x)+std::abs(b.y-target_y);
            return da!=db ? da<db:a.y!=b.y ? a.y<b.y:a.x<b.x;
        });
        std::optional<sim::Cell> origin;
        for(const auto cell:candidates){
            const auto proposed=buildings(cell);const auto roads=curve(cell);
            if(!view.world().validate({sim::CommandType::PlaceHousehold,cell}).accepted ||
                !std::all_of(proposed.begin(),proposed.end(),[&](const auto& command){
                    return view.world().validate(command).accepted;}) ||
                !std::all_of(roads.begin(),roads.end(),[&](auto road){
                    const auto permissions=view.world().map_permissions();
                    return view.world().validate({sim::CommandType::PlaceRoad,road}).accepted &&
                        !permissions->fixed_passage(road) && permissions->cell_height(road)==
                        permissions->cell_height(roads.front());}))continue;
            origin=cell;break;
        }
        require(origin.has_value(),"Xia has no supported compact curved-road fixture placement.");
        for(const auto& command:buildings(*origin))require(view.execute(command).accepted,
            "Ordinary paid fixture building was rejected.");
        const auto watch=*view.world().building_owner_at({origin->x+12,origin->y+3});
        require(view.world().construction_spent_total()==530 && view.world().treasury()==770 &&
            view.world().workforce_supply()==0 && view.world().workers_assigned(watch)==0,
            "Zero-House paid setup differs from unchanged rules.");
        for(int i=0;i<2000;++i){
            view.tick_once();
            require(view.world().workers_assigned(watch)==0,"Unstaffed Watch acquired invented workers.");
            for(const auto& c:view.world().couriers())if(c.owner==watch)
                require(c.phase==sim::CourierPhase::IdleAtWorkshop&&c.cargo==0,
                    "Unstaffed Watch invented a patrol.");
        }
        require(view.world().ticks()==2000&&view.world().burning_buildings()==3,
            "Unmodified fire rules did not create three natural incidents.");
        for(const auto cell:curve(*origin))require(view.execute({sim::CommandType::PlaceRoad,cell}).accepted,
            "Ordinary paid curved-road placement was rejected.");
        require(view.execute({sim::CommandType::PlaceHousehold,*origin}).accepted,
            "Ordinary paid House placement was rejected.");
        require(view.world().workers_assigned(watch)==2&&view.world().workforce_supply()>=2&&
            view.world().construction_spent_total()==652&&view.world().treasury()==508,
            "Normal House staffing or paid totals differ: workers="+std::to_string(view.world().workers_assigned(watch))+" supply="+std::to_string(view.world().workforce_supply())+" spent="+std::to_string(view.world().construction_spent_total())+" funds="+std::to_string(view.world().treasury()));
        require(view.world().fire_state_valid()&&view.world().navigation_valid()&&
            view.world().production_balance_valid()&&view.world().city_economy_valid(),
            "Fixture invariants failed.");
        fs::create_directory(root/"saves");view.save_now();
        const auto document=oe::persistence::read_save(save);
        require(document.source_schema_version==19&&document.map_permissions_policy_version==1,
            "Fixture save did not retain schema19/policy1.");
        oe::menu::Settings settings;settings.data_root=data;settings.last_map=map;
        settings.last_save=save;settings.profile=sim::RulesProfile::CityV16;
        settings.prepared_starter=false;settings.autosave_enabled=true;oe::menu::write_settings(root,settings);
        // A separate technical waiting branch uses an ordinary future-road cut.
        // The begun edge completes before the existing route waits; no state is injected.
        auto waiting=view.world();
        while(waiting.ticks()<2021)waiting.tick();
        const auto waiting_courier=std::find_if(waiting.couriers().begin(),waiting.couriers().end(),
            [&](const auto& c){return c.owner==watch;});
        require(waiting_courier!=waiting.couriers().end(),"Waiting branch has no existing Inspector.");
        const auto inspector_id=waiting_courier->id;
        const auto cut=sim::Cell{origin->x+8,origin->y+1};
        require(waiting.execute({sim::CommandType::RemoveRoad,cut}).accepted &&
            waiting.courier(inspector_id).route_pending && waiting.courier(inspector_id).edge_progress>0,
            "Ordinary future-road cut did not preserve the begun edge.");
        for(int budget=0;budget<10 && waiting.courier(inspector_id).edge_progress>0;++budget)waiting.tick();
        require(waiting.courier(inspector_id).route_pending &&
            waiting.courier(inspector_id).edge_progress==0 && waiting.burning_buildings()==3,
            "Waiting branch moved or extinguished before arrival.");
        auto waiting_document=view.capture_save_document();waiting_document.world=waiting.snapshot();
        oe::persistence::write_save(root/"saves/fire-inspector-waiting.json",waiting_document,data,
            view.buildable_mask());
        nlohmann::json waiting_expected=nlohmann::json::array();
        for(int i=0;i<30;++i){
            const auto& c=waiting.courier(inspector_id);const auto pos=waiting.courier_position(inspector_id);
            waiting_expected.push_back({{"tick",waiting.ticks()},{"id",static_cast<unsigned>(c.id)},
                {"owner",static_cast<unsigned>(c.owner)},{"target",static_cast<unsigned>(c.target)},
                {"phase",static_cast<unsigned>(c.phase)},{"position",{pos->x,pos->y}},
                {"burning",waiting.burning_buildings()},{"waiting",c.route_pending}});
            waiting.tick();
        }
        std::ofstream(root/"expected-waiting.json")<<waiting_expected.dump(2)<<'\n';
        // A single ordinary control-world copy retains its complete immutable
        // permissions. It records expectations only; the player's save stays at2000.
        auto control=view.world();nlohmann::json patrol=nlohmann::json::array();
        bool dispatched=false,arrived=false,returned=false,further=false;
        std::uint64_t dispatch_tick=0,arrival_tick=0,return_tick=0;
        sim::BuildingId target=static_cast<sim::BuildingId>(0);
        for(int i=0;i<450;++i){
            control.tick();
            for(const auto& c:control.couriers())if(c.owner==watch){
                const auto pos=control.courier_position(c.id);
                patrol.push_back({{"tick",control.ticks()},{"id",static_cast<unsigned>(c.id)},
                    {"owner",static_cast<unsigned>(c.owner)},{"target",static_cast<unsigned>(c.target)},
                    {"phase",static_cast<unsigned>(c.phase)},{"position",pos ? nlohmann::json::array({pos->x,pos->y}):nlohmann::json(nullptr)},
                    {"burning",control.burning_buildings()},{"waiting",c.route_pending}});
                if(!dispatched&&c.phase==sim::CourierPhase::ToWarehouse){
                    dispatched=true;dispatch_tick=control.ticks();target=c.target;
                    require(control.burning_buildings()==3,"Fire ended at dispatch.");
                }
                if(dispatched&&!arrived&&!control.building_on_fire(target)){
                    arrived=true;arrival_tick=control.ticks();
                    require(c.phase==sim::CourierPhase::Returning&&control.building_fire_protected(target),
                        "Natural Inspector arrival failed to extinguish/protect its target.");
                }
                if(arrived&&!returned&&c.phase==sim::CourierPhase::IdleAtWorkshop){
                    returned=true;return_tick=control.ticks();
                }
                if(returned&&c.phase==sim::CourierPhase::ToWarehouse)further=true;
            }
        }
        require(dispatched&&arrived&&returned&&further,"Fixture has no complete real Inspector patrol.");
        std::ofstream(root/"expected-patrol.json")<<patrol.dump(2)<<'\n';
        const auto inspector=view.fire_inspector_display_stats();
        nlohmann::json report={{"kind","Local technical FireInspector fixture"},
            {"created_by","ordinary paid buildings/roads/House and2000 unchanged ticks"},
            {"human_player_flow",false},{"funds_workers_goods_or_protection_grants",false},
            {"map",map.generic_string()},{"tick",view.world().ticks()},{"schema",19},{"rule",3},{"policy",1},
            {"origin",{origin->x,origin->y}},{"watch",static_cast<unsigned>(watch)},
            {"watch_workers",2},{"workforce_supply",view.world().workforce_supply()},{"roads",curve(*origin).size()},{"construction_spent",652},{"funds",508},
            {"burning_buildings",3},{"Inspector_dispatch_tick",dispatch_tick},
            {"Inspector_arrival_tick",arrival_tick},{"Inspector_return_tick",return_tick},
            {"further_ordinary_patrol",further},{"technical_waiting_save","saves/fire-inspector-waiting.json"},
            {"technical_waiting_tick",waiting_expected.front().at("tick")},{"Inspector_status",inspector.active?"Curated original clip":"Fallback"},
            {"fallback_reason",inspector.fallback_reason}};
        std::ofstream(root/"fixture-report.json")<<report.dump(2)<<'\n';std::cout<<report.dump(2)<<'\n';
        view.shutdown();SDL_DestroyRenderer(renderer);renderer=nullptr;
        SDL_DestroyWindow(window);window=nullptr;SDL_Quit();return 0;
    }catch(const std::exception& error){
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
        SDL_Quit();std::cerr<<"Inspector fixture: "<<error.what()<<'\n';return 1;
    }
}
