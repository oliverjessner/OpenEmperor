#include "simulation/original/OriginalScheduler.h"
#include "persistence/SandboxSave.h"
#include "core/PerformanceDiagnostics.h"
#include "simulation/OriginalGoodsLedger.h"

#include "maps/MapCatalog.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include "maps/MapRulesCheck.h"
#include "maps/SandboxPlacement.h"
#include "maps/StoredGraphicsPlan.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace openemperor::persistence {
namespace {
using json=nlohmann::json;
namespace fs=std::filesystem;
constexpr std::uintmax_t max_save_bytes=8U*1024U*1024U;
// Only the local Rule10 projection may use the active entity bounds while
// reusing frozen infrastructure syntax. No persisted field selects this mode.
enum class SnapshotSyntax { Frozen, OriginalProduction };

void require(bool yes,const char* why) { if (!yes) throw std::runtime_error(why); }
const json& field(const json& j,const char* name) {
    require(j.is_object() && j.contains(name),"missing required save field");
    return j.at(name);
}
std::uint64_t number(const json& j,std::uint64_t max=UINT64_MAX) {
    require(j.is_number_integer() && !j.is_number_float(),"save integer has wrong type");
    if (j.is_number_unsigned()) {
        const auto n=j.get<std::uint64_t>();
        require(n<=max,"save integer exceeds range");
        return n;
    }
    const auto n=j.get<std::int64_t>();
    require(n>=0 && static_cast<std::uint64_t>(n)<=max,"save integer exceeds range");
    return static_cast<std::uint64_t>(n);
}
std::int64_t signed_number(const json& j) {
    require(j.is_number_integer() && !j.is_number_float(),"save signed integer has wrong type");
    if (j.is_number_unsigned()) {
        const auto n=j.get<std::uint64_t>();
        require(n<=static_cast<std::uint64_t>(INT64_MAX),"save signed integer exceeds range");
        return static_cast<std::int64_t>(n);
    }
    return j.get<std::int64_t>();
}
int small(const json& j,int max=INT32_MAX) { return static_cast<int>(number(j,static_cast<std::uint64_t>(max))); }
std::string str(const json& j) { require(j.is_string(),"save string has wrong type"); return j.get<std::string>(); }
bool boolean(const json& j) { require(j.is_boolean(),"save bool has wrong type"); return j.get<bool>(); }
json cell_json(simulation::Cell c) { return json::array({c.x,c.y}); }
simulation::Cell cell(const json& j) {
    require(j.is_array() && j.size()==2,"invalid saved cell");
    return {small(j[0],511),small(j[1],511)};
}
json cells_json(const std::vector<simulation::Cell>& values) {
    json out=json::array(); for (auto c:values) out.push_back(cell_json(c)); return out;
}
std::vector<simulation::Cell> cells(const json& j,std::size_t limit) {
    require(j.is_array() && j.size()<=limit,"saved cell array exceeds grid");
    std::vector<simulation::Cell> out; out.reserve(j.size());
    for (const auto& value:j) out.push_back(cell(value));
    return out;
}
json optional_cell(std::optional<simulation::Cell> c) { return c ? cell_json(*c):json(nullptr); }
std::optional<simulation::Cell> optional_cell_read(const json& j) {
    return j.is_null() ? std::nullopt:std::optional<simulation::Cell>(cell(j));
}
std::string digest(const unsigned char* bytes,std::size_t size) {
    auto* raw=EVP_MD_CTX_new(); require(raw!=nullptr,"SHA-256 context allocation failed");
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(raw,EVP_MD_CTX_free);
    require(EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1 &&
            EVP_DigestUpdate(ctx.get(),bytes,size)==1,"SHA-256 calculation failed");
    std::array<unsigned char,EVP_MAX_MD_SIZE> output{}; unsigned length=0;
    require(EVP_DigestFinal_ex(ctx.get(),output.data(),&length)==1 && length==32,"SHA-256 finalization failed");
    constexpr char hex[]="0123456789abcdef";
    std::string result; result.reserve(64);
    for (unsigned i=0;i<length;++i) { result+=hex[output[i]>>4]; result+=hex[output[i]&15]; }
    return result;
}
std::string hash_file(const fs::path& path) {
    std::ifstream input(path,std::ios::binary); require(input.good(),"cannot open original map for SHA-256");
    auto* raw=EVP_MD_CTX_new(); require(raw!=nullptr,"SHA-256 context allocation failed");
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx(raw,EVP_MD_CTX_free);
    require(EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)==1,"SHA-256 initialization failed");
    std::array<char,65536> buffer{};
    while (input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count=input.gcount();
        if (count>0) require(EVP_DigestUpdate(ctx.get(),buffer.data(),static_cast<std::size_t>(count))==1,
                             "SHA-256 calculation failed");
    }
    require(input.eof() && !input.bad(),"original map read failed");
    std::array<unsigned char,EVP_MAX_MD_SIZE> output{}; unsigned length=0;
    require(EVP_DigestFinal_ex(ctx.get(),output.data(),&length)==1 && length==32,"SHA-256 finalization failed");
    constexpr char hex[]="0123456789abcdef"; std::string result; result.reserve(64);
    for (unsigned i=0;i<length;++i) { result+=hex[output[i]>>4]; result+=hex[output[i]&15]; }
    return result;
}
bool valid_hash(const std::string& hash) {
    if (hash.size()!=64) return false;
    for (char c:hash) if (!((c>='0' && c<='9') || (c>='a' && c<='f'))) return false;
    return true;
}
bool below(const fs::path& path,const fs::path& root) {
    auto a=path.begin(),b=root.begin();
    for (;b!=root.end();++a,++b) if (a==path.end() || *a!=*b) return false;
    return true;
}
void safe_relative(const fs::path& path) {
    require(!path.empty() && !path.is_absolute() && !path.has_root_name() &&
            !path.has_root_directory(),"map reference must be relative");
    for (const auto& part:path)
        require(part!=".." && part!="." && part!="", "unsafe map reference path");
}
json snapshot_json(const simulation::WorldSnapshot& s) {
    const bool industry=simulation::industry_profile(s.profile);
    const bool city=simulation::city_profile(s.profile);
    const bool food=simulation::food_profile(s.profile);
    const bool service=simulation::service_profile(s.profile);
    const bool population=simulation::population_profile(s.profile);
    const bool scalable=simulation::scalable_profile(s.profile);
    const bool operation_controls=simulation::fire_profile(s.profile) ||
        (s.profile==simulation::RulesProfile::CityV11 && s.rule_version>=3);
    json bs=json::array(),cs=json::array();
    const std::size_t building_count=scalable ? s.buildings.size():(service ? simulation::max_buildings:
                              food ? simulation::city_v7_max_buildings:
                              industry ? simulation::legacy_max_buildings:7U);
    for (std::size_t i=0;i<building_count;++i) {
        const auto& b=s.buildings[i];
        json entry={{"id",static_cast<std::uint32_t>(b.id)},
        {"kind",static_cast<int>(b.kind)},{"cell",cell_json(b.cell)},{"placed",b.placed},
        {"input_clay",b.input_clay},{"output",b.output},{"pottery_stock",b.pottery_stock},
        {"reserved_incoming",b.reserved_incoming},{"progress",b.progress},
        {"active_recipe_clay",b.active_recipe_clay},{"recipes_completed",b.recipes_completed},
        {"placed_tick",b.placed_tick},{"demand_progress",b.demand_progress},
        {"fulfilled_demand",b.fulfilled_demand},{"missed_demand",b.missed_demand},
        {"consumed_total",b.consumed_total},{"last_demand_status",b.last_demand_status}};
        if (industry) entry["clay_extracted"]=b.clay_extracted;
        if (food) {
            entry["food_stock"]=b.food_stock;
            entry["reserved_food_incoming"]=b.reserved_food_incoming;
            entry["food_consumed_total"]=b.food_consumed_total;
            entry["food_produced"]=b.food_produced;
        }
        if (service) entry["service_until_tick"]=b.service_until_tick;
        if (population) entry["population"]=b.population;
        if (operation_controls) {
            entry["operating_enabled"]=b.operating_enabled;
            entry["workforce_priority"]=static_cast<int>(b.workforce_priority);
        }
        if (simulation::fire_profile(s.profile)) {
            entry["fire_risk"]=b.fire_risk;
            entry["fire_protection_until_tick"]=b.fire_protection_until_tick;
            entry["fire_until_tick"]=b.fire_until_tick;
        }
        if (simulation::desirability_profile(s.profile)) entry["taxes_paid_total"]=b.taxes_paid_total;
        if (simulation::health_profile(s.profile)) {
            entry["health_risk"]=b.health_risk;
            entry["health_protection_until_tick"]=b.health_protection_until_tick;
            entry["sick_until_tick"]=b.sick_until_tick;
        }
        bs.push_back(std::move(entry));
    }
    const std::size_t courier_count=scalable ? s.couriers.size():(service ? simulation::max_couriers:
                              food ? simulation::city_v7_max_couriers:
                              industry ? simulation::legacy_max_couriers:3U);
    for (std::size_t i=0;i<courier_count;++i) {
        const auto& c=s.couriers[i];
        json entry={{"id",static_cast<std::uint32_t>(c.id)},
        {"owner",static_cast<std::uint32_t>(c.owner)},{"target",static_cast<std::uint32_t>(c.target)},
        {"good",static_cast<int>(c.good)},{"enabled",c.enabled},{"phase",static_cast<int>(c.phase)},
        {"cargo",c.cargo},{"reserved",c.reserved},{"path",cells_json(c.path)},
        {"path_vertex",c.path_vertex},{"edge_progress",c.edge_progress},
        {"route_pending",c.route_pending},
        {"route_checked_revision",c.route_checked_revision ? json(*c.route_checked_revision):json(nullptr)},
        {"reroute_attempts",c.reroute_attempts}};
        if (industry) {
            entry["role"]=static_cast<int>(c.role);
            entry["last_dispatched_pottery"]=c.last_dispatched_pottery ?
                json(static_cast<unsigned>(*c.last_dispatched_pottery)):json(nullptr);
        }
        if (scalable) entry["last_dispatched_target"]=c.last_dispatched_target ?
            json(static_cast<std::uint32_t>(*c.last_dispatched_target)):json(nullptr);
        cs.push_back(std::move(entry));
    }
    json result={{"width",s.width},{"height",s.height},{"ticks",s.ticks},
        {"command_sequence",s.command_sequence},{"road_revision",s.road_revision},
        {"roads_placed_total",s.roads_placed_total},{"roads_removed_total",s.roads_removed_total},
        {"next_household_id",s.next_household_id},
        {"last_dispatched_household",s.last_dispatched_household ?
            json(static_cast<unsigned>(*s.last_dispatched_household)):json(nullptr)},
        {"roads",cells_json(s.roads)},{"workshop",optional_cell(s.workshop)},
        {"warehouse",optional_cell(s.warehouse)},{"total_produced",s.total_produced},
        {"clay_extracted_total",s.clay_extracted_total},
        {"pottery_completed_total",s.pottery_completed_total},
        {"workshop_stock",s.workshop_stock},{"production_progress",s.production_progress},
        {"courier_cargo",s.courier_cargo},{"warehouse_stock",s.warehouse_stock},
        {"phase",static_cast<int>(s.phase)},{"path",cells_json(s.path)},
        {"path_vertex",s.path_vertex},{"edge_progress",s.edge_progress},
        {"buildings",bs},{"couriers",cs}};
    if (industry) {
        result["next_production_id"]=s.next_production_id;
        result["next_courier_id"]=s.next_courier_id;
    }
    if (scalable) result["next_building_id"]=s.next_building_id;
    if (city) {
        result["treasury"]=s.treasury;
        result["taxes_collected_total"]=s.taxes_collected_total;
        result["construction_spent_total"]=s.construction_spent_total;
    }
    if (simulation::maintenance_profile(s.profile))
        result["maintenance_spent_total"]=s.maintenance_spent_total;
    if (food) {
        result["food_produced_total"]=s.food_produced_total;
        result["last_dispatched_food_household"]=s.last_dispatched_food_household ?
            json(static_cast<unsigned>(*s.last_dispatched_food_household)):json(nullptr);
    }
    if (service) {
        result["last_dispatched_service_household"]=s.last_dispatched_service_household ?
            json(static_cast<unsigned>(*s.last_dispatched_service_household)):json(nullptr);
    }
    if (simulation::fire_profile(s.profile) ||
        (s.profile==simulation::RulesProfile::CityV11 && s.rule_version==4)) {
        const auto& h=s.demolition_history;
        result["demolition_history"]={{"buildings",h.buildings},
            {"clay_extracted",h.clay_extracted},{"pottery_completed",h.pottery_completed},
            {"food_produced",h.food_produced},{"pottery_consumed",h.pottery_consumed},
            {"food_consumed",h.food_consumed},{"taxes",h.taxes},
            {"construction_spent",h.construction_spent}};
    }
    if(s.profile==simulation::RulesProfile::CityV16 && (s.rule_version==5 || s.rule_version==6)) {
        result["next_bridge_id"]=s.next_bridge_id;
        result["bridges"]=json::array();
        for(const auto& b:s.bridges)result["bridges"].push_back({{"id",b.id.value},{"axis",unsigned(b.axis)},
            {"bank_a",cell_json(b.bank_a)},{"bank_b",cell_json(b.bank_b)},{"water_cells",cells_json(b.water_cells)},
            {"deck_height",int(b.deck_height)},{"paid_cost",b.paid_cost},{"placed_tick",b.placed_tick}});
    }
    if(s.profile==simulation::RulesProfile::CityV16 && s.rule_version==6) {
        result["next_ramp_id"]=s.next_ramp_id;result["ramps"]=json::array();
        for(const auto& r:s.ramps)result["ramps"].push_back({{"id",r.id.value},
            {"endpoint_a",cell_json(r.endpoint_a)},{"endpoint_b",cell_json(r.endpoint_b)},
            {"height_a",int(r.height_a)},{"height_b",int(r.height_b)},
            {"paid_cost",r.paid_cost},{"placed_tick",r.placed_tick}});
    }
    return result;
}
simulation::WorldSnapshot parse_snapshot(const json& j,std::uint64_t schema,
                                         simulation::RulesProfile profile,SnapshotSyntax syntax) {
    using namespace simulation;
    WorldSnapshot s;
    s.profile=profile;
    s.width=small(field(j,"width"),512); s.height=small(field(j,"height"),512);
    require(s.width>0 && s.height>0,"invalid saved grid dimensions");
    const auto limit=static_cast<std::size_t>(s.width)*static_cast<std::size_t>(s.height);
    s.ticks=number(field(j,"ticks"),UINT64_MAX-1);
    s.command_sequence=number(field(j,"command_sequence"),UINT64_MAX-1);
    s.road_revision=number(field(j,"road_revision"));
    if (schema>=2) {
        s.roads_placed_total=number(field(j,"roads_placed_total"));
        s.roads_removed_total=number(field(j,"roads_removed_total"));
    }
    s.roads=cells(field(j,"roads"),limit);
    s.workshop=optional_cell_read(field(j,"workshop"));
    s.warehouse=optional_cell_read(field(j,"warehouse"));
    s.total_produced=number(field(j,"total_produced"));
    s.clay_extracted_total=number(field(j,"clay_extracted_total"));
    s.pottery_completed_total=number(field(j,"pottery_completed_total"));
    s.workshop_stock=small(field(j,"workshop_stock"));
    s.production_progress=small(field(j,"production_progress"));
    s.courier_cargo=small(field(j,"courier_cargo"));
    s.warehouse_stock=small(field(j,"warehouse_stock"));
    s.phase=static_cast<CourierPhase>(small(field(j,"phase"),2));
    s.path=cells(field(j,"path"),limit);
    s.path_vertex=static_cast<std::size_t>(number(field(j,"path_vertex"),limit));
    s.edge_progress=small(field(j,"edge_progress"),Rules::edge_ticks);
    const auto& bs=field(j,"buildings"); const auto& cs=field(j,"couriers");
    if (schema<17) for (const auto& b:bs)
        for (const auto* name:{"health_risk","health_protection_until_tick","sick_until_tick"})
            if (b.contains(name)) require(number(b.at(name),0)==0,"health state requires schema 17");
    if (schema<14) for (const auto& b:bs)
        for (const auto* name:{"fire_risk","fire_protection_until_tick","fire_until_tick"})
            if (b.contains(name)) require(number(b.at(name),0)==0,"fire state in a non-fire schema");
    if (schema<15) for (const auto& b:bs)
        require(!b.contains("taxes_paid_total"),"tax payment authority requires schema 15 or 16");
    if (schema>=13) {
        const auto& h=field(j,"demolition_history");
        auto& out=s.demolition_history;
        out.buildings=number(field(h,"buildings"),UINT32_MAX-1);
        out.clay_extracted=number(field(h,"clay_extracted"));
        out.pottery_completed=number(field(h,"pottery_completed"));
        out.food_produced=number(field(h,"food_produced"));
        out.pottery_consumed=number(field(h,"pottery_consumed"));
        out.food_consumed=number(field(h,"food_consumed"));
        out.taxes=number(field(h,"taxes"));
        out.construction_spent=number(field(h,"construction_spent"));
    }
    require(schema==21 || schema==22 || (!j.contains("bridges") && !j.contains("next_bridge_id")),"bridge authority requires schema 21");
    require(schema==22 || (!j.contains("ramps") && !j.contains("next_ramp_id")),"ramp authority requires schema22");
    if (schema>=10 && schema<=22) {
        const bool v11=schema>=11;
        const auto building_limit=syntax==SnapshotSyntax::OriginalProduction ? Rules::original_production_building_limit:building_collection_limit(profile);
        const auto courier_limit=syntax==SnapshotSyntax::OriginalProduction ? Rules::original_production_courier_limit:courier_collection_limit(profile);
        require(bs.is_array() && bs.size()<=building_limit &&
                cs.is_array() && cs.size()<=courier_limit,
                "invalid scalable City entity array length");
        std::uint32_t previous=0;
        s.buildings.reserve(bs.size());
        for (const auto& b:bs) {
            BuildingSnapshot x;
            const auto id=static_cast<std::uint32_t>(number(field(b,"id"),UINT32_MAX));
            require(id>previous,"City-v10 building IDs must be positive, unique and sorted");
            previous=id; x.id=static_cast<BuildingId>(id);
            x.kind=static_cast<Object>(small(field(b,"kind"),schema>=17 ? 12:schema==16 ? 11:schema>=14 ? 10:v11 ? 9:8));
            require(x.kind==Object::ClaySource || x.kind==Object::Pottery ||
                    x.kind==Object::Warehouse || x.kind==Object::Household ||
                    x.kind==Object::Farm || x.kind==Object::ServicePost ||
                    (v11 && x.kind==Object::Market) || (schema>=14 && x.kind==Object::FireWatch) || (schema>=16 && x.kind==Object::Well) ||
                    (schema>=17 && x.kind==Object::HealthPost),
                    "invalid scalable City building kind");
            x.cell=cell(field(b,"cell")); x.placed=boolean(field(b,"placed"));
            require(x.placed,"City-v10 saves may not contain building holes");
            x.input_clay=small(field(b,"input_clay")); x.output=small(field(b,"output"));
            x.pottery_stock=small(field(b,"pottery_stock"));
            x.reserved_incoming=small(field(b,"reserved_incoming"));
            x.progress=small(field(b,"progress"));
            x.active_recipe_clay=small(field(b,"active_recipe_clay"));
            x.recipes_completed=number(field(b,"recipes_completed"));
            x.placed_tick=number(field(b,"placed_tick"));
            x.demand_progress=small(field(b,"demand_progress"),Rules::household_demand_ticks-1);
            x.fulfilled_demand=number(field(b,"fulfilled_demand"));
            x.missed_demand=number(field(b,"missed_demand"));
            x.consumed_total=number(field(b,"consumed_total"));
            x.last_demand_status=small(field(b,"last_demand_status"),2);
            x.clay_extracted=number(field(b,"clay_extracted"));
            x.food_stock=small(field(b,"food_stock"));
            x.reserved_food_incoming=small(field(b,"reserved_food_incoming"));
            x.food_consumed_total=number(field(b,"food_consumed_total"));
            x.food_produced=number(field(b,"food_produced"));
            x.service_until_tick=number(field(b,"service_until_tick"));
            x.population=small(field(b,"population"),Rules::household_level2_capacity);
            if (schema>=12) {
                x.operating_enabled=boolean(field(b,"operating_enabled"));
                x.workforce_priority=static_cast<WorkforcePriority>(
                    small(field(b,"workforce_priority"),2));
                require(x.workforce_priority==WorkforcePriority::High ||
                        x.workforce_priority==WorkforcePriority::Normal ||
                        x.workforce_priority==WorkforcePriority::Low,
                        "invalid workforce priority");
                require(World::operation_controllable(x.kind) ||
                        (x.operating_enabled && x.workforce_priority==WorkforcePriority::Normal),
                        "operation controls are not valid for this building");
            }
            if (schema>=14) {
                x.fire_risk=small(field(b,"fire_risk"),Rules::fire_risk_threshold-1);
                x.fire_protection_until_tick=number(field(b,"fire_protection_until_tick"));
                x.fire_until_tick=number(field(b,"fire_until_tick"));
            }
            if (schema>=15) {
                x.taxes_paid_total=number(field(b,"taxes_paid_total"));
                require(x.kind==Object::Household || x.taxes_paid_total==0,
                        "non-Household tax payment authority must be zero");
            }
            if (schema>=17) {
                x.health_risk=small(field(b,"health_risk"),Rules::health_risk_threshold-1);
                x.health_protection_until_tick=number(field(b,"health_protection_until_tick"));
                x.sick_until_tick=number(field(b,"sick_until_tick"));
                require(x.kind==Object::Household ||
                        (!x.health_risk && !x.health_protection_until_tick && !x.sick_until_tick),
                        "non-Household health authority must be zero");
            }
            s.buildings.push_back(x);
        }
        previous=0; s.couriers.reserve(cs.size());
        for (const auto& c:cs) {
            CourierSnapshot x;
            const auto id=static_cast<std::uint32_t>(number(field(c,"id"),UINT32_MAX));
            require(id>previous,"City-v10 courier IDs must be positive, unique and sorted");
            previous=id; x.id=static_cast<CourierId>(id);
            x.owner=static_cast<BuildingId>(number(field(c,"owner"),UINT32_MAX));
            x.target=static_cast<BuildingId>(number(field(c,"target"),UINT32_MAX));
            x.role=static_cast<CourierRole>(small(field(c,"role"),schema>=17 ? 11:schema>=14 ? 10:v11 ? 9:5));
            require(x.role!=CourierRole::None,"invalid scalable City courier role");
            const auto& old_last=field(c,"last_dispatched_pottery");
            if (!old_last.is_null()) x.last_dispatched_pottery=static_cast<BuildingId>(
                number(old_last,UINT32_MAX));
            const auto& last=field(c,"last_dispatched_target");
            if (!last.is_null()) x.last_dispatched_target=static_cast<BuildingId>(
                number(last,UINT32_MAX));
            x.good=static_cast<Good>(small(field(c,"good"),3));
            x.enabled=boolean(field(c,"enabled"));
            x.phase=static_cast<CourierPhase>(small(field(c,"phase"),2));
            x.cargo=small(field(c,"cargo")); x.reserved=small(field(c,"reserved"));
            x.path=cells(field(c,"path"),limit);
            x.path_vertex=static_cast<std::size_t>(number(field(c,"path_vertex"),limit));
            x.edge_progress=small(field(c,"edge_progress"),Rules::edge_ticks);
            x.route_pending=boolean(field(c,"route_pending"));
            const auto& checked=field(c,"route_checked_revision");
            if (!checked.is_null()) x.route_checked_revision=number(checked);
            x.reroute_attempts=number(field(c,"reroute_attempts"));
            s.couriers.push_back(std::move(x));
        }
        s.next_building_id=static_cast<std::uint32_t>(number(field(j,"next_building_id"),UINT32_MAX));
        s.next_courier_id=static_cast<std::uint32_t>(number(field(j,"next_courier_id"),UINT32_MAX));
        const auto treasury=field(j,"treasury");
        require(treasury.is_number_integer(),"City treasury must be an integer");
        if (schema>=18) {
            s.treasury=signed_number(treasury);
            s.maintenance_spent_total=number(field(j,"maintenance_spent_total"));
        } else {
            s.treasury=treasury.get<std::int64_t>();
            require(s.treasury>=0,"City treasury cannot be negative");
        }
        s.taxes_collected_total=number(field(j,"taxes_collected_total"));
        s.construction_spent_total=number(field(j,"construction_spent_total"));
        s.food_produced_total=number(field(j,"food_produced_total"));
        if(schema==21 || schema==22) {
            s.next_bridge_id=std::uint32_t(number(field(j,"next_bridge_id"),UINT32_MAX));
            const auto& bridges=field(j,"bridges");
            require(bridges.is_array() && bridges.size()<=bridge_max_spans,"saved bridge count exceeds bound");
            for(const auto& value:bridges) {
                require(value.is_object() && value.size()==8,"saved bridge requires exactly its canonical fields");
                BridgeSpan b;b.id={std::uint32_t(number(field(value,"id"),UINT32_MAX))};
                b.axis=static_cast<BridgeAxis>(small(field(value,"axis"),1));
                b.bank_a=cell(field(value,"bank_a"));b.bank_b=cell(field(value,"bank_b"));
                b.water_cells=cells(field(value,"water_cells"),bridge_max_water_cells);
                const auto deck=signed_number(field(value,"deck_height"));
                require(deck>=-128 && deck<=127,"saved bridge deck height exceeds signed byte");b.deck_height=std::int8_t(deck);
                b.paid_cost=signed_number(field(value,"paid_cost"));b.placed_tick=number(field(value,"placed_tick"));
                s.bridges.push_back(std::move(b));
            }
        }
        if(schema==22) {
            s.next_ramp_id=std::uint32_t(number(field(j,"next_ramp_id"),UINT32_MAX));
            const auto& ramps=field(j,"ramps");
            require(ramps.is_array()&&ramps.size()<=ramp_max_links,"saved ramp count exceeds bound");
            for(const auto& value:ramps) {
                require(value.is_object()&&value.size()==7,"saved ramp requires exactly its canonical fields");
                RampLink r;r.id={std::uint32_t(number(field(value,"id"),UINT32_MAX))};
                r.endpoint_a=cell(field(value,"endpoint_a"));r.endpoint_b=cell(field(value,"endpoint_b"));
                const auto a=signed_number(field(value,"height_a")),b=signed_number(field(value,"height_b"));
                require(a>=-128&&a<=127&&b>=-128&&b<=127,"saved ramp height exceeds signed byte");
                r.height_a=std::int8_t(a);r.height_b=std::int8_t(b);
                r.paid_cost=signed_number(field(value,"paid_cost"));r.placed_tick=number(field(value,"placed_tick"));
                s.ramps.push_back(r);
            }
        }
        return s;
    }
    const std::size_t expected_buildings=schema>=8 ? max_buildings:
        schema==7 ? city_v7_max_buildings:
        schema>=5 ? legacy_max_buildings:schema==4 ? 7U:schema==3 ? 4U:3U;
    const std::size_t expected_couriers=schema>=8 ? max_couriers:
        schema==7 ? city_v7_max_couriers:
        schema>=5 ? legacy_max_couriers:schema>=3 ? 3U:2U;
    require(bs.is_array() && bs.size()==expected_buildings &&
            cs.is_array() && cs.size()==expected_couriers,
            "invalid building or courier array length");
    s.buildings.resize(simulation::max_buildings);
    s.couriers.resize(simulation::max_couriers);
    for (std::size_t i=0;i<s.buildings.size();++i)
        s.buildings[i].id=static_cast<BuildingId>(i+1);
    s.couriers[2].id=CourierId::Household;
    s.couriers[2].owner=BuildingId::Warehouse;
    s.couriers[2].target=BuildingId::Household;
    s.couriers[2].good=Good::Pottery;
    for (std::size_t i=0;i<s.couriers.size();++i) {
        auto& c=s.couriers[i];
        c.id=static_cast<CourierId>(i+1);
        c.role=i==0 ? CourierRole::Clay:i==1 ? CourierRole::Pottery:
            i==2 ? CourierRole::Household:CourierRole::None;
        if (i>=3) c.owner=static_cast<BuildingId>(i+5);
    }
    s.couriers[5].role=CourierRole::Food;
    s.couriers[5].owner=BuildingId::Farm;
    s.couriers[5].target=BuildingId::Household;
    s.couriers[5].good=Good::Food;
    s.couriers[6].role=CourierRole::Service;
    s.couriers[6].owner=BuildingId::ServicePost;
    s.couriers[6].target=BuildingId::Household;
    s.couriers[6].good=Good::Goods;
    std::array<bool,max_buildings> seen{};
    for (std::size_t i=0;i<expected_buildings;++i) {
        const auto& b=bs[i];
        const auto id=small(field(b,"id"),schema>=8 ? max_buildings:
            schema==7 ? city_v7_max_buildings:
            schema>=5 ? legacy_max_buildings:schema==4 ? 7:4);
        require(id>0 && (schema<4 || !seen[static_cast<std::size_t>(id-1)]),
                "duplicate or invalid building ID");
        const auto slot=schema>=4 ? static_cast<std::size_t>(id-1):i;
        seen[slot]=true;
        auto& x=s.buildings[slot];
        x.id=static_cast<BuildingId>(id);
        x.kind=static_cast<Object>(small(field(b,"kind"),schema>=8 ? 8:schema==7 ? 7:6));
        x.cell=cell(field(b,"cell")); x.placed=boolean(field(b,"placed"));
        x.input_clay=small(field(b,"input_clay")); x.output=small(field(b,"output"));
        x.pottery_stock=small(field(b,"pottery_stock"));
        x.reserved_incoming=small(field(b,"reserved_incoming"));
        x.progress=small(field(b,"progress"));
        x.active_recipe_clay=small(field(b,"active_recipe_clay"));
        x.recipes_completed=number(field(b,"recipes_completed"));
        if (schema>=3) {
            x.placed_tick=number(field(b,"placed_tick"));
            x.demand_progress=small(field(b,"demand_progress"),Rules::household_demand_ticks-1);
            x.fulfilled_demand=number(field(b,"fulfilled_demand"));
            x.missed_demand=number(field(b,"missed_demand"));
            x.consumed_total=number(field(b,"consumed_total"));
            x.last_demand_status=small(field(b,"last_demand_status"),2);
        }
        if (schema>=5) x.clay_extracted=number(field(b,"clay_extracted"));
        if (schema>=7) {
            x.food_stock=small(field(b,"food_stock"));
            x.reserved_food_incoming=small(field(b,"reserved_food_incoming"));
            x.food_consumed_total=number(field(b,"food_consumed_total"));
            x.food_produced=number(field(b,"food_produced"));
        }
        if (schema>=8) x.service_until_tick=number(field(b,"service_until_tick"));
        if (schema>=9) {
            x.population=small(field(b,"population"),Rules::household_level2_capacity);
            const int capacity=x.fulfilled_demand>=Rules::city_v7_level2_demands ?
                Rules::household_level2_capacity:
                x.fulfilled_demand>=Rules::city_v7_level1_demands ?
                    Rules::household_level1_capacity:Rules::household_level0_capacity;
            require(x.placed && x.kind==Object::Household ?
                    x.population>=Rules::household_min_population && x.population<=capacity:
                    x.population==0,
                    "invalid City v9 population");
        }
    }
    if (schema<5) s.buildings[0].clay_extracted=s.clay_extracted_total;
    if (schema>=4) {
        for (std::size_t i=0;i<expected_buildings;++i)
            require(seen[i],"missing building ID");
        s.next_household_id=static_cast<std::uint8_t>(small(field(j,"next_household_id"),household_id_end));
        const auto& last=field(j,"last_dispatched_household");
        if (!last.is_null()) s.last_dispatched_household=static_cast<BuildingId>(
            small(last,household_id_end-1));
        if (schema>=5) {
            s.next_production_id=static_cast<std::uint8_t>(small(field(j,"next_production_id"),10));
            s.next_courier_id=static_cast<std::uint8_t>(small(field(j,"next_courier_id"),6));
        }
    } else if (schema==3 && s.buildings[3].placed) s.next_household_id=5;
    std::array<bool,max_couriers> seen_couriers{};
    for (std::size_t i=0;i<expected_couriers;++i) {
        const auto& c=cs[i];
        const auto id=small(field(c,"id"),schema>=8 ? max_couriers:
            schema==7 ? city_v7_max_couriers:
            schema>=5 ? legacy_max_couriers:3);
        require(id>0 && (schema<5 || !seen_couriers[static_cast<std::size_t>(id-1)]),
            "duplicate or invalid courier ID");
        const auto slot=schema>=5 ? static_cast<std::size_t>(id-1):i;
        seen_couriers[slot]=true;
        auto& x=s.couriers[slot];
        x.id=static_cast<CourierId>(id);
        x.owner=static_cast<BuildingId>(small(field(c,"owner"),schema>=8 ? max_buildings:
            schema==7 ? city_v7_max_buildings:
            schema>=5 ? legacy_max_buildings:schema==4 ? 7:4));
        x.target=static_cast<BuildingId>(small(field(c,"target"),schema>=8 ? max_buildings:
            schema==7 ? city_v7_max_buildings:
            schema>=5 ? legacy_max_buildings:schema==4 ? 7:4));
        if (schema>=5) {
            x.role=static_cast<CourierRole>(small(field(c,"role"),schema>=8 ? 5:schema==7 ? 4:3));
            const auto& last=field(c,"last_dispatched_pottery");
            if (!last.is_null()) x.last_dispatched_pottery=static_cast<BuildingId>(small(last,
                schema>=8 ? max_buildings:schema==7 ? city_v7_max_buildings:
                legacy_max_buildings));
        }
        x.good=static_cast<Good>(small(field(c,"good"),schema>=7 ? 3:2));
        x.enabled=boolean(field(c,"enabled"));
        x.phase=static_cast<CourierPhase>(small(field(c,"phase"),2));
        x.cargo=small(field(c,"cargo")); x.reserved=small(field(c,"reserved"));
        x.path=cells(field(c,"path"),limit);
        x.path_vertex=static_cast<std::size_t>(number(field(c,"path_vertex"),limit));
        x.edge_progress=small(field(c,"edge_progress"),Rules::edge_ticks);
        if (schema>=2) {
            x.route_pending=boolean(field(c,"route_pending"));
            const auto& checked=field(c,"route_checked_revision");
            if (!checked.is_null()) x.route_checked_revision=number(checked);
            x.reroute_attempts=number(field(c,"reroute_attempts"));
        }
    }
    if (schema>=5) for (std::size_t i=0;i<expected_couriers;++i)
        require(seen_couriers[i],"missing courier ID");
    if (schema>=6) {
        const auto treasury=field(j,"treasury");
        require(treasury.is_number_integer(),"City treasury must be an integer");
        s.treasury=treasury.get<std::int64_t>();
        require(s.treasury>=0,"City treasury cannot be negative");
        s.taxes_collected_total=number(field(j,"taxes_collected_total"));
        s.construction_spent_total=number(field(j,"construction_spent_total"));
    }
    if (schema>=7) {
        s.food_produced_total=number(field(j,"food_produced_total"));
        const auto& last=field(j,"last_dispatched_food_household");
        if (!last.is_null()) s.last_dispatched_food_household=static_cast<BuildingId>(
            small(last,household_id_end-1));
    }
    if (schema>=8) {
        const auto& last=field(j,"last_dispatched_service_household");
        if (!last.is_null()) s.last_dispatched_service_household=static_cast<BuildingId>(
            small(last,household_id_end-1));
    }
    if (schema==1) {
        const auto placed=s.profile==RulesProfile::LogisticsV1 ?
            static_cast<unsigned>(s.workshop.has_value())+static_cast<unsigned>(s.warehouse.has_value()) :
            static_cast<unsigned>(s.buildings[0].placed)+static_cast<unsigned>(s.buildings[1].placed)+
            static_cast<unsigned>(s.buildings[2].placed);
        require(s.road_revision==s.roads.size()+placed,
                "legacy placement revision does not match saved objects");
        s.roads_placed_total=s.roads.size();
        s.roads_removed_total=0;
    }
    return s;
}
json document_json(const SaveDocument& d);
SaveDocument parse_document(const json& j,SnapshotSyntax syntax=SnapshotSyntax::Frozen);

