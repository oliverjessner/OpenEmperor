// Local technical budget scenes, paid through the unchanged production rules.
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
using Json=nlohmann::json;
namespace {
void check(bool value,const char* reason) { if (!value) throw std::runtime_error(reason); }
void write_json(const fs::path& path,const Json& value) {
    std::ofstream out(path);out<<value.dump(2)<<'\n';check(bool(out),"Cannot write private fixture report.");
}
std::vector<sim::Command> buildings(sim::Cell origin,bool support=false) {
    std::vector<sim::Command> result{{sim::CommandType::PlaceHousehold,{origin.x+6,origin.y}},
        {sim::CommandType::PlaceHousehold,{origin.x+6,origin.y+3}},
        {sim::CommandType::PlaceHousehold,{origin.x+9,origin.y}},
        {sim::CommandType::PlaceClaySource,origin},
        {sim::CommandType::PlacePottery,{origin.x,origin.y+3}},
        {sim::CommandType::PlaceWarehouse,{origin.x+3,origin.y}},
        {sim::CommandType::PlaceMarket,{origin.x+3,origin.y+3}}};
    if (support) {
        result.push_back({sim::CommandType::PlaceFireWatch,{origin.x+14,origin.y+3}});
        result.push_back({sim::CommandType::PlaceWell,{origin.x+11,origin.y+3}});
    }
    result.push_back({sim::CommandType::PlaceFarm,{origin.x+4,origin.y+3}});
    return result;
}
Json paid_command(sim::Command command) {
    return {{"type",static_cast<unsigned>(command.type)},{"cell",{command.cell.x,command.cell.y}}};
}
Json actual_facts(const sim::World& world) {
    Json result={{"tick",world.ticks()},{"funds",world.treasury()},
        {"construction_spent",world.construction_spent_total()},
        {"taxes_received",world.taxes_collected_total()},
        {"maintenance_paid",world.maintenance_spent_total()},
        {"installed_maintenance",world.current_maintenance_rate()},
        {"population_workforce",world.workforce_supply()},
        {"workers_assigned",world.workforce_used()},
        {"active_worker_demand",world.active_workforce_required()},
        {"installed_worker_demand",world.workforce_required()},
        {"roads",world.roads_placed_total()}};
    result["buildings"]=Json::array();
    for (const auto& building:world.buildings()) result["buildings"].push_back({
        {"id",static_cast<unsigned>(building.id)},{"kind",static_cast<unsigned>(building.kind)},
        {"cell",{building.cell.x,building.cell.y}},{"placed_tick",building.placed_tick},
        {"workers_assigned",world.workers_assigned(building.id)},
        {"workers_needed",world.workforce_required(building.id)}});
    return result;
}
void configure_visuals(oe::SandboxView& view,const oe::VisualSelection& selected) {
    view.set_walker_visuals(selected.walker,selected.walker_source);
    view.set_building_visuals(selected.building,selected.building_source);
    view.set_road_visuals(selected.road,selected.road_source);
    view.set_fire_visuals(selected.fire,selected.fire_source,selected.fire_fallback_reason);
    view.set_fire_inspector_visuals(selected.fire_inspector,selected.fire_inspector_source,
        selected.fire_inspector_fallback_reason);
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==3 || argc==4,"Usage: openemperor-economy-start-fixture <original-data> <fresh-app-root> [resource-root]");
        const auto data=fs::canonical(argv[1]),root=fs::canonical(argv[2]);
        check(fs::is_directory(root)&&fs::is_empty(root),"A fresh empty private app root is required.");
        const auto resources=argc==4 ? fs::canonical(argv[3]):
            oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
        const auto selected=oe::detect_and_select_visual_profiles(data,resources);
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        check(SDL_CreateWindowAndRenderer("Local startup budget control",1100,700,
            SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        const auto load=[&] { return oe::maps::load_stored_map_session(data,"Cities/Xia.map",
            oe::maps::FootprintPolicy::EdgeByte4x4Preview,oe::maps::StoredGraphicsProfile::Slot8); };
        auto stored=load();const oe::maps::MapGeometry geometry(stored.map.declared_map_size);
        oe::SandboxView view(std::move(stored),false,sim::RulesProfile::CityV16,3);
        view.configure_save(data,"Cities/Xia.map",root/"saves/critical-before.json");
        configure_visuals(view,selected);view.initialize(window,renderer);
        std::vector<sim::Cell> origins;
        for (int y=0;y+5<view.world().height();++y) for (int x=0;x+15<view.world().width();++x)
            origins.push_back({x,y});
        const int center=static_cast<int>(geometry.border+geometry.declared_size/2);
        std::sort(origins.begin(),origins.end(),[&](auto a,auto b) {
            const int da=std::abs(a.x+7-center)+std::abs(a.y+2-center);
            const int db=std::abs(b.x+7-center)+std::abs(b.y+2-center);
            return da!=db ? da<db:a.y!=b.y ? a.y<b.y:a.x<b.x;
        });
        std::optional<sim::Cell> origin;
        for (const auto candidate:origins) {
            const auto proposed=buildings(candidate,true);
            if (!std::all_of(proposed.begin(),proposed.end(),[&](const auto command) {
                    return view.world().validate(command).accepted;
                }) || !view.world().validate({sim::CommandType::PlaceHousehold,
                    {candidate.x+9,candidate.y+3}}).accepted) continue;
            bool roads_valid=true;
            for (int dx=0;dx<=14;++dx) {
                const sim::Cell cell{candidate.x+dx,candidate.y+2};
                if (!view.world().validate({sim::CommandType::PlaceRoad,cell}).accepted ||
                    view.world().fixed_passage(cell)) roads_valid=false;
            }
            if (roads_valid) { origin=candidate;break; }
        }
        check(origin.has_value(),"Xia has no supported compact budget fixture position.");
        Json commands=Json::array();
        for (const auto command:buildings(*origin)) {
            check(view.execute(command).accepted,"Ordinary paid fixture building failed.");
            commands.push_back(paid_command(command));
        }
        const auto farm=*view.world().building_owner_at({origin->x+4,origin->y+3});
        check(view.world().treasury()==310&&view.world().workforce_supply()==18&&
            view.world().workforce_required()==20&&view.world().workers_assigned(farm)==0,
            "Three-House staffing does not match the unchanged whole-building allocator.");
        std::vector<sim::Cell> roads;
        for (int dx=0;dx<=14;++dx) roads.push_back({origin->x+dx,origin->y+2});
        auto reserved_house=sim::building_footprint_cells(sim::RulesProfile::CityV16,3,
            sim::Object::Household,{origin->x+9,origin->y+3});
        for (const auto kind:std::array{sim::Object::Well,sim::Object::FireWatch}) {
            const auto cell=kind==sim::Object::Well ? sim::Cell{origin->x+11,origin->y+3}:
                sim::Cell{origin->x+14,origin->y+3};
            const auto cells=sim::building_footprint_cells(sim::RulesProfile::CityV16,3,kind,cell);
            reserved_house.insert(reserved_house.end(),cells.begin(),cells.end());
        }
        std::vector<sim::Cell> extra;
        for (int y=0;y<view.world().height();++y) for (int x=0;x<view.world().width();++x) {
            const sim::Cell cell{x,y};
            if (std::find(roads.begin(),roads.end(),cell)!=roads.end() ||
                std::find(reserved_house.begin(),reserved_house.end(),cell)!=reserved_house.end() || view.world().fixed_passage(cell) ||
                !view.world().validate({sim::CommandType::PlaceRoad,cell}).accepted) continue;
            extra.push_back(cell);
        }
        std::sort(extra.begin(),extra.end(),[&](auto a,auto b) {
            const int da=std::abs(a.x-(origin->x+7))+std::abs(a.y-(origin->y+2));
            const int db=std::abs(b.x-(origin->x+7))+std::abs(b.y-(origin->y+2));
            return da!=db ? da<db:a.y!=b.y ? a.y<b.y:a.x<b.x;
        });
        check(extra.size()>=105,"Not enough ordinary paid road cells for the technical fixture.");
        roads.insert(roads.end(),extra.begin(),extra.begin()+105);
        for (std::size_t i=0;i<102;++i) {
            const sim::Command command{sim::CommandType::PlaceRoad,roads[i]};
            const auto paid=view.execute(command);check(paid.accepted&&paid.changed,"Ordinary paid road failed.");
            commands.push_back(paid_command(command));
        }
        check(view.world().treasury()==106&&view.world().construction_spent_total()==1194&&
            view.world().current_maintenance_rate()==40&&view.world().ticks()==0,
            "Critical pre-purchase fixture totals changed.");
        fs::create_directory(root/"saves");view.save_now();
        const auto critical=view.capture_save_document();
        sim::World failed(view.world().map_permissions(),sim::RulesProfile::CityV16,3);
        Json failed_commands=Json::array();
        for (const auto command:buildings(*origin,true)) {
            const auto paid=failed.execute(command);check(paid.accepted&&paid.changed,"Failed-start paid building failed.");
            failed_commands.push_back(paid_command(command));
        }
        for (std::size_t i=0;i<72;++i) {
            const sim::Command command{sim::CommandType::PlaceRoad,roads[i]};
            const auto paid=failed.execute(command);check(paid.accepted&&paid.changed,"Failed-start paid road failed.");
            failed_commands.push_back(paid_command(command));
        }
        const auto failed_farm=*failed.building_owner_at({origin->x+4,origin->y+3});
        check(failed.treasury()==26&&failed.workers_assigned(failed_farm)==0&&
            failed.workforce_supply()==18&&failed.workforce_required()==22&&
            failed.current_maintenance_rate()==46,"Similar failed-start totals changed.");
        auto failed_document=critical;failed_document.world=failed.snapshot();
        oe::persistence::write_save(root/"saves/failed-start.json",failed_document,data,view.buildable_mask());
        auto committed=view.world();const sim::Command warned{sim::CommandType::PlaceHousehold,
            {origin->x+9,origin->y+3}};
        check(committed.execute(warned).accepted&&committed.treasury()==26&&
            committed.workforce_supply()==24,"Critical fourth House ordinary purchase changed.");
        auto committed_document=critical;committed_document.world=committed.snapshot();
        oe::persistence::write_save(root/"expected-critical-commit.json",committed_document,data,view.buildable_mask());
        const auto camera=view.camera();
        Json report={{"kind","Local technical startup budget fixture; not a reconstruction of Oliver's screenshot"},
            {"created_by","ordinary paid commands at tick0, unchanged costs/staffing, no added funds/goods/workers"},
            {"screenshot_differences","Failed fixture has18 residents, rather than screenshot16; its known paid commands include FireWatch and Well. Active demand22/full-plan24, rate46 and Funds26 are separately derived valid test facts, not a reconstruction of screenshot history."},
            {"critical_before",actual_facts(view.world())},{"similar_failed_start",actual_facts(failed)},
            {"critical_fourth_House_after",actual_facts(committed)},
            {"Service_Post_actual_cost",100},{"Service_Post_gap_in_failed_start",74},
            {"Service_Post_gap_after_critical_House",74},{"origin",{origin->x,origin->y}},
            {"planned_critical_command",paid_command(warned)},{"critical_commands",commands},
            {"failed_start_commands",failed_commands},{"camera",{{"zoom",camera.zoom},
                {"offset",{camera.offset.x,camera.offset.y}},{"border",geometry.border},
                {"world_height",view.world().map_permissions()->cell_height(*origin)},
                {"render_output",{1100,700}}}}};
        oe::SandboxView prepared(load(),true,sim::RulesProfile::CityV16,3);
        prepared.configure_save(data,"Cities/Xia.map",root/"saves/prepared-starter.json");
        configure_visuals(prepared,selected);prepared.initialize(window,renderer);
        check(prepared.world().ticks()==0&&prepared.world().treasury()==20&&
            prepared.world().construction_spent_total()==1280&&prepared.world().workforce_supply()==24&&
            prepared.world().workforce_used()==24&&prepared.world().current_maintenance_rate()==48,
            "Production prepared starter is no longer the unchanged dry paid starter.");
        prepared.save_now();report["prepared_starter_tick0"]=actual_facts(prepared.world());
        Json trace=Json::array();
        for (int i=0;i<=800;++i) {
            trace.push_back(actual_facts(prepared.world()));
            if (i<800) prepared.tick_once();
        }
        write_json(root/"prepared-income-trace.json",trace);
        oe::persistence::write_save(root/"expected-prepared-tick800.json",prepared.capture_save_document(),data,
            prepared.buildable_mask());report["prepared_starter_tick400"]=trace[400];
        report["prepared_starter_tick800"]=trace[800];
        write_json(root/"fixture-report.json",report);
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/Xia.map";
        settings.last_save=root/"saves/critical-before.json";settings.profile=sim::RulesProfile::CityV16;
        settings.prepared_starter=false;settings.autosave_enabled=true;oe::menu::write_settings(root,settings);
        prepared.shutdown();view.shutdown();SDL_DestroyRenderer(renderer);renderer=nullptr;
        SDL_DestroyWindow(window);window=nullptr;SDL_Quit();
        std::cout<<"Technical Xia fixture: Funds106 before House80; afterwards26, missing Service Post100/gap74.\n"
            <<"Similar failed-start save: Funds26, Service gap74, population18, Farm0/4, upkeep46/400t.\n"
            <<"Production prepared starter remains paid1280/1300 and24/24; actual income trace recorded.\n";
        return 0;
    } catch (const std::exception& error) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();std::cerr<<"Economy start fixture: "<<error.what()<<'\n';return 1;
    }
}
