// Local diagnostic scene generator. This is not an application cheat or save migration.
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
#include <stdexcept>
#include <vector>

namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
namespace {
void require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    try {
        require(argc==3,"Usage: openemperor-fire-visual-fixture <original-data-directory> <fresh-app-root>");
        const auto data=fs::canonical(argv[1]);
        const auto root=fs::canonical(argv[2]);
        require(fs::is_directory(root) && fs::is_empty(root),"Fixture requires an empty private app root.");
        const fs::path map="Cities/Xia.map";
        const auto save=root/"saves/fire-visual-fixture.json";
        oe::persistence::validate_save_target(save,data);
        const auto resource_root=oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
        const auto selected=oe::detect_and_select_visual_profiles(data,resource_root);
        require(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy") && SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        require(SDL_CreateWindowAndRenderer("Local fire presentation fixture",1100,800,
            SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto session=oe::maps::load_stored_map_session(data,map,
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8);
        const auto map_size=session.map.declared_map_size;
        oe::maps::MapGeometry geometry(map_size);
        oe::SandboxView view(std::move(session),false,sim::RulesProfile::CityV16,3);
        view.configure_save(data,map,save);
        view.set_walker_visuals(selected.walker,selected.walker_source);
        view.set_building_visuals(selected.building,selected.building_source);
        view.set_road_visuals(selected.road,selected.road_source);
        view.set_fire_visuals(selected.fire,selected.fire_source,selected.fire_fallback_reason);
        view.initialize(window,renderer);
        // Find a compact ordinary placement near the playable-map center. No
        // original record, map coordinate or custom mask supplies authority.
        std::vector<sim::Cell> candidates;
        const int target_x=static_cast<int>(geometry.border+map_size/2)-6;
        const int target_y=static_cast<int>(geometry.border+map_size/2)-3;
        for(int y=0;y+8<view.world().height();++y)
            for(int x=0;x+12<view.world().width();++x) candidates.push_back({x,y});
        std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b) {
            const int da=std::abs(a.x-target_x)+std::abs(a.y-target_y);
            const int db=std::abs(b.x-target_x)+std::abs(b.y-target_y);
            return da!=db ? da<db:a.y!=b.y ? a.y<b.y:a.x<b.x;
        });
        const auto commands=[](sim::Cell o) {
            return std::array<sim::Command,5>{{
                {sim::CommandType::PlaceHousehold,o},
                {sim::CommandType::PlacePottery,{o.x+4,o.y+1}},
                {sim::CommandType::PlaceWarehouse,{o.x+8,o.y+2}},
                {sim::CommandType::PlaceServicePost,{o.x+4,o.y+5}},
                {sim::CommandType::PlaceFireWatch,{o.x+10,o.y+6}}}};
        };
        std::optional<sim::Cell> origin;
        for(const auto cell:candidates) {
            const auto proposed=commands(cell);
            if(std::all_of(proposed.begin(),proposed.end(),[&](const auto& command) {
                return view.world().validate(command).accepted;
            })) { origin=cell;break; }
        }
        require(origin.has_value(),"Xia has no supported compact fixture placement.");
        for(const auto& command:commands(*origin))
            require(view.execute(command).accepted,"Ordinary paid fixture placement was rejected.");
        const auto spent=view.world().construction_spent_total();
        require(spent==590 && view.world().treasury()==710 && view.world().ticks()==0,
            "Fixture paid placement totals differ from unchanged City-v16 rules.");
        // Disconnected buildings receive no invented visit or coverage. The
        // unmodified placement-relative risk reaches threshold at tick 2000.
        for(int i=0;i<2000;++i)view.tick_once();
        require(view.world().burning_buildings()==4 && view.world().ticks()==2000,
            "Natural fire rules did not create the intended four-building fixture.");
        require(view.world().fire_state_valid() && view.world().navigation_valid() &&
            view.world().production_balance_valid() && view.world().city_economy_valid(),
            "Fixture World invariants failed.");
        fs::create_directory(root/"saves");view.save_now();
        oe::menu::Settings settings;settings.data_root=data;settings.last_map=map;
        settings.last_save=save;settings.profile=sim::RulesProfile::CityV16;
        settings.prepared_starter=false;settings.autosave_enabled=true;
        oe::menu::write_settings(root,settings);
        const auto fire=view.fire_display_stats();
        const auto document=oe::persistence::read_save(save);
        require(document.source_schema_version==19 && document.map_permissions_policy_version==1,
            "Fixture save did not retain City-v16 rule-3 schema 19/policy 1.");
        nlohmann::json report={{"kind","Local technical fire-presentation fixture"},
            {"created_by","ordinary paid commands and 2000 unchanged World ticks"},
            {"human_player_flow",false},{"funds_goods_coverage_or_fire_field_grants",false},
            {"map",map.generic_string()},{"tick",view.world().ticks()},
            {"rule",view.world().rule_version()},{"policy",document.map_permissions_policy_version},
            {"schema",document.source_schema_version},{"origin",{origin->x,origin->y}},
            {"construction_spent",spent},{"funds",view.world().treasury()},
            {"burning_buildings",view.world().burning_buildings()},
            {"fire_visual",fire.animated ? "Original animated clip":"Fallback"},
            {"fallback_reason",fire.fallback_reason},{"clip",fire.clip_id},
            {"frames",fire.frames},{"texture_uploads",fire.texture_uploads}};
        std::ofstream(root/"fixture-report.json")<<report.dump(2)<<'\n';
        std::cout<<report.dump(2)<<'\n';view.shutdown();
        SDL_DestroyRenderer(renderer);renderer=nullptr;SDL_DestroyWindow(window);window=nullptr;
        SDL_Quit();return 0;
    } catch(const std::exception& exception) {
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
        SDL_Quit();std::cerr<<"Fire fixture: "<<exception.what()<<'\n';return 1;
    }
}