json original_house_json(const original::HouseState& h) {
    return {{"type",static_cast<unsigned>(h.type)},{"desirability",int(h.desirability)},
        {"occupants",h.occupants},{"devolve_counter",h.devolve_counter},{"water",h.water},
        {"herbalist",h.herbalist},{"acupuncture",h.acupuncture},{"music",h.music},
        {"acrobat",h.acrobat},{"drama",h.drama},{"ancestor",h.ancestor},
        {"confucian",h.confucian},{"daoist",h.daoist},{"buddhist",h.buddhist},
        {"food_quality",h.food_quality},{"hemp",h.hemp},{"ceramics",h.ceramics},
        {"tea",h.tea},{"silk",h.silk},{"bronzeware",h.bronzeware},{"lacquerware",h.lacquerware}};
}
original::HouseState parse_original_house(const json& j) {
    original::HouseState h;
    h.type=static_cast<original::HouseType>(small(field(j,"type"),UINT16_MAX));
    require(original::is_house_type(h.type),"invalid original House type");
    const auto desirability=signed_number(field(j,"desirability"));
    require(desirability>=INT8_MIN && desirability<=INT8_MAX,"original House desirability exceeds signed byte");
    h.desirability=static_cast<std::int8_t>(desirability);
    h.occupants=static_cast<std::int16_t>(number(field(j,"occupants"),INT16_MAX));
    h.devolve_counter=static_cast<std::uint8_t>(number(field(j,"devolve_counter"),UINT8_MAX));
    h.water=boolean(field(j,"water"));
    const auto byte=[&](const char* key) { return static_cast<std::uint8_t>(number(field(j,key),UINT8_MAX)); };
    h.herbalist=byte("herbalist");h.acupuncture=byte("acupuncture");h.music=byte("music");
    h.acrobat=byte("acrobat");h.drama=byte("drama");h.ancestor=byte("ancestor");
    h.confucian=byte("confucian");h.daoist=byte("daoist");h.buddhist=byte("buddhist");
    h.food_quality=byte("food_quality");
    const auto stock=[&](const char* key) { return static_cast<std::int16_t>(number(field(j,key),INT16_MAX)); };
    h.hemp=stock("hemp");h.ceramics=stock("ceramics");h.tea=stock("tea");h.silk=stock("silk");
    h.bronzeware=stock("bronzeware");h.lacquerware=stock("lacquerware");
    return h;
}
json original_economy_json(const simulation::OriginalEconomyState& state) {
    require(state.model_source.has_value(),"schema23 requires an original model source identity");
    const auto& source=*state.model_source;
    require(source.format_version==original::model_schema_version && valid_hash(source.sha256) &&
            source.byte_count>0 && source.byte_count<=original::max_model_bytes &&
            static_cast<std::size_t>(source.difficulty)<original::difficulty_count,
            "invalid schema23 original model source identity");
    json houses=json::array();
    for (const auto& h:state.houses)
        houses.push_back({{"building_id",static_cast<std::uint32_t>(h.building_id)},
            {"house",original_house_json(h.house)},{"cycle_phase",h.cycle_phase},
            {"original_status",h.original_status},{"access_enabled",h.access_enabled}});
    return {{"model_source",{{"format_version",source.format_version},{"sha256",source.sha256},
        {"byte_count",source.byte_count},{"difficulty",static_cast<unsigned>(source.difficulty)}}},
        {"houses",std::move(houses)}};
}
simulation::OriginalEconomyState parse_original_economy(const json& j) {
    simulation::OriginalEconomyState state;
    const auto& source=field(j,"model_source");
    simulation::OriginalModelSource model;
    model.format_version=static_cast<std::uint32_t>(number(field(source,"format_version"),UINT32_MAX));
    model.sha256=str(field(source,"sha256"));
    model.byte_count=number(field(source,"byte_count"),original::max_model_bytes);
    model.difficulty=static_cast<original::Difficulty>(number(field(source,"difficulty"),original::difficulty_count-1));
    state.model_source=std::move(model);
    const auto& houses=field(j,"houses");
    require(houses.is_array() && houses.size()<=simulation::building_collection_limit(simulation::RulesProfile::CityV16),
            "schema23 original House count exceeds bound");
    std::uint32_t previous=0;
    for (const auto& value:houses) {
        simulation::OriginalHouseState h;
        const auto id=static_cast<std::uint32_t>(number(field(value,"building_id"),UINT32_MAX));
        require(id>previous,"schema23 original House IDs must be positive, unique and sorted");
        previous=id;h.building_id=static_cast<simulation::BuildingId>(id);
        h.house=parse_original_house(field(value,"house"));
        h.cycle_phase=static_cast<std::uint8_t>(number(field(value,"cycle_phase"),UINT8_MAX));
        h.original_status=static_cast<std::uint8_t>(number(field(value,"original_status"),UINT8_MAX));
        h.access_enabled=boolean(field(value,"access_enabled"));
        state.houses.push_back(h);
    }
    (void)original_economy_json(state); // Validate the source identity as well.
    return state;
}
json original_document_json(const SaveDocument& d) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalEconomyRuleVersion &&
            !d.world.original_influence && !d.world.original_goods,
            "schema23 requires unchanged City-v16 rule7 without influence sources");
    require(d.world.original_economy.has_value(),"schema23 requires separate original economy authority");
    require(d.source_schema_version==0 || d.source_schema_version==23,"rule7 requires schema23; legacy saves are not migrated");
    require(d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion &&
            valid_hash(d.map_permissions_sha256),"schema23 requires immutable policy3 identity");
    // Reuse only the canonical infrastructure representation. No World is cast
    // to another rule generation and no old save is upgraded.
    auto projected=d;
    projected.world.rule_version=6;
    projected.world.original_economy.reset();
    auto result=document_json(projected);
    result["schema_version"]=23;
    result["rules"]["version"]=simulation::kOriginalEconomyRuleVersion;
    result["world"]["original_economy"]=original_economy_json(*d.world.original_economy);
    return result;
}
bool canonical_fields(const json& value,const json& canonical) {
    if (canonical.is_number_integer()) return value.is_number_integer() && value==canonical;
    if (value.type()!=canonical.type()) return false;
    if (canonical.is_object()) {
        if (value.size()!=canonical.size()) return false;
        for (const auto& [key,expected]:canonical.items())
            if (!value.contains(key) || !canonical_fields(value.at(key),expected)) return false;
        return true;
    }
    if (canonical.is_array()) {
        if (value.size()!=canonical.size()) return false;
        for (std::size_t i=0;i<canonical.size();++i)
            if (!canonical_fields(value[i],canonical[i])) return false;
        return true;
    }
    return value==canonical;
}
json original_influence_json(const simulation::OriginalInfluenceState& state) {
    using simulation::OriginalInfluenceSourceType;
    require(state.sources.size()<=simulation::original_influence_source_limit &&
            state.removed_sources.size()<=simulation::original_influence_purchase_limit,
            "schema24 influence source collections exceed bounds");
    json sources=json::array(),removed=json::array();
    for (const auto& source:state.sources) {
        require(source.type==OriginalInfluenceSourceType::Well72,"unsupported schema24 influence type");
        sources.push_back({{"building_id",static_cast<std::uint32_t>(source.building_id)},{"type",72}});
    }
    for (const auto& source:state.removed_sources) {
        require(source.type==OriginalInfluenceSourceType::Well72,"unsupported schema24 removed influence type");
        removed.push_back({{"building_id",static_cast<std::uint32_t>(source.building_id)},
            {"type",72},{"placed_tick",source.placed_tick},{"removed_tick",source.removed_tick}});
    }
    return {{"sources",std::move(sources)},{"removed_sources",std::move(removed)}};
}
simulation::OriginalInfluenceState parse_original_influence(const json& j) {
    using namespace simulation;
    OriginalInfluenceState state;
    const auto& sources=field(j,"sources");const auto& removed=field(j,"removed_sources");
    require(sources.is_array() && sources.size()<=original_influence_source_limit &&
            removed.is_array() && removed.size()<=original_influence_purchase_limit,
            "schema24 influence source collections exceed bounds");
    std::uint32_t previous=0;
    for (const auto& source:sources) {
        const auto id=static_cast<std::uint32_t>(number(field(source,"building_id"),UINT32_MAX));
        require(id>previous && number(field(source,"type"),72)==72,
                "schema24 influence IDs must be sorted positive IDs of type72");
        previous=id;
        state.sources.push_back({static_cast<BuildingId>(id),OriginalInfluenceSourceType::Well72});
    }
    previous=0;
    for (const auto& source:removed) {
        const auto id=static_cast<std::uint32_t>(number(field(source,"building_id"),UINT32_MAX));
        require(id>previous && number(field(source,"type"),72)==72,
                "schema24 removed influence IDs must be sorted positive IDs of type72");
        previous=id;
        state.removed_sources.push_back({static_cast<BuildingId>(id),OriginalInfluenceSourceType::Well72,
            number(field(source,"placed_tick")),number(field(source,"removed_tick"))});
    }
    return state;
}
json influence_document_json(const SaveDocument& d) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalInfluenceRuleVersion && !d.world.original_goods &&
            d.world.original_influence && d.world.original_economy,
            "schema24 requires separate City-v16 rule8 influence authority");
    require(d.source_schema_version==0 || d.source_schema_version==24,
            "rule8 requires schema24; existing saves are not migrated");
    require(d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion &&
            valid_hash(d.map_permissions_sha256),"schema24 requires immutable policy3 identity");
    simulation::World::validate_original_economy_snapshot(d.world);
    simulation::World::validate_original_influence_snapshot(d.world);
    // Only a local DTO/JSON copy reuses the old canonical infrastructure fields.
    // A live World is never cast to another rule or restored through that rule.
    auto projected=d;
    projected.world.rule_version=6;
    projected.world.original_economy.reset();projected.world.original_influence.reset();
    for (auto& building:projected.world.buildings)
        if (building.kind==simulation::Object::OriginalInfluenceSource) building.kind=simulation::Object::Well;
    auto result=document_json(projected);
    result["schema_version"]=24;result["rules"]["version"]=simulation::kOriginalInfluenceRuleVersion;
    for (std::size_t i=0;i<d.world.buildings.size();++i)
        result["world"]["buildings"][i]["kind"]=static_cast<int>(d.world.buildings[i].kind);
    result["world"]["original_economy"]=original_economy_json(*d.world.original_economy);
    result["world"]["original_influence"]=original_influence_json(*d.world.original_influence);
    return result;
}
SaveDocument parse_influence_document(const json& j) {
    require(str(field(field(j,"rules"),"id"))==simulation::city_v16_profile_name &&
            number(field(field(j,"rules"),"version"))==simulation::kOriginalInfluenceRuleVersion,
            "schema24 requires City-v16 rule8");
    auto projected=j;
    projected["schema_version"]=22;projected["rules"]["version"]=6;
    projected["world"].erase("original_economy");projected["world"].erase("original_influence");
    auto& buildings=projected["world"]["buildings"];
    require(buildings.is_array() && buildings.size()<=simulation::building_collection_limit(simulation::RulesProfile::CityV16),
            "schema24 building collection exceeds bound");
    for (auto& building:buildings) {
        const auto kind=number(field(building,"kind"),static_cast<unsigned>(simulation::Object::OriginalInfluenceSource));
        if (kind==static_cast<unsigned>(simulation::Object::OriginalInfluenceSource))
            building["kind"]=static_cast<int>(simulation::Object::Well);
    }
    auto d=parse_document(projected);
    d.source_schema_version=24;d.world.rule_version=simulation::kOriginalInfluenceRuleVersion;
    for (std::size_t i=0;i<d.world.buildings.size();++i)
        d.world.buildings[i].kind=static_cast<simulation::Object>(number(field(field(field(j,"world"),"buildings")[i],"kind")));
    d.world.original_economy=parse_original_economy(field(field(j,"world"),"original_economy"));
    d.world.original_influence=parse_original_influence(field(field(j,"world"),"original_influence"));
    simulation::World::validate_original_economy_snapshot(d.world);
    simulation::World::validate_original_influence_snapshot(d.world);
    require(canonical_fields(j,influence_document_json(d)),"schema24 contains unknown or noncanonical save fields");
    return d;
}
json original_goods_json(const simulation::OriginalGoodsState& state) {
    require(state.stocks.empty() && state.cargo.empty() && state.reservations.empty(),
            "schema25 goods containers, cargo and reservations are unsupported");
    json totals=json::array();
    for (std::size_t i=0;i<state.totals.size();++i) {
        const auto& total=state.totals[i];
        require(!total.produced && !total.imported && !total.consumed && !total.demolished,
                "schema25 positive original goods history is unsupported");
        totals.push_back({{"good",i},{"produced",total.produced},{"imported",total.imported},
            {"consumed",total.consumed},{"demolished",total.demolished}});
    }
    return {{"totals",std::move(totals)},{"stocks",json::array()},
        {"cargo",json::array()},{"reservations",json::array()}};
}
simulation::OriginalGoodsState parse_original_goods(const json& j) {
    simulation::OriginalGoodsState state;
    const auto& totals=field(j,"totals");
    require(totals.is_array() && totals.size()==state.totals.size(),
            "schema25 requires exactly six original goods accounts");
    for (std::size_t i=0;i<state.totals.size();++i) {
        const auto& value=totals[i];
        require(number(field(value,"good"),state.totals.size()-1)==i,
                "schema25 original goods identities must be complete, unique and sorted");
        state.totals[i]={number(field(value,"produced")),number(field(value,"imported")),
            number(field(value,"consumed")),number(field(value,"demolished"))};
    }
    for (const char* key:{"stocks","cargo","reservations"}) {
        const auto& values=field(j,key);
        require(values.is_array() && values.empty(),
                "schema25 goods containers, cargo and reservations are unsupported");
    }
    (void)original_goods_json(state);
    return state;
}
json goods_document_json(const SaveDocument& d) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalGoodsRuleVersion &&
            d.world.original_goods && d.world.original_economy && d.world.original_influence,
            "schema25 requires separate City-v16 rule9 goods authority");
    require(d.source_schema_version==0 || d.source_schema_version==25,
            "rule9 requires schema25; existing saves are not migrated");
    simulation::World::validate_original_goods_snapshot(d.world);
    // Only a local DTO copy reuses Rule8's unchanged canonical infrastructure.
    // A live World is never cast or restored through an older rule generation.
    auto projected=d;
    projected.world.rule_version=simulation::kOriginalInfluenceRuleVersion;
    projected.source_schema_version=24;
    projected.world.original_goods.reset();
    auto result=influence_document_json(projected);
    result["schema_version"]=25;result["rules"]["version"]=d.world.rule_version;
    result["world"]["original_goods"]=original_goods_json(*d.world.original_goods);
    return result;
}
SaveDocument parse_goods_document(const json& j) {
    require(str(field(field(j,"rules"),"id"))==simulation::city_v16_profile_name &&
            number(field(field(j,"rules"),"version"))==simulation::kOriginalGoodsRuleVersion,
            "schema25 requires City-v16 rule9");
    auto projected=j;
    projected["schema_version"]=24;projected["rules"]["version"]=simulation::kOriginalInfluenceRuleVersion;
    projected["world"].erase("original_goods");
    auto d=parse_influence_document(projected);
    d.source_schema_version=25;d.world.rule_version=simulation::kOriginalGoodsRuleVersion;
    d.world.original_goods=parse_original_goods(field(field(j,"world"),"original_goods"));
    simulation::World::validate_original_goods_snapshot(d.world);
    require(canonical_fields(j,goods_document_json(d)),"schema25 contains unknown or noncanonical save fields");
    return d;
}
json production_goods_json(const simulation::OriginalGoodsState& state) {
    require(state.stocks.size()<=simulation::original_goods_owner_limit &&
            state.cargo.size()<=simulation::original_goods_courier_limit &&
            state.reservations.size()<=simulation::original_goods_reservation_limit,
            "schema26 original goods collection exceeds its bounded context");
    json totals=json::array(),stocks=json::array(),cargo=json::array(),reservations=json::array();
    for (std::size_t i=0;i<state.totals.size();++i) {
        const auto& total=state.totals[i];
        totals.push_back({{"good",i},{"produced",total.produced},{"imported",total.imported},
            {"consumed",total.consumed},{"demolished",total.demolished}});
    }
    for (const auto& stock:state.stocks)
        stocks.push_back({{"building_id",static_cast<std::uint32_t>(stock.building_id)},
            {"amounts",stock.amounts}});
    const auto transfer=[](simulation::OriginalGoodsTransferKey key) {
        return json{{"courier_id",static_cast<std::uint32_t>(key.courier_id)},{"sequence",key.sequence}};
    };
    for (const auto& value:state.cargo)
        cargo.push_back({{"transfer",transfer(value.transfer)},{"good",static_cast<unsigned>(value.good)},
            {"source",static_cast<std::uint32_t>(value.source)},{"target",static_cast<std::uint32_t>(value.target)},
            {"amount",value.amount}});
    for (const auto& value:state.reservations)
        reservations.push_back({{"transfer",transfer(value.transfer)},{"good",static_cast<unsigned>(value.good)},
            {"source",static_cast<std::uint32_t>(value.source)},{"target",static_cast<std::uint32_t>(value.target)},
            {"phase",static_cast<unsigned>(value.phase)},{"reserved",value.reserved},{"loaded",value.loaded},
            {"delivered",value.delivered},{"returned",value.returned}});
    return {{"totals",std::move(totals)},{"stocks",std::move(stocks)},
        {"cargo",std::move(cargo)},{"reservations",std::move(reservations)}};
}
simulation::OriginalGoodsState parse_production_goods(const json& j) {
    simulation::OriginalGoodsState state;
    const auto& totals=field(j,"totals");
    require(totals.is_array() && totals.size()==state.totals.size(),
            "schema26 requires exactly six original goods accounts");
    for (std::size_t i=0;i<state.totals.size();++i) {
        const auto& value=totals[i];
        require(number(field(value,"good"),state.totals.size()-1)==i,
                "schema26 goods identities must be complete, unique and sorted");
        state.totals[i]={number(field(value,"produced")),number(field(value,"imported")),
            number(field(value,"consumed")),number(field(value,"demolished"))};
    }
    const auto& stocks=field(j,"stocks");
    require(stocks.is_array() && stocks.size()<=simulation::original_goods_owner_limit,
            "schema26 stock collection exceeds bound");
    std::uint32_t previous=0;
    for (const auto& value:stocks) {
        simulation::OriginalGoodsStock stock;
        const auto id=static_cast<std::uint32_t>(number(field(value,"building_id"),UINT32_MAX));
        require(id>previous,"schema26 stock IDs must be positive, unique and sorted");
        previous=id;stock.building_id=static_cast<simulation::BuildingId>(id);
        const auto& amounts=field(value,"amounts");
        require(amounts.is_array() && amounts.size()==stock.amounts.size(),
                "schema26 stock requires exactly six amounts");
        for (std::size_t i=0;i<stock.amounts.size();++i) stock.amounts[i]=small(amounts[i]);
        state.stocks.push_back(stock);
    }
    const auto transfer=[](const json& value) {
        simulation::OriginalGoodsTransferKey key;
        key.courier_id=static_cast<simulation::CourierId>(number(field(value,"courier_id"),UINT32_MAX));
        key.sequence=number(field(value,"sequence"));
        require(static_cast<std::uint32_t>(key.courier_id)>0 && key.sequence>0,
                "schema26 transfer requires a positive courier identity and sequence");
        return key;
    };
    const auto ordered=[](simulation::OriginalGoodsTransferKey prior,simulation::OriginalGoodsTransferKey current) {
        return static_cast<std::uint32_t>(prior.courier_id)<static_cast<std::uint32_t>(current.courier_id);
    };
    const auto& cargo=field(j,"cargo");
    require(cargo.is_array() && cargo.size()<=simulation::original_goods_courier_limit,
            "schema26 cargo collection exceeds bound");
    for (const auto& value:cargo) {
        simulation::OriginalGoodsCargo item;
        item.transfer=transfer(field(value,"transfer"));
        require(state.cargo.empty() || ordered(state.cargo.back().transfer,item.transfer),
                "schema26 cargo requires sorted unique courier identities");
        item.good=static_cast<original::Goods>(number(field(value,"good"),original::goods_count-1));
        item.source=static_cast<simulation::BuildingId>(number(field(value,"source"),UINT32_MAX));
        item.target=static_cast<simulation::BuildingId>(number(field(value,"target"),UINT32_MAX));
        item.amount=small(field(value,"amount"));state.cargo.push_back(item);
    }
    const auto& reservations=field(j,"reservations");
    require(reservations.is_array() && reservations.size()<=simulation::original_goods_reservation_limit,
            "schema26 reservation collection exceeds bound");
    for (const auto& value:reservations) {
        simulation::OriginalGoodsReservation item;
        item.transfer=transfer(field(value,"transfer"));
        require(state.reservations.empty() || ordered(state.reservations.back().transfer,item.transfer),
                "schema26 reservations require one sorted latest sequence per courier");
        item.good=static_cast<original::Goods>(number(field(value,"good"),original::goods_count-1));
        item.source=static_cast<simulation::BuildingId>(number(field(value,"source"),UINT32_MAX));
        item.target=static_cast<simulation::BuildingId>(number(field(value,"target"),UINT32_MAX));
        item.phase=static_cast<simulation::OriginalGoodsTransferPhase>(number(field(value,"phase"),5));
        item.reserved=small(field(value,"reserved"));item.loaded=small(field(value,"loaded"));
        item.delivered=small(field(value,"delivered"));item.returned=small(field(value,"returned"));
        state.reservations.push_back(item);
    }
    return state;
}
json production_food_json(const simulation::OriginalFoodState& food,std::uint32_t rule_version) {
    require(food.farms.size()<=simulation::original_food_farm_limit(simulation::RulesProfile::CityV16,rule_version) &&
            food.stocks.size()<=simulation::original_food_stock_limit(simulation::RulesProfile::CityV16,rule_version) &&
            food.trips.size()<=simulation::original_food_trip_limit(simulation::RulesProfile::CityV16,rule_version),
            "schema26 Food collection exceeds bounded context");
    const auto transfer=[](simulation::OriginalGoodsTransferKey key) {
        return json{{"courier_id",static_cast<std::uint32_t>(key.courier_id)},{"sequence",key.sequence}};
    };
    json farms=json::array(),stocks=json::array(),trips=json::array();
    for (const auto& farm:food.farms)
        farms.push_back({{"farm_id",static_cast<std::uint32_t>(farm.farm_id)},
            {"field_id",farm.field_id ? json(static_cast<std::uint32_t>(*farm.field_id)):json(nullptr)},
            {"crop",static_cast<unsigned>(farm.crop)},{"harvested",farm.harvested}});
    for (const auto& stock:food.stocks) {
        json witness=nullptr;
        if (stock.quality_witness) {
            const auto& w=*stock.quality_witness;
            witness={{"transfer",transfer(w.transfer)},{"tick",w.tick},{"accepted",w.accepted},
                {"consumed_before",w.consumed_before},{"previous_quality",w.previous_quality}};
        }
        stocks.push_back({{"building_id",static_cast<std::uint32_t>(stock.building_id)},
            {"amounts",stock.amounts},{"received",stock.received},{"departed",stock.departed},
            {"returned",stock.returned},{"consumed",stock.consumed},{"missed_demands",stock.missed_demands},
            {"quality_witness",std::move(witness)}});
    }
    for (const auto& trip:food.trips)
        trips.push_back({{"transfer",transfer(trip.transfer)},
            {"source",static_cast<std::uint32_t>(trip.source)},{"target",static_cast<std::uint32_t>(trip.target)},
            {"phase",static_cast<unsigned>(trip.phase)},{"loaded",trip.loaded},
            {"delivered",trip.delivered},{"returned",trip.returned}});
    return {{"farms",std::move(farms)},{"stocks",std::move(stocks)},{"trips",std::move(trips)}};
}
simulation::OriginalFoodState parse_production_food(const json& j,std::uint32_t rule_version) {
    simulation::OriginalFoodState food;
    const auto id=[](const json& value) {
        const auto n=number(value,UINT32_MAX);require(n>0,"schema26 Food identity must be positive");
        return static_cast<simulation::BuildingId>(n);
    };
    const auto transfer=[](const json& value) {
        simulation::OriginalGoodsTransferKey key;
        key.courier_id=static_cast<simulation::CourierId>(number(field(value,"courier_id"),UINT32_MAX));
        key.sequence=number(field(value,"sequence"));
        require(static_cast<std::uint32_t>(key.courier_id)>0 && key.sequence>0,
                "schema26 Food transfer requires positive actor and sequence");
        return key;
    };
    const auto amounts=[](const json& value) {
        simulation::OriginalFoodAmounts out{};
        require(value.is_array() && value.size()==out.size(),"schema26 Food requires exactly two amounts");
        for (std::size_t i=0;i<out.size();++i) out[i]=static_cast<std::int32_t>(number(value[i],INT32_MAX));
        return out;
    };
    const auto counters=[](const json& value) {
        simulation::OriginalFoodCounters out{};
        require(value.is_array() && value.size()==out.size(),"schema26 Food requires exactly two counters");
        for (std::size_t i=0;i<out.size();++i) out[i]=number(value[i]);
        return out;
    };
    const auto& farms=field(j,"farms");
    require(farms.is_array() && farms.size()<=simulation::original_food_farm_limit(simulation::RulesProfile::CityV16,rule_version),
            "original Food farm collection exceeds versioned bound");
    for (const auto& value:farms) {
        simulation::OriginalFoodFarmState farm;farm.farm_id=id(field(value,"farm_id"));
        require(food.farms.empty() || food.farms.back().farm_id<farm.farm_id,
                "schema26 Food farm IDs must be sorted and unique");
        const auto& child=field(value,"field_id");if (!child.is_null()) farm.field_id=id(child);
        farm.crop=static_cast<simulation::OriginalFoodCrop>(number(field(value,"crop"),simulation::original_food_crop_count-1));
        farm.harvested=number(field(value,"harvested"));food.farms.push_back(farm);
    }
    const auto& stocks=field(j,"stocks");
    require(stocks.is_array() && stocks.size()<=simulation::original_food_stock_limit(simulation::RulesProfile::CityV16,rule_version),
            "original Food stock collection exceeds versioned bound");
    for (const auto& value:stocks) {
        simulation::OriginalFoodStock stock;stock.building_id=id(field(value,"building_id"));
        require(food.stocks.empty() || food.stocks.back().building_id<stock.building_id,
                "schema26 Food stock IDs must be sorted and unique");
        stock.amounts=amounts(field(value,"amounts"));stock.received=counters(field(value,"received"));
        stock.departed=counters(field(value,"departed"));stock.returned=counters(field(value,"returned"));
        stock.consumed=counters(field(value,"consumed"));stock.missed_demands=number(field(value,"missed_demands"));
        const auto& witness=field(value,"quality_witness");
        if (!witness.is_null()) {
            simulation::OriginalFoodQualityWitness w;w.transfer=transfer(field(witness,"transfer"));
            w.tick=number(field(witness,"tick"));w.accepted=amounts(field(witness,"accepted"));
            w.consumed_before=counters(field(witness,"consumed_before"));
            w.previous_quality=static_cast<std::uint8_t>(number(field(witness,"previous_quality"),UINT8_MAX));
            stock.quality_witness=w;
        }
        food.stocks.push_back(stock);
    }
    const auto& trips=field(j,"trips");
    require(trips.is_array() && trips.size()<=simulation::original_food_trip_limit(simulation::RulesProfile::CityV16,rule_version),
            "original Food trip collection exceeds versioned bound");
    for (const auto& value:trips) {
        simulation::OriginalFoodTrip trip;trip.transfer=transfer(field(value,"transfer"));
        require(food.trips.empty() || food.trips.back().transfer.courier_id<trip.transfer.courier_id,
                "schema26 Food trips require sorted unique courier IDs");
        trip.source=id(field(value,"source"));trip.target=id(field(value,"target"));
        trip.phase=static_cast<simulation::OriginalGoodsTransferPhase>(number(field(value,"phase"),5));
        trip.loaded=amounts(field(value,"loaded"));trip.delivered=amounts(field(value,"delivered"));
        trip.returned=amounts(field(value,"returned"));food.trips.push_back(trip);
    }
    return food;
}
json production_services_json(const simulation::OriginalServicesState& services) {
    require(services.providers.size()<=simulation::original_service_provider_limit &&
            services.trips.size()<=simulation::original_service_trip_limit &&
            services.witnesses.size()<=simulation::original_service_witness_limit,
            "schema26 medical service collection exceeds bounded context");
    const auto transfer=[](simulation::OriginalGoodsTransferKey key) {
        return json{{"courier_id",static_cast<std::uint32_t>(key.courier_id)},{"sequence",key.sequence}};
    };
    json providers=json::array(),trips=json::array(),witnesses=json::array();
    for (const auto& provider:services.providers)
        providers.push_back({{"building_id",static_cast<std::uint32_t>(provider.building_id)},
            {"type",static_cast<unsigned>(provider.type)}});
    for (const auto& trip:services.trips)
        trips.push_back({{"transfer",transfer(trip.transfer)},
            {"target",static_cast<std::uint32_t>(trip.target)},
            {"dispatched_tick",trip.dispatched_tick},
            {"arrival_tick",trip.arrival_tick ? json(*trip.arrival_tick):json(nullptr)},
            {"visit_applied",trip.visit_applied}});
    for (const auto& witness:services.witnesses)
        witnesses.push_back({{"building_id",static_cast<std::uint32_t>(witness.building_id)},
            {"role",static_cast<unsigned>(witness.role)},
            {"provider_id",static_cast<std::uint32_t>(witness.provider_id)},
            {"transfer",transfer(witness.transfer)},{"arrival_tick",witness.arrival_tick}});
    return {{"providers",std::move(providers)},{"trips",std::move(trips)},{"witnesses",std::move(witnesses)}};
}
simulation::OriginalServicesState parse_production_services(const json& j) {
    simulation::OriginalServicesState services;
    const auto id=[](const json& value) {
        const auto n=number(value,UINT32_MAX);require(n>0,"schema26 medical service identity must be positive");
        return static_cast<simulation::BuildingId>(n);
    };
    const auto transfer=[](const json& value) {
        simulation::OriginalGoodsTransferKey key;
        key.courier_id=static_cast<simulation::CourierId>(number(field(value,"courier_id"),UINT32_MAX));
        key.sequence=number(field(value,"sequence"));
        require(static_cast<std::uint32_t>(key.courier_id)>0 && key.sequence>0,
                "schema26 medical service transfer requires a positive actor and sequence");
        return key;
    };
    const auto& providers=field(j,"providers");
    require(providers.is_array() && providers.size()<=simulation::original_service_provider_limit,"schema26 service provider collection exceeds bound");
    for (const auto& value:providers) {
        simulation::OriginalServiceProvider provider;provider.building_id=id(field(value,"building_id"));
        require(services.providers.empty() || services.providers.back().building_id<provider.building_id,
                "schema26 medical providers require sorted unique IDs");
        provider.type=static_cast<simulation::OriginalServiceProviderType>(number(field(value,"type"),8));
        services.providers.push_back(provider);
    }
    const auto& trips=field(j,"trips");
    require(trips.is_array() && trips.size()<=simulation::original_service_trip_limit,"schema26 service trip collection exceeds bound");
    for (const auto& value:trips) {
        simulation::OriginalServiceTrip trip;trip.transfer=transfer(field(value,"transfer"));
        require(services.trips.empty() || services.trips.back().transfer.courier_id<trip.transfer.courier_id,
                "schema26 medical trips require sorted unique courier IDs");
        trip.target=id(field(value,"target"));trip.dispatched_tick=number(field(value,"dispatched_tick"));
        const auto& arrival=field(value,"arrival_tick");
        if (!arrival.is_null()) trip.arrival_tick=number(arrival);
        trip.visit_applied=boolean(field(value,"visit_applied"));services.trips.push_back(trip);
    }
    const auto& witnesses=field(j,"witnesses");
    require(witnesses.is_array() && witnesses.size()<=simulation::original_service_witness_limit,"schema26 service witness collection exceeds bound");
    for (const auto& value:witnesses) {
        simulation::OriginalServiceWitness witness;witness.building_id=id(field(value,"building_id"));
        const auto role=number(field(value,"role"),8);
        witness.role=static_cast<original::ServiceRole>(role);
        if (!services.witnesses.empty()) {
            const auto& previous=services.witnesses.back();
            require(previous.building_id<witness.building_id ||
                    (previous.building_id==witness.building_id && previous.role<witness.role),
                    "schema26 medical witnesses require sorted unique House/category pairs");
        }
        witness.provider_id=id(field(value,"provider_id"));witness.transfer=transfer(field(value,"transfer"));
        witness.arrival_tick=number(field(value,"arrival_tick"));services.witnesses.push_back(witness);
    }
    return services;
}
simulation::BuildingId production_id(const json& j) {
    const auto value=number(j,UINT32_MAX);
    require(value>0,"schema26 historical identity must be positive");
    return static_cast<simulation::BuildingId>(value);
}
json production_transfer_json(simulation::OriginalGoodsTransferKey key) {
    return {{"courier_id",static_cast<std::uint32_t>(key.courier_id)},{"sequence",key.sequence}};
}
simulation::OriginalGoodsTransferKey parse_production_transfer(const json& j) {
    simulation::OriginalGoodsTransferKey key;
    key.courier_id=static_cast<simulation::CourierId>(number(field(j,"courier_id"),UINT32_MAX));
    key.sequence=number(field(j,"sequence"));
    require(static_cast<std::uint32_t>(key.courier_id)>0 && key.sequence>0,
            "schema26 historical transfer requires positive actor and sequence");
    return key;
}
template<std::size_t N> std::array<std::uint64_t,N> parse_production_counters(const json& j) {
    std::array<std::uint64_t,N> result{};
    require(j.is_array() && j.size()==N,"schema26 historical counter array has wrong size");
    for (std::size_t i=0;i<N;++i) result[i]=number(j[i]);
    return result;
}
json production_industry_json(const simulation::OriginalIndustryState& state) {
    require(state.sources.size()<=simulation::original_industry_source_limit &&
            state.managed_ore.size()<=simulation::original_managed_ore_limit &&
            state.auxiliary.size()<=simulation::original_aux_owner_limit &&
            state.aux_trips.size()<=simulation::original_aux_courier_limit &&
            state.clay_trips.size()<=simulation::original_clay_trip_limit,
            "schema26 industry collection exceeds bounded context");
    json sources=json::array(),ore=json::array(),stocks=json::array(),aux_trips=json::array(),clay_trips=json::array();
    for (const auto& source:state.sources) {
        json receipt=nullptr;
        if (source.last_clay_arrival) {
            const auto& value=*source.last_clay_arrival;
            receipt={{"transfer",production_transfer_json(value.transfer)},
                {"source",static_cast<std::uint32_t>(value.source)},
                {"amount",value.amount},{"arrival_tick",value.arrival_tick}};
        }
        sources.push_back({{"building_id",static_cast<std::uint32_t>(source.building_id)},
            {"plant_id",source.plant_id ? json(static_cast<std::uint32_t>(*source.plant_id)):json(nullptr)},
            {"completed",source.completed},{"progress",source.progress},
            {"goods_inputs_used",source.goods_inputs_used},{"aux_inputs_used",source.aux_inputs_used},
            {"clay_received",source.clay_received},{"clay_used",source.clay_used},
            {"last_clay_arrival",std::move(receipt)}});
    }
    for (const auto& value:state.managed_ore)
        ore.push_back({{"building_id",static_cast<std::uint32_t>(value.building_id)},
            {"stock",value.stock},{"progress",value.progress},{"extracted",value.extracted},{"used",value.used}});
    for (const auto& value:state.auxiliary)
        stocks.push_back({{"building_id",static_cast<std::uint32_t>(value.building_id)},
            {"amounts",value.amounts},{"received",value.received},{"departed",value.departed},
            {"returned",value.returned},{"consumed",value.consumed}});
    for (const auto& value:state.aux_trips)
        aux_trips.push_back({{"transfer",production_transfer_json(value.transfer)},
            {"source",static_cast<std::uint32_t>(value.source)},{"target",static_cast<std::uint32_t>(value.target)},
            {"material",static_cast<unsigned>(value.material)},{"phase",static_cast<unsigned>(value.phase)},
            {"loaded",value.loaded},{"delivered",value.delivered},{"returned",value.returned},
            {"dispatched_tick",value.dispatched_tick},
            {"arrival_tick",value.arrival_tick ? json(*value.arrival_tick):json(nullptr)}});
    for (const auto& value:state.clay_trips)
        clay_trips.push_back({{"transfer",production_transfer_json(value.transfer)},
            {"source",static_cast<std::uint32_t>(value.source)},{"target",static_cast<std::uint32_t>(value.target)},
            {"loaded",value.loaded},{"delivered",value.delivered},{"dispatched_tick",value.dispatched_tick},
            {"arrival_tick",value.arrival_tick ? json(*value.arrival_tick):json(nullptr)}});
    return {{"sources",std::move(sources)},{"managed_ore",std::move(ore)},
        {"auxiliary",std::move(stocks)},{"aux_trips",std::move(aux_trips)},{"clay_trips",std::move(clay_trips)}};
}
simulation::OriginalIndustryState parse_production_industry(const json& j) {
    simulation::OriginalIndustryState result;
    const auto collection=[&](const char* key,std::size_t limit)->const json& {
        const auto& value=field(j,key);
        require(value.is_array() && value.size()<=limit,"schema26 industry collection exceeds bound");
        return value;
    };
    for (const auto& value:collection("sources",simulation::original_industry_source_limit)) {
        simulation::OriginalIndustrySource source;source.building_id=production_id(field(value,"building_id"));
        require(result.sources.empty() || result.sources.back().building_id<source.building_id,
                "schema26 industry source IDs must be sorted unique");
        const auto& plant=field(value,"plant_id");if (!plant.is_null()) source.plant_id=production_id(plant);
        source.completed=number(field(value,"completed"));source.progress=small(field(value,"progress"));
        source.goods_inputs_used=parse_production_counters<original::goods_count>(field(value,"goods_inputs_used"));
        source.aux_inputs_used=parse_production_counters<simulation::original_aux_material_count>(field(value,"aux_inputs_used"));
        source.clay_received=number(field(value,"clay_received"));source.clay_used=number(field(value,"clay_used"));
        const auto& receipt=field(value,"last_clay_arrival");
        if (!receipt.is_null()) {
            simulation::OriginalClayReceipt row;row.transfer=parse_production_transfer(field(receipt,"transfer"));
            row.source=production_id(field(receipt,"source"));row.amount=small(field(receipt,"amount"));
            row.arrival_tick=number(field(receipt,"arrival_tick"));source.last_clay_arrival=row;
        }
        result.sources.push_back(source);
    }
    for (const auto& value:collection("managed_ore",simulation::original_managed_ore_limit)) {
        simulation::OriginalManagedOre row;row.building_id=production_id(field(value,"building_id"));
        require(result.managed_ore.empty() || result.managed_ore.back().building_id<row.building_id,
                "schema26 managed ore IDs must be sorted unique");
        row.stock=small(field(value,"stock"));row.progress=small(field(value,"progress"));
        row.extracted=number(field(value,"extracted"));row.used=number(field(value,"used"));result.managed_ore.push_back(row);
    }
    for (const auto& value:collection("auxiliary",simulation::original_aux_owner_limit)) {
        simulation::OriginalAuxStock row;row.building_id=production_id(field(value,"building_id"));
        require(result.auxiliary.empty() || result.auxiliary.back().building_id<row.building_id,
                "schema26 material owner IDs must be sorted unique");
        const auto& amounts=field(value,"amounts");
        require(amounts.is_array() && amounts.size()==simulation::original_aux_material_count,
                "schema26 material stock requires exactly five amounts");
        for (std::size_t i=0;i<row.amounts.size();++i) row.amounts[i]=small(amounts[i]);
        row.received=parse_production_counters<simulation::original_aux_material_count>(field(value,"received"));
        row.departed=parse_production_counters<simulation::original_aux_material_count>(field(value,"departed"));
        row.returned=parse_production_counters<simulation::original_aux_material_count>(field(value,"returned"));
        row.consumed=parse_production_counters<simulation::original_aux_material_count>(field(value,"consumed"));
        result.auxiliary.push_back(row);
    }
    for (const auto& value:collection("aux_trips",simulation::original_aux_courier_limit)) {
        simulation::OriginalAuxTrip row;row.transfer=parse_production_transfer(field(value,"transfer"));
        require(result.aux_trips.empty() || result.aux_trips.back().transfer.courier_id<row.transfer.courier_id,
                "schema26 material trips require sorted unique actor IDs");
        row.source=production_id(field(value,"source"));row.target=production_id(field(value,"target"));
        row.material=static_cast<simulation::OriginalAuxMaterial>(number(field(value,"material"),simulation::original_aux_material_count-1));
        row.phase=static_cast<simulation::OriginalGoodsTransferPhase>(number(field(value,"phase"),5));
        row.loaded=small(field(value,"loaded"));row.delivered=small(field(value,"delivered"));row.returned=small(field(value,"returned"));
        row.dispatched_tick=number(field(value,"dispatched_tick"));
        const auto& arrival=field(value,"arrival_tick");if (!arrival.is_null()) row.arrival_tick=number(arrival);
        result.aux_trips.push_back(row);
    }
    for (const auto& value:collection("clay_trips",simulation::original_clay_trip_limit)) {
        simulation::OriginalClayTrip row;row.transfer=parse_production_transfer(field(value,"transfer"));
        require(result.clay_trips.empty() || result.clay_trips.back().transfer.courier_id<row.transfer.courier_id,
                "schema26 native Clay trips require sorted unique actor IDs");
        row.source=production_id(field(value,"source"));row.target=production_id(field(value,"target"));
        row.loaded=small(field(value,"loaded"));row.delivered=small(field(value,"delivered"));
        row.dispatched_tick=number(field(value,"dispatched_tick"));
        const auto& arrival=field(value,"arrival_tick");if (!arrival.is_null()) row.arrival_tick=number(arrival);
        result.clay_trips.push_back(row);
    }
    return result;
}
json production_water_witness_json(const simulation::OriginalWaterWitness& w) {
    return {{"building_id",static_cast<std::uint32_t>(w.building_id)},
        {"provider_id",static_cast<std::uint32_t>(w.provider_id)},
        {"transfer",production_transfer_json(w.transfer)},{"arrival_tick",w.arrival_tick}};
}
simulation::OriginalWaterWitness parse_production_water_witness(const json& j) {
    return {production_id(field(j,"building_id")),production_id(field(j,"provider_id")),
        parse_production_transfer(field(j,"transfer")),number(field(j,"arrival_tick"))};
}
json production_water_json(const simulation::OriginalWaterState& state) {
    require(state.providers.size()<=simulation::original_water_provider_limit &&
            state.trips.size()<=simulation::original_water_trip_limit &&
            state.houses.size()<=simulation::original_water_house_limit,"schema26 Water context exceeds bound");
    json providers=json::array(),trips=json::array(),houses=json::array();
    for (const auto& p:state.providers) providers.push_back({{"building_id",static_cast<std::uint32_t>(p.building_id)}});
    for (const auto& t:state.trips)
        trips.push_back({{"transfer",production_transfer_json(t.transfer)},
            {"target",static_cast<std::uint32_t>(t.target)},{"dispatched_tick",t.dispatched_tick},
            {"arrival_tick",t.arrival_tick ? json(*t.arrival_tick):json(nullptr)},{"visit_applied",t.visit_applied}});
    for (const auto& h:state.houses)
        houses.push_back({{"building_id",static_cast<std::uint32_t>(h.building_id)},
            {"normal_timer",h.normal_timer},{"special_timer",h.special_timer},
            {"current_witness",h.current_witness ? production_water_witness_json(*h.current_witness):json(nullptr)},
            {"evaluation_witness",h.evaluation_witness ? production_water_witness_json(*h.evaluation_witness):json(nullptr)}});
    return {{"providers",std::move(providers)},{"trips",std::move(trips)},{"houses",std::move(houses)}};
}
simulation::OriginalWaterState parse_production_water(const json& j) {
    simulation::OriginalWaterState result;
    const auto& providers=field(j,"providers");
    require(providers.is_array() && providers.size()<=simulation::original_water_provider_limit,"schema26 Water provider bound");
    for (const auto& p:providers) {
        const auto id=production_id(field(p,"building_id"));
        require(result.providers.empty() || result.providers.back().building_id<id,"schema26 Water providers must be sorted unique");
        result.providers.push_back({id});
    }
    const auto& trips=field(j,"trips");
    require(trips.is_array() && trips.size()<=simulation::original_water_trip_limit,"schema26 Water trip bound");
    for (const auto& value:trips) {
        simulation::OriginalWaterTrip t;t.transfer=parse_production_transfer(field(value,"transfer"));
        require(result.trips.empty() || result.trips.back().transfer.courier_id<t.transfer.courier_id,
                "schema26 Water trips must be sorted unique");
        t.target=production_id(field(value,"target"));t.dispatched_tick=number(field(value,"dispatched_tick"));
        const auto& arrival=field(value,"arrival_tick");if (!arrival.is_null()) t.arrival_tick=number(arrival);
        t.visit_applied=boolean(field(value,"visit_applied"));result.trips.push_back(t);
    }
    const auto& houses=field(j,"houses");
    require(houses.is_array() && houses.size()<=simulation::original_water_house_limit,"schema26 Water house bound");
    for (const auto& value:houses) {
        simulation::OriginalWaterHouseState h;h.building_id=production_id(field(value,"building_id"));
        require(result.houses.empty() || result.houses.back().building_id<h.building_id,"schema26 Water houses must be sorted unique");
        h.normal_timer=static_cast<std::uint8_t>(number(field(value,"normal_timer"),96));
        h.special_timer=static_cast<std::uint8_t>(number(field(value,"special_timer"),0));
        const auto& current=field(value,"current_witness");if (!current.is_null()) h.current_witness=parse_production_water_witness(current);
        const auto& evaluated=field(value,"evaluation_witness");if (!evaluated.is_null()) h.evaluation_witness=parse_production_water_witness(evaluated);
        result.houses.push_back(h);
    }
    return result;
}
json production_housing_service_json(const simulation::OriginalServiceWitness& w) {
    return {{"building_id",static_cast<std::uint32_t>(w.building_id)},{"role",static_cast<unsigned>(w.role)},
        {"provider_id",static_cast<std::uint32_t>(w.provider_id)},
        {"transfer",production_transfer_json(w.transfer)},{"arrival_tick",w.arrival_tick}};
}
simulation::OriginalServiceWitness parse_production_housing_service(const json& j) {
    simulation::OriginalServiceWitness result;result.building_id=production_id(field(j,"building_id"));
    const auto role=number(field(j,"role"),8);
    result.role=static_cast<original::ServiceRole>(role);result.provider_id=production_id(field(j,"provider_id"));
    result.transfer=parse_production_transfer(field(j,"transfer"));result.arrival_tick=number(field(j,"arrival_tick"));
    return result;
}
json production_housing_food_json(const simulation::OriginalFoodQualityWitness& w) {
    return {{"transfer",production_transfer_json(w.transfer)},{"tick",w.tick},{"accepted",w.accepted},
        {"consumed_before",w.consumed_before},{"previous_quality",w.previous_quality}};
}
simulation::OriginalFoodQualityWitness parse_production_housing_food(const json& j) {
    simulation::OriginalFoodQualityWitness result;result.transfer=parse_production_transfer(field(j,"transfer"));
    result.tick=number(field(j,"tick"));const auto& accepted=field(j,"accepted");
    require(accepted.is_array() && accepted.size()==result.accepted.size(),"schema26 historical Food requires exactly two amounts");
    for (std::size_t i=0;i<result.accepted.size();++i) result.accepted[i]=static_cast<std::int32_t>(number(accepted[i],INT32_MAX));
    result.consumed_before=parse_production_counters<simulation::original_food_crop_count>(field(j,"consumed_before"));
    result.previous_quality=static_cast<std::uint8_t>(number(field(j,"previous_quality"),UINT8_MAX));return result;
}
json production_housing_evaluation_json(const simulation::OriginalHousingEvaluation& e) {
    json services=json::array();
    for (const auto& witness:e.service_witnesses)
        services.push_back(witness ? production_housing_service_json(*witness):json(nullptr));
    return {{"tick",e.tick},{"input",original_house_json(e.input)},{"service_witnesses",std::move(services)},
        {"water_evaluation_witness",e.water_evaluation_witness ? production_water_witness_json(*e.water_evaluation_witness):json(nullptr)},
        {"food_quality_witness",e.food_quality_witness ? production_housing_food_json(*e.food_quality_witness):json(nullptr)},
        {"food_received",e.food_received},{"food_consumed",e.food_consumed},{"goods_consumed_before",e.goods_consumed_before}};
}
simulation::OriginalHousingEvaluation parse_production_housing_evaluation(const json& j) {
    simulation::OriginalHousingEvaluation result;result.tick=number(field(j,"tick"));
    result.input=parse_original_house(field(j,"input"));const auto& services=field(j,"service_witnesses");
    require(services.is_array() && services.size()==result.service_witnesses.size(),"schema26 historical services require exactly nine categories");
    for (std::size_t i=0;i<result.service_witnesses.size();++i)
        if (!services[i].is_null()) result.service_witnesses[i]=parse_production_housing_service(services[i]);
    const auto& water=field(j,"water_evaluation_witness");if (!water.is_null()) result.water_evaluation_witness=parse_production_water_witness(water);
    const auto& food=field(j,"food_quality_witness");if (!food.is_null()) result.food_quality_witness=parse_production_housing_food(food);
    result.food_received=parse_production_counters<simulation::original_food_crop_count>(field(j,"food_received"));
    result.food_consumed=parse_production_counters<simulation::original_food_crop_count>(field(j,"food_consumed"));
    result.goods_consumed_before=parse_production_counters<original::goods_count>(field(j,"goods_consumed_before"));return result;
}
json production_housing_transition_json(const simulation::OriginalHousingTransition& transition) {
    return {{"kind",static_cast<unsigned>(transition.kind)},
        {"evaluation",production_housing_evaluation_json(transition.evaluation)},
        {"admitted",transition.admitted}};
}
simulation::OriginalHousingTransition parse_production_housing_transition(const json& j) {
    simulation::OriginalHousingTransition result;
    result.kind=static_cast<simulation::OriginalHousingTransitionKind>(number(field(j,"kind"),1));
    result.evaluation=parse_production_housing_evaluation(field(j,"evaluation"));
    result.admitted=static_cast<std::int16_t>(number(field(j,"admitted"),2));
    return result;
}
json production_housing_json(const simulation::OriginalHousingState& state) {
    require(state.neutral_source_origins.size()<=37 && state.houses.size()<=simulation::Rules::city_v10_household_limit,
            "schema26 housing context exceeds bound");
    json sources=json::array(),houses=json::array();
    for (const auto& source:state.neutral_source_origins)
        sources.push_back({{"building_id",static_cast<std::uint32_t>(source.building_id)},
            {"origin",json::array({source.origin.x,source.origin.y})}});
    for (const auto& h:state.houses) {
        json rows=json::array();for (const auto& row:h.consumption_rows) rows.push_back({{"events",row.events},{"consumed",row.consumed}});
        json transitions=json::array();
        for (const auto& transition:h.first_transitions)
            transitions.push_back(transition ? production_housing_transition_json(*transition):json(nullptr));
        houses.push_back({{"building_id",static_cast<std::uint32_t>(h.building_id)},
            {"first_admission_tick",h.first_admission_tick ? json(*h.first_admission_tick):json(nullptr)},
            {"consumption_rows",std::move(rows)},
            {"last_evaluation",h.last_evaluation ? production_housing_evaluation_json(*h.last_evaluation):json(nullptr)},
            {"first_transitions",std::move(transitions)}});
    }
    return {{"neutral_source_origins",std::move(sources)},{"houses",std::move(houses)}};
}
simulation::OriginalHousingState parse_production_housing(const json& j) {
    simulation::OriginalHousingState result;const auto& sources=field(j,"neutral_source_origins");
    require(sources.is_array() && sources.size()<=37,"schema26 housing origin collection exceeds bound");
    for (const auto& value:sources) {
        simulation::OriginalHousingSourceOrigin source;source.building_id=production_id(field(value,"building_id"));
        require(result.neutral_source_origins.empty() || result.neutral_source_origins.back().building_id<source.building_id,
                "schema26 housing origin IDs must be sorted unique");
        source.origin=cell(field(value,"origin"));result.neutral_source_origins.push_back(source);
    }
    const auto& houses=field(j,"houses");
    require(houses.is_array() && houses.size()<=simulation::Rules::city_v10_household_limit,"schema26 housing House collection exceeds bound");
    for (const auto& value:houses) {
        simulation::OriginalHousingHouse h;h.building_id=production_id(field(value,"building_id"));
        require(result.houses.empty() || result.houses.back().building_id<h.building_id,"schema26 housing Houses must be sorted unique");
        const auto& first=field(value,"first_admission_tick");if (!first.is_null()) h.first_admission_tick=number(first);
        const auto& rows=field(value,"consumption_rows");
        require(rows.is_array() && rows.size()==h.consumption_rows.size(),"schema26 housing requires exactly fifteen consumption rows");
        for (std::size_t i=0;i<h.consumption_rows.size();++i) {
            h.consumption_rows[i].events=number(field(rows[i],"events"));
            h.consumption_rows[i].consumed=parse_production_counters<original::goods_count>(field(rows[i],"consumed"));
        }
        const auto& last=field(value,"last_evaluation");if (!last.is_null()) h.last_evaluation=parse_production_housing_evaluation(last);
        const auto& transitions=field(value,"first_transitions");
        require(transitions.is_array() && transitions.size()==h.first_transitions.size(),
                "schema26 housing requires exactly fifteen first-transition slots");
        for (std::size_t i=0;i<h.first_transitions.size();++i)
            if (!transitions[i].is_null()) h.first_transitions[i]=parse_production_housing_transition(transitions[i]);
        result.houses.push_back(h);
    }
    return result;
}
json production_finance_json(const simulation::OriginalFinanceState& state) {
    json houses=json::array();
    for (const auto& h:state.houses) {
        json rows=json::array();
        for (const auto& row:h.rows)
            rows.push_back({{"events",row.events},{"resident_units",row.resident_units}});
        json last=nullptr;
        if (h.last_assessment) {
            const auto& value=*h.last_assessment;
            last={{"tick",value.tick},{"type",static_cast<unsigned>(value.type)},
                  {"occupants",value.occupants}};
        }
        houses.push_back({{"building_id",static_cast<std::uint32_t>(h.building_id)},
            {"rows",std::move(rows)},{"last_assessment",std::move(last)}});
    }
    return {{"houses",std::move(houses)}};
}
simulation::OriginalFinanceState parse_production_finance(const json& j) {
    simulation::OriginalFinanceState result;
    const auto& houses=field(j,"houses");
    require(houses.is_array() && houses.size()<=simulation::Rules::city_v10_household_limit,
            "schema26 Finance house bound");
    for (const auto& value:houses) {
        simulation::OriginalFinanceHouse h;h.building_id=production_id(field(value,"building_id"));
        require(result.houses.empty() || result.houses.back().building_id<h.building_id,
                "schema26 Finance houses must be sorted unique");
        const auto& rows=field(value,"rows");
        require(rows.is_array() && rows.size()==original::house_row_count,"schema26 Finance requires fifteen rows");
        for (std::size_t i=0;i<h.rows.size();++i) {
            h.rows[i].events=number(field(rows[i],"events"));
            h.rows[i].resident_units=number(field(rows[i],"resident_units"));
        }
        const auto& last=field(value,"last_assessment");
        if (!last.is_null()) {
            simulation::OriginalTaxAssessment input;
            input.tick=number(field(last,"tick"));
            const auto type=number(field(last,"type"),17);
            require(type>=3 && type!=11,"schema26 Finance assessment needs an inhabited House type");
            input.type=static_cast<original::HouseType>(type);
            input.occupants=static_cast<std::int16_t>(number(field(last,"occupants"),INT16_MAX));
            require(input.occupants>0,"schema26 Finance assessment needs actual residents");
            h.last_assessment=input;
        }
        result.houses.push_back(h);
    }
    return result;
}
json fire_protection_json(const simulation::OriginalFireProtectionState& state,int width) {
    const auto trip=[&](const simulation::OriginalFireTrip& t) {
        json cells=json::array();for (const auto c:t.travelled) cells.push_back(c.y*width+c.x);
        return json::array({static_cast<std::uint32_t>(t.courier_id),static_cast<std::uint32_t>(t.owner),
            static_cast<std::uint32_t>(t.target),t.dispatched_tick,t.road_revision,t.assigned_workers,t.population,t.operating,std::move(cells)});
    };
    json trips=json::array(),arrivals=json::array(),taxes=json::array();
    for (const auto& t:state.trips) trips.push_back(trip(t));
    for (const auto& e:state.arrivals) arrivals.push_back(json::array({e.tick,e.extinguished,trip(e.trip)}));
    for (const auto& e:state.taxes) taxes.push_back(json::array({static_cast<std::uint32_t>(e.building_id),
        e.assessment.tick,static_cast<unsigned>(e.assessment.type),e.assessment.occupants}));
    return {{"travelled_cells",state.travelled_cells},{"trips",std::move(trips)},{"arrivals",std::move(arrivals)},{"taxes",std::move(taxes)}};
}
simulation::OriginalFireProtectionState parse_fire_protection(const json& j,int width,int height) {
    simulation::OriginalFireProtectionState result;
    result.travelled_cells=number(field(j,"travelled_cells"),simulation::original_fire_path_cell_limit);
    std::size_t cells=0;
    const auto trip=[&](const json& value) {
        require(value.is_array() && value.size()==9,"schema27 requires complete Inspector dispatch/travel provenance");
        simulation::OriginalFireTrip t;
        t.courier_id=static_cast<simulation::CourierId>(number(value[0],UINT32_MAX));
        t.owner=static_cast<simulation::BuildingId>(number(value[1],UINT32_MAX));
        t.target=static_cast<simulation::BuildingId>(number(value[2],UINT32_MAX));
        t.dispatched_tick=number(value[3],original::scheduler_max_tick);t.road_revision=number(value[4]);
        t.assigned_workers=small(value[5],simulation::Rules::fire_watch_workers);
        t.population=small(value[6],INT16_MAX*20);t.operating=boolean(value[7]);
        require(value[8].is_array() && value[8].size()<=simulation::original_fire_path_cell_limit-cells,
                "schema27 historical path cell budget exceeded");
        for (const auto& encoded:value[8]) {
            const auto flat=number(encoded,static_cast<std::uint64_t>(width)*static_cast<std::uint64_t>(height)-1);
            t.travelled.push_back({static_cast<int>(flat%static_cast<std::uint64_t>(width)),static_cast<int>(flat/static_cast<std::uint64_t>(width))});++cells;
        }
        return t;
    };
    const auto& trips=field(j,"trips");const auto& arrivals=field(j,"arrivals");const auto& taxes=field(j,"taxes");
    require(trips.is_array() && trips.size()<=2 && arrivals.is_array() && arrivals.size()<=simulation::original_fire_arrival_limit &&
            taxes.is_array() && taxes.size()<=simulation::original_fire_tax_limit,"schema27 fire/tax journal collection bound exceeded");
    for (const auto& t:trips) result.trips.push_back(trip(t));
    for (const auto& e:arrivals) {
        require(e.is_array() && e.size()==3,"schema27 arrival requires time, actual extinguishing and dispatch provenance");
        result.arrivals.push_back({trip(e[2]),number(e[0],original::scheduler_max_tick),boolean(e[1])});
    }
    for (const auto& e:taxes) {
        require(e.is_array() && e.size()==4,"schema27 tax requires a complete historical pre-tick input");
        result.taxes.push_back({static_cast<simulation::BuildingId>(number(e[0],UINT32_MAX)),
            {number(e[1],original::scheduler_max_tick),static_cast<original::HouseType>(number(e[2],17)),
             static_cast<std::int16_t>(small(e[3],INT16_MAX))}});
    }
    require(cells==result.travelled_cells,"schema27 historical path counter differs from journal");
    return result;
}
json production_document_json(const SaveDocument& d) {
    const bool fire=simulation::original_fire_protection_enabled(d.world.profile,d.world.rule_version);
    const auto schema=simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ? 34U:
        simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version) ? 33U:
        simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version) ? 32U:
        simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version) ? 31U:
        simulation::original_cardinal_water_enabled(d.world.profile,d.world.rule_version) ? 30U:
        simulation::original_market_logistics_enabled(d.world.profile,d.world.rule_version) ? 29U:
        simulation::original_sustainable_economy_enabled(d.world.profile,d.world.rule_version) ? 28U:fire ? 27U:26U;
    require(simulation::original_production_enabled(d.world.profile,d.world.rule_version) &&
            d.world.original_production && d.world.original_goods &&
            d.world.original_economy && d.world.original_influence,
            "schema26 requires separate City-v16 rule10 production authority");
    require(d.source_schema_version==0 || d.source_schema_version==schema,
            "Rule10/11/12/13/14/15/16/17/18 require their exact schema26/27/28/29/30/31/32/33/34; existing saves are not migrated");
    require(d.world.original_production->development_revision==simulation::kOriginalProductionDevelopmentRevision,
            "schema26 development revision differs from this executable");
    require(d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion &&
            valid_hash(d.map_permissions_sha256),"schema26 requires immutable policy3 identity");
    simulation::World::validate_original_production_snapshot(d.world);
    // Reuse infrastructure syntax only. No positive state enters a frozen
    // schema23/24/25 admission path or an older live World restore.
    auto projected=d;
    projected.world.rule_version=6;
    projected.world.original_economy.reset();projected.world.original_influence.reset();
    projected.world.original_goods.reset();projected.world.original_production.reset();
    for (auto& building:projected.world.buildings) {
        if (simulation::original_elite_house_kind(building.kind)) building.kind=simulation::Object::Household;
        else if (building.kind==simulation::Object::OriginalInfluenceSource) building.kind=simulation::Object::Well;
        else if (building.kind==simulation::Object::OriginalHempFarm) building.kind=simulation::Object::Pottery;
        else if (building.kind==simulation::Object::OriginalHempField) building.kind=simulation::Object::Well;
        else if (building.kind==simulation::Object::OriginalFoodFarm) building.kind=simulation::Object::Farm;
        else if (building.kind==simulation::Object::OriginalWheatField || building.kind==simulation::Object::OriginalMilletField)
            building.kind=simulation::Object::Well;
        else if (building.kind>=simulation::Object::OriginalLoggingShed38 &&
                 building.kind<=simulation::Object::OriginalSilkwormShed239)
            building.kind=simulation::Object::Pottery;
        else if (building.kind==simulation::Object::OriginalTeaBush26 ||
                 building.kind==simulation::Object::OriginalLacquerTree27 ||
                 building.kind==simulation::Object::OriginalMulberryTree28)
            building.kind=simulation::Object::Well;
        else if (building.kind==simulation::Object::OriginalAncestorShrine214 ||
                 building.kind==simulation::Object::OriginalWaterWell72 ||
                 building.kind==simulation::Object::OriginalDaoistShrine215 ||
                 building.kind==simulation::Object::OriginalBuddhistShrine217 ||
                 building.kind==simulation::Object::OriginalConfucianAcademy219 ||
                 building.kind==simulation::Object::OriginalMusicSchool211 ||
                 building.kind==simulation::Object::OriginalAcrobatSchool212 ||
                 building.kind==simulation::Object::OriginalDramaSchool213)
            building.kind=simulation::Object::ServicePost;
    }
    for (auto& courier:projected.world.couriers) {
        if (courier.role==simulation::CourierRole::OriginalHempToWarehouse ||
            courier.role==simulation::CourierRole::OriginalLuxuryToWarehouse ||
            courier.role==simulation::CourierRole::OriginalAuxToWarehouse ||
            courier.role==simulation::CourierRole::OriginalAuxSupplier)
            courier.role=simulation::CourierRole::MarketPotteryInbound;
        else if (courier.role==simulation::CourierRole::OriginalFoodToMarket)
            courier.role=simulation::CourierRole::MarketFoodInbound;
        else if (courier.role==simulation::CourierRole::OriginalFoodToHouse)
            courier.role=simulation::CourierRole::MarketFoodDistribution;
        else if (courier.role==simulation::CourierRole::OriginalAncestor ||
                 courier.role==simulation::CourierRole::OriginalWater ||
                 courier.role==simulation::CourierRole::OriginalDaoist ||
                 courier.role==simulation::CourierRole::OriginalBuddhist ||
                 courier.role==simulation::CourierRole::OriginalConfucian ||
                 courier.role==simulation::CourierRole::OriginalMusic ||
                 courier.role==simulation::CourierRole::OriginalAcrobat ||
                 courier.role==simulation::CourierRole::OriginalDrama)
            courier.role=simulation::CourierRole::Service;
    }
    auto result=document_json(projected);
    result["schema_version"]=schema;result["rules"]["version"]=d.world.rule_version;
    if (schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34) result["rules"]["market_logistics"]=simulation::original_market_logistics_contract;
    if (schema==30 || schema==31 || schema==32 || schema==33 || schema==34) result["rules"]["cardinal_water"]=simulation::original_cardinal_water_contract;
    if (schema==31) result["rules"]["inspector_coordination"]=simulation::original_inspector_coordination_contract;
    if (schema==32 || schema==33 || schema==34) result["rules"]["deadline_fire_dispatch"]=simulation::original_deadline_fire_dispatch_contract;
    if (schema==33 || schema==34) result["rules"]["deadline_service_dispatch"]=simulation::original_deadline_service_dispatch_contract;
    if (schema==34) result["rules"]["deadline_food_dispatch"]=simulation::original_deadline_food_dispatch_contract;
    for (std::size_t i=0;i<d.world.buildings.size();++i)
        result["world"]["buildings"][i]["kind"]=static_cast<int>(d.world.buildings[i].kind);
    for (std::size_t i=0;i<d.world.couriers.size();++i)
        result["world"]["couriers"][i]["role"]=static_cast<int>(d.world.couriers[i].role);
    result["world"]["original_economy"]=original_economy_json(*d.world.original_economy);
    result["world"]["original_influence"]=original_influence_json(*d.world.original_influence);
    result["world"]["original_goods"]=production_goods_json(*d.world.original_goods);
    const auto& production=*d.world.original_production;
    json farms=json::array(),consumption=json::array();
    for (const auto& farm:production.hemp_farms)
        farms.push_back({{"farm_id",static_cast<std::uint32_t>(farm.farm_id)},
            {"field_id",farm.field_id ? json(static_cast<std::uint32_t>(*farm.field_id)):json(nullptr)},
            {"harvested",farm.harvested}});
    for (const auto& house:production.house_consumption)
        consumption.push_back({{"building_id",static_cast<std::uint32_t>(house.building_id)},
            {"consumed",house.consumed}});
    result["world"]["original_production"]={{"development_revision",production.development_revision},
        {"residents_admitted",production.residents_admitted},{"pottery_handed_off",production.pottery_handed_off},
        {"hemp_farms",std::move(farms)},{"house_consumption",std::move(consumption)},
        {"food",production_food_json(production.food,d.world.rule_version)},
        {"services",production_services_json(production.services)},
        {"water",production_water_json(production.water)},{"housing",production_housing_json(production.housing)},
        {"industry",production_industry_json(production.industry)},
        {"finance",production_finance_json(production.finance)}};
    if (fire) result["world"]["original_production"]["fire_protection"]=fire_protection_json(*production.fire_protection,d.world.width);
    return result;
}
SaveDocument parse_production_document(const json& j) {
    const auto schema=number(field(j,"schema_version"));
    const bool fire=schema==27 || schema==28 || schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34;
    if (schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34) require(str(field(field(j,"rules"),"market_logistics"))==simulation::original_market_logistics_contract,
        "schema29 requires its exact authored market-logistics contract");
    if (schema==30 || schema==31 || schema==32 || schema==33 || schema==34) require(str(field(field(j,"rules"),"cardinal_water"))==simulation::original_cardinal_water_contract,
        "schema30 requires its exact verified cardinal-water contract");
    if (schema==31) require(str(field(field(j,"rules"),"inspector_coordination"))==simulation::original_inspector_coordination_contract,
        "schema31 requires its exact authored Inspector-coordination contract");
    if (schema==32 || schema==33 || schema==34) require(str(field(field(j,"rules"),"deadline_fire_dispatch"))==simulation::original_deadline_fire_dispatch_contract,
        "schema32 requires its exact authored deadline-fire-dispatch contract");
    if (schema==33 || schema==34) require(str(field(field(j,"rules"),"deadline_service_dispatch"))==simulation::original_deadline_service_dispatch_contract,
        "schema33 requires its exact authored deadline-service-dispatch contract");
    if (schema==34) require(str(field(field(j,"rules"),"deadline_food_dispatch"))==simulation::original_deadline_food_dispatch_contract,
        "schema34 requires its exact authored deadline-food-dispatch contract");
    const auto rule_version=schema==34 ? simulation::kOriginalDeadlineFoodDispatchRuleVersion:
        schema==33 ? simulation::kOriginalDeadlineServiceDispatchRuleVersion:
        schema==32 ? simulation::kOriginalDeadlineFireDispatchRuleVersion:
        schema==31 ? simulation::kOriginalInspectorCoordinationRuleVersion:
        schema==30 ? simulation::kOriginalCardinalWaterRuleVersion:
        schema==29 ? simulation::kOriginalMarketLogisticsRuleVersion:
        schema==28 ? simulation::kOriginalSustainableRuleVersion:
        fire ? simulation::kOriginalFireProtectionRuleVersion:simulation::kOriginalProductionRuleVersion;
    require(str(field(field(j,"rules"),"id"))==simulation::city_v16_profile_name &&
            number(field(field(j,"rules"),"version"))==rule_version,
            "schema26/27/28/29/30/31/32/33/34 require their exact explicit City-v16 Rule10/11/12/13/14/15/16/17/18");
    const auto& world=field(j,"world");const auto& production=field(world,"original_production");
    simulation::OriginalProductionState state;
    state.development_revision=static_cast<std::uint32_t>(number(field(production,"development_revision"),UINT32_MAX));
    require(state.development_revision==simulation::kOriginalProductionDevelopmentRevision,
            "schema26 development revision differs from this executable");
    state.residents_admitted=number(field(production,"residents_admitted"));
    state.pottery_handed_off=number(field(production,"pottery_handed_off"));
    state.finance=parse_production_finance(field(production,"finance"));
    const auto& farms=field(production,"hemp_farms");
    require(farms.is_array() && farms.size()<=2,"schema26 Hemp farm collection exceeds bound");
    std::uint32_t previous_farm=0;
    for (const auto& value:farms) {
        simulation::OriginalHempFarmState farm;
        const auto id=static_cast<std::uint32_t>(number(field(value,"farm_id"),UINT32_MAX));
        require(id>previous_farm,"schema26 Hemp farm IDs must be positive, unique and sorted");
        previous_farm=id;farm.farm_id=static_cast<simulation::BuildingId>(id);
        const auto& field_id=field(value,"field_id");
        if (!field_id.is_null()) {
            const auto child=static_cast<std::uint32_t>(number(field_id,UINT32_MAX));
            require(child>0,"schema26 Hemp field identity must be positive or null");
            farm.field_id=static_cast<simulation::BuildingId>(child);
        }
        farm.harvested=number(field(value,"harvested"));state.hemp_farms.push_back(farm);
    }
    const auto& consumption=field(production,"house_consumption");
    require(consumption.is_array() && consumption.size()<=simulation::Rules::city_v10_household_limit,
            "schema26 House consumption collection exceeds bound");
    std::uint32_t previous_house=0;
    for (const auto& value:consumption) {
        simulation::OriginalHouseConsumptionState house;
        const auto id=static_cast<std::uint32_t>(number(field(value,"building_id"),UINT32_MAX));
        require(id>previous_house,"schema26 consumption House IDs must be positive, unique and sorted");
        previous_house=id;house.building_id=static_cast<simulation::BuildingId>(id);
        const auto& amounts=field(value,"consumed");
        require(amounts.is_array() && amounts.size()==house.consumed.size(),
                "schema26 House consumption requires exactly six amounts");
        for (std::size_t i=0;i<house.consumed.size();++i) house.consumed[i]=number(amounts[i]);
        state.house_consumption.push_back(house);
    }
    state.food=parse_production_food(field(production,"food"),rule_version);
    state.services=parse_production_services(field(production,"services"));
    state.water=parse_production_water(field(production,"water"));
    state.housing=parse_production_housing(field(production,"housing"));
    state.industry=parse_production_industry(field(production,"industry"));
    auto projected=j;
    projected["schema_version"]=22;projected["rules"]["version"]=6;
    for (const char* key:{"original_economy","original_influence","original_goods","original_production"})
        projected["world"].erase(key);
    auto& buildings=projected["world"]["buildings"];
    require(buildings.is_array() && buildings.size()<=simulation::Rules::original_production_building_limit,
            "schema26 building collection exceeds bound");
    for (auto& building:buildings) {
        const auto kind=number(field(building,"kind"),static_cast<unsigned>(simulation::Object::OriginalEliteHouse));
        if (simulation::original_elite_house_kind(static_cast<simulation::Object>(kind)))
            building["kind"]=static_cast<int>(simulation::Object::Household);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalInfluenceSource))
            building["kind"]=static_cast<int>(simulation::Object::Well);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalHempFarm))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalHempField))
            building["kind"]=static_cast<int>(simulation::Object::Well);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalFoodFarm))
            building["kind"]=static_cast<int>(simulation::Object::Farm);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalWheatField) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalMilletField))
            building["kind"]=static_cast<int>(simulation::Object::Well);
        else if (kind>=static_cast<unsigned>(simulation::Object::OriginalLoggingShed38) &&
                 kind<=static_cast<unsigned>(simulation::Object::OriginalSilkwormShed239))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalTeaBush26) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalLacquerTree27) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalMulberryTree28))
            building["kind"]=static_cast<int>(simulation::Object::Well);
        else if (kind==static_cast<unsigned>(simulation::Object::OriginalAncestorShrine214) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalWaterWell72) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalDaoistShrine215) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalBuddhistShrine217) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalConfucianAcademy219) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalMusicSchool211) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalAcrobatSchool212) ||
                 kind==static_cast<unsigned>(simulation::Object::OriginalDramaSchool213))
            building["kind"]=static_cast<int>(simulation::Object::ServicePost);
    }
    auto& couriers=projected["world"]["couriers"];
    require(couriers.is_array() && couriers.size()<=simulation::Rules::original_production_courier_limit,
            "schema26 courier collection exceeds bound");
    for (auto& courier:couriers) {
        const auto role=number(field(courier,"role"),static_cast<unsigned>(simulation::CourierRole::OriginalAuxSupplier));
        if (role==static_cast<unsigned>(simulation::CourierRole::OriginalHempToWarehouse) ||
            role==static_cast<unsigned>(simulation::CourierRole::OriginalLuxuryToWarehouse) ||
            role==static_cast<unsigned>(simulation::CourierRole::OriginalAuxToWarehouse) ||
            role==static_cast<unsigned>(simulation::CourierRole::OriginalAuxSupplier))
            courier["role"]=static_cast<int>(simulation::CourierRole::MarketPotteryInbound);
        else if (role==static_cast<unsigned>(simulation::CourierRole::OriginalFoodToMarket))
            courier["role"]=static_cast<int>(simulation::CourierRole::MarketFoodInbound);
        else if (role==static_cast<unsigned>(simulation::CourierRole::OriginalFoodToHouse))
            courier["role"]=static_cast<int>(simulation::CourierRole::MarketFoodDistribution);
        else if (role==static_cast<unsigned>(simulation::CourierRole::OriginalAncestor) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalWater) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalDaoist) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalBuddhist) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalConfucian) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalMusic) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalAcrobat) ||
                 role==static_cast<unsigned>(simulation::CourierRole::OriginalDrama))
            courier["role"]=static_cast<int>(simulation::CourierRole::Service);
    }
    auto d=parse_document(projected,SnapshotSyntax::OriginalProduction);
    d.source_schema_version=static_cast<std::uint32_t>(schema);d.world.rule_version=rule_version;
    for (std::size_t i=0;i<d.world.buildings.size();++i)
        d.world.buildings[i].kind=static_cast<simulation::Object>(number(field(field(world,"buildings")[i],"kind")));
    for (std::size_t i=0;i<d.world.couriers.size();++i)
        d.world.couriers[i].role=static_cast<simulation::CourierRole>(number(field(field(world,"couriers")[i],"role")));
    d.world.original_economy=parse_original_economy(field(world,"original_economy"));
    d.world.original_influence=parse_original_influence(field(world,"original_influence"));
    d.world.original_goods=parse_production_goods(field(world,"original_goods"));
    if (fire) state.fire_protection=parse_fire_protection(field(production,"fire_protection"),d.world.width,d.world.height);
    d.world.original_production=state;
    simulation::World::validate_original_production_snapshot(d.world);
    require(canonical_fields(j,production_document_json(d)),"schema26 contains unknown or noncanonical save fields");
    return d;
}
SaveDocument parse_original_document(const json& j) {
    require(str(field(field(j,"rules"),"id"))==simulation::city_v16_profile_name &&
            number(field(field(j,"rules"),"version"))==simulation::kOriginalEconomyRuleVersion,
            "schema23 requires City-v16 rule7");
    auto projected=j;
    projected["schema_version"]=22;
    projected["rules"]["version"]=6;
    projected["world"].erase("original_economy");
    auto d=parse_document(projected);
    d.source_schema_version=23;
    d.world.rule_version=simulation::kOriginalEconomyRuleVersion;
    d.world.original_economy=parse_original_economy(field(field(j,"world"),"original_economy"));
    simulation::World::validate_original_economy_snapshot(d.world);
    // Equality checks every nested key and retained value; ignored legacy
    // fields, unknown keys and missing canonical fields cannot become authority.
    require(canonical_fields(j,original_document_json(d)),"schema23 contains unknown or noncanonical save fields");
    return d;
}
json document_json(const SaveDocument& d) {
    if (simulation::original_production_enabled(d.world.profile,d.world.rule_version))
        return production_document_json(d);
    require(!d.world.original_production,"original production authority requires City-v16 rule10/schema26");
    if (simulation::original_goods_enabled(d.world.profile,d.world.rule_version))
        return goods_document_json(d);
    require(!d.world.original_goods,"original goods authority requires City-v16 rule9/schema25");
    if (simulation::original_influence_enabled(d.world.profile,d.world.rule_version))
        return influence_document_json(d);
    require(!d.world.original_influence,"influence source authority requires City-v16 rule8/schema24");
    if (simulation::original_economy_enabled(d.world.profile,d.world.rule_version))
        return original_document_json(d);
    require(!d.world.original_economy,"original economy authority requires City-v16 rule7/schema23");
    json out={{"format","openemperor-sandbox-save"},{"schema_version",
             d.world.profile==simulation::RulesProfile::CityV16 ?
                (d.world.rule_version==6 ? 22:d.world.rule_version==5 ? 21:d.world.rule_version==4 ? 20:d.world.rule_version==3 ? 19:18):
             d.world.profile==simulation::RulesProfile::CityV15 ? 17:
             d.world.profile==simulation::RulesProfile::CityV14 ? 16:
             d.world.profile==simulation::RulesProfile::CityV13 ? 15:
             d.world.profile==simulation::RulesProfile::CityV12 ? 14:
             d.world.profile==simulation::RulesProfile::CityV11 ?
                (d.world.rule_version==4 ? 13:d.world.rule_version==3 ? 12:11):
             d.world.profile==simulation::RulesProfile::CityV10 ? 10:
             d.world.profile==simulation::RulesProfile::CityV9 ? 9:
             d.world.profile==simulation::RulesProfile::CityV8 ? 8:
             d.world.profile==simulation::RulesProfile::CityV7 ? 7:
             d.world.profile==simulation::RulesProfile::CityV6 ? 6:
             d.world.profile==simulation::RulesProfile::IndustryV5 ? 5:4},
        {"map",{{"relative_path",d.map_relative.generic_string()},{"sha256",d.map_sha256},
                {"part_index",0},{"grid_width",d.world.width},{"grid_height",d.world.height}}},
        {"profiles",{{"graphics",maps::stored_graphics_slot8_profile},
                     {"footprint","edge-byte-4x4"},{"buildability",maps::sandbox_buildable_profile},
                     {"buildable_sha256",d.buildable_sha256}}},
        {"rules",{{"id",rules_profile_name(d.world.profile)},
                  {"version",d.world.rule_version}}},
        {"world",snapshot_json(d.world)}};
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        (d.world.rule_version>=3 && d.world.rule_version<=6))
        out["profiles"]["map_permissions"]={{"version",d.map_permissions_policy_version},
                                             {"sha256",d.map_permissions_sha256}};
    return out;
}
SaveDocument parse_document(const json& j,SnapshotSyntax syntax) {
    require(str(field(j,"format"))=="openemperor-sandbox-save","unknown save format");
    const auto schema=number(field(j,"schema_version"));
    require(syntax==SnapshotSyntax::Frozen || schema==22,"active syntax is restricted to the local schema22 projection");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==31 ||
            !field(j,"rules").contains("inspector_coordination"),
            "Inspector coordination marker requires exact schema31");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==32 || schema==33 || schema==34 ||
            !field(j,"rules").contains("deadline_fire_dispatch"),
            "Deadline fire-dispatch marker requires exact schema32,33 or34");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==33 || schema==34 ||
            !field(j,"rules").contains("deadline_service_dispatch"),
            "Deadline service-dispatch marker requires exact schema33 or34");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==34 ||
            !field(j,"rules").contains("deadline_food_dispatch"),
            "Deadline Food-dispatch marker requires exact schema34");
    if (schema==26 || schema==27 || schema==28 || schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34) return parse_production_document(j);
    if (schema==25) return parse_goods_document(j);
    if (schema==24) return parse_influence_document(j);
    if (schema==23) return parse_original_document(j);
    require(schema>=1 && schema<=22,"unsupported save schema version");
    require(!j.contains("original_production") && !field(j,"world").contains("original_production"),
            "original production authority requires schema26");
    require(!j.contains("original_economy") && !field(j,"world").contains("original_economy"),
            "original economy authority requires schema23");
    require(!j.contains("original_influence") && !field(j,"world").contains("original_influence"),
            "influence source authority requires schema24");
    require(!j.contains("original_goods") && !field(j,"world").contains("original_goods"),
            "original goods authority requires schema25");
    const auto& m=field(j,"map"); const auto& p=field(j,"profiles");
    const auto& r=field(j,"rules");
    SaveDocument d;
    d.source_schema_version=static_cast<std::uint32_t>(schema);
    d.map_relative=fs::path(str(field(m,"relative_path")));
    safe_relative(d.map_relative);
    d.map_sha256=str(field(m,"sha256"));
    d.buildable_sha256=str(field(p,"buildable_sha256"));
    require(valid_hash(d.map_sha256) && valid_hash(d.buildable_sha256),"invalid save fingerprint");
    if (schema==19 || schema==20 || schema==21 || schema==22) {
        const auto& policy=field(p,"map_permissions");
        require(policy.is_object() && policy.size()==2,
                "save map permissions must contain only version and SHA-256, not transit links");
        d.map_permissions_policy_version=static_cast<std::uint32_t>(number(field(policy,"version"),UINT32_MAX));
        require(d.map_permissions_policy_version==(schema==19 ? simulation::kMapPermissionsPolicyVersion:
                schema==20 ? simulation::kOriginalRoadsMapPermissionsPolicyVersion:simulation::kBridgesMapPermissionsPolicyVersion),
                "unsupported saved map-permissions policy version");
        d.map_permissions_sha256=str(field(policy,"sha256"));
        require(valid_hash(d.map_permissions_sha256),"invalid map-permissions fingerprint");
    }
    require(number(field(m,"part_index"))==0,"unsupported map part index");
    require(str(field(p,"graphics"))==maps::stored_graphics_slot8_profile &&
            str(field(p,"footprint"))=="edge-byte-4x4" &&
            str(field(p,"buildability"))==maps::sandbox_buildable_profile,
            "unsupported sandbox map or buildability profile");
    const auto id=str(field(r,"id"));
    if (id==simulation::profile_name) d.world.profile=simulation::RulesProfile::LogisticsV1;
    else if (id==simulation::production_profile_name) d.world.profile=simulation::RulesProfile::ProductionV2;
    else if (id==simulation::household_profile_name) d.world.profile=simulation::RulesProfile::HouseholdV3;
    else if (id==simulation::settlement_profile_name) d.world.profile=simulation::RulesProfile::SettlementV4;
    else if (id==simulation::industry_profile_name) d.world.profile=simulation::RulesProfile::IndustryV5;
    else if (id==simulation::city_profile_name) d.world.profile=simulation::RulesProfile::CityV6;
    else if (id==simulation::city_v7_profile_name) d.world.profile=simulation::RulesProfile::CityV7;
    else if (id==simulation::city_v8_profile_name) d.world.profile=simulation::RulesProfile::CityV8;
    else if (id==simulation::city_v9_profile_name) d.world.profile=simulation::RulesProfile::CityV9;
    else if (id==simulation::city_v10_profile_name) d.world.profile=simulation::RulesProfile::CityV10;
    else if (id==simulation::city_v11_profile_name) d.world.profile=simulation::RulesProfile::CityV11;
    else if (id==simulation::city_v12_profile_name) d.world.profile=simulation::RulesProfile::CityV12;
    else if (id==simulation::city_v13_profile_name) d.world.profile=simulation::RulesProfile::CityV13;
    else if (id==simulation::city_v14_profile_name) d.world.profile=simulation::RulesProfile::CityV14;
    else if (id==simulation::city_v15_profile_name) d.world.profile=simulation::RulesProfile::CityV15;
    else if (id==simulation::city_v16_profile_name) d.world.profile=simulation::RulesProfile::CityV16;
    else throw std::runtime_error("unknown sandbox rule ID");
    d.world.rule_version=static_cast<std::uint32_t>(number(field(r,"version"),UINT32_MAX));
    require((d.world.profile==simulation::RulesProfile::ProductionV2 ?
                d.world.rule_version==(schema==1 ? 1U:2U):
                simulation::rule_version_supported(d.world.profile,d.world.rule_version)) &&
            (d.world.profile!=simulation::RulesProfile::HouseholdV3 || schema>=3) &&
            (d.world.profile!=simulation::RulesProfile::SettlementV4 || schema==4) &&
            (d.world.profile!=simulation::RulesProfile::IndustryV5 || schema==5) &&
            (d.world.profile!=simulation::RulesProfile::CityV6 || schema==6) &&
            (d.world.profile!=simulation::RulesProfile::CityV7 || schema==7) &&
            (d.world.profile!=simulation::RulesProfile::CityV8 || schema==8) &&
            (d.world.profile!=simulation::RulesProfile::CityV9 || schema==9) &&
            (d.world.profile!=simulation::RulesProfile::CityV10 || schema==10) &&
            (d.world.profile!=simulation::RulesProfile::CityV12 || schema==14) &&
            (d.world.profile!=simulation::RulesProfile::CityV13 || schema==15) &&
            (d.world.profile!=simulation::RulesProfile::CityV14 || schema==16) &&
            (d.world.profile!=simulation::RulesProfile::CityV15 || schema==17) &&
            (d.world.profile!=simulation::RulesProfile::CityV16 || schema==18 || schema==19 || schema==20 || schema==21 || schema==22) &&
            (d.world.profile!=simulation::RulesProfile::CityV11 ||
                (schema==11 ? d.world.rule_version<=2 :
                 (schema==12 && d.world.rule_version==3) ||
                 (schema==13 && d.world.rule_version==4))) &&
            (schema!=5 || d.world.profile==simulation::RulesProfile::IndustryV5) &&
            (schema!=6 || d.world.profile==simulation::RulesProfile::CityV6) &&
            (schema!=7 || d.world.profile==simulation::RulesProfile::CityV7) &&
            (schema!=8 || d.world.profile==simulation::RulesProfile::CityV8) &&
            (schema!=9 || d.world.profile==simulation::RulesProfile::CityV9) &&
            (schema!=10 || d.world.profile==simulation::RulesProfile::CityV10) &&
            (schema!=11 || (d.world.profile==simulation::RulesProfile::CityV11 &&
                            d.world.rule_version<=2)) &&
            (schema!=12 || (d.world.profile==simulation::RulesProfile::CityV11 &&
                            d.world.rule_version==3)) &&
            (schema!=13 || (d.world.profile==simulation::RulesProfile::CityV11 &&
                            d.world.rule_version==4)) &&
            (schema!=14 || (d.world.profile==simulation::RulesProfile::CityV12 &&
                            d.world.rule_version==1)) &&
            (schema!=15 || (d.world.profile==simulation::RulesProfile::CityV13 &&
                            d.world.rule_version==1)) &&
            (schema!=16 || (d.world.profile==simulation::RulesProfile::CityV14 &&
                            d.world.rule_version==1)),
            "unsupported sandbox rule version");
    require(schema!=17 || (d.world.profile==simulation::RulesProfile::CityV15 &&
            d.world.rule_version==1),"schema 17 requires City-v15 rule 1");
    require(schema!=18 || (d.world.profile==simulation::RulesProfile::CityV16 &&
            (d.world.rule_version==1 || d.world.rule_version==2)),"schema 18 requires City-v16 rule 1 or 2");
    require(schema!=19 || (d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==3),"schema 19 requires City-v16 rule 3");
    require(schema!=20 || (d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==4),"schema 20 requires City-v16 rule 4");
    require(schema!=21 || (d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==5),"schema 21 requires City-v16 rule 5");
    require(schema!=22 || (d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==6),"schema22 requires City-v16 rule6");
    require(syntax==SnapshotSyntax::Frozen || (d.world.profile==simulation::RulesProfile::CityV16 && d.world.rule_version==6),
            "active syntax is restricted to the local City-v16 rule6 projection");
    auto parsed=parse_snapshot(field(j,"world"),schema,d.world.profile,syntax);
    parsed.profile=d.world.profile; parsed.rule_version=d.world.rule_version;
    if (schema==1 && parsed.profile==simulation::RulesProfile::ProductionV2) {
        parsed.rule_version=2;
        d.migrated_from_schema1=true;
    }
    if (schema==1 && parsed.profile==simulation::RulesProfile::LogisticsV1)
        d.migrated_from_schema1=true;
    d.world=std::move(parsed);
    require(number(field(m,"grid_width"),512)==static_cast<std::uint64_t>(d.world.width) &&
            number(field(m,"grid_height"),512)==static_cast<std::uint64_t>(d.world.height),
            "map grid and world grid disagree");
    return d;
}
} // namespace

