// Isolated technical fixture: normal paid Xia starter and ordinary Service visits.
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
}
Json facts(const sim::World& world,bool canonical) {
    Json result={{"tick",world.ticks()},{"funds",world.treasury()},{"taxes",world.taxes_collected_total()},
        {"maintenance",world.maintenance_spent_total()},{"population",world.workforce_supply()},
        {"construction_spent",world.construction_spent_total()},{"houses",Json::array()},{"service",Json::array()}};
    if (canonical) result["canonical_complete_world"]=world.canonical_state();
    for (const auto& b:world.buildings()) if (b.placed&&b.kind==sim::Object::Household)
        result["houses"].push_back({{"id",static_cast<unsigned>(b.id)},{"coverage_until",b.service_until_tick},
            {"active",world.household_service_active(b.id)},{"population",b.population},
            {"taxes_paid",b.taxes_paid_total}});
    for (const auto& c:world.couriers()) if (c.role==sim::CourierRole::Service) {
        Json trip={{"id",static_cast<unsigned>(c.id)},{"owner",static_cast<unsigned>(c.owner)},
            {"target",static_cast<unsigned>(c.target)},{"phase",static_cast<unsigned>(c.phase)},
            {"cargo",c.cargo},{"reserved",c.reserved},{"pending",c.route_pending},
            {"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},{"path",Json::array()}};
        for (const auto p:c.path) trip["path"].push_back({p.x,p.y});
        if (const auto position=world.courier_position(c.id)) trip["position"]={position->x,position->y};
        result["service"].push_back(std::move(trip));
    }
    return result;
}
}
int main(int argc,char** argv) {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;
    try {
        check(argc==3||argc==4,"Usage: openemperor-service-walker-fixture <original-data> <fresh-app-root> [resource-root]");
        const auto data=fs::canonical(argv[1]),root=fs::canonical(argv[2]);
        check(fs::is_directory(root)&&fs::is_empty(root),"Fresh empty isolated app root required.");
        const auto resources=argc==4?fs::canonical(argv[3]):oe::locate_resource_root(fs::canonical(argv[0]).parent_path());
        const auto selected=oe::detect_and_select_visual_profiles(data,resources);
        check(SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy")&&SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        check(SDL_CreateWindowAndRenderer("Technical paid Service fixture",1100,700,SDL_WINDOW_HIDDEN,&window,&renderer),SDL_GetError());
        auto stored=oe::maps::load_stored_map_session(data,"Cities/Xia.map",oe::maps::FootprintPolicy::EdgeByte4x4Preview,
            oe::maps::StoredGraphicsProfile::Slot8);
        oe::SandboxView view(std::move(stored),true,sim::RulesProfile::CityV16,3);
        fs::create_directory(root/"saves");view.configure_save(data,"Cities/Xia.map",root/"saves/service-travelling.json");
        configure(view,selected);view.initialize(window,renderer);
        check(view.world().ticks()==0&&view.world().construction_spent_total()==1280&&view.world().treasury()==20&&
            view.world().workforce_supply()==24&&view.world().workforce_used()==24,
            "Production paid1280/1300 and24/24 starter changed.");
        std::optional<sim::CourierId> service;
        for (const auto& c:view.world().couriers()) if (c.role==sim::CourierRole::Service) {
            check(!service,"Fixture unexpectedly has multiple Service couriers.");service=c.id;
            check(view.world().building(c.owner).kind==sim::Object::ServicePost&&
                view.world().building_staffed(c.owner)&&view.world().workers_assigned(c.owner)==2,
                "Ordinary Service Post is not genuinely staffed2/2.");
        }
        check(service.has_value(),"Ordinary paid starter has no Service courier.");
        const auto save=[&](const fs::path& path) {
            oe::persistence::write_save(path,view.capture_save_document(),data,view.buildable_mask());
        };
        save(root/"saves/paid-starter.json");
        Json report={{"kind","isolated technical ordinary paid Xia Service fixture"},
            {"rules","sandbox-city-v16"},{"rule_version",3},{"map_policy",1},
            {"paid_starter",facts(view.world(),false)},{"service_courier",static_cast<unsigned>(*service)},
            {"source","unchanged production paid prepared starter and ordinary ticks; no injected funds, goods, workers or coverage"},
            {"coverage_events",Json::array()}};
        const auto camera=view.camera();report["camera"]={{"offset",{camera.offset.x,camera.offset.y}},{"zoom",camera.zoom}};
        std::ofstream trace(root/"complete-world-trace.jsonl");
        trace<<facts(view.world(),true).dump()<<'\n';
        bool travelling=false,returned=false,home_after_return=false;std::uint64_t travelling_tick=0;
        for (int i=1;i<=1600;++i) {
            const auto before=view.world().snapshot();view.tick_once();const auto& c=view.world().courier(*service);
            check(c.cargo==0&&c.reserved==0,"Service courier gained cargo/reservation.");
            trace<<facts(view.world(),true).dump()<<'\n';
            for (const auto& b:view.world().buildings()) if (b.placed&&b.kind==sim::Object::Household) {
                const auto old=std::find_if(before.buildings.begin(),before.buildings.end(),[&](const auto& prior) { return prior.id==b.id; });
                check(old!=before.buildings.end(),"Existing House missing from complete snapshot.");
                if (b.service_until_tick!=old->service_until_tick) {
                    check(c.phase==sim::CourierPhase::Returning&&c.target==b.id&&
                        b.service_until_tick==view.world().ticks()+sim::Rules::service_coverage_ticks,
                        "Service coverage changed outside actual arrival.");
                    report["coverage_events"].push_back({{"tick",view.world().ticks()},
                        {"house",static_cast<unsigned>(b.id)},{"until",b.service_until_tick}});
                }
            }
            if (!travelling&&c.phase==sim::CourierPhase::ToWarehouse&&c.path_vertex>=1&&c.edge_progress>0) {
                save(root/"saves/service-travelling.json");travelling=true;travelling_tick=view.world().ticks();
                report["travelling"]=facts(view.world(),false);
            }
            if (!returned&&c.phase==sim::CourierPhase::Returning) {
                save(root/"saves/service-returning.json");returned=true;report["returning"]=facts(view.world(),false);
            }
            if (returned&&!home_after_return&&c.phase==sim::CourierPhase::IdleAtWorkshop) {
                save(root/"saves/service-home.json");home_after_return=true;report["home_after_return"]=facts(view.world(),false);
            }
            if (i==400||i==800||i==1200||i==1600) save(root/("expected-tick-"+std::to_string(i)+".json"));
        }
        check(bool(trace),"Full canonical control trace write failed.");
        check(travelling&&returned&&home_after_return&&!report["coverage_events"].empty(),
            "Ordinary paid Service dispatch/arrival/return cycle not demonstrated.");
        report["travelling_tick"]=travelling_tick;report["actual_tick1600"]=facts(view.world(),false);
        write(root/"fixture-report.json",report);
        oe::menu::Settings settings;settings.data_root=data;settings.last_map="Cities/Xia.map";
        settings.profile=sim::RulesProfile::CityV16;settings.prepared_starter=false;settings.autosave_enabled=true;
        settings.last_save=root/"saves/service-travelling.json";oe::menu::write_settings(root,settings);
        view.shutdown();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        std::cout<<"TECHNICAL PAID SERVICE:1280/1300,24/24 workers, Service Post2/2, no injected coverage. Travelling Xia save tick"
            <<travelling_tick<<"; actual House arrival and visible-return state prepared.\n";return 0;
    } catch (const std::exception& e) {
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
        std::cerr<<"Service walker fixture: "<<e.what()<<'\n';return 1;
    }
}
