// Isolated technical fixture: normal paid Xia starter and ordinary Health visits.
#include "app/MenuStorage.h"
#include "app/ResourceLocator.h"
#include "app/SandboxView.h"
#include "app/VisualSelection.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <array>
#include <openssl/sha.h>
#include <stdexcept>

namespace {
namespace fs=std::filesystem;
namespace oe=openemperor;
namespace sim=oe::simulation;
using Json=nlohmann::json;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
void write(const fs::path& path,const Json& value) {
    std::ofstream out(path);out<<value.dump(2)<<'\n';check(bool(out),"Private fixture report write failed.");
}
void configure(oe::SandboxView& view,const oe::VisualSelection& selected) {
    view.set_walker_visuals(selected.walker,selected.walker_source);
    view.set_building_visuals(selected.building,selected.building_source);
    view.set_road_visuals(selected.road,selected.road_source);
    view.set_fire_visuals(selected.fire,selected.fire_source,selected.fire_fallback_reason);
    view.set_fire_inspector_visuals(selected.fire_inspector,selected.fire_inspector_source,selected.fire_inspector_fallback_reason);
    view.set_market_walker_visuals(selected.market_walker,selected.market_walker_source,selected.market_walker_fallback_reason);
    view.set_service_walker_visuals(selected.service_walker,selected.service_walker_source,selected.service_walker_fallback_reason);
    view.set_health_walker_visuals(selected.health_walker,selected.health_walker_source,selected.health_walker_fallback_reason);
}
std::string sha(const std::string& value) {
    std::array<unsigned char,32> bytes{};
    SHA256(reinterpret_cast<const unsigned char*>(value.data()),value.size(),bytes.data());
    constexpr char hex[]="0123456789abcdef";std::string out;
    for(auto byte:bytes){out+=hex[byte>>4];out+=hex[byte&15];}return out;
}
Json facts(const sim::World& world) {
    Json result={{"tick",world.ticks()},{"funds",world.treasury()},{"taxes",world.taxes_collected_total()},
        {"maintenance",world.maintenance_spent_total()},{"workforce",world.workforce_supply()},
        {"assigned",world.workforce_used()},{"construction_spent",world.construction_spent_total()},
        {"houses",Json::array()},{"health",Json::array()},{"posts",Json::array()}};
    for(const auto& b:world.buildings())if(b.placed&&b.kind==sim::Object::Household)
        result["houses"].push_back({{"id",static_cast<unsigned>(b.id)},{"risk",b.health_risk},
            {"sick_until",b.sick_until_tick},{"protection_until",b.health_protection_until_tick},
            {"sick",world.household_sick(b.id)},{"population",b.population},{"food",b.food_stock},
            {"pottery",b.pottery_stock},{"service_until",b.service_until_tick},{"fire_until",b.fire_until_tick}});
    for(const auto& b:world.buildings())if(b.placed&&b.kind==sim::Object::HealthPost)
        result["posts"].push_back({{"id",static_cast<unsigned>(b.id)},{"workers",world.workers_assigned(b.id)},
            {"required",2},{"footprint",2},{"origin",{b.cell.x,b.cell.y}}});
    for(const auto& c:world.couriers())if(c.role==sim::CourierRole::HealthWorker){
        Json trip={{"id",static_cast<unsigned>(c.id)},{"owner",static_cast<unsigned>(c.owner)},
            {"target",static_cast<unsigned>(c.target)},{"phase",static_cast<unsigned>(c.phase)},
            {"cargo",c.cargo},{"reserved",c.reserved},{"pending",c.route_pending},
            {"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},{"path",Json::array()}};
        for(auto p:c.path)trip["path"].push_back({p.x,p.y});
        if(auto p=world.courier_position(c.id))trip["position"]={p->x,p->y};
        result["health"].push_back(std::move(trip));
    }
    return result;
}
// These bounded load-time choices create technical saves with ordinary paid
// commands; this search is never part of the live renderer/navigation engine.
sim::Cell connected_purchase(const sim::World& world,sim::CommandType type) {
    std::vector<sim::Cell> candidates;
    for(int y=0;y<world.height();++y)for(int x=0;x<world.width();++x)candidates.push_back({x,y});
    const auto seed=world.building(static_cast<sim::BuildingId>(7)).cell;
    std::sort(candidates.begin(),candidates.end(),[&](auto a,auto b){
        const int da=std::abs(a.x-seed.x-4)+std::abs(a.y-seed.y-1),db=std::abs(b.x-seed.x-4)+std::abs(b.y-seed.y-1);
        return da!=db?da<db:a.y!=b.y?a.y<b.y:a.x<b.x;
    });
    const auto kind=type==sim::CommandType::PlaceHousehold?sim::Object::Household:sim::Object::HealthPost;
    for(auto cell:candidates)if(world.validate({type,cell}).accepted)
        for(auto f:sim::building_footprint_cells(world.profile(),world.rule_version(),kind,cell))
            for(auto d:{sim::Cell{1,0},sim::Cell{-1,0},sim::Cell{0,1},sim::Cell{0,-1}}){
                const sim::Cell road{f.x+d.x,f.y+d.y};
                if(world.object_at(road)==sim::Object::Road&&world.map_permissions()->cell_height(road)==world.map_permissions()->cell_height(f))return cell;
            }
    throw std::runtime_error("Xia has no normal affordable connected House/Post candidate.");
}

}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==3||argc==4,"Usage: openemperor-health-walker-fixture <original-data> <fresh-app-root> [resource-root]");
        const auto data=fs::canonical(argv[1]),root=fs::canonical(argv[2]);
        check(fs::is_directory(root)&&fs::is_empty(root),"Fresh empty isolated app root required.");
        const auto resources=argc==4?fs::canonical(argv[3]):oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
        const auto selected=oe::detect_and_select_visual_profiles(data,resources);
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        check(SDL_CreateWindowAndRenderer("Technical paid Health fixture",1100,700,SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto stored=oe::maps::load_stored_map_session(data,"Cities/Xia.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8);
        oe::SandboxView view(std::move(stored),true,sim::RulesProfile::CityV16,3);
        fs::create_directory(root/"saves");view.configure_save(data,"Cities/Xia.map",root/"saves/health-travelling.json");
        configure(view,selected);view.initialize(window,renderer);
        check(view.world().ticks()==0&&view.world().construction_spent_total()==1280&&view.world().treasury()==20&&
            view.world().workforce_supply()==24&&view.world().workforce_used()==24,
            "Production paid1280/1300 and24/24 starter changed.");
        const auto save=[&](const fs::path& path){oe::persistence::write_save(path,view.capture_save_document(),data,view.buildable_mask());};
        Json report={{"kind","isolated technical Health scene from normally earned City16 rule3 Xia"},
            {"source","paid1280/1300 production starter; actual taxes; two paid Houses; natural dry illness; paid 2x2 Posts; ordinary ticks; no authority injections"},
            {"paid_starter",facts(view.world())},{"commands",Json::array()},{"arrival_events",Json::array()}};
        const auto camera=view.camera();report["camera"]={{"offset",{camera.offset.x,camera.offset.y}},{"zoom",camera.zoom}};
        save(root/"saves/paid-starter.json");
        // Independent negative branch: insufficient workers after a genuinely
        // paid Post purchase. It is not labelled as a travelling positive case.
        auto negative=view.world();while(negative.ticks()<1600)negative.tick();
        const auto empty_cell=connected_purchase(negative,sim::CommandType::PlaceHealthPost);
        check(negative.execute({sim::CommandType::PlaceHealthPost,empty_cell}).accepted,"Negative paid Post rejected.");
        const auto empty_post=*negative.building_owner_at(empty_cell);check(negative.workers_assigned(empty_post)==0,"Negative Post is not genuinely0/2.");
        Json negative_index=Json::array();const auto negative_record=[&](){while(negative_index.size()<negative.ticks())negative_index.push_back(nullptr);const auto canonical=negative.canonical_state();negative_index.push_back({{"tick",negative.ticks()},{"sha256",sha(canonical)},{"bytes",canonical.size()}});};negative_record();
        for(int i=0;i<30;++i){negative.tick();negative_record();for(const auto& c:negative.couriers())if(c.owner==empty_post)
            check(c.phase==sim::CourierPhase::IdleAtWorkshop&&c.cargo==0&&c.reserved==0,"Unstaffed Post invented a patrol.");}
        auto empty_doc=view.capture_save_document();empty_doc.world=negative.snapshot();
        oe::persistence::write_save(root/"saves/health-unstaffed.json",empty_doc,data,view.buildable_mask());
        report["unstaffed_negative"]=facts(negative);
        for(int i=0;i<30;++i){negative.tick();negative_record();}
        auto negative_end=view.capture_save_document();negative_end.world=negative.snapshot();
        oe::persistence::write_save(root/"expected-unstaffed-tick1660.json",negative_end,data,view.buildable_mask());write(root/"control-index-unstaffed.json",negative_index);
        Json index=Json::array();std::ofstream facts_out(root/"ordinary-health-trace.jsonl");
        std::optional<sim::CourierId> first;bool travelling=false,returned=false,home=false,prevented=false;
        const auto record=[&](){const auto canonical=view.world().canonical_state();index.push_back({{"tick",view.world().ticks()},{"sha256",sha(canonical)},{"bytes",canonical.size()}});facts_out<<facts(view.world()).dump()<<'\n';};
        const auto purchase=[&](sim::CommandType type){const auto cell=connected_purchase(view.world(),type);
            check(view.execute({type,cell}).accepted,"Normally earned House/Post purchase rejected.");
            report["commands"].push_back({{"tick",view.world().ticks()},{"type",type==sim::CommandType::PlaceHousehold?"PlaceHousehold":"PlaceHealthPost"},{"cell",{cell.x,cell.y}},{"after",facts(view.world())}});
            return *view.world().building_owner_at(cell);};
        record();
        for(int tick=1;tick<=4000;++tick){const auto before=view.world().snapshot();view.tick_once();
            if(tick==1600||tick==2000)purchase(sim::CommandType::PlaceHousehold);
            if(tick==3400){
                for(const auto& b:view.world().buildings())if(b.placed&&b.kind==sim::Object::Household&&static_cast<unsigned>(b.id)<=10)
                    check(view.world().household_sick(b.id)&&b.sick_until_tick==4600,"Old dry Houses did not become naturally sick at3400.");
                save(root/"saves/health-natural-sick.json");const auto post=purchase(sim::CommandType::PlaceHealthPost);
                check(view.world().workers_assigned(post)==2,"Paid Post lacks actual2/2 workers.");
                for(const auto& c:view.world().couriers())if(c.owner==post)first=c.id;
                check(first.has_value(),"Paid Post has no existing HealthWorker.");save(root/"saves/health-idle.json");}
            if(tick==3550){const auto post=purchase(sim::CommandType::PlaceHealthPost);check(view.world().workers_assigned(post)==2,"Second paid Post lacks actual2/2 workers.");save(root/"saves/health-two-posts.json");}
            for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::HealthWorker)check(c.cargo==0&&c.reserved==0,"HealthWorker gained physical cargo/reservation.");
            for(const auto& b:view.world().buildings())if(b.placed&&b.kind==sim::Object::Household){
                const auto old=std::find_if(before.buildings.begin(),before.buildings.end(),[&](const auto& prior){return prior.id==b.id;});
                if(old==before.buildings.end()||old->health_protection_until_tick==b.health_protection_until_tick)continue;
                bool arrival=false;for(const auto& c:view.world().couriers())if(c.role==sim::CourierRole::HealthWorker&&c.target==b.id&&c.phase==sim::CourierPhase::Returning)arrival=true;
                check(arrival&&b.health_risk==0&&b.health_protection_until_tick==view.world().ticks()+2400,"Health protection changed without actual arrival.");
                const bool cured=old->sick_until_tick>view.world().ticks();check(!cured||b.sick_until_tick==view.world().ticks(),"Actual arrival did not cure existing sickness.");
                report["arrival_events"].push_back({{"tick",tick},{"house",static_cast<unsigned>(b.id)},{"cured",cured},{"until",b.health_protection_until_tick}});
                if(!cured&&!prevented){save(root/"saves/health-prevention.json");prevented=true;}}
            if(first){const auto& c=view.world().courier(*first);
                if(!travelling&&c.phase==sim::CourierPhase::ToWarehouse&&c.path_vertex>=1&&c.edge_progress>0){save(root/"saves/health-travelling.json");travelling=true;report["travelling"]=facts(view.world());}
                if(!returned&&c.phase==sim::CourierPhase::Returning){save(root/"saves/health-returning.json");returned=true;report["returning"]=facts(view.world());}
                if(returned&&!home&&c.phase==sim::CourierPhase::IdleAtWorkshop){save(root/"saves/health-home.json");home=true;report["home"]=facts(view.world());}}
            if(tick==1600||tick==2000||tick==3400||tick==3440||tick==3480||tick==3520||tick==3550||tick==3600||tick==3800||tick==4000)save(root/("expected-tick-"+std::to_string(tick)+".json"));
            record();}
        check(travelling&&returned&&home&&prevented&&!report["arrival_events"].empty(),"Ordinary Health cure/prevention/return cycle incomplete.");
        check(bool(facts_out),"Private Health facts write failed.");write(root/"control-index.json",index);report["final"]=facts(view.world());write(root/"fixture-report.json",report);
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/Xia.map";settings.profile=sim::RulesProfile::CityV16;
        settings.prepared_starter=false;settings.autosave_enabled=true;settings.last_save=root/"saves/health-travelling.json";oe::menu::write_settings(root,settings);
        view.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"TECHNICAL HEALTH: normally earned extra Houses and paid2x2 Health Posts2/2, natural illness and actual cure/prevention/return. Load Sandbox -> Load selected save -> Space.\n";return 0;
    } catch (const std::exception& e) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        std::cerr<<"Health walker fixture: "<<e.what()<<'\n';return 1;
    }
}