SaveDocument make_document(const fs::path& root,const fs::path& relative,
                           const std::vector<std::uint8_t>& mask,const simulation::World& world) {
    safe_relative(relative);
    const auto map=maps::resolve_map_path(root,relative);
    SaveDocument d;
    d.map_relative=relative;d.map_sha256=hash_file(map);
    d.buildable_sha256=digest(mask.data(),mask.size());d.world=world.snapshot();
    if (simulation::original_production_enabled(d.world.profile,d.world.rule_version)) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        require(static_cast<bool>(d.prepared_original_rules_model),"schema26 requires a prepared original rules model");
        require(d.prepared_permissions && d.prepared_permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion,
                "schema26 requires complete immutable policy3 map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        if (simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_cardinal_water_enabled(d.world.profile,d.world.rule_version)) {
            d.prepared_original_terrain_facts=world.original_terrain_facts();
            require(d.prepared_original_terrain_facts && d.prepared_original_terrain_facts->source_map_sha256()==d.map_sha256,
                simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema34 World terrain context differs from complete source-map SHA-256":
                simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema33 World terrain context differs from complete source-map SHA-256":
                simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema32 World terrain context differs from complete source-map SHA-256":
                simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version) ?
                    "schema31 World terrain context differs from complete source-map SHA-256":
                    "schema30 World terrain context differs from complete source-map SHA-256");
            if (simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_deadline_food_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_deadline_service_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_deadline_fire_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_inspector_coordination(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else (void)restore_original_cardinal_water(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
        } else if (simulation::original_market_logistics_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_market_logistics(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else if (simulation::original_sustainable_economy_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_sustainable_economy(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else if (simulation::original_fire_protection_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_fire_protection(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else (void)restore_original_production_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        return d;
    }
    if (simulation::original_goods_enabled(d.world.profile,d.world.rule_version)) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        require(static_cast<bool>(d.prepared_original_rules_model),"schema25 requires a prepared original rules model");
        require(d.prepared_permissions && d.prepared_permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion,
                "schema25 requires complete immutable policy3 map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        (void)restore_original_goods_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        return d;
    }
    if (simulation::original_influence_enabled(d.world.profile,d.world.rule_version)) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        require(static_cast<bool>(d.prepared_original_rules_model),"schema24 requires a prepared original rules model");
        require(d.prepared_permissions && d.prepared_permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion,
                "schema24 requires complete immutable policy3 map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        (void)restore_original_influence_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        return d;
    }
    if (world.original_economy_enabled()) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        require(static_cast<bool>(d.prepared_original_rules_model),"schema23 requires a prepared original rules model");
        require(d.prepared_permissions && d.prepared_permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion,
                "schema23 requires complete immutable policy3 map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        (void)restore_original_economy_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        return d;
    }
    // Validate the snapshot against the actual mask before publishing it.
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        (d.world.rule_version>=3 && d.world.rule_version<=6)) {
        d.prepared_permissions=world.map_permissions();
        require(static_cast<bool>(d.prepared_permissions),"City-v16 map-integrated rules require prepared map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        (void)simulation::World::restore(d.world,d.prepared_permissions);
    } else (void)simulation::World::restore(d.world,mask);
    return d;
}
SaveDocument read_save(const fs::path& path) {
    performance::increment(performance::Counter::FileReads);
    std::error_code error;
    const auto size=fs::file_size(path,error);
    require(!error && size>0 && size<=max_save_bytes,"save file is missing, empty or exceeds 8 MiB");
    std::ifstream input(path,std::ios::binary);
    require(input.good(),"cannot open save file");
    std::string bytes(static_cast<std::size_t>(size),'\0');
    input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    require(input.gcount()==static_cast<std::streamsize>(bytes.size()) && !input.bad(),"truncated save file");
    require(input.peek()==std::char_traits<char>::eof(),"save file changed while reading");
    // JSON's callback sees container starts before conversion, bounding recursion depth.
    std::vector<std::set<std::string>> object_keys;
    bool duplicate_key=false;
    const auto j=json::parse(bytes,[&](int depth,json::parse_event_t event,json& parsed) {
        if ((event==json::parse_event_t::object_start || event==json::parse_event_t::array_start) &&
            depth>32) throw std::runtime_error("save JSON nesting exceeds 32");
        if (event==json::parse_event_t::object_start) object_keys.emplace_back();
        else if (event==json::parse_event_t::key) {
            require(!object_keys.empty(),"invalid save JSON object nesting");
            if (!object_keys.back().insert(parsed.get<std::string>()).second) duplicate_key=true;
        } else if (event==json::parse_event_t::object_end) {
            require(!object_keys.empty(),"invalid save JSON object nesting");
            object_keys.pop_back();
        }
        return true;
    });
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==23)
        require(!duplicate_key,"schema23 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==24)
        require(!duplicate_key,"schema24 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==25)
        require(!duplicate_key,"schema25 duplicate save key");
    if (j.is_object() && j.contains("schema_version") &&
        (j.at("schema_version")==26 || j.at("schema_version")==27 || j.at("schema_version")==28 || j.at("schema_version")==29 || j.at("schema_version")==30 || j.at("schema_version")==31 || j.at("schema_version")==32 || j.at("schema_version")==33 || j.at("schema_version")==34))
        require(!duplicate_key,"original production duplicate save key");
    return parse_document(j);
}
simulation::OriginalModelSource original_model_source(const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid original model difficulty");
    const auto result=original::load_model_data(root);
    require(static_cast<bool>(result),"schema23 original model source is unavailable or invalid");
    const auto& provenance=result.model->provenance();
    require(provenance.source_kind==original::ModelSourceKind::File && valid_hash(provenance.sha256),
            "schema23 requires a verified file model source");
    return {provenance.format_version,provenance.sha256,
            static_cast<std::uint64_t>(provenance.byte_count),difficulty};
}
std::shared_ptr<const original::OriginalRulesModel> prepare_original_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid original model difficulty");
    auto loaded=original::load_model_data(root);
    if (!loaded) throw std::runtime_error("original rules model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_original_production_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid original production model difficulty");
    auto loaded=original::load_model_data(root);
    if (!loaded) throw std::runtime_error("original production model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_production(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const simulation::OriginalTerrainFacts> prepare_original_terrain_facts(
        const fs::path& root,const fs::path& relative) {
    safe_relative(relative);
    const auto path=maps::resolve_map_path(root,relative);
    const auto container=maps::EmperorContainer::open(path);
    // Fingerprint the same immutable bytes that the shared parser decodes,
    // rather than a separate pre-open pathname read (which could race an ABA
    // replacement). Recheck the live path after complete preparation below.
    const auto source_hash=maps::map_input_sha256(container);
    require(!container.multipart() && container.parts().size()==1,
            "cardinal-water facts require one complete standalone original map part");
    const auto map=maps::read_emperor_map(container,0);
    require(map.stored_width==maps::stored_grid_width && map.stored_height==maps::stored_grid_height &&
            map.terrain_raw.logical_offset==maps::terrain_logical_offset &&
            map.terrain_raw.values.size()==maps::stored_grid_width*maps::stored_grid_height &&
            maps::MapGeometry(map.declared_map_size).supported,
            "cardinal-water facts require the complete supported original terrain storage profile");
    require(hash_file(path)==source_hash,"original map changed while preparing immutable terrain facts");
    return std::make_shared<const simulation::OriginalTerrainFacts>(map.terrain_raw.values,source_hash);
}
SaveDocument upgrade_city_v11_v2_to_v3(const SaveDocument& source) {
    require(source.world.profile==simulation::RulesProfile::CityV11 &&
            source.world.rule_version==2,
            "only City-v11 rule version 2 saves can enable operation controls");
    SaveDocument upgraded=source;
    upgraded.world.rule_version=3;
    upgraded.source_schema_version=12;
    for (auto& building:upgraded.world.buildings) {
        building.operating_enabled=true;
        building.workforce_priority=simulation::WorkforcePriority::Normal;
    }
    return upgraded;
}
SaveDocument upgrade_city_v11_v3_to_v4(const SaveDocument& source) {
    require(source.world.profile==simulation::RulesProfile::CityV11 &&
            source.world.rule_version==3 && source.world.demolition_history==simulation::DemolitionHistory{},
            "only unchanged City-v11 rule version 3 saves can enable demolition");
    SaveDocument upgraded=source;
    upgraded.world.rule_version=4;
    upgraded.source_schema_version=13;
    return upgraded;
}
simulation::World restore_save(const SaveDocument& d,const fs::path& root,
                               std::vector<std::uint8_t> mask) {
    return restore_save(d,root,std::move(mask),d.prepared_permissions);
}
std::string map_permissions_fingerprint(const simulation::MapPermissions& permissions,
                                        const std::string& map_sha256) {
    require(valid_hash(map_sha256),"invalid original-map fingerprint for map permissions");
    require(simulation::supported_map_permissions_policy(permissions.policy_version()),
            "unsupported map-permissions policy version");
    const auto framed=std::string(permissions.policy_version()==simulation::kMapPermissionsPolicyVersion ?
        "openemperor-map-permissions-v1\n":permissions.policy_version()==simulation::kOriginalRoadsMapPermissionsPolicyVersion ?
        "openemperor-map-permissions-v2\n":"openemperor-map-permissions-v3\n")+
        std::to_string(permissions.policy_version())+"\n"+map_sha256+"\n"+permissions.canonical_state();
    return digest(reinterpret_cast<const unsigned char*>(framed.data()),framed.size());
}
simulation::World restore_save(const SaveDocument& d,const fs::path& root,
                               std::vector<std::uint8_t> mask,
                               std::shared_ptr<const simulation::MapPermissions> permissions) {
    require(!simulation::original_production_enabled(d.world.profile,d.world.rule_version),
            "City-v16 rule10 requires explicit schema26 development restore");
    require(!simulation::original_goods_enabled(d.world.profile,d.world.rule_version),
            "City-v16 rule9 requires explicit schema25 development restore");
    require(!simulation::original_influence_enabled(d.world.profile,d.world.rule_version),
            "City-v16 rule8 requires explicit schema24 development restore");
    require(!simulation::original_economy_enabled(d.world.profile,d.world.rule_version),
            "City-v16 rule 7 runtime is not ready; use explicit schema23 development restore");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        (d.world.rule_version>=3 && d.world.rule_version<=6)) {
        const auto expected_schema=d.world.rule_version==3 ? 19U:d.world.rule_version==4 ? 20U:d.world.rule_version==5 ? 21U:22U;
        const auto expected_policy=d.world.rule_version==3 ? simulation::kMapPermissionsPolicyVersion:
            d.world.rule_version==4 ? simulation::kOriginalRoadsMapPermissionsPolicyVersion:simulation::kBridgesMapPermissionsPolicyVersion;
        require(d.source_schema_version==0 || d.source_schema_version==expected_schema,
                d.world.rule_version==3 ? "City-v16 rule 3 requires schema19; legacy saves are not migrated":
                    d.world.rule_version==4 ? "City-v16 rule 4 requires schema20; legacy saves are not migrated":
                    "City-v16 rule 5 requires schema21; legacy saves are not migrated");
        require(d.map_permissions_policy_version==expected_policy,
                "unsupported saved map-permissions policy version");
        if (!permissions)
            permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,d.map_permissions_policy_version);
        require(permissions && permissions->policy_version()==d.map_permissions_policy_version,
                "prepared map-permissions policy differs from save");
        require(valid_hash(d.map_permissions_sha256) &&
                map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
                "map-permissions SHA-256 differs from save (permissions or topology changed)");
        return simulation::World::restore(d.world,std::move(permissions));
    }
    require(d.map_permissions_policy_version==0 && d.map_permissions_sha256.empty(),
            "saved map-permissions policy requires matching City-v16 map-integrated rules");
    return simulation::World::restore(d.world,std::move(mask));
}
simulation::World restore_original_economy_development(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalEconomyRuleVersion,
            "schema23 development restore requires City-v16 rule7");
    require(d.source_schema_version==0 || d.source_schema_version==23,
            "rule7 requires schema23; legacy saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema23 requires an original model source identity");
    (void)original_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=prepare_original_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
            source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
            "schema23 prepared original rules model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,
            "schema23 live original model source identity differs from save");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
            d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion,
            "schema23 requires matching immutable policy3 map permissions");
    require(map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
            "map-permissions SHA-256 differs from save (permissions or topology changed)");
    return simulation::World::restore_original_economy_development(d.world,std::move(permissions),std::move(model));
}
simulation::World restore_original_influence_development(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalInfluenceRuleVersion,
            "schema24 development restore requires City-v16 rule8");
    require(d.source_schema_version==0 || d.source_schema_version==24,
            "rule8 requires schema24; existing saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema24 requires an original model source identity");
    (void)influence_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=prepare_original_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
            source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
            "schema24 prepared original rules model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,
            "schema24 live original model source identity differs from save");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
            d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion,
            "schema24 requires matching immutable policy3 map permissions");
    require(map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
            "map-permissions SHA-256 differs from save (permissions or topology changed)");
    return simulation::World::restore_original_influence_development(d.world,std::move(permissions),std::move(model));
}
simulation::World restore_original_goods_development(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    require(d.world.profile==simulation::RulesProfile::CityV16 &&
            d.world.rule_version==simulation::kOriginalGoodsRuleVersion,
            "schema25 development restore requires City-v16 rule9");
    require(d.source_schema_version==0 || d.source_schema_version==25,
            "rule9 requires schema25; existing saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema25 requires an original model source identity");
    (void)goods_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=prepare_original_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
            source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
            "schema25 prepared original rules model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,
            "schema25 live original model source identity differs from save");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
            d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion,
            "schema25 requires matching immutable policy3 map permissions");
    require(map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
            "map-permissions SHA-256 differs from save (permissions or topology changed)");
    return simulation::World::restore_original_goods_development(d.world,std::move(permissions),std::move(model));
}
simulation::World restore_original_production_development(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    require(d.world.profile==simulation::RulesProfile::CityV16 && d.world.rule_version==simulation::kOriginalProductionRuleVersion,
            "schema26 development restore requires City-v16 rule10");
    require(d.source_schema_version==0 || d.source_schema_version==26,
            "rule10 requires schema26; existing saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema26 requires an original model source identity");
    (void)production_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=prepare_original_production_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
            source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
            "schema26 prepared original rules model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,
            "schema26 live original model source identity differs from save");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
            d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion,
            "schema26 requires matching immutable policy3 map permissions");
    require(map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
            "map-permissions SHA-256 differs from save (permissions or topology changed)");
    return simulation::World::restore_original_production_development(d.world,std::move(permissions),std::move(model));
}
namespace {
simulation::World restore_original_fire_generation(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,std::uint32_t rule_version,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain=nullptr) {
    const bool food=rule_version==simulation::kOriginalDeadlineFoodDispatchRuleVersion;
    const bool service=rule_version==simulation::kOriginalDeadlineServiceDispatchRuleVersion;
    const bool deadline=rule_version==simulation::kOriginalDeadlineFireDispatchRuleVersion;
    const bool coordination=rule_version==simulation::kOriginalInspectorCoordinationRuleVersion;
    const bool cardinal=rule_version==simulation::kOriginalCardinalWaterRuleVersion;
    const bool logistics=rule_version==simulation::kOriginalMarketLogisticsRuleVersion;
    const bool sustainable=rule_version==simulation::kOriginalSustainableRuleVersion;
    const auto schema=food ? 34U:service ? 33U:deadline ? 32U:coordination ? 31U:cardinal ? 30U:logistics ? 29U:sustainable ? 28U:27U;
    require(d.world.profile==simulation::RulesProfile::CityV16 && d.world.rule_version==rule_version,
            "original fire restore requires its exact explicit City-v16 rule generation");
    require(d.source_schema_version==0 || d.source_schema_version==schema,
            "original fire restore requires its exact schema; existing saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema27 requires an original model source identity");
    (void)production_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=prepare_original_production_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
            source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
            "schema27 prepared original rules model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,
            "schema27 live original model source identity differs from save");
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
            d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion,
            "schema27 requires matching immutable policy3 map permissions");
    require(map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
            "map-permissions SHA-256 differs from save (permissions or topology changed)");
    if (cardinal || coordination || deadline || service || food) {
        // Re-read source facts even when a session dependency was provided.
        // Claimed source hashes or JSON-derived bitsets never become authority.
        auto live=prepare_original_terrain_facts(root,d.map_relative);
        require(live->source_map_sha256()==d.map_sha256,
                food ? "schema34 live terrain source SHA-256 differs from save":
                service ? "schema33 live terrain source SHA-256 differs from save":
                deadline ? "schema32 live terrain source SHA-256 differs from save":
                coordination ? "schema31 live terrain source SHA-256 differs from save":
                    "schema30 live terrain source SHA-256 differs from save");
        if (terrain) require(*terrain==*live,food ? "schema34 prepared terrain facts differ from live original source":
            service ? "schema33 prepared terrain facts differ from live original source":
            deadline ? "schema32 prepared terrain facts differ from live original source":
            coordination ? "schema31 prepared terrain facts differ from live original source":
            "schema30 prepared terrain facts differ from live original source");
        if (d.prepared_original_terrain_facts)
            require(*d.prepared_original_terrain_facts==*live,food ? "schema34 document terrain facts differ from live original source":
                service ? "schema33 document terrain facts differ from live original source":
                deadline ? "schema32 document terrain facts differ from live original source":
                coordination ? "schema31 document terrain facts differ from live original source":
                "schema30 document terrain facts differ from live original source");
        if (food)
            return simulation::World::restore_original_deadline_food_dispatch(d.world,std::move(permissions),std::move(model),std::move(live));
        if (service)
            return simulation::World::restore_original_deadline_service_dispatch(d.world,std::move(permissions),std::move(model),std::move(live));
        if (deadline)
            return simulation::World::restore_original_deadline_fire_dispatch(d.world,std::move(permissions),std::move(model),std::move(live));
        if (coordination)
            return simulation::World::restore_original_inspector_coordination(d.world,std::move(permissions),std::move(model),std::move(live));
        return simulation::World::restore_original_cardinal_water(d.world,std::move(permissions),std::move(model),std::move(live));
    }
    if (logistics) return simulation::World::restore_original_market_logistics(d.world,std::move(permissions),std::move(model));
    return sustainable ? simulation::World::restore_original_sustainable_economy(d.world,std::move(permissions),std::move(model)):
        simulation::World::restore_original_fire_protection(d.world,std::move(permissions),std::move(model));
}
}
simulation::World restore_original_fire_protection(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalFireProtectionRuleVersion);
}
simulation::World restore_original_sustainable_economy(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalSustainableRuleVersion);
}
simulation::World restore_original_market_logistics(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalMarketLogisticsRuleVersion);
}
simulation::World restore_original_cardinal_water(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalCardinalWaterRuleVersion,std::move(terrain));
}
simulation::World restore_original_inspector_coordination(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalInspectorCoordinationRuleVersion,std::move(terrain));
}
simulation::World restore_original_deadline_fire_dispatch(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalDeadlineFireDispatchRuleVersion,std::move(terrain));
}
simulation::World restore_original_deadline_service_dispatch(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalDeadlineServiceDispatchRuleVersion,std::move(terrain));
}
simulation::World restore_original_deadline_food_dispatch(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalDeadlineFoodDispatchRuleVersion,std::move(terrain));
}
void validate_save_target(const fs::path& path,const fs::path& root) {
    require(!path.empty(),"save path is empty");
    const auto absolute=fs::absolute(path).lexically_normal();
    const auto data=fs::canonical(root);
    require(!below(absolute,data),"save target is inside original data directory");
    fs::path current=absolute.root_path();
    for (const auto& component: absolute.relative_path()) {
        current/=component;
        std::error_code error;
        if (fs::is_symlink(fs::symlink_status(current,error)))
            throw std::runtime_error("save target path contains a symlink");
    }
    const auto parent=absolute.parent_path();
    const auto resolved=fs::weakly_canonical(parent);
    require(!below(resolved,data),"save target resolves inside original data directory");
    const auto status=fs::symlink_status(absolute);
    require(!fs::exists(status) || fs::is_regular_file(status),"save target is not a regular file");
}
void write_save(const fs::path& path,const SaveDocument& d,const fs::path& root,
                const std::vector<std::uint8_t>& mask,WriteFault fault) {
    performance::increment(performance::Counter::FileWrites);
    validate_save_target(path,root);
    // Recheck original inputs and all world invariants before writing any bytes.
    if (simulation::original_production_enabled(d.world.profile,d.world.rule_version))
        if (simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_deadline_food_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_deadline_service_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_deadline_fire_dispatch(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_inspector_coordination(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_cardinal_water_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_cardinal_water(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_market_logistics_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_market_logistics(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else if (simulation::original_sustainable_economy_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_sustainable_economy(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else if (simulation::original_fire_protection_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_fire_protection(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
        else (void)restore_original_production_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
    else if (simulation::original_goods_enabled(d.world.profile,d.world.rule_version))
        (void)restore_original_goods_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
    else if (simulation::original_influence_enabled(d.world.profile,d.world.rule_version))
        (void)restore_original_influence_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
    else if (simulation::original_economy_enabled(d.world.profile,d.world.rule_version))
        (void)restore_original_economy_development(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model);
    else (void)restore_save(d,root,mask);
    const auto bytes=document_json(d).dump(2);
    require(bytes.size()<=max_save_bytes,"save JSON exceeds 8 MiB");
    const auto target=fs::absolute(path).lexically_normal();
    fs::create_directories(target.parent_path());
    validate_save_target(target,root);
    platform::atomic_replace(target,bytes,[&] { validate_save_target(target,root); },fault);
}
} // namespace openemperor::persistence
