// On-demand technical paid-command replay on real original maps.
// No new rules, parser, World injection or original NPC simulation. Reports and
// saves require a fresh ignored workspace; normal App/human evidence is separate.
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"
#include "maps/LandscapeProvenance.h"
#include "maps/GreatWallMapPresentation.h"
#include "maps/SandboxPlacement.h"
#include "maps/StoredMapSession.h"
#include "renderer/StoredGraphicsRenderer.h"
#include "persistence/SandboxSave.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <map>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>
using namespace openemperor;
namespace fs=std::filesystem;namespace sim=simulation;using J=nlohmann::json;
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct Loaded {
 maps::ParsedEmperorMap map;maps::MapGeometry geometry;std::vector<std::uint8_t> legacy;
 std::shared_ptr<const sim::MapPermissions> policy;J source;std::unique_ptr<StoredGraphicsRenderer> background;
 Loaded(maps::ParsedEmperorMap m):map(std::move(m)),geometry(map.declared_map_size){}
};
Loaded load(const fs::path& data,const fs::path& relative,SDL_Renderer* renderer,
            maps::GreatWallPresentationMode mode,const std::string& expected_sha) {
 auto session=maps::load_stored_map_session(data,relative,maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8,mode,expected_sha);
 Loaded result{std::move(session.map)};
 result.background=std::make_unique<StoredGraphicsRenderer>(std::move(session.plan));result.background->initialize(renderer);
 result.legacy=maps::make_sandbox_buildable_mask(result.background->plan(),result.geometry);
 result.policy=maps::load_sandbox_map_permissions(result.map,result.background->plan(),result.geometry,result.legacy,1,expected_sha);
 const auto container=maps::EmperorContainer::open(maps::resolve_map_path(data,relative));const auto hash=maps::map_input_sha256(container);
 require(hash==expected_sha,"Original map changed during fixture preparation");
 const auto entities=maps::read_original_map_entities(container,0);J active=J::array();
 for(const auto& e:entities.records)if(e.active())active.push_back({{"id",e.serialized_original_id},{"class",maps::original_entity_class_name(e.entity_class)},{"type",e.type},{"status",e.status},{"side",e.footprint_side},{"origin",{int(e.local_x)+int(result.geometry.border),int(e.local_y)+int(result.geometry.border)}},{"record_logical",e.provenance.logical_record_offset}});
 result.source={{"map",relative.generic_string()},{"input_sha256",hash},{"size",result.map.declared_map_size},{"border",result.geometry.border},{"policy_version",result.policy->policy_version()},{"policy_fingerprint",persistence::map_permissions_fingerprint(*result.policy,hash)},{"mode",maps::great_wall_presentation_mode_name(mode)},{"manager_records",entities.records.size()},{"active_original_entities",active},{"decoded_assets",result.background->plan().decoded_assets},{"texture_uploads",result.background->plan().texture_uploads},{"logical_texture_bytes",result.background->plan().logical_texture_bytes}};
 return result;
}
J facts(const sim::World& w) {
 J result={{"tick",w.ticks()},{"buildings",w.buildings().size()},{"couriers",w.couriers().size()},{"population",w.total_population()},{"funds",w.treasury()},{"spent",w.construction_spent_total()},{"taxes",w.taxes_collected_total()},{"upkeep",w.maintenance_spent_total()},{"workers",w.workforce_supply()},{"assigned",w.workforce_used()},{"required",w.workforce_required()},{"clay_extracted",w.clay_extracted_total()},{"pottery_produced",w.pottery_completed_total()},{"food_produced",w.food_produced_total()},{"building_details",J::array()},{"courier_details",J::array()}};
 for(const auto& b:w.buildings())result["building_details"].push_back({{"id",unsigned(b.id)},{"kind",unsigned(b.kind)},{"origin",{b.cell.x,b.cell.y}},{"population",b.population},{"workers",w.workers_assigned(b.id)},{"required",w.workforce_required(b.id)},{"food",b.food_stock},{"pottery",b.pottery_stock},{"fulfilled",b.fulfilled_demand},{"missed",b.missed_demand},{"output",b.output},{"input_clay",b.input_clay}});
 for(const auto& c:w.couriers()) {
  J r={{"id",unsigned(c.id)},{"role",unsigned(c.role)},{"owner",unsigned(c.owner)},{"target",unsigned(c.target)},{"phase",sim::courier_phase_name(c.phase)},{"cargo",c.cargo},{"reserved",c.reserved},{"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},{"dispatch",sim::courier_dispatch_status_name(w.courier_dispatch_status(c.id).status)},{"path",J::array()}};
  for(const auto cell:c.path)r["path"].push_back({cell.x,cell.y});if(auto pos=w.courier_position(c.id))r["position"]={pos->x,pos->y};result["courier_details"].push_back(r);
 }
 return result;
}
std::vector<sim::Command> starter(sim::Cell o) {
 auto at=[&](int x,int y){return sim::Cell{o.x+x,o.y+y};};
 std::vector<sim::Command> result={{sim::CommandType::PlaceClaySource,at(0,0)},{sim::CommandType::PlacePottery,at(0,3)},{sim::CommandType::PlaceWarehouse,at(3,0)},{sim::CommandType::PlaceFarm,at(4,3)},{sim::CommandType::PlaceMarket,at(3,3)},{sim::CommandType::PlaceServicePost,at(5,3)}};
 for(int dx=0;dx<=14;++dx)result.push_back({sim::CommandType::PlaceRoad,at(dx,2)});
 for(auto h:std::array<sim::Cell,4>{{{6,0},{6,3},{9,0},{9,3}}})result.push_back({sim::CommandType::PlaceHousehold,at(h.x,h.y)});
 result.push_back({sim::CommandType::PlaceFireWatch,at(14,3)});return result;
}
sim::Cell find_starter(const sim::World& empty) {
 // Same existing compact paid starter's cell requirements, plus actual legal
 // horizontal transport edges. Every command is then executed and checked.
 for(int y=0;y+4<empty.height();++y)for(int x=0;x+14<empty.width();++x) {
  const sim::Cell o{x,y};bool good=true;
  for(const auto& c:starter(o)) {
   const auto object=sim::placed_object(c.type);
   const auto footprint=c.type==sim::CommandType::PlaceRoad ? std::vector<sim::Cell>{c.cell}:sim::building_footprint_cells(empty.profile(),empty.rule_version(),object.value(),c.cell);
   for(const auto cell:footprint)if(!empty.buildable(cell) || empty.object_at(cell)!=sim::Object::Empty)good=false;
  }
  for(int dx=0;dx<14;++dx)if(!empty.map_permissions()->transport_edge_allowed({x+dx,y+2},{x+dx+1,y+2}))good=false;
  if(good)return o;
 }
 throw std::runtime_error("Existing compact starter pattern not found; alternative layouts not exhausted.");
}
J execute_plan(sim::World& world,const std::vector<sim::Command>& plan) {
 J commands=J::array();
 for(const auto& c:plan){const auto validated=world.validate(c);const auto r=world.execute(c);require(validated.accepted && r.accepted && r.changed,"ordinary paid recipe command rejected");commands.push_back({{"type",unsigned(c.type)},{"cell",{c.cell.x,c.cell.y}},{"construction_spent_total",world.construction_spent_total()},{"funds",world.treasury()},{"sequence",r.sequence}});}
 return commands;
}
J cell_facts(const Loaded& map,const sim::World& world,sim::Cell cell) {
 require(map.policy->in_bounds(cell),"diagnostic cell outside storage grid"); const auto i=std::size_t(cell.y)*maps::stored_grid_width+std::size_t(cell.x);return {{"cell",{cell.x,cell.y}},{"raw_terrain",map.map.terrain_raw.values[i]},{"raw_objects",map.map.objects_raw.values[i]},{"signed_height",map.policy->cell_height(cell)},{"offmap",bool(map.map.terrain_raw.values[i]&0x80000U)},{"legacy_buildability",map.legacy[i]},{"building_allowed",map.policy->building_allowed(cell)},{"road_allowed",map.policy->road_allowed(cell)},{"protected_original",map.policy->protected_original(cell)},{"building_blocker",sim::build_blocker_name(map.policy->building_blocker(cell))},{"road_blocker",sim::build_blocker_name(map.policy->road_blocker(cell))},{"sandbox_object",unsigned(world.object_at(cell))}};
}
J diagnose(const Loaded& map,const sim::World& world,sim::Cell cell) {
 J r={{"screenshot_coordinate","UNVERIFIED; this separately identified test cell is not claimed to match Oliver's screenshot"},{"selected_storage",{cell.x,cell.y}},{"select","read-only selection; no simulation command"},{"cell",cell_facts(map,world,cell)},{"tools",J::array()},{"house_2x2",J::array()}};
 for(auto type:{sim::CommandType::RemoveRoad,sim::CommandType::PlaceRoad,sim::CommandType::PlaceHousehold}){auto v=world.validate({type,cell});r["tools"].push_back({{"type",unsigned(type)},{"accepted",v.accepted},{"reason",v.reason},{"blocker",sim::build_blocker_name(v.diagnostic.blocker)}});}
 for(auto c:sim::building_footprint_cells(world.profile(),world.rule_version(),sim::Object::Household,cell))r["house_2x2"].push_back(cell_facts(map,world,c));return r;
}

bool beneath(const fs::path& child, const fs::path& parent) {
    auto c=child.begin();
    for (auto p=parent.begin(); p!=parent.end(); ++p,++c)
        if (c==child.end() || *c!=*p) return false;
    return true;
}
void save_world(const fs::path& output, const char* label, const fs::path& data,
                const fs::path& relative, const Loaded& map, const sim::World& world) {
    const auto document=persistence::make_document(data,relative,map.legacy,world);
    const auto path=output/"saves"/(std::string(label)+".json");
    persistence::write_save(path,document,data,map.legacy);
    const auto read=persistence::read_save(path);
    const auto restored=persistence::restore_save(read,data,map.legacy,map.policy);
    require(read.source_schema_version==19 && read.world==document.world &&
        restored.snapshot()==world.snapshot() && read.map_sha256==document.map_sha256 &&
        read.buildable_sha256==document.buildable_sha256 &&
        read.map_permissions_policy_version==1 &&
        read.map_permissions_sha256==document.map_permissions_sha256,
        "Complete save/readback/restore context mismatch");
}
struct SdlSession {
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SdlSession() {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        require(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        window=SDL_CreateWindow("Private map playability fixture",1280,720,SDL_WINDOW_HIDDEN);
        if (!window) { SDL_Quit(); throw std::runtime_error(SDL_GetError()); }
        renderer=SDL_CreateRenderer(window,"software");
        if (!renderer) {
            const std::string error=SDL_GetError(); SDL_DestroyWindow(window); SDL_Quit();
            throw std::runtime_error(error);
        }
    }
    ~SdlSession() { SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit(); }
};
int main(int argc,char** argv) {
    std::optional<fs::path> failure_output;
    try {
        require(argc==4,"usage: openemperor-map-playability-fixture DATA exact-relative-map fresh-.local-root");
        const auto data=fs::canonical(argv[1]);
        const fs::path relative=argv[2];
        const auto output=fs::weakly_canonical(argv[3]);
        const auto private_root=fs::canonical(fs::current_path()/".local");
        require(beneath(output,private_root) && output!=private_root && !beneath(output,data),
                "Fixture root must be ignored and outside original inputs");
        require(!fs::exists(output) || (fs::is_directory(output) && fs::is_empty(output)),
                "Fixture root must be fresh and empty");
        failure_output=output;
        const auto checked=maps::check_map_rules({data,relative,sim::RulesProfile::CityV16,3,1});
        if (checked.status!=maps::MapRulesStatus::MapRulesChecked)
            throw std::runtime_error("Original map rules rejected: "+checked.detail);
        fs::create_directories(output/"saves");
        SdlSession sdl;
        J report;
        {
            auto map=load(data,relative,sdl.renderer,maps::GreatWallPresentationMode::Automatic,checked.input_sha256);
            sim::World empty(map.policy,sim::RulesProfile::CityV16,3);
            report={{"source",map.source},{"map_rules",checked.detail},{"empty",facts(empty)},
                {"evidence","Technical ordinary-production-command replay; not normal Application/native/human acceptance"},
                {"no_original_NPC_import",true},{"save_schema",19},{"rule",3}};
            require(empty.buildings().empty() && empty.couriers().empty() &&
                empty.total_population()==0 && empty.treasury()==1300,"Empty city authority changed");
            save_world(output,"A-empty",data,relative,map,empty);
            const auto origin=find_starter(empty);
            report["origin"]={origin.x,origin.y};
            report["test_cell_diagnosis_empty"]=diagnose(map,empty,{origin.x+1,origin.y+2});
            const auto at=[&](int x,int y){return sim::Cell{origin.x+x,origin.y+y};};
            sim::World mini(map.policy,sim::RulesProfile::CityV16,3);
            std::vector<sim::Command> commands{
                {sim::CommandType::PlaceHousehold,at(6,0)},
                {sim::CommandType::PlaceHousehold,at(9,0)}};
            for (int dx=1;dx<=9;++dx) commands.push_back({sim::CommandType::PlaceRoad,at(dx,2)});
            commands.push_back({sim::CommandType::PlaceClaySource,at(0,0)});
            commands.push_back({sim::CommandType::PlacePottery,at(0,3)});
            report["mini_commands"]=execute_plan(mini,commands);
            require(mini.construction_spent_total()==478 && mini.treasury()==822 &&
                mini.workforce_supply()==12 && mini.workforce_used()==10,"Paid mini authority changed");
            report["mini_tick0"]=facts(mini);
            save_world(output,"B-paid-courier",data,relative,map,mini);
            report["mini_phase_events"]=J::array();
            std::map<unsigned,sim::CourierPhase> phases;
            for (const auto& c:mini.couriers()) phases[unsigned(c.id)]=c.phase;
            bool clay_dispatch=false,clay_arrival=false,clay_home=false;
            for (int tick=0;tick<400;++tick) {
                mini.tick();
                for (const auto& c:mini.couriers()) if (phases[unsigned(c.id)]!=c.phase) {
                    report["mini_phase_events"].push_back({{"tick",mini.ticks()},
                        {"courier",unsigned(c.id)},{"role",unsigned(c.role)},
                        {"phase",sim::courier_phase_name(c.phase)},{"facts",facts(mini)}});
                    if (c.role==sim::CourierRole::Clay) {
                        clay_dispatch=clay_dispatch || c.phase==sim::CourierPhase::ToWarehouse;
                        clay_arrival=clay_arrival || c.phase==sim::CourierPhase::Returning;
                        clay_home=clay_home || c.phase==sim::CourierPhase::IdleAtWorkshop;
                    }
                    phases[unsigned(c.id)]=c.phase;
                }
            }
            require(clay_dispatch && clay_arrival && clay_home,"Mini has no complete real Clay trip");
            report["mini_complete_clay_trip"]=true;
            report["mini_tick400"]=facts(mini);
            sim::World supply(map.policy,sim::RulesProfile::CityV16,3);
            report["supply_commands"]=execute_plan(supply,starter(origin));
            require(supply.construction_spent_total()==1280 && supply.treasury()==20 &&
                supply.workforce_supply()==24 && supply.workforce_used()==24,
                "Paid compact starter authority changed");
            report["supply_tick0"]=facts(supply);
            save_world(output,"C-paid-supply",data,relative,map,supply);
            std::optional<Loaded> stone;
            try { stone.emplace(load(data,relative,sdl.renderer,maps::GreatWallPresentationMode::PreviewStone,checked.input_sha256)); }
            catch (const std::exception& error) {
                // Optional preview preparation cannot make an Automatic session
                // unsupported. A prepared preview must still compare exactly.
                report["stone_preparation_error"]=error.what();
            }
            std::optional<sim::World> stone_supply;
            if (stone) {
                stone_supply.emplace(stone->policy,sim::RulesProfile::CityV16,3);
                execute_plan(*stone_supply,starter(origin));
                require(map.legacy==stone->legacy &&
                    map.policy->canonical_state()==stone->policy->canonical_state() &&
                    supply.snapshot()==stone_supply->snapshot(),"Preview changed initial map/World authority");
                report["stone_source"]=stone->source;
            }
            report["automatic_stone_mask_policy_tick0_snapshot_equal"]=stone.has_value();
            std::uint64_t first_tax=0;
            for (int tick=0;tick<1600;++tick) {
                supply.tick();
                if (stone_supply) {
                    stone_supply->tick();
                    require(supply.snapshot()==stone_supply->snapshot(),"Preview continuation authority differs");
                }
                if (!first_tax && supply.taxes_collected_total()>0) first_tax=supply.ticks();
            }
            require(first_tax!=0,"Full supply has no actual tax in the observed interval");
            report["automatic_stone_all1600_full_snapshots_equal"]=stone_supply.has_value();
            report["first_tax"]=first_tax;
            report["supply_tick1600"]=facts(supply);
            report["test_cell_diagnosis_paid"]=diagnose(map,supply,at(1,2));
            save_world(output,"D-observed-supply-1600",data,relative,map,supply);
        }
        require(StoredGraphicsRenderer::live_texture_count()==0,"Fixture texture ownership leak");
        report["complete_save_readback_restore_pass"]=true;
        std::ofstream out(output/"fixture-report.json");
        out<<report.dump(2)<<'\n'; require(bool(out),"Private report write failed");
        std::cout<<report["origin"].dump()<<"; first tax "<<report["first_tax"]<<'\n';
        return 0;
    } catch (const std::exception& error) {
        if (failure_output) {
            std::error_code filesystem_error;
            fs::create_directories(*failure_output,filesystem_error);
            if (!filesystem_error) {
                std::ofstream out(*failure_output/"fixture-failure.json");
                out<<J{{"status","failed"},{"reason",error.what()},
                    {"rule",3},{"policy",1},
                    {"evidence","Technical preparation failure; no fabricated map, World or goods"}}.dump(2)<<'\n';
            }
            std::cerr<<"Private diagnostic root: "<<failure_output->string()<<'\n';
        }
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
