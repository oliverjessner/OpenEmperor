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
    require(!j.contains("source_cell_ramps") && !j.contains("next_source_cell_ramp_id"),
            "source-cell ramp authority requires exact schema36");
    require(!j.contains("land_trade"),"land trade authority requires exact schema37");
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
json agriculture_transfer_json(simulation::OriginalGoodsTransferKey key) {
    return {{"courier_id",static_cast<std::uint32_t>(key.courier_id)},{"sequence",key.sequence}};
}
template<std::size_t Count> json agriculture_quality_json(const simulation::BasicAgricultureQualityWitness<Count>& w) {
    return {{"transfer",agriculture_transfer_json(w.transfer)},{"tick",w.tick},{"accepted",w.accepted},
        {"consumed_before",w.consumed_before},{"previous_quality",w.previous_quality}};
}
template<std::size_t Count> json agriculture_json(const simulation::BasicAgricultureState<Count>& state) {
    using namespace simulation;
    require(state.farms.size()<=Rules::original_production_building_limit &&
            state.fields.size()<=agriculture_field_limit &&
            state.stocks.size()<=Rules::original_production_building_limit &&
            state.trips.size()<=Rules::original_production_courier_limit &&
            state.housing_evaluations.size()<=Rules::city_v10_household_limit*16,
            "schema40 Agriculture collection exceeds bounded context");
    json farms=json::array(),fields=json::array(),stocks=json::array(),trips=json::array(),housing=json::array();
    for (const auto& row:state.farms) farms.push_back({{"farm_id",static_cast<std::uint32_t>(row.farm_id)}});
    for (const auto& row:state.fields) fields.push_back({{"field_id",static_cast<std::uint32_t>(row.field_id)},
        {"farm_id",static_cast<std::uint32_t>(row.farm_id)},{"crop",static_cast<unsigned>(row.crop)},
        {"quantity",row.quantity},{"productive_events",row.productive_events},{"harvested",row.harvested},
        {"last_harvest_cycle",row.last_harvest_cycle}});
    for (const auto& row:state.stocks) stocks.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
        {"amounts",row.amounts},{"received",row.received},{"departed",row.departed},{"returned",row.returned},
        {"consumed",row.consumed},{"missed_demands",row.missed_demands},
        {"quality_witness",row.quality_witness ? agriculture_quality_json(*row.quality_witness):json(nullptr)}});
    for (const auto& row:state.trips) trips.push_back({{"transfer",agriculture_transfer_json(row.transfer)},
        {"source",static_cast<std::uint32_t>(row.source)},{"target",static_cast<std::uint32_t>(row.target)},
        {"phase",static_cast<unsigned>(row.phase)},{"loaded",row.loaded},{"delivered",row.delivered},{"returned",row.returned}});
    for (const auto& row:state.housing_evaluations) housing.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
        {"tick",row.tick},{"quality_witness",row.quality_witness ? agriculture_quality_json(*row.quality_witness):json(nullptr)},
        {"received",row.received},{"consumed",row.consumed}});
    return {{"revision",Count==agriculture_crop_count ? agriculture_revision:Count==fishing_food_count ? fishing_food_revision:Count==processed_food_count ? processed_food_revision:hunting_food_revision},
        {"catalog",Count==agriculture_crop_count ? agriculture_catalog:Count==fishing_food_count ? fishing_food_catalog:Count==processed_food_count ? processed_food_catalog:hunting_food_catalog},{"farms",std::move(farms)},
        {"fields",std::move(fields)},{"stocks",std::move(stocks)},{"trips",std::move(trips)},
        {"housing_evaluations",std::move(housing)}};
}
template<std::size_t Count> simulation::BasicAgricultureState<Count> parse_agriculture_width(const json& j) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==(Count==agriculture_crop_count ? agriculture_revision:Count==fishing_food_count ? fishing_food_revision:Count==processed_food_count ? processed_food_revision:hunting_food_revision) &&
            str(field(j,"catalog"))==(Count==agriculture_crop_count ? agriculture_catalog:Count==fishing_food_count ? fishing_food_catalog:Count==processed_food_count ? processed_food_catalog:hunting_food_catalog),"schema40 requires its exact Agriculture revision and catalog");
    const auto id=[](const json& value) {
        const auto n=number(value,UINT32_MAX);require(n>0,"schema40 Agriculture identity must be positive");
        return static_cast<BuildingId>(n);
    };
    const auto transfer=[](const json& value) {
        OriginalGoodsTransferKey key;
        key.courier_id=static_cast<CourierId>(number(field(value,"courier_id"),UINT32_MAX));
        key.sequence=number(field(value,"sequence"));
        require(static_cast<std::uint32_t>(key.courier_id)>0 && key.sequence>0,
                "schema40 Agriculture transfer requires positive actor and sequence");
        return key;
    };
    const auto amounts=[](const json& value) {
        std::array<std::int32_t,Count> out{};
        require(value.is_array() && value.size()==out.size(),"schema40 Agriculture requires exactly five amounts");
        for (std::size_t i=0;i<out.size();++i) out[i]=static_cast<std::int32_t>(number(value[i],INT32_MAX));
        return out;
    };
    const auto counters=[](const json& value) {
        std::array<std::uint64_t,Count> out{};
        require(value.is_array() && value.size()==out.size(),"schema40 Agriculture requires exactly five counters");
        for (std::size_t i=0;i<out.size();++i) out[i]=number(value[i]);
        return out;
    };
    const auto quality=[&](const json& value) {
        BasicAgricultureQualityWitness<Count> w;w.transfer=transfer(field(value,"transfer"));w.tick=number(field(value,"tick"));
        w.accepted=amounts(field(value,"accepted"));w.consumed_before=counters(field(value,"consumed_before"));
        w.previous_quality=static_cast<std::uint8_t>(number(field(value,"previous_quality"),UINT8_MAX));
        return w;
    };
    BasicAgricultureState<Count> state;
    const auto& farms=field(j,"farms");
    require(farms.is_array() && farms.size()<=Rules::original_production_building_limit,"schema40 Farm collection exceeds bound");
    for (const auto& value:farms) {
        AgricultureFarm row;row.farm_id=id(field(value,"farm_id"));
        require(state.farms.empty() || state.farms.back().farm_id<row.farm_id,"schema40 Farm IDs must be sorted and unique");
        state.farms.push_back(row);
    }
    const auto& fields=field(j,"fields");
    require(fields.is_array() && fields.size()<=agriculture_field_limit,"schema40 Field collection exceeds bound");
    for (const auto& value:fields) {
        AgricultureField row;row.field_id=id(field(value,"field_id"));row.farm_id=id(field(value,"farm_id"));
        require(state.fields.empty() || state.fields.back().field_id<row.field_id,"schema40 Field IDs must be sorted and unique");
        row.crop=static_cast<AgricultureCrop>(number(field(value,"crop"),agriculture_crop_count-1));
        row.quantity=static_cast<std::uint32_t>(number(field(value,"quantity"),UINT32_MAX));
        row.productive_events=number(field(value,"productive_events"));row.harvested=number(field(value,"harvested"));
        row.last_harvest_cycle=number(field(value,"last_harvest_cycle"));state.fields.push_back(row);
    }
    const auto& stocks=field(j,"stocks");
    require(stocks.is_array() && stocks.size()<=Rules::original_production_building_limit,"schema40 Food-stock collection exceeds bound");
    for (const auto& value:stocks) {
        BasicAgricultureStock<Count> row;row.building_id=id(field(value,"building_id"));
        require(state.stocks.empty() || state.stocks.back().building_id<row.building_id,"schema40 Food-stock IDs must be sorted and unique");
        row.amounts=amounts(field(value,"amounts"));row.received=counters(field(value,"received"));
        row.departed=counters(field(value,"departed"));row.returned=counters(field(value,"returned"));
        row.consumed=counters(field(value,"consumed"));row.missed_demands=number(field(value,"missed_demands"));
        const auto& witness=field(value,"quality_witness");if (!witness.is_null()) row.quality_witness=quality(witness);
        state.stocks.push_back(row);
    }
    const auto& trips=field(j,"trips");
    require(trips.is_array() && trips.size()<=Rules::original_production_courier_limit,"schema40 Food-trip collection exceeds bound");
    for (const auto& value:trips) {
        BasicAgricultureTrip<Count> row;row.transfer=transfer(field(value,"transfer"));
        require(state.trips.empty() || state.trips.back().transfer.courier_id<row.transfer.courier_id,
                "schema40 Food-trip courier IDs must be sorted and unique");
        row.source=id(field(value,"source"));row.target=id(field(value,"target"));
        row.phase=static_cast<OriginalGoodsTransferPhase>(number(field(value,"phase"),5));
        row.loaded=amounts(field(value,"loaded"));row.delivered=amounts(field(value,"delivered"));
        row.returned=amounts(field(value,"returned"));state.trips.push_back(row);
    }
    const auto& housing=field(j,"housing_evaluations");
    require(housing.is_array() && housing.size()<=Rules::city_v10_household_limit*16,"schema40 Housing Food evidence exceeds bound");
    for (const auto& value:housing) {
        BasicAgricultureHousingEvaluation<Count> row;row.building_id=id(field(value,"building_id"));row.tick=number(field(value,"tick"));
        require(state.housing_evaluations.empty() ||
                state.housing_evaluations.back().building_id<row.building_id ||
                (state.housing_evaluations.back().building_id==row.building_id && state.housing_evaluations.back().tick<row.tick),
                "schema40 Housing Food evidence must have sorted unique House/tick identities");
        const auto& witness=field(value,"quality_witness");if (!witness.is_null()) row.quality_witness=quality(witness);
        row.received=counters(field(value,"received"));row.consumed=counters(field(value,"consumed"));
        state.housing_evaluations.push_back(row);
    }
    require(canonical_fields(j,agriculture_json(state)),"schema40 contains unknown or noncanonical Agriculture fields");
    return state;
}
simulation::AgricultureState parse_agriculture(const json& j) {
    return parse_agriculture_width<simulation::agriculture_crop_count>(j);
}
simulation::FishingFoodState parse_fishing_food(const json& j) {
    return parse_agriculture_width<simulation::fishing_food_count>(j);
}
json fishing_json(const simulation::FishingState& state) {
    using namespace simulation;
    require(state.quays.size()<=fishing_quay_limit && state.boats.size()<=fishing_quay_limit &&
        state.sites.size()<=fishing_site_limit,"schema44 Fishing collection exceeds bound");
    json quays=json::array(),boats=json::array(),sites=json::array();
    for(const auto& q:state.quays) quays.push_back({{"quay_id",static_cast<std::uint32_t>(q.quay_id)},
        {"orientation",q.orientation},{"unloaded",q.unloaded},{"boat",q.boat ? json(q.boat->value):json(nullptr)}});
    for(const auto& b:state.boats) boats.push_back({{"id",b.id.value},{"quay_id",static_cast<std::uint32_t>(b.quay_id)},
        {"site_index",b.site_index},{"phase",static_cast<unsigned>(b.phase)},
        {"movement",static_cast<unsigned>(b.movement)},{"current",cell_json(b.current)},
        {"path",cells_json(b.path)},{"path_vertex",b.path_vertex},{"edge_progress",b.edge_progress},
        {"work_progress",b.work_progress},{"cargo",b.cargo},{"reserved",b.reserved},
        {"route_revision",b.route_revision},{"launched_tick",b.launched_tick},{"return_requested",b.return_requested}});
    for(const auto& site:state.sites) sites.push_back({{"cell",cell_json(site.cell)},{"stock",site.stock},
        {"regeneration_progress",site.regeneration_progress},{"regenerated",site.regenerated},
        {"caught",site.caught},{"source_marker",site.source_marker}});
    return {{"quays",std::move(quays)},{"boats",std::move(boats)},{"sites",std::move(sites)},
        {"next_boat_id",state.next_boat_id},{"launched",state.launched},{"retired",state.retired}};
}
simulation::FishingState parse_fishing(const json& j) {
    using namespace simulation;
    FishingState state;state.next_boat_id=number(field(j,"next_boat_id"));
    state.launched=number(field(j,"launched"));state.retired=number(field(j,"retired"));
    const auto id=[](const json& v){const auto n=number(v,UINT32_MAX);require(n>0,"schema44 Fishing requires positive Quay IDs");return static_cast<BuildingId>(n);};
    const auto& quays=field(j,"quays");const auto& boats=field(j,"boats");const auto& sites=field(j,"sites");
    require(quays.is_array() && quays.size()<=fishing_quay_limit && boats.is_array() &&
        boats.size()<=fishing_quay_limit && sites.is_array() && sites.size()<=fishing_site_limit,
        "schema44 Fishing collection exceeds bound");
    for(const auto& v:quays) {
        FishingQuay q;q.quay_id=id(field(v,"quay_id"));q.orientation=static_cast<std::uint8_t>(number(field(v,"orientation"),3));
        q.unloaded=number(field(v,"unloaded"));const auto& boat=field(v,"boat");
        if(!boat.is_null()) q.boat=FishingBoatId{number(boat)};
        state.quays.push_back(q);
    }
    for(const auto& v:boats) {
        FishingBoat b;b.id={number(field(v,"id"))};b.quay_id=id(field(v,"quay_id"));
        b.site_index=static_cast<std::size_t>(number(field(v,"site_index"),fishing_site_limit-1));
        b.phase=static_cast<FishingBoatPhase>(number(field(v,"phase"),static_cast<unsigned>(FishingBoatPhase::Unloading)));
        b.movement=static_cast<FishingMovementResult>(number(field(v,"movement"),static_cast<unsigned>(FishingMovementResult::Failed)));
        b.current=cell(field(v,"current"));b.path=cells(field(v,"path"),OriginalTerrainFacts::cell_count);
        b.path_vertex=static_cast<std::size_t>(number(field(v,"path_vertex"),OriginalTerrainFacts::cell_count));
        b.edge_progress=static_cast<std::uint8_t>(number(field(v,"edge_progress"),fishing_edge_ticks-1));
        b.work_progress=static_cast<std::uint16_t>(number(field(v,"work_progress"),fishing_catch_ticks));
        b.cargo=small(field(v,"cargo"),fishing_boat_capacity);b.reserved=small(field(v,"reserved"),fishing_boat_capacity);
        b.route_revision=number(field(v,"route_revision"));b.launched_tick=number(field(v,"launched_tick"));
        b.return_requested=boolean(field(v,"return_requested"));
        state.boats.push_back(std::move(b));
    }
    for(const auto& v:sites) {
        FishingSite site;site.cell=cell(field(v,"cell"));site.stock=small(field(v,"stock"),fishing_site_capacity);
        site.regeneration_progress=static_cast<std::uint16_t>(number(field(v,"regeneration_progress"),fishing_regeneration_ticks-1));
        site.regenerated=number(field(v,"regenerated"));site.caught=number(field(v,"caught"));
        site.source_marker=boolean(field(v,"source_marker"));state.sites.push_back(site);
    }
    require(canonical_fields(j,fishing_json(state)),"schema44 contains unknown or noncanonical Fishing fields");
    return state;
}
json hunting_json(const simulation::HuntingState& state) {
    using namespace simulation;
    require(state.tents.size()<=hunting_tent_limit && state.hunters.size()<=hunting_tent_limit &&
        state.habitats.size()<=hunting_habitat_limit && state.animals.size()<=hunting_habitat_limit*hunting_habitat_capacity,
        "schema45 Hunting collection exceeds bound");
    json tents=json::array(),hunters=json::array(),habitats=json::array(),animals=json::array();
    for(const auto& row:state.tents) tents.push_back({{"tent_id",static_cast<std::uint32_t>(row.tent_id)},
        {"unloaded",row.unloaded},{"hunter",row.hunter ? json(row.hunter->value):json(nullptr)}});
    for(const auto& row:state.habitats) habitats.push_back({{"centre",cell_json(row.centre)},
        {"species",static_cast<unsigned>(row.species)},{"regeneration_progress",row.regeneration_progress},
        {"births",row.births},{"killed",row.killed},{"source_marker",row.source_marker}});
    for(const auto& row:state.animals) animals.push_back({{"id",row.id.value},{"habitat_index",row.habitat_index},
        {"species",static_cast<unsigned>(row.species)},{"current",cell_json(row.current)},
        {"next",row.next ? cell_json(*row.next):json(nullptr)},{"edge_progress",row.edge_progress},
        {"born_tick",row.born_tick},{"reserved_by",row.reserved_by ? json(row.reserved_by->value):json(nullptr)}});
    for(const auto& row:state.hunters) hunters.push_back({{"id",row.id.value},{"tent_id",static_cast<std::uint32_t>(row.tent_id)},
        {"target",row.target ? json(row.target->value):json(nullptr)},{"target_cell",cell_json(row.target_cell)},
        {"habitat_index",row.habitat_index},{"phase",static_cast<unsigned>(row.phase)},
        {"movement",static_cast<unsigned>(row.movement)},{"current",cell_json(row.current)},
        {"path",cells_json(row.path)},{"path_vertex",row.path_vertex},{"edge_progress",row.edge_progress},
        {"work_progress",row.work_progress},{"cargo",row.cargo},{"reserved",row.reserved},
        {"route_revision",row.route_revision},{"launched_tick",row.launched_tick},{"return_requested",row.return_requested},
        {"kill_cell",row.kill_cell ? cell_json(*row.kill_cell):json(nullptr)},
        {"killed_tick",row.killed_tick ? json(*row.killed_tick):json(nullptr)}});
    return {{"tents",std::move(tents)},{"hunters",std::move(hunters)},{"habitats",std::move(habitats)},
        {"animals",std::move(animals)},{"next_animal_id",state.next_animal_id},{"next_hunter_id",state.next_hunter_id},
        {"launched",state.launched},{"retired",state.retired}};
}
simulation::HuntingState parse_hunting(const json& j) {
    using namespace simulation;
    HuntingState state;state.next_animal_id=number(field(j,"next_animal_id"));state.next_hunter_id=number(field(j,"next_hunter_id"));
    state.launched=number(field(j,"launched"));state.retired=number(field(j,"retired"));
    const auto id=[](const json& v){const auto n=number(v,UINT32_MAX);require(n>0,"schema45 requires positive Tent IDs");return static_cast<BuildingId>(n);};
    const auto species=[](const json& v){const auto n=number(v,77);require(n>=75,"schema45 unsupported prey species");return static_cast<HuntingSpecies>(n);};
    const auto& tents=field(j,"tents");const auto& hunters=field(j,"hunters");
    const auto& habitats=field(j,"habitats");const auto& animals=field(j,"animals");
    require(tents.is_array() && tents.size()<=hunting_tent_limit && hunters.is_array() && hunters.size()<=hunting_tent_limit &&
        habitats.is_array() && habitats.size()<=hunting_habitat_limit && animals.is_array() &&
        animals.size()<=hunting_habitat_limit*hunting_habitat_capacity,"schema45 Hunting collection exceeds bound");
    for(const auto& v:tents) {
        HuntingTent row;row.tent_id=id(field(v,"tent_id"));row.unloaded=number(field(v,"unloaded"));
        if(!field(v,"hunter").is_null()) row.hunter=HuntingHunterId{number(field(v,"hunter"))};state.tents.push_back(row);
    }
    for(const auto& v:habitats) {
        HuntingHabitat row;row.centre=cell(field(v,"centre"));row.species=species(field(v,"species"));
        row.regeneration_progress=static_cast<std::uint16_t>(number(field(v,"regeneration_progress"),hunting_regeneration_ticks-1));
        row.births=number(field(v,"births"));row.killed=number(field(v,"killed"));row.source_marker=boolean(field(v,"source_marker"));
        state.habitats.push_back(row);
    }
    for(const auto& v:animals) {
        HuntingAnimal row;row.id={number(field(v,"id"))};row.habitat_index=static_cast<std::size_t>(number(field(v,"habitat_index"),hunting_habitat_limit-1));
        row.species=species(field(v,"species"));row.current=cell(field(v,"current"));
        if(!field(v,"next").is_null()) row.next=cell(field(v,"next"));
        row.edge_progress=static_cast<std::uint8_t>(number(field(v,"edge_progress"),hunting_edge_ticks-1));
        row.born_tick=number(field(v,"born_tick"));
        if(!field(v,"reserved_by").is_null()) row.reserved_by=HuntingHunterId{number(field(v,"reserved_by"))};state.animals.push_back(row);
    }
    for(const auto& v:hunters) {
        HuntingHunter row;row.id={number(field(v,"id"))};row.tent_id=id(field(v,"tent_id"));
        if(!field(v,"target").is_null()) row.target=HuntingAnimalId{number(field(v,"target"))};
        row.target_cell=cell(field(v,"target_cell"));row.habitat_index=static_cast<std::size_t>(number(field(v,"habitat_index"),hunting_habitat_limit-1));
        row.phase=static_cast<HuntingHunterPhase>(number(field(v,"phase"),static_cast<unsigned>(HuntingHunterPhase::Unloading)));
        row.movement=static_cast<HuntingMovementResult>(number(field(v,"movement"),static_cast<unsigned>(HuntingMovementResult::Failed)));
        row.current=cell(field(v,"current"));row.path=cells(field(v,"path"),OriginalTerrainFacts::cell_count);
        row.path_vertex=static_cast<std::size_t>(number(field(v,"path_vertex"),OriginalTerrainFacts::cell_count));
        row.edge_progress=static_cast<std::uint8_t>(number(field(v,"edge_progress"),hunting_edge_ticks-1));
        row.work_progress=static_cast<std::uint16_t>(number(field(v,"work_progress"),hunting_work_ticks));
        row.cargo=small(field(v,"cargo"),hunting_hunter_capacity);row.reserved=small(field(v,"reserved"),hunting_hunter_capacity);
        row.route_revision=number(field(v,"route_revision"));row.launched_tick=number(field(v,"launched_tick"));
        row.return_requested=boolean(field(v,"return_requested"));
        if(!field(v,"kill_cell").is_null()) row.kill_cell=cell(field(v,"kill_cell"));
        if(!field(v,"killed_tick").is_null()) row.killed_tick=number(field(v,"killed_tick"));state.hunters.push_back(std::move(row));
    }
    require(canonical_fields(j,hunting_json(state)),"schema45 contains unknown or noncanonical Hunting fields");return state;
}
json irrigation_work_witness_json(const simulation::IrrigationWorkWitness& witness) {
    return {{"base_work",witness.base_work},{"wet_work_ticks",witness.wet_work_ticks},
        {"bonus_work",witness.bonus_work},{"clipped_bonus",witness.clipped_bonus},{"wasted_work",witness.wasted_work}};
}
simulation::IrrigationWorkWitness parse_irrigation_work_witness(const json& j) {
    require(j.is_object(),"schema49 requires an immutable irrigation readiness work witness");
    simulation::IrrigationWorkWitness witness;
    witness.base_work=number(field(j,"base_work"));witness.wet_work_ticks=number(field(j,"wet_work_ticks"));
    witness.bonus_work=number(field(j,"bonus_work"));witness.clipped_bonus=number(field(j,"clipped_bonus"));
    witness.wasted_work=number(field(j,"wasted_work"));
    require(canonical_fields(j,irrigation_work_witness_json(witness)),
        "schema49 contains unknown irrigation readiness work authority");
    return witness;
}
json plantations_receipt_json(const simulation::PlantationReceipt& receipt,bool seasonal,bool productivity) {
    json result={{"grower_id",static_cast<std::uint64_t>(receipt.grower_id)},
        {"courier_id",static_cast<std::uint32_t>(receipt.courier_id)},
        {"dispatched_tick",receipt.dispatched_tick},{"picked_tick",receipt.picked_tick},
        {"returned_tick",receipt.returned_tick}};
    if(seasonal)result["ready_tick"]=receipt.ready_tick;
    else require(receipt.ready_tick==0,"Historical plantation receipt cannot retain seasonal authority");
    require(receipt.irrigation_ready_work.has_value()==productivity,
        "Plantation receipt readiness work authority requires exact schema49");
    if(productivity)result["irrigation_ready_work"]=irrigation_work_witness_json(*receipt.irrigation_ready_work);
    return result;
}
json plantations_json(const simulation::PlantationsState& state,bool seasonal=false,bool productivity=false) {
    using namespace simulation;
    require(state.plants.size()<=plantation_plant_limit && state.sources.size()<=plantation_source_limit,
            "schema41 plantation collection exceeds bounded context");
    json plants=json::array(),sources=json::array();
    for (const auto& row:state.plants) plants.push_back({
        {"plant_id",static_cast<std::uint32_t>(row.plant_id)},
        {"source_id",static_cast<std::uint32_t>(row.source_id)},{"growth",row.growth},
        {"productive_ticks",row.productive_ticks},{"grown",row.grown},
        {"harvested",row.harvested},{"returned",row.returned},
        {"last_receipt",row.last_receipt ? plantations_receipt_json(*row.last_receipt,seasonal,productivity):json(nullptr)}});
    for (const auto& row:state.sources) {
        json trip=nullptr;
        if (row.latest_trip) {
            const auto& t=*row.latest_trip;
            trip={{"grower_id",static_cast<std::uint64_t>(t.grower_id)},
                {"plant_id",static_cast<std::uint32_t>(t.plant_id)},{"dispatched_tick",t.dispatched_tick},
                {"picked_tick",t.picked_tick ? json(*t.picked_tick):json(nullptr)},
                {"returned_tick",t.returned_tick ? json(*t.returned_tick):json(nullptr)},
                {"delivered",t.delivered}};
            if(seasonal)trip["ready_tick"]=t.ready_tick;
            else require(t.ready_tick==0,"Historical plantation trip cannot retain seasonal authority");
            require(t.irrigation_ready_work.has_value()==productivity,
                "Plantation trip readiness work authority requires exact schema49");
            if(productivity)trip["irrigation_ready_work"]=irrigation_work_witness_json(*t.irrigation_ready_work);
        }
        sources.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
            {"courier_id",static_cast<std::uint32_t>(row.courier_id)},
            {"spawned",row.spawned},{"latest_trip",std::move(trip)}});
    }
    return {{"revision",plantations_revision},{"catalog",plantations_catalog},
        {"next_grower_id",state.next_grower_id},{"plants",std::move(plants)},{"sources",std::move(sources)}};
}
simulation::PlantationsState parse_plantations(const json& j,bool seasonal=false,bool productivity=false) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==plantations_revision &&
            str(field(j,"catalog"))==plantations_catalog,"schema41 requires its exact plantation revision and catalog");
    const auto positive=[](const json& value,std::uint64_t bound=UINT64_MAX) {
        const auto result=number(value,bound);require(result>0,"schema41 plantation identity must be positive");
        return result;
    };
    const auto building_id=[&](const json& value){return static_cast<BuildingId>(positive(value,UINT32_MAX));};
    const auto courier_id=[&](const json& value){return static_cast<CourierId>(positive(value,UINT32_MAX));};
    const auto grower_id=[&](const json& value){return static_cast<PlantationGrowerId>(positive(value));};
    PlantationsState state;state.next_grower_id=positive(field(j,"next_grower_id"));
    const auto& plants=field(j,"plants");
    require(plants.is_array() && plants.size()<=plantation_plant_limit,"schema41 plant collection exceeds bound");
    for (const auto& value:plants) {
        PlantationPlant row;row.plant_id=building_id(field(value,"plant_id"));row.source_id=building_id(field(value,"source_id"));
        require(state.plants.empty() || state.plants.back().plant_id<row.plant_id,
                "schema41 plant IDs must be sorted and unique");
        row.growth=static_cast<std::uint32_t>(number(field(value,"growth"),plantation_growth_ticks));
        row.productive_ticks=number(field(value,"productive_ticks"));row.grown=number(field(value,"grown"));
        row.harvested=number(field(value,"harvested"));row.returned=number(field(value,"returned"));
        const auto& receipt=field(value,"last_receipt");
        if (!receipt.is_null()) row.last_receipt=PlantationReceipt{grower_id(field(receipt,"grower_id")),
            courier_id(field(receipt,"courier_id")),number(field(receipt,"dispatched_tick")),
            number(field(receipt,"picked_tick")),number(field(receipt,"returned_tick"))};
        if(seasonal && row.last_receipt)row.last_receipt->ready_tick=number(field(receipt,"ready_tick"));
        if(productivity && row.last_receipt)row.last_receipt->irrigation_ready_work=
            parse_irrigation_work_witness(field(receipt,"irrigation_ready_work"));
        state.plants.push_back(row);
    }
    const auto& sources=field(j,"sources");
    require(sources.is_array() && sources.size()<=plantation_source_limit,"schema41 plantation source collection exceeds bound");
    for (const auto& value:sources) {
        PlantationSource row;row.building_id=building_id(field(value,"building_id"));row.courier_id=courier_id(field(value,"courier_id"));
        require(state.sources.empty() || state.sources.back().building_id<row.building_id,
                "schema41 plantation sources must have sorted unique IDs");
        row.spawned=number(field(value,"spawned"));const auto& trip=field(value,"latest_trip");
        if (!trip.is_null()) {
            PlantationTrip t;t.grower_id=grower_id(field(trip,"grower_id"));t.plant_id=building_id(field(trip,"plant_id"));
            t.dispatched_tick=number(field(trip,"dispatched_tick"));
            if (!field(trip,"picked_tick").is_null()) t.picked_tick=number(field(trip,"picked_tick"));
            if (!field(trip,"returned_tick").is_null()) t.returned_tick=number(field(trip,"returned_tick"));
            t.delivered=static_cast<std::uint8_t>(number(field(trip,"delivered"),1));row.latest_trip=t;
            if(seasonal)row.latest_trip->ready_tick=number(field(trip,"ready_tick"));
            if(productivity)row.latest_trip->irrigation_ready_work=parse_irrigation_work_witness(field(trip,"irrigation_ready_work"));
        }
        state.sources.push_back(row);
    }
    require(canonical_fields(j,plantations_json(state,seasonal,productivity)),"schema41 contains unknown or noncanonical plantation fields");
    return state;
}
json irrigation_json(const simulation::IrrigationState& state) {
    require(state.ditches.size()<=simulation::irrigation_ditch_limit &&
            state.pumps.size()<=simulation::Rules::original_production_building_limit,
            "schema42 irrigation collections exceed their bounds");
    json ditches=json::array(),pumps=json::array();
    for (const auto& ditch:state.ditches)
        ditches.push_back({{"cell",cell_json(ditch.cell)},{"placed_tick",ditch.placed_tick},
            {"placed_sequence",ditch.placed_sequence}});
    for (const auto& pump:state.pumps)
        pumps.push_back({{"building_id",static_cast<std::uint32_t>(pump.building_id)},
            {"orientation",pump.orientation},
            {"eligible_since_tick",pump.eligible_since_tick ? json(*pump.eligible_since_tick):json(nullptr)}});
    return {{"revision",simulation::irrigation_revision},{"ditches",std::move(ditches)},
        {"pumps",std::move(pumps)},{"ditches_placed_total",state.ditches_placed_total},
        {"ditches_removed_total",state.ditches_removed_total},
        {"ditch_construction_spent",state.ditch_construction_spent}};
}
simulation::IrrigationState parse_irrigation(const json& j) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==irrigation_revision,
            "schema42 requires its exact irrigation revision");
    IrrigationState state;
    state.ditches_placed_total=number(field(j,"ditches_placed_total"));
    state.ditches_removed_total=number(field(j,"ditches_removed_total"));
    state.ditch_construction_spent=number(field(j,"ditch_construction_spent"));
    const auto& ditches=field(j,"ditches");
    require(ditches.is_array() && ditches.size()<=irrigation_ditch_limit,
            "schema42 ditch collection exceeds its bound");
    for (const auto& value:ditches) {
        IrrigationDitch ditch{cell(field(value,"cell")),number(field(value,"placed_tick")),
            number(field(value,"placed_sequence"))};
        require(state.ditches.empty() || state.ditches.back().cell.y<ditch.cell.y ||
                (state.ditches.back().cell.y==ditch.cell.y && state.ditches.back().cell.x<ditch.cell.x),
                "schema42 ditch cells must be unique and sorted by y,x");
        state.ditches.push_back(ditch);
    }
    const auto& pumps=field(j,"pumps");
    require(pumps.is_array() && pumps.size()<=Rules::original_production_building_limit,
            "schema42 pump collection exceeds its bound");
    std::uint32_t previous=0;
    for (const auto& value:pumps) {
        const auto id=static_cast<std::uint32_t>(number(field(value,"building_id"),UINT32_MAX));
        require(id>previous,"schema42 pump identities must be positive, unique and sorted");
        previous=id;
        IrrigationPump pump; pump.building_id=static_cast<BuildingId>(id);
        pump.orientation=static_cast<std::uint8_t>(number(field(value,"orientation"),3));
        const auto& epoch=field(value,"eligible_since_tick");
        if (!epoch.is_null()) pump.eligible_since_tick=number(epoch);
        state.pumps.push_back(pump);
    }
    require(canonical_fields(j,irrigation_json(state)),"schema42 contains unknown or noncanonical irrigation fields");
    return state;
}
json music_arrival_json(const simulation::OriginalMusicArrival& a) {
    return {{"performer_id",static_cast<std::uint64_t>(a.performer_id)},
        {"school_id",static_cast<std::uint32_t>(a.school_id)},
        {"courier_id",static_cast<std::uint32_t>(a.courier_id)},
        {"dispatched_tick",a.dispatched_tick},{"arrival_tick",a.arrival_tick}};
}
simulation::OriginalMusicArrival parse_music_arrival(const json& j) {
    const auto performer=number(field(j,"performer_id"));
    const auto school=number(field(j,"school_id"),UINT32_MAX);
    const auto courier=number(field(j,"courier_id"),UINT32_MAX);
    require(performer&&school&&courier,"schema39 Music arrival requires positive identities");
    return {static_cast<simulation::PerformerId>(performer),static_cast<simulation::BuildingId>(school),
        static_cast<simulation::CourierId>(courier),number(field(j,"dispatched_tick")),number(field(j,"arrival_tick"))};
}
json venue_walker_json(const simulation::OriginalVenueWalkerEvidence& e) {
    return {{"walker_id",static_cast<std::uint64_t>(e.walker_id)},{"dispatched_tick",e.dispatched_tick},
        {"dispatch_music",music_arrival_json(e.dispatch_music)},
        {"arrival_music",e.arrival_music?music_arrival_json(*e.arrival_music):json(nullptr)}};
}
simulation::OriginalVenueWalkerEvidence parse_venue_walker(const json& j) {
    simulation::OriginalVenueWalkerEvidence e;const auto walker=number(field(j,"walker_id"));
    require(walker>0,"schema39 venue walker ID must be positive");
    e.walker_id=static_cast<simulation::VenueWalkerId>(walker);
    e.dispatched_tick=number(field(j,"dispatched_tick"));
    e.dispatch_music=parse_music_arrival(field(j,"dispatch_music"));
    const auto& arrival=field(j,"arrival_music");if(!arrival.is_null())e.arrival_music=parse_music_arrival(arrival);
    return e;
}
json production_services_json(const simulation::OriginalServicesState& services,bool venue_service=false) {
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
    for (const auto& trip:services.trips) {
        json value={{"transfer",transfer(trip.transfer)},
            {"target",static_cast<std::uint32_t>(trip.target)},
            {"dispatched_tick",trip.dispatched_tick},
            {"arrival_tick",trip.arrival_tick ? json(*trip.arrival_tick):json(nullptr)},
            {"visit_applied",trip.visit_applied}};
        require(venue_service||!trip.venue_walker,"venue walker evidence is forbidden before schema39");
        if(venue_service)value["venue_walker"]=trip.venue_walker?venue_walker_json(*trip.venue_walker):json(nullptr);
        trips.push_back(std::move(value));
    }
    for (const auto& witness:services.witnesses) {
        json value={{"building_id",static_cast<std::uint32_t>(witness.building_id)},
            {"role",static_cast<unsigned>(witness.role)},
            {"provider_id",static_cast<std::uint32_t>(witness.provider_id)},
            {"transfer",transfer(witness.transfer)},{"arrival_tick",witness.arrival_tick}};
        require(venue_service||!witness.venue_walker,"venue witness evidence is forbidden before schema39");
        if(venue_service)value["venue_walker"]=witness.venue_walker?venue_walker_json(*witness.venue_walker):json(nullptr);
        witnesses.push_back(std::move(value));
    }
    return {{"providers",std::move(providers)},{"trips",std::move(trips)},{"witnesses",std::move(witnesses)}};
}
simulation::OriginalServicesState parse_production_services(const json& j,bool venue_service=false) {
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
        provider.type=static_cast<simulation::OriginalServiceProviderType>(number(field(value,"type"),venue_service?9:8));
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
        trip.visit_applied=boolean(field(value,"visit_applied"));
        if(venue_service) {const auto& evidence=field(value,"venue_walker");if(!evidence.is_null())trip.venue_walker=parse_venue_walker(evidence);}
        services.trips.push_back(trip);
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
        witness.arrival_tick=number(field(value,"arrival_tick"));
        if(venue_service) {const auto& evidence=field(value,"venue_walker");if(!evidence.is_null())witness.venue_walker=parse_venue_walker(evidence);}
        services.witnesses.push_back(witness);
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
json production_clay_trip_json(const simulation::OriginalClayTrip& value) {
    return {{"transfer",production_transfer_json(value.transfer)},
        {"source",static_cast<std::uint32_t>(value.source)},{"target",static_cast<std::uint32_t>(value.target)},
        {"loaded",value.loaded},{"delivered",value.delivered},{"dispatched_tick",value.dispatched_tick},
        {"arrival_tick",value.arrival_tick ? json(*value.arrival_tick):json(nullptr)}};
}
simulation::OriginalClayTrip parse_production_clay_trip(const json& value,
        std::optional<simulation::CourierId> previous=std::nullopt) {
    simulation::OriginalClayTrip row;row.transfer=parse_production_transfer(field(value,"transfer"));
    require(!previous || *previous<row.transfer.courier_id,"schema26 native Clay trips require sorted unique actor IDs");
    row.source=production_id(field(value,"source"));row.target=production_id(field(value,"target"));
    row.loaded=small(field(value,"loaded"));row.delivered=small(field(value,"delivered"));
    row.dispatched_tick=number(field(value,"dispatched_tick"));
    const auto& arrival=field(value,"arrival_tick");if (!arrival.is_null()) row.arrival_tick=number(arrival);
    return row;
}
json production_safe_demolition_json(const simulation::OriginalSafeDemolitionState& state) {
    require(state.retired.size()<=simulation::original_safe_demolition_retired_limit,"schema35 retired purchase collection exceeds bound");
    json retired=json::array();
    for (const auto& entry:state.retired)
        retired.push_back({{"building_id",static_cast<std::uint32_t>(entry.building_id)},
            {"kind",static_cast<unsigned>(entry.kind)},{"origin",cell_json({entry.origin_x,entry.origin_y})},
            {"placed_tick",entry.placed_tick},{"removed_tick",entry.removed_tick},{"clay_extracted",entry.clay_extracted},
            {"courier_id",entry.courier_id ? json(static_cast<std::uint32_t>(*entry.courier_id)):json(nullptr)},
            {"clay_trip",entry.clay_trip ? production_clay_trip_json(*entry.clay_trip):json(nullptr)}});
    return {{"retired",std::move(retired)}};
}
simulation::OriginalSafeDemolitionState parse_production_safe_demolition(const json& j) {
    simulation::OriginalSafeDemolitionState result;
    const auto& retired=field(j,"retired");
    require(retired.is_array() && retired.size()<=simulation::original_safe_demolition_retired_limit,
            "schema35 retired purchase collection exceeds bound");
    for (const auto& value:retired) {
        simulation::OriginalRetiredPurchase entry;entry.building_id=production_id(field(value,"building_id"));
        require(result.retired.empty() || result.retired.back().building_id<entry.building_id,
                "schema35 retired purchase IDs must be sorted unique");
        entry.kind=static_cast<simulation::OriginalRetiredKind>(number(field(value,"kind"),1));
        const auto origin=cell(field(value,"origin"));entry.origin_x=origin.x;entry.origin_y=origin.y;
        entry.placed_tick=number(field(value,"placed_tick"),original::scheduler_max_tick);
        entry.removed_tick=number(field(value,"removed_tick"),original::scheduler_max_tick);
        entry.clay_extracted=number(field(value,"clay_extracted"));
        const auto& actor=field(value,"courier_id");
        if (!actor.is_null()) {
            const auto id=number(actor,UINT32_MAX);require(id>0,"schema35 retired Courier ID must be positive");
            entry.courier_id=static_cast<simulation::CourierId>(id);
        }
        const auto& trip=field(value,"clay_trip");if (!trip.is_null()) entry.clay_trip=parse_production_clay_trip(trip);
        result.retired.push_back(entry);
    }
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
    for (const auto& value:state.clay_trips) clay_trips.push_back(production_clay_trip_json(value));
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
        auto row=parse_production_clay_trip(value,result.clay_trips.empty() ? std::nullopt:
            std::optional<simulation::CourierId>(result.clay_trips.back().transfer.courier_id));
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
json production_housing_service_json(const simulation::OriginalServiceWitness& w,bool venue_service=false) {
    json result={{"building_id",static_cast<std::uint32_t>(w.building_id)},{"role",static_cast<unsigned>(w.role)},
        {"provider_id",static_cast<std::uint32_t>(w.provider_id)},
        {"transfer",production_transfer_json(w.transfer)},{"arrival_tick",w.arrival_tick}};
    require(venue_service||!w.venue_walker,"historical venue evidence is forbidden before schema39");
    if(venue_service)result["venue_walker"]=w.venue_walker?venue_walker_json(*w.venue_walker):json(nullptr);
    return result;
}
simulation::OriginalServiceWitness parse_production_housing_service(const json& j,bool venue_service=false) {
    simulation::OriginalServiceWitness result;result.building_id=production_id(field(j,"building_id"));
    const auto role=number(field(j,"role"),8);
    result.role=static_cast<original::ServiceRole>(role);result.provider_id=production_id(field(j,"provider_id"));
    result.transfer=parse_production_transfer(field(j,"transfer"));result.arrival_tick=number(field(j,"arrival_tick"));
    if(venue_service) {const auto& evidence=field(j,"venue_walker");if(!evidence.is_null())result.venue_walker=parse_venue_walker(evidence);}
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
json production_housing_evaluation_json(const simulation::OriginalHousingEvaluation& e,bool venue_service=false) {
    json services=json::array();
    for (const auto& witness:e.service_witnesses)
        services.push_back(witness ? production_housing_service_json(*witness,venue_service):json(nullptr));
    return {{"tick",e.tick},{"input",original_house_json(e.input)},{"service_witnesses",std::move(services)},
        {"water_evaluation_witness",e.water_evaluation_witness ? production_water_witness_json(*e.water_evaluation_witness):json(nullptr)},
        {"food_quality_witness",e.food_quality_witness ? production_housing_food_json(*e.food_quality_witness):json(nullptr)},
        {"food_received",e.food_received},{"food_consumed",e.food_consumed},{"goods_consumed_before",e.goods_consumed_before}};
}
simulation::OriginalHousingEvaluation parse_production_housing_evaluation(const json& j,bool venue_service=false) {
    simulation::OriginalHousingEvaluation result;result.tick=number(field(j,"tick"));
    result.input=parse_original_house(field(j,"input"));const auto& services=field(j,"service_witnesses");
    require(services.is_array() && services.size()==result.service_witnesses.size(),"schema26 historical services require exactly nine categories");
    for (std::size_t i=0;i<result.service_witnesses.size();++i)
        if (!services[i].is_null()) result.service_witnesses[i]=parse_production_housing_service(services[i],venue_service);
    const auto& water=field(j,"water_evaluation_witness");if (!water.is_null()) result.water_evaluation_witness=parse_production_water_witness(water);
    const auto& food=field(j,"food_quality_witness");if (!food.is_null()) result.food_quality_witness=parse_production_housing_food(food);
    result.food_received=parse_production_counters<simulation::original_food_crop_count>(field(j,"food_received"));
    result.food_consumed=parse_production_counters<simulation::original_food_crop_count>(field(j,"food_consumed"));
    result.goods_consumed_before=parse_production_counters<original::goods_count>(field(j,"goods_consumed_before"));return result;
}
json production_housing_transition_json(const simulation::OriginalHousingTransition& transition,bool venue_service=false) {
    return {{"kind",static_cast<unsigned>(transition.kind)},
        {"evaluation",production_housing_evaluation_json(transition.evaluation,venue_service)},
        {"admitted",transition.admitted}};
}
simulation::OriginalHousingTransition parse_production_housing_transition(const json& j,bool venue_service=false) {
    simulation::OriginalHousingTransition result;
    result.kind=static_cast<simulation::OriginalHousingTransitionKind>(number(field(j,"kind"),1));
    result.evaluation=parse_production_housing_evaluation(field(j,"evaluation"),venue_service);
    result.admitted=static_cast<std::int16_t>(number(field(j,"admitted"),2));
    return result;
}
json production_housing_json(const simulation::OriginalHousingState& state,bool venue_service=false) {
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
            transitions.push_back(transition ? production_housing_transition_json(*transition,venue_service):json(nullptr));
        houses.push_back({{"building_id",static_cast<std::uint32_t>(h.building_id)},
            {"first_admission_tick",h.first_admission_tick ? json(*h.first_admission_tick):json(nullptr)},
            {"consumption_rows",std::move(rows)},
            {"last_evaluation",h.last_evaluation ? production_housing_evaluation_json(*h.last_evaluation,venue_service):json(nullptr)},
            {"first_transitions",std::move(transitions)}});
    }
    return {{"neutral_source_origins",std::move(sources)},{"houses",std::move(houses)}};
}
simulation::OriginalHousingState parse_production_housing(const json& j,bool venue_service=false) {
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
        const auto& last=field(value,"last_evaluation");if (!last.is_null()) h.last_evaluation=parse_production_housing_evaluation(last,venue_service);
        const auto& transitions=field(value,"first_transitions");
        require(transitions.is_array() && transitions.size()==h.first_transitions.size(),
                "schema26 housing requires exactly fifteen first-transition slots");
        for (std::size_t i=0;i<h.first_transitions.size();++i)
            if (!transitions[i].is_null()) h.first_transitions[i]=parse_production_housing_transition(transitions[i],venue_service);
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
json production_culture_json(const simulation::OriginalCultureState& state,bool venue_service=false) {
    using namespace simulation;
    require(state.schools.size()<=original_music_school_limit && state.venues.size()<=original_culture_venue_limit,
            "schema38 culture collections exceed their bounds");
    json schools=json::array(),venues=json::array();
    for (const auto& school:state.schools) {
        json trip=nullptr;
        if (school.latest_trip) {
            const auto& t=*school.latest_trip;
            trip={{"performer_id",static_cast<std::uint64_t>(t.performer_id)},
                {"venue_id",static_cast<std::uint32_t>(t.venue_id)},{"dispatched_tick",t.dispatched_tick},
                {"arrival_tick",t.arrival_tick ? json(*t.arrival_tick):json(nullptr)},{"accepted",t.accepted}};
        }
        schools.push_back({{"building_id",static_cast<std::uint32_t>(school.building_id)},
            {"courier_id",static_cast<std::uint32_t>(school.courier_id)},{"spawn_counter",school.spawn_counter},
            {"attempts",school.attempts},{"spawned",school.spawned},{"latest_trip",std::move(trip)}});
    }
    for (const auto& venue:state.venues) {
        json arrival=nullptr;
        if (venue.last_arrival) {
            const auto& a=*venue.last_arrival;
            arrival={{"performer_id",static_cast<std::uint64_t>(a.performer_id)},
                {"school_id",static_cast<std::uint32_t>(a.school_id)},
                {"courier_id",static_cast<std::uint32_t>(a.courier_id)},
                {"dispatched_tick",a.dispatched_tick},{"arrival_tick",a.arrival_tick}};
        }
        venues.push_back({{"building_id",static_cast<std::uint32_t>(venue.building_id)},
            {"music_timer",venue.music_timer},{"last_arrival",std::move(arrival)}});
    }
    json result={{"next_performer_id",state.next_performer_id},{"schools",std::move(schools)},
        {"venues",std::move(venues)}};
    require(state.venue_house_service.has_value()==venue_service,
            "venue House service authority is mandatory only for schema39");
    if(venue_service) {
        const auto& service=*state.venue_house_service;
        require(service.sources.size()<=original_culture_venue_limit,"schema39 venue source collection exceeds bound");
        json sources=json::array();
        for(const auto& row:service.sources)
            sources.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
                {"courier_id",static_cast<std::uint32_t>(row.courier_id)},{"spawn_counter",row.spawn_counter},
                {"attempts",row.attempts},{"spawned",row.spawned},
                {"first_music",row.first_music?music_arrival_json(*row.first_music):json(nullptr)}});
        result["venue_house_service"]={{"next_walker_id",service.next_walker_id},{"sources",std::move(sources)}};
    }
    return result;
}
simulation::OriginalCultureState parse_production_culture(const json& j,bool venue_service=false) {
    using namespace simulation;
    OriginalCultureState state;
    state.next_performer_id=number(field(j,"next_performer_id"));
    require(state.next_performer_id>0,"schema38 next performer ID must be positive");
    const auto performer=[](const json& value) {
        const auto id=number(value);require(id>0,"schema38 performer ID must be positive");
        return static_cast<PerformerId>(id);
    };
    const auto courier=[](const json& value) {
        const auto id=number(value,UINT32_MAX);require(id>0,"schema38 courier ID must be positive");
        return static_cast<CourierId>(id);
    };
    const auto& schools=field(j,"schools");
    require(schools.is_array() && schools.size()<=original_music_school_limit,"schema38 school collection exceeds bound");
    for (const auto& value:schools) {
        OriginalMusicSchool row;row.building_id=production_id(field(value,"building_id"));
        require(state.schools.empty() || state.schools.back().building_id<row.building_id,
                "schema38 school IDs must be sorted unique");
        row.courier_id=courier(field(value,"courier_id"));
        row.spawn_counter=static_cast<std::uint8_t>(number(field(value,"spawn_counter"),UINT8_MAX));
        row.attempts=number(field(value,"attempts"));row.spawned=number(field(value,"spawned"));
        const auto& trip=field(value,"latest_trip");
        if (!trip.is_null()) {
            OriginalMusicTrip t;t.performer_id=performer(field(trip,"performer_id"));
            t.venue_id=production_id(field(trip,"venue_id"));t.dispatched_tick=number(field(trip,"dispatched_tick"));
            const auto& arrival=field(trip,"arrival_tick");if (!arrival.is_null()) t.arrival_tick=number(arrival);
            t.accepted=boolean(field(trip,"accepted"));row.latest_trip=t;
        }
        state.schools.push_back(row);
    }
    const auto& venues=field(j,"venues");
    require(venues.is_array() && venues.size()<=original_culture_venue_limit,"schema38 venue collection exceeds bound");
    for (const auto& value:venues) {
        OriginalCultureVenue row;row.building_id=production_id(field(value,"building_id"));
        require(state.venues.empty() || state.venues.back().building_id<row.building_id,
                "schema38 venue IDs must be sorted unique");
        row.music_timer=static_cast<std::uint8_t>(number(field(value,"music_timer"),32));
        const auto& arrival=field(value,"last_arrival");
        if (!arrival.is_null()) row.last_arrival=OriginalMusicArrival{
            performer(field(arrival,"performer_id")),production_id(field(arrival,"school_id")),
            courier(field(arrival,"courier_id")),number(field(arrival,"dispatched_tick")),number(field(arrival,"arrival_tick"))};
        state.venues.push_back(row);
    }
    if(venue_service) {
        OriginalVenueHouseServiceState service;const auto& value=field(j,"venue_house_service");
        service.next_walker_id=number(field(value,"next_walker_id"));
        require(service.next_walker_id>0,"schema39 next walker ID must be positive");
        const auto& sources=field(value,"sources");
        require(sources.is_array()&&sources.size()<=original_culture_venue_limit,"schema39 venue source collection exceeds bound");
        for(const auto& source:sources) {
            OriginalVenueWalkerSource row;row.building_id=production_id(field(source,"building_id"));
            require(service.sources.empty()||service.sources.back().building_id<row.building_id,
                    "schema39 venue sources must have sorted unique IDs");
            row.courier_id=courier(field(source,"courier_id"));
            row.spawn_counter=static_cast<std::uint8_t>(number(field(source,"spawn_counter"),UINT8_MAX));
            row.attempts=number(field(source,"attempts"));row.spawned=number(field(source,"spawned"));
            const auto& first_music=field(source,"first_music");
            if(!first_music.is_null())row.first_music=parse_music_arrival(first_music);
            service.sources.push_back(row);
        }
        state.venue_house_service=std::move(service);
    }
    return state;
}
std::uint32_t protected_agriculture_save_target(const fs::path& path) {
    // Protect Rule30/31/32/33 even when retained state is damaged. Historical
    // writers may still replace their historical targets exactly as before.
    std::error_code error;const auto size=fs::file_size(path,error);
    if(error || !size) return 0;
    // No valid historical save exceeds this bound. A later schema46/47/48/49/50/51/52 key in
    // an oversized damaged target cannot be ruled out by a bounded prefix.
    if(size>max_save_bytes) return UINT32_MAX;
    std::ifstream input(path,std::ios::binary);if(!input.good()) return 0;
    std::string bytes(static_cast<std::size_t>(std::min(size,max_save_bytes)),'\0');
    input.read(bytes.data(),static_cast<std::streamsize>(bytes.size()));
    bytes.resize(static_cast<std::size_t>(input.gcount()));
    struct ProtectedTarget {std::uint32_t schema;};
    bool schema_key=false;
    try {
        (void)json::parse(bytes,[&](int depth,json::parse_event_t event,json& parsed) {
            // Ignore over-deep unknown payload without ending recognition of
            // a later top-level schema key. The bounded JSON reader retains
            // no such payload as authority or as a new target admission rule.
            if(depth>32) return false;
            if(event==json::parse_event_t::key && depth==1)
                schema_key=parsed=="schema_version";
            else if(event==json::parse_event_t::value && depth==1 && schema_key) {
                schema_key=false;
                if(parsed==46 || parsed=="46") throw ProtectedTarget{46};
                if(parsed==47 || parsed=="47") throw ProtectedTarget{47};
                if(parsed==48 || parsed=="48") throw ProtectedTarget{48};
                if(parsed==49 || parsed=="49") throw ProtectedTarget{49};
                if(parsed==50 || parsed=="50") throw ProtectedTarget{50};
                if(parsed==51 || parsed=="51") throw ProtectedTarget{51};
                if(parsed==52 || parsed=="52") throw ProtectedTarget{52};
            }
            return true;
        },false);
    } catch(const ProtectedTarget& target) {return target.schema;}
    return 0;
}
json multi_field_agriculture_json(const simulation::MultiFieldAgricultureState& state) {
    using namespace simulation;
    require(state.farms.size()<=original_food_farm_limit(RulesProfile::CityV16,kMultiFieldAgricultureRuleVersion) &&
            state.fields.size()<=agriculture_field_limit,"schema46 multi-field collection exceeds bound");
    json farms=json::array(),fields=json::array();
    for(const auto& row:state.farms)
        farms.push_back({{"farm_id",static_cast<std::uint32_t>(row.farm_id)},
            {"last_field",row.last_field ? json(static_cast<std::uint32_t>(*row.last_field)):json(nullptr)}});
    for(const auto& row:state.fields)
        fields.push_back({{"field_id",static_cast<std::uint32_t>(row.field_id)},
            {"farm_id",static_cast<std::uint32_t>(row.farm_id)},{"productive_ticks",row.productive_ticks}});
    return {{"revision",multi_field_agriculture_revision},{"farms",std::move(farms)},{"fields",std::move(fields)}};
}
simulation::MultiFieldAgricultureState parse_multi_field_agriculture(const json& j) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==multi_field_agriculture_revision,
            "schema46 requires its exact multi-field revision");
    const auto id=[](const json& value) {
        const auto n=number(value,UINT32_MAX);require(n>0,"schema46 multi-field identity must be positive");
        return static_cast<BuildingId>(n);
    };
    MultiFieldAgricultureState state;
    const auto& farms=field(j,"farms");
    require(farms.is_array() && farms.size()<=original_food_farm_limit(RulesProfile::CityV16,kMultiFieldAgricultureRuleVersion),
            "schema46 multi-field Farm collection exceeds bound");
    for(const auto& value:farms) {
        MultiFieldFarmSchedule row;row.farm_id=id(field(value,"farm_id"));
        require(state.farms.empty() || state.farms.back().farm_id<row.farm_id,
                "schema46 Farm schedule IDs must be sorted and unique");
        const auto& last=field(value,"last_field");if(!last.is_null())row.last_field=id(last);
        state.farms.push_back(row);
    }
    const auto& fields=field(j,"fields");
    require(fields.is_array() && fields.size()<=agriculture_field_limit,"schema46 field-work collection exceeds bound");
    for(const auto& value:fields) {
        MultiFieldWork row;row.field_id=id(field(value,"field_id"));row.farm_id=id(field(value,"farm_id"));
        row.productive_ticks=number(field(value,"productive_ticks"));
        require(state.fields.empty() || state.fields.back().field_id<row.field_id,
                "schema46 field-work IDs must be sorted and unique");
        state.fields.push_back(row);
    }
    require(canonical_fields(j,multi_field_agriculture_json(state)),"schema46 contains unknown multi-field authority");
    return state;
}
json physical_food_grower_receipt_json(const simulation::PhysicalFoodGrowerReceipt& receipt,bool seasonal,bool productivity) {
    json result={{"grower_id",receipt.grower_id.value},{"courier_id",static_cast<std::uint32_t>(receipt.courier_id)},
        {"crop",static_cast<unsigned>(receipt.crop)},{"dispatched_tick",receipt.dispatched_tick},
        {"arrived_tick",receipt.arrived_tick},{"harvested_tick",receipt.harvested_tick},
        {"returned_tick",receipt.returned_tick},{"work_at_harvest",receipt.work_at_harvest}};
    if(seasonal) {
        result["wasted_work_at_harvest"]=receipt.wasted_work_at_harvest;
        result["ready_tick"]=receipt.ready_tick;
    } else require(receipt.wasted_work_at_harvest==0 && receipt.ready_tick==0,"Rule31 receipt cannot retain seasonal authority");
    if(productivity)result["irrigation_bonus_at_harvest"]=receipt.irrigation_bonus_at_harvest;
    else require(receipt.irrigation_bonus_at_harvest==0,"Historical receipt cannot retain irrigation productivity authority");
    return result;
}
json physical_food_grower_trip_json(const simulation::PhysicalFoodGrowerTrip& trip,bool seasonal,bool productivity) {
    json result={{"grower_id",trip.grower_id.value},{"field_id",static_cast<std::uint32_t>(trip.field_id)},
        {"crop",static_cast<unsigned>(trip.crop)},{"phase",static_cast<unsigned>(trip.phase)},
        {"dispatched_tick",trip.dispatched_tick},{"arrived_tick",trip.arrived_tick ? json(*trip.arrived_tick):json(nullptr)},
        {"harvested_tick",trip.harvested_tick ? json(*trip.harvested_tick):json(nullptr)},
        {"returned_tick",trip.returned_tick ? json(*trip.returned_tick):json(nullptr)},
        {"work_before",trip.work_before},{"work_ticks",trip.work_ticks},{"wait_left",trip.wait_left},
        {"delivered",trip.delivered},{"cancel_requested",trip.cancel_requested}};
    if(seasonal) {
        result["wasted_work_before"]=trip.wasted_work_before;
        result["ready_tick"]=trip.ready_tick ? json(*trip.ready_tick):json(nullptr);
    } else require(trip.wasted_work_before==0 && !trip.ready_tick,"Rule31 trip cannot retain seasonal authority");
    if(productivity)result["irrigation_bonus_before"]=trip.irrigation_bonus_before;
    else require(trip.irrigation_bonus_before==0,"Historical trip cannot retain irrigation productivity authority");
    return result;
}
json physical_food_growers_json(const simulation::PhysicalFoodGrowersState& state,bool seasonal=false,bool productivity=false) {
    using namespace simulation;
    require(state.farms.size()<=original_food_farm_limit(RulesProfile::CityV16,kPhysicalFoodGrowersRuleVersion) &&
            state.fields.size()<=agriculture_field_limit,"schema47 physical Grower collection exceeds bound");
    json farms=json::array(),fields=json::array();
    for(const auto& row:state.farms)
        farms.push_back({{"farm_id",static_cast<std::uint32_t>(row.farm_id)},
            {"courier_id",static_cast<std::uint32_t>(row.courier_id)},{"spawned",row.spawned},{"retired",row.retired},
            {"latest_trip",row.latest_trip ? physical_food_grower_trip_json(*row.latest_trip,seasonal,productivity):json(nullptr)}});
    for(const auto& row:state.fields)
        fields.push_back({{"field_id",static_cast<std::uint32_t>(row.field_id)},
            {"farm_id",static_cast<std::uint32_t>(row.farm_id)},{"returned",row.returned},
            {"last_receipt",row.last_receipt ? physical_food_grower_receipt_json(*row.last_receipt,seasonal,productivity):json(nullptr)}});
    return {{"revision",physical_food_growers_revision},{"next_grower_id",state.next_grower_id},
        {"farms",std::move(farms)},{"fields",std::move(fields)}};
}
simulation::PhysicalFoodGrowersState parse_physical_food_growers(const json& j,bool seasonal=false,bool productivity=false) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==physical_food_growers_revision,
        "schema47 requires its exact physical Grower revision");
    const auto id=[](const json& value) {
        const auto n=number(value,UINT32_MAX);require(n>0,"schema47 paid identity must be positive");return n;
    };
    const auto grower_id=[](const json& value) {
        const auto n=number(value);require(n>0,"schema47 Grower lifetime must be positive");return PhysicalFoodGrowerId{n};
    };
    const auto crop=[](const json& value) {return static_cast<AgricultureCrop>(number(value,agriculture_crop_count-1));};
    const auto epoch=[](const json& value)->std::optional<std::uint64_t> {
        return value.is_null() ? std::nullopt:std::optional<std::uint64_t>(number(value));
    };
    PhysicalFoodGrowersState state;state.next_grower_id=number(field(j,"next_grower_id"));
    require(state.next_grower_id>0,"schema47 next Grower lifetime must be positive");
    const auto& farms=field(j,"farms");
    require(farms.is_array() && farms.size()<=original_food_farm_limit(RulesProfile::CityV16,kPhysicalFoodGrowersRuleVersion),
        "schema47 physical Farm collection exceeds bound");
    for(const auto& value:farms) {
        PhysicalFoodGrowerFarm row;row.farm_id=static_cast<BuildingId>(id(field(value,"farm_id")));
        row.courier_id=static_cast<CourierId>(id(field(value,"courier_id")));
        row.spawned=number(field(value,"spawned"));row.retired=number(field(value,"retired"));
        require(state.farms.empty() || state.farms.back().farm_id<row.farm_id,
            "schema47 physical Farm IDs must be sorted and unique");
        const auto& latest=field(value,"latest_trip");
        if(!latest.is_null()) {
            PhysicalFoodGrowerTrip trip;trip.grower_id=grower_id(field(latest,"grower_id"));
            trip.field_id=static_cast<BuildingId>(id(field(latest,"field_id")));trip.crop=crop(field(latest,"crop"));
            const auto phase=number(field(latest,"phase"),static_cast<unsigned>(PhysicalFoodGrowerPhase::Returning));
            require(phase>=static_cast<unsigned>(PhysicalFoodGrowerPhase::Outward),"schema47 Grower phase is unsupported");
            trip.phase=static_cast<PhysicalFoodGrowerPhase>(phase);trip.dispatched_tick=number(field(latest,"dispatched_tick"));
            trip.arrived_tick=epoch(field(latest,"arrived_tick"));trip.harvested_tick=epoch(field(latest,"harvested_tick"));
            trip.returned_tick=epoch(field(latest,"returned_tick"));trip.work_before=number(field(latest,"work_before"));
            trip.work_ticks=static_cast<std::uint32_t>(number(field(latest,"work_ticks"),multi_field_work_per_portion));
            trip.wait_left=static_cast<std::uint8_t>(number(field(latest,"wait_left"),physical_food_grower_wait_ticks));
            trip.delivered=static_cast<std::uint8_t>(number(field(latest,"delivered"),1));
            trip.cancel_requested=boolean(field(latest,"cancel_requested"));row.latest_trip=trip;
            if(seasonal) {
                row.latest_trip->wasted_work_before=number(field(latest,"wasted_work_before"));
                row.latest_trip->ready_tick=epoch(field(latest,"ready_tick"));
            }
            if(productivity)row.latest_trip->irrigation_bonus_before=number(field(latest,"irrigation_bonus_before"));
        }
        state.farms.push_back(row);
    }
    const auto& fields=field(j,"fields");
    require(fields.is_array() && fields.size()<=agriculture_field_limit,"schema47 physical Field collection exceeds bound");
    for(const auto& value:fields) {
        PhysicalFoodGrowerField row;row.field_id=static_cast<BuildingId>(id(field(value,"field_id")));
        row.farm_id=static_cast<BuildingId>(id(field(value,"farm_id")));row.returned=number(field(value,"returned"));
        require(state.fields.empty() || state.fields.back().field_id<row.field_id,
            "schema47 physical Field IDs must be sorted and unique");
        const auto& last=field(value,"last_receipt");
        if(!last.is_null()) {
            PhysicalFoodGrowerReceipt receipt;receipt.grower_id=grower_id(field(last,"grower_id"));
            receipt.courier_id=static_cast<CourierId>(id(field(last,"courier_id")));receipt.crop=crop(field(last,"crop"));
            receipt.dispatched_tick=number(field(last,"dispatched_tick"));receipt.arrived_tick=number(field(last,"arrived_tick"));
            receipt.harvested_tick=number(field(last,"harvested_tick"));receipt.returned_tick=number(field(last,"returned_tick"));
            receipt.work_at_harvest=number(field(last,"work_at_harvest"));row.last_receipt=receipt;
            if(seasonal) {
                row.last_receipt->wasted_work_at_harvest=number(field(last,"wasted_work_at_harvest"));
                row.last_receipt->ready_tick=number(field(last,"ready_tick"));
            }
            if(productivity)row.last_receipt->irrigation_bonus_at_harvest=number(field(last,"irrigation_bonus_at_harvest"));
        }
        state.fields.push_back(row);
    }
    require(canonical_fields(j,physical_food_growers_json(state,seasonal,productivity)),"schema47 contains unknown physical Grower authority");
    return state;
}
json seasonal_hemp_trip_json(const simulation::SeasonalHempTrip& trip,bool productivity) {
    json result={{"grower_id",trip.grower_id.value},{"field_id",static_cast<std::uint32_t>(trip.field_id)},
        {"dispatched_tick",trip.dispatched_tick},{"ready_tick",trip.ready_tick},
        {"picked_tick",trip.picked_tick ? json(*trip.picked_tick):json(nullptr)},
        {"returned_tick",trip.returned_tick ? json(*trip.returned_tick):json(nullptr)},{"delivered",trip.delivered}};
    require(trip.irrigation_ready_work.has_value()==productivity,
        "Hemp trip readiness work authority requires exact schema49");
    if(productivity)result["irrigation_ready_work"]=irrigation_work_witness_json(*trip.irrigation_ready_work);
    return result;
}
json seasonal_hemp_receipt_json(const simulation::SeasonalHempReceipt& receipt,bool productivity) {
    json result={{"grower_id",receipt.grower_id.value},{"courier_id",static_cast<std::uint32_t>(receipt.courier_id)},
        {"field_id",static_cast<std::uint32_t>(receipt.field_id)},{"dispatched_tick",receipt.dispatched_tick},
        {"ready_tick",receipt.ready_tick},{"picked_tick",receipt.picked_tick},{"returned_tick",receipt.returned_tick}};
    require(receipt.irrigation_ready_work.has_value()==productivity,
        "Hemp receipt readiness work authority requires exact schema49");
    if(productivity)result["irrigation_ready_work"]=irrigation_work_witness_json(*receipt.irrigation_ready_work);
    return result;
}
json seasonal_agriculture_json(const simulation::SeasonalAgricultureState& state,bool productivity=false) {
    using namespace simulation;
    require(state.fields.size()<=agriculture_field_limit && state.plants.size()<=plantation_plant_limit &&
        state.hemp_farms.size()<=2,"schema48 seasonal collections exceed inherited bounds");
    json fields=json::array(),plants=json::array(),hemp=json::array();
    for(const auto& row:state.fields)fields.push_back({{"field_id",static_cast<std::uint32_t>(row.field_id)},
        {"farm_id",static_cast<std::uint32_t>(row.farm_id)},{"wasted_work",row.wasted_work},
        {"expired_ready",row.expired_ready},{"harvest_work",row.harvest_work},
        {"wasted_harvest_work",row.wasted_harvest_work},{"harvest_progress",row.harvest_progress},
        {"ready_tick",row.ready_tick ? json(*row.ready_tick):json(nullptr)}});
    for(const auto& row:state.plants)plants.push_back({{"plant_id",static_cast<std::uint32_t>(row.plant_id)},
        {"source_id",static_cast<std::uint32_t>(row.source_id)},{"wasted_work",row.wasted_work},
        {"expired_ready",row.expired_ready},{"ready_tick",row.ready_tick ? json(*row.ready_tick):json(nullptr)}});
    for(const auto& row:state.hemp_farms)hemp.push_back({{"farm_id",static_cast<std::uint32_t>(row.farm_id)},
        {"courier_id",static_cast<std::uint32_t>(row.courier_id)},{"productive_ticks",row.productive_ticks},
        {"wasted_work",row.wasted_work},{"grown",row.grown},{"picked",row.picked},
        {"expired_ready",row.expired_ready},{"spawned",row.spawned},{"retired",row.retired},
        {"ready_tick",row.ready_tick ? json(*row.ready_tick):json(nullptr)},
        {"latest_trip",row.latest_trip ? seasonal_hemp_trip_json(*row.latest_trip,productivity):json(nullptr)},
        {"last_receipt",row.last_receipt ? seasonal_hemp_receipt_json(*row.last_receipt,productivity):json(nullptr)}});
    return {{"revision",seasonal_agriculture_revision},{"next_hemp_grower_id",state.next_hemp_grower_id},
        {"fields",std::move(fields)},{"plants",std::move(plants)},{"hemp_farms",std::move(hemp)}};
}
simulation::SeasonalAgricultureState parse_seasonal_agriculture(const json& j,bool productivity=false) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==seasonal_agriculture_revision,
        "schema48 requires its exact seasonal Agriculture revision");
    const auto positive=[](const json& value,std::uint64_t bound=UINT64_MAX) {
        const auto n=number(value,bound);require(n>0,"schema48 paid/lifetime identity must be positive");return n;
    };
    const auto id=[&](const json& value){return static_cast<BuildingId>(positive(value,UINT32_MAX));};
    const auto epoch=[](const json& value)->std::optional<std::uint64_t> {
        return value.is_null() ? std::nullopt:std::optional<std::uint64_t>(number(value));
    };
    SeasonalAgricultureState state;state.next_hemp_grower_id=positive(field(j,"next_hemp_grower_id"));
    const auto& fields=field(j,"fields");
    require(fields.is_array() && fields.size()<=agriculture_field_limit,"schema48 seasonal Field collection exceeds bound");
    for(const auto& value:fields) {
        SeasonalFoodField row;row.field_id=id(field(value,"field_id"));row.farm_id=id(field(value,"farm_id"));
        require(state.fields.empty() || state.fields.back().field_id<row.field_id,"schema48 seasonal Fields must be sorted and unique");
        row.wasted_work=number(field(value,"wasted_work"));row.expired_ready=number(field(value,"expired_ready"));
        row.harvest_work=number(field(value,"harvest_work"));row.wasted_harvest_work=number(field(value,"wasted_harvest_work"));
        row.harvest_progress=static_cast<std::uint8_t>(number(field(value,"harvest_progress"),seasonal_harvest_work_ticks-1));
        row.ready_tick=epoch(field(value,"ready_tick"));state.fields.push_back(row);
    }
    const auto& plants=field(j,"plants");
    require(plants.is_array() && plants.size()<=plantation_plant_limit,"schema48 seasonal plant collection exceeds bound");
    for(const auto& value:plants) {
        SeasonalPlant row;row.plant_id=id(field(value,"plant_id"));row.source_id=id(field(value,"source_id"));
        require(state.plants.empty() || state.plants.back().plant_id<row.plant_id,"schema48 seasonal plants must be sorted and unique");
        row.wasted_work=number(field(value,"wasted_work"));row.expired_ready=number(field(value,"expired_ready"));
        row.ready_tick=epoch(field(value,"ready_tick"));state.plants.push_back(row);
    }
    const auto& hemp=field(j,"hemp_farms");
    require(hemp.is_array() && hemp.size()<=2,"schema48 seasonal Hemp collection exceeds bound");
    for(const auto& value:hemp) {
        SeasonalHempFarm row;row.farm_id=id(field(value,"farm_id"));
        row.courier_id=static_cast<CourierId>(positive(field(value,"courier_id"),UINT32_MAX));
        require(state.hemp_farms.empty() || state.hemp_farms.back().farm_id<row.farm_id,"schema48 Hemp Farms must be sorted and unique");
        row.productive_ticks=number(field(value,"productive_ticks"));row.wasted_work=number(field(value,"wasted_work"));
        row.grown=number(field(value,"grown"));row.picked=number(field(value,"picked"));
        row.expired_ready=number(field(value,"expired_ready"));row.spawned=number(field(value,"spawned"));
        row.retired=number(field(value,"retired"));row.ready_tick=epoch(field(value,"ready_tick"));
        const auto& trip=field(value,"latest_trip");
        if(!trip.is_null()) {
            SeasonalHempTrip t;t.grower_id={positive(field(trip,"grower_id"))};t.field_id=id(field(trip,"field_id"));
            t.dispatched_tick=number(field(trip,"dispatched_tick"));t.ready_tick=number(field(trip,"ready_tick"));
            t.picked_tick=epoch(field(trip,"picked_tick"));t.returned_tick=epoch(field(trip,"returned_tick"));
            t.delivered=static_cast<std::uint8_t>(number(field(trip,"delivered"),1));
            if(productivity)t.irrigation_ready_work=parse_irrigation_work_witness(field(trip,"irrigation_ready_work"));
            row.latest_trip=t;
        }
        const auto& receipt=field(value,"last_receipt");
        if(!receipt.is_null()) {
            SeasonalHempReceipt r;r.grower_id={positive(field(receipt,"grower_id"))};
            r.courier_id=static_cast<CourierId>(positive(field(receipt,"courier_id"),UINT32_MAX));
            r.field_id=id(field(receipt,"field_id"));r.dispatched_tick=number(field(receipt,"dispatched_tick"));
            r.ready_tick=number(field(receipt,"ready_tick"));r.picked_tick=number(field(receipt,"picked_tick"));
            r.returned_tick=number(field(receipt,"returned_tick"));
            if(productivity)r.irrigation_ready_work=parse_irrigation_work_witness(field(receipt,"irrigation_ready_work"));
            row.last_receipt=r;
        }
        state.hemp_farms.push_back(row);
    }
    require(canonical_fields(j,seasonal_agriculture_json(state,productivity)),"schema48 contains unknown seasonal authority");return state;
}
json irrigation_productivity_json(const simulation::IrrigationProductivityState& state) {
    using namespace simulation;
    require(state.work.size()<=agriculture_field_limit+plantation_plant_limit+2,
        "schema49 irrigation work collection exceeds inherited crop bounds");
    json work=json::array();
    for(const auto& row:state.work)work.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
        {"wet_work_ticks",row.wet_work_ticks},{"bonus_work",row.bonus_work},{"clipped_bonus",row.clipped_bonus}});
    return {{"revision",irrigation_productivity_revision},{"work",std::move(work)}};
}
simulation::IrrigationProductivityState parse_irrigation_productivity(const json& j) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==irrigation_productivity_revision,
        "schema49 requires its exact irrigation productivity revision");
    const auto& work=field(j,"work");
    require(work.is_array() && work.size()<=agriculture_field_limit+plantation_plant_limit+2,
        "schema49 irrigation work collection exceeds inherited crop bounds");
    IrrigationProductivityState state;
    for(const auto& value:work) {
        IrrigationProductivityWork row;const auto id=number(field(value,"building_id"),UINT32_MAX);
        require(id>0,"schema49 irrigation work requires positive paid crop identities");
        row.building_id=static_cast<BuildingId>(id);
        require(state.work.empty() || state.work.back().building_id<row.building_id,
            "schema49 irrigation work identities must be sorted and unique");
        row.wet_work_ticks=number(field(value,"wet_work_ticks"));
        row.bonus_work=number(field(value,"bonus_work"));row.clipped_bonus=number(field(value,"clipped_bonus"));
        state.work.push_back(row);
    }
    require(canonical_fields(j,irrigation_productivity_json(state)),
        "schema49 contains unknown irrigation productivity authority");
    return state;
}
std::uint64_t positive_processing(const json& value,std::uint64_t maximum=UINT64_MAX) {
    const auto result=number(value,maximum);require(result>0,"schema50 requires a positive recipe sequence or grade");return result;
}
json food_processing_batch_json(const simulation::FoodProcessingBatch& batch) {
    return {{"sequence",batch.sequence},{"started_tick",batch.started_tick},{"grade",batch.grade},
        {"ingredients",batch.ingredients},{"work",batch.work}};
}
simulation::FoodProcessingBatch parse_food_processing_batch(const json& value) {
    simulation::FoodProcessingBatch batch;
    batch.sequence=positive_processing(field(value,"sequence"));batch.started_tick=number(field(value,"started_tick"));
    batch.grade=static_cast<std::uint8_t>(positive_processing(field(value,"grade"),4));
    const auto& ingredients=field(value,"ingredients");
    require(ingredients.is_array() && ingredients.size()==batch.ingredients.size(),
        "schema50 recipe requires exactly seven ingredient flags");
    for(std::size_t i=0;i<batch.ingredients.size();++i)
        batch.ingredients[i]=static_cast<std::uint8_t>(number(ingredients[i],1));
    batch.work=static_cast<std::uint32_t>(number(field(value,"work"),80));
    require(canonical_fields(value,food_processing_batch_json(batch)),"schema50 contains unknown recipe authority");
    return batch;
}
json food_processing_receipt_json(const simulation::FoodProcessingReceipt& receipt) {
    return {{"batch",food_processing_batch_json(receipt.batch)},{"completed_tick",receipt.completed_tick}};
}
simulation::FoodProcessingReceipt parse_food_processing_receipt(const json& value) {
    simulation::FoodProcessingReceipt receipt{parse_food_processing_batch(field(value,"batch")),number(field(value,"completed_tick"))};
    require(canonical_fields(value,food_processing_receipt_json(receipt)),"schema50 contains unknown completion receipt authority");
    return receipt;
}
json food_processing_arrival_json(const simulation::FoodProcessingArrival& arrival) {
    return {{"transfer",production_transfer_json(arrival.transfer)},
        {"source",static_cast<std::uint32_t>(arrival.source)},{"tick",arrival.tick},{"amount",arrival.amount}};
}
simulation::FoodProcessingArrival parse_food_processing_arrival(const json& value) {
    simulation::FoodProcessingArrival arrival;arrival.transfer=parse_production_transfer(field(value,"transfer"));
    arrival.source=production_id(field(value,"source"));arrival.tick=number(field(value,"tick"));
    arrival.amount=static_cast<std::int32_t>(positive_processing(field(value,"amount"),4));
    require(canonical_fields(value,food_processing_arrival_json(arrival)),"schema50 contains unknown first arrival authority");
    return arrival;
}
template<class T,std::size_t Count,class Encode> json processing_history_json(const std::array<std::optional<T>,Count>& rows,Encode encode) {
    json result=json::array();for(const auto& row:rows)result.push_back(row ? encode(*row):json(nullptr));return result;
}
template<class T,std::size_t Count,class Decode> std::array<std::optional<T>,Count> parse_processing_history(const json& value,Decode decode) {
    require(value.is_array() && value.size()==Count,"schema50 first-provenance array has wrong width");
    std::array<std::optional<T>,Count> result{};
    for(std::size_t i=0;i<Count;++i)if(!value[i].is_null())result[i]=decode(value[i]);
    return result;
}
json food_processing_json(const simulation::FoodProcessingState& state) {
    using namespace simulation;
    require(state.mills.size()<=food_processing_mill_limit && state.shops.size()<=food_processing_shop_limit &&
        state.houses.size()<=Rules::city_v10_household_limit,"schema50 processing collection exceeds bounded limits");
    json mills=json::array(),shops=json::array(),houses=json::array();
    for(const auto& row:state.mills) {
        const auto receipt=row.last_receipt ? food_processing_receipt_json(*row.last_receipt):json(nullptr);
        mills.push_back({{"mill_id",static_cast<std::uint32_t>(row.mill_id)},{"requested_grade",row.requested_grade},
            {"started",row.started},{"completed",row.completed},{"productive_ticks",row.productive_ticks},
            {"ingredients_consumed",row.ingredients_consumed},{"completed_by_grade",row.completed_by_grade},
            {"first_ingredients",processing_history_json(row.first_ingredients,food_processing_arrival_json)},
            {"first_completions",processing_history_json(row.first_completions,food_processing_receipt_json)},
            {"active",row.active ? food_processing_batch_json(*row.active):json(nullptr)},
            {"last_receipt",std::move(receipt)}});
    }
    for(const auto& row:state.shops)shops.push_back({{"shop_id",static_cast<std::uint32_t>(row.shop_id)},
        {"market_id",static_cast<std::uint32_t>(row.market_id)},
        {"first_meals",processing_history_json(row.first_meals,food_processing_arrival_json)}});
    for(const auto& row:state.houses)houses.push_back({{"house_id",static_cast<std::uint32_t>(row.house_id)},
        {"first_receipt",agriculture_quality_json(row.first_receipt)},
        {"first_by_grade",processing_history_json(row.first_by_grade,food_processing_arrival_json)},
        {"first_consumption",row.first_consumption ? json{{"tick",row.first_consumption->tick},
            {"grade",row.first_consumption->grade}}:json(nullptr)}});
    return {{"revision",food_processing_revision},{"mills",std::move(mills)},
        {"shops",std::move(shops)},{"houses",std::move(houses)}};
}
simulation::FoodProcessingState parse_food_processing(const json& j) {
    using namespace simulation;
    require(number(field(j,"revision"),UINT32_MAX)==food_processing_revision,
        "schema50 requires its exact Food processing revision");
    FoodProcessingState state;
    const auto& mills=field(j,"mills");
    require(mills.is_array() && mills.size()<=food_processing_mill_limit,"schema50 Mill collection exceeds bound");
    for(const auto& value:mills) {
        MillProcessing row;row.mill_id=production_id(field(value,"mill_id"));
        require(state.mills.empty() || state.mills.back().mill_id<row.mill_id,"schema50 Mill IDs must be sorted unique");
        row.requested_grade=static_cast<std::uint8_t>(positive_processing(field(value,"requested_grade"),4));
        row.started=number(field(value,"started"));row.completed=number(field(value,"completed"));
        row.productive_ticks=number(field(value,"productive_ticks"));
        const auto& consumed=field(value,"ingredients_consumed");
        require(consumed.is_array() && consumed.size()==row.ingredients_consumed.size(),
            "schema50 ingredient histories require exactly four recipe grades");
        for(std::size_t i=0;i<row.ingredients_consumed.size();++i)
            row.ingredients_consumed[i]=parse_production_counters<7>(consumed[i]);
        row.completed_by_grade=parse_production_counters<4>(field(value,"completed_by_grade"));
        row.first_ingredients=parse_processing_history<FoodProcessingArrival,7>(field(value,"first_ingredients"),parse_food_processing_arrival);
        row.first_completions=parse_processing_history<FoodProcessingReceipt,4>(field(value,"first_completions"),parse_food_processing_receipt);
        const auto& active=field(value,"active");if(!active.is_null())row.active=parse_food_processing_batch(active);
        const auto& receipt=field(value,"last_receipt");
        if(!receipt.is_null())row.last_receipt=parse_food_processing_receipt(receipt);
        state.mills.push_back(row);
    }
    const auto& shops=field(j,"shops");
    require(shops.is_array() && shops.size()<=food_processing_shop_limit,"schema50 Food Shop collection exceeds bound");
    for(const auto& value:shops) {
        FoodProcessingShop row;row.shop_id=production_id(field(value,"shop_id"));row.market_id=production_id(field(value,"market_id"));
        row.first_meals=parse_processing_history<FoodProcessingArrival,4>(field(value,"first_meals"),parse_food_processing_arrival);
        require(state.shops.empty() || state.shops.back().shop_id<row.shop_id,"schema50 Food Shop IDs must be sorted unique");
        state.shops.push_back(row);
    }
    const auto& houses=field(j,"houses");
    require(houses.is_array() && houses.size()<=Rules::city_v10_household_limit,"schema50 meal House history exceeds bound");
    for(const auto& value:houses) {
        MealHouseHistory row;row.house_id=production_id(field(value,"house_id"));
        require(state.houses.empty() || state.houses.back().house_id<row.house_id,"schema50 meal House IDs must be sorted unique");
        const auto& receipt=field(value,"first_receipt");auto& witness=row.first_receipt;
        witness.transfer=parse_production_transfer(field(receipt,"transfer"));witness.tick=number(field(receipt,"tick"));
        const auto& accepted=field(receipt,"accepted");
        require(accepted.is_array() && accepted.size()==processed_food_count,"schema50 meal receipt requires twelve amounts");
        for(std::size_t i=0;i<processed_food_count;++i)witness.accepted[i]=static_cast<std::int32_t>(number(accepted[i],INT32_MAX));
        witness.consumed_before=parse_production_counters<processed_food_count>(field(receipt,"consumed_before"));
        witness.previous_quality=static_cast<std::uint8_t>(number(field(receipt,"previous_quality"),UINT8_MAX));
        row.first_by_grade=parse_processing_history<FoodProcessingArrival,4>(field(value,"first_by_grade"),parse_food_processing_arrival);
        const auto& consumed=field(value,"first_consumption");
        if(!consumed.is_null())row.first_consumption=MealConsumptionReceipt{number(field(consumed,"tick")),
            static_cast<std::uint8_t>(positive_processing(field(consumed,"grade"),4))};
        state.houses.push_back(row);
    }
    require(canonical_fields(j,food_processing_json(state)),"schema50 contains unknown or noncanonical processing authority");
    return state;
}
json food_warehouse_delivery_json(const simulation::FoodWarehouseDelivery& arrival) {
    return {{"transfer",production_transfer_json(arrival.transfer)},
        {"target",static_cast<std::uint32_t>(arrival.target)},{"tick",arrival.tick},{"amount",arrival.amount}};
}
simulation::FoodWarehouseDelivery parse_food_warehouse_delivery(const json& value) {
    simulation::FoodWarehouseDelivery arrival;arrival.transfer=parse_production_transfer(field(value,"transfer"));
    arrival.target=production_id(field(value,"target"));arrival.tick=number(field(value,"tick"));
    arrival.amount=static_cast<std::int32_t>(positive_processing(field(value,"amount"),4));
    require(canonical_fields(value,food_warehouse_delivery_json(arrival)),"schema51 contains unknown outbound Food receipt authority");
    return arrival;
}
json food_warehouses_json(const simulation::FoodWarehousesState& state) {
    using namespace simulation;
    require(state.warehouses.size()<=Rules::city_v10_warehouse_limit && state.markets.size()<=Rules::city_v11_market_limit,
        "schema51 Warehouse or Market provenance collection exceeds bounded limits");
    json warehouses=json::array(),markets=json::array();
    for(const auto& row:state.warehouses)warehouses.push_back({{"warehouse_id",static_cast<std::uint32_t>(row.warehouse_id)},
        {"first_received",processing_history_json(row.first_received,food_processing_arrival_json)},
        {"first_delivered",processing_history_json(row.first_delivered,food_warehouse_delivery_json)}});
    for(const auto& row:state.markets)markets.push_back({{"market_id",static_cast<std::uint32_t>(row.market_id)},
        {"first_received",processing_history_json(row.first_received,food_processing_arrival_json)}});
    return {{"revision",food_warehouses_revision},{"warehouses",std::move(warehouses)},{"markets",std::move(markets)}};
}
simulation::FoodWarehousesState parse_food_warehouses(const json& value) {
    using namespace simulation;
    require(number(field(value,"revision"),UINT32_MAX)==food_warehouses_revision,"schema51 requires its exact Food Warehouse revision");
    FoodWarehousesState state;
    const auto& warehouses=field(value,"warehouses");const auto& markets=field(value,"markets");
    require(warehouses.is_array() && warehouses.size()<=Rules::city_v10_warehouse_limit,"schema51 Warehouse provenance collection exceeds bound");
    for(const auto& item:warehouses){
        FoodWarehouseHistory row;row.warehouse_id=production_id(field(item,"warehouse_id"));
        require(state.warehouses.empty() || state.warehouses.back().warehouse_id<row.warehouse_id,"schema51 Warehouse IDs must be sorted unique");
        row.first_received=parse_processing_history<FoodProcessingArrival,processed_food_count>(field(item,"first_received"),parse_food_processing_arrival);
        row.first_delivered=parse_processing_history<FoodWarehouseDelivery,processed_food_count>(field(item,"first_delivered"),parse_food_warehouse_delivery);
        state.warehouses.push_back(row);
    }
    require(markets.is_array() && markets.size()<=Rules::city_v11_market_limit,"schema51 Market provenance collection exceeds bound");
    for(const auto& item:markets){
        FoodWarehouseMarket row;row.market_id=production_id(field(item,"market_id"));
        require(state.markets.empty() || state.markets.back().market_id<row.market_id,"schema51 Market IDs must be sorted unique");
        row.first_received=parse_processing_history<FoodProcessingArrival,processed_food_first_meal>(field(item,"first_received"),parse_food_processing_arrival);
        state.markets.push_back(row);
    }
    require(canonical_fields(value,food_warehouses_json(state)),"schema51 contains unknown or noncanonical Food Warehouse provenance");
    return state;
}
json agriculture_drain_work_json(const simulation::AgricultureDrainWork& state) {
    return {{"work",irrigation_work_witness_json(state.work)},{"harvested",state.harvested}};
}
simulation::AgricultureDrainWork parse_agriculture_drain_work(const json& value) {
    simulation::AgricultureDrainWork state{parse_irrigation_work_witness(field(value,"work")),number(field(value,"harvested"))};
    require(canonical_fields(value,agriculture_drain_work_json(state)),"schema52 contains unknown initial drain-work provenance");
    return state;
}
json agriculture_reconfiguration_json(const simulation::AgricultureReconfigurationState& state) {
    using namespace simulation;
    require(state.entries.size()<=agriculture_reconfiguration_limit,"schema52 drain/removal journal exceeds its bounded limit");
    json entries=json::array();
    for(const auto& row:state.entries)entries.push_back({{"building_id",static_cast<std::uint32_t>(row.building_id)},
        {"started_tick",row.started_tick},{"started_sequence",row.started_sequence},
        {"removed_tick",row.removed_tick ? json(*row.removed_tick):json(nullptr)},
        {"removed_sequence",row.removed_sequence ? json(*row.removed_sequence):json(nullptr)},
        {"initial_work",row.initial_work ? agriculture_drain_work_json(*row.initial_work):json(nullptr)}});
    return {{"revision",agriculture_reconfiguration_revision},{"entries",std::move(entries)}};
}
simulation::AgricultureReconfigurationState parse_agriculture_reconfiguration(const json& value) {
    using namespace simulation;
    require(number(field(value,"revision"),UINT32_MAX)==agriculture_reconfiguration_revision,
        "schema52 requires its exact agriculture reconfiguration revision");
    AgricultureReconfigurationState state;const auto& entries=field(value,"entries");
    require(entries.is_array() && entries.size()<=agriculture_reconfiguration_limit,"schema52 drain/removal journal exceeds its bounded limit");
    for(const auto& item:entries){
        AgricultureReconfigurationEntry row;row.building_id=production_id(field(item,"building_id"));
        require(state.entries.empty() || state.entries.back().building_id<row.building_id,"schema52 drain/removal IDs must be sorted unique");
        row.started_tick=number(field(item,"started_tick"));row.started_sequence=number(field(item,"started_sequence"));
        require(row.started_sequence>0,"schema52 drain request requires its positive paid command sequence");
        const auto& initial=field(item,"initial_work");if(!initial.is_null())row.initial_work=parse_agriculture_drain_work(initial);
        const auto& removed_tick=field(item,"removed_tick");const auto& removed_sequence=field(item,"removed_sequence");
        require(removed_tick.is_null()==removed_sequence.is_null(),"schema52 closed removal requires both tick and command sequence");
        if(!removed_tick.is_null()){
            row.removed_tick=number(removed_tick);row.removed_sequence=number(removed_sequence);
            require(*row.removed_sequence>0,"schema52 closed removal requires a positive command sequence");
        }
        state.entries.push_back(row);
    }
    require(canonical_fields(value,agriculture_reconfiguration_json(state)),"schema52 contains unknown or noncanonical drain/removal authority");
    return state;
}
json production_document_json(const SaveDocument& d) {
    const bool reconfiguration=simulation::agriculture_reconfiguration_enabled(d.world.profile,d.world.rule_version);
    const bool warehouses=simulation::food_warehouses_enabled(d.world.profile,d.world.rule_version);
    const bool processing=simulation::food_processing_enabled(d.world.profile,d.world.rule_version);
    const bool productivity=simulation::irrigation_productivity_enabled(d.world.profile,d.world.rule_version);
    const bool seasonal=simulation::seasonal_agriculture_enabled(d.world.profile,d.world.rule_version);
    const bool physical=simulation::physical_food_growers_enabled(d.world.profile,d.world.rule_version);
    const bool multi=simulation::multi_field_agriculture_enabled(d.world.profile,d.world.rule_version);
    const bool hunting=simulation::playable_hunting_enabled(d.world.profile,d.world.rule_version);
    const bool fishing=simulation::playable_fishing_enabled(d.world.profile,d.world.rule_version);
    const bool playable=simulation::playable_agriculture_enabled(d.world.profile,d.world.rule_version);
    const bool irrigation=simulation::original_irrigation_enabled(d.world.profile,d.world.rule_version);
    const bool plantations=simulation::original_plantations_enabled(d.world.profile,d.world.rule_version);
    const bool agriculture=simulation::original_agriculture_family_enabled(d.world.profile,d.world.rule_version);
    const bool fire=simulation::original_fire_protection_enabled(d.world.profile,d.world.rule_version);
    const bool culture=simulation::original_music_venues_enabled(d.world.profile,d.world.rule_version);
    const bool venue_service=simulation::original_venue_house_service_enabled(d.world.profile,d.world.rule_version);
    const auto schema=reconfiguration ? 52U:warehouses ? 51U:processing ? 50U:productivity ? 49U:seasonal ? 48U:physical ? 47U:multi ? 46U:hunting ? 45U:fishing ? 44U:playable ? 43U:irrigation ? 42U:plantations ? 41U:agriculture ? 40U:venue_service ? 39U:culture ? 38U:simulation::original_safe_demolition_enabled(d.world.profile,d.world.rule_version) ? 35U:
        simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ? 34U:
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
            "Original production rules require their exact schema; existing saves are not migrated");
    require(d.world.original_production->development_revision==simulation::kOriginalProductionDevelopmentRevision,
            "schema26 development revision differs from this executable");
    require(d.world.original_production->culture.has_value()==culture,
            "culture authority is mandatory only for Rule22/23 schema38/39");
    require(d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion &&
            valid_hash(d.map_permissions_sha256),"schema26 requires immutable policy3 identity");
    require(d.world.agriculture.has_value()==(agriculture && !fishing),"Agriculture authority requires its exact versioned generation");
    require(d.world.processed_food.has_value()==processing && d.world.food_processing.has_value()==processing,
        "Food processing requires exclusive Rule34/schema50 authority");
    require(d.world.agriculture_reconfiguration.has_value()==reconfiguration,"Agriculture reconfiguration authority requires exact Rule36/schema52");
    if(reconfiguration) simulation::World::validate_agriculture_reconfiguration_snapshot(d.world);
    require(d.world.food_warehouses.has_value()==warehouses,"Food Warehouse authority requires exact Rule35/schema51");
    if(warehouses) simulation::World::validate_food_warehouses_snapshot(d.world);
    if(processing) simulation::World::validate_food_processing_snapshot(d.world);
    require(d.world.irrigation_productivity.has_value()==productivity,"Irrigation productivity requires exact Rule33/schema49");
    if(productivity) simulation::World::validate_irrigation_productivity_snapshot(d.world);
    require(d.world.seasonal_agriculture.has_value()==seasonal,"Seasonal Agriculture requires exact Rule32/schema48");
    if(seasonal) simulation::World::validate_seasonal_agriculture_snapshot(d.world);
    require(d.world.physical_food_growers.has_value()==physical,"Physical Grower authority requires exact Rule31/schema47");
    if(physical) simulation::World::validate_physical_food_growers_snapshot(d.world);
    require(d.world.multi_field_agriculture.has_value()==multi,"Multi-field authority requires exact Rule30/schema46");
    if(multi) simulation::World::validate_multi_field_agriculture_snapshot(d.world);
    require(d.world.hunting_food.has_value()==(hunting && !processing) && d.world.hunting.has_value()==hunting,
        "Hunting requires exact schema45 seven-kind Food and physical Hunting authority");
    if(hunting) simulation::World::validate_hunting_snapshot(d.world);
    require(d.world.fishing_food.has_value()==(fishing && !hunting) && d.world.fishing.has_value()==fishing,
        "Fishing requires exact schema44 six-kind Food and physical Fishing authority");
    if(fishing) simulation::World::validate_fishing_snapshot(d.world);
    require(d.world.plantations.has_value()==plantations,"Plantation authority requires exact Rule25/schema41");
    require(d.world.irrigation.has_value()==irrigation,"Irrigation authority requires exact Rule26/schema42");
    if (irrigation) simulation::World::validate_irrigation_snapshot(d.world);
    simulation::World::validate_original_production_snapshot(d.world);
    // Reuse infrastructure syntax only. No positive state enters a frozen
    // schema23/24/25 admission path or an older live World restore.
    auto projected=d;
    projected.world.rule_version=6;
    projected.world.original_economy.reset();projected.world.original_influence.reset();
    projected.world.original_goods.reset();projected.world.original_production.reset();projected.world.agriculture.reset();
    projected.world.plantations.reset();
    projected.world.irrigation.reset();projected.world.fishing_food.reset();projected.world.fishing.reset();
    projected.world.hunting_food.reset();projected.world.hunting.reset();
    projected.world.processed_food.reset();projected.world.food_processing.reset();projected.world.food_warehouses.reset();projected.world.agriculture_reconfiguration.reset();
    projected.world.multi_field_agriculture.reset();
    projected.world.physical_food_growers.reset();
    projected.world.seasonal_agriculture.reset();
    projected.world.irrigation_productivity.reset();
    for (auto& building:projected.world.buildings) {
        if (processing && (building.kind==simulation::Object::OriginalMill53 || building.kind==simulation::Object::OriginalFoodShop66)) building.kind=simulation::Object::Pottery;
        else if (hunting && building.kind==simulation::Object::OriginalHuntersTent) building.kind=simulation::Object::Pottery;
        else if (fishing && building.kind==simulation::Object::OriginalFishingQuay) building.kind=simulation::Object::Pottery;
        else if (irrigation && building.kind==simulation::Object::OriginalIrrigationPump) building.kind=simulation::Object::Pottery;
        else if (culture && building.kind==simulation::Object::OriginalTheatre75) building.kind=simulation::Object::ServicePost;
        else if (simulation::original_elite_house_kind(building.kind)) building.kind=simulation::Object::Household;
        else if (building.kind==simulation::Object::OriginalInfluenceSource) building.kind=simulation::Object::Well;
        else if (building.kind==simulation::Object::OriginalHempFarm) building.kind=simulation::Object::Pottery;
        else if (building.kind==simulation::Object::OriginalHempField) building.kind=simulation::Object::Well;
        else if (building.kind==simulation::Object::OriginalFoodFarm) building.kind=simulation::Object::Farm;
        else if (building.kind==simulation::Object::OriginalWheatField || building.kind==simulation::Object::OriginalMilletField ||
                 (agriculture && (building.kind==simulation::Object::OriginalRiceField ||
                  building.kind==simulation::Object::OriginalCabbageField || building.kind==simulation::Object::OriginalSoybeanField)))
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
        if (warehouses && courier.role==simulation::CourierRole::OriginalWarehouseFood) courier.role=simulation::CourierRole::MarketFoodDistribution;
        else if (processing && (courier.role==simulation::CourierRole::OriginalMealsToShop || courier.role==simulation::CourierRole::OriginalMealsToHouse))
            courier.role=simulation::CourierRole::MarketFoodDistribution;
        else if (physical && courier.role==simulation::CourierRole::OriginalFoodGrower)
            courier.role=simulation::CourierRole::Service;
        else if (seasonal && courier.role==simulation::CourierRole::OriginalSeasonalHempGrower)
            courier.role=simulation::CourierRole::Service;
        else if (plantations && courier.role==simulation::CourierRole::OriginalPlantationGrower)
            courier.role=simulation::CourierRole::Service;
        else if ((culture && courier.role==simulation::CourierRole::OriginalMusicPerformer) ||
            (venue_service && courier.role==simulation::CourierRole::OriginalVenueMusic)) courier.role=simulation::CourierRole::Service;
        else if (courier.role==simulation::CourierRole::OriginalHempToWarehouse ||
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
    if (agriculture) result["rules"]["agriculture"]=simulation::original_agriculture_contract;
    if (plantations) result["rules"]["plantations"]=simulation::original_plantations_contract;
    if (irrigation) result["rules"]["irrigation"]=simulation::irrigation_contract;
    if (playable) {
        result["rules"]["playable_agriculture"]=simulation::playable_agriculture_contract;
        result["rules"]["playable_agriculture_revision"]=simulation::playable_agriculture_revision;
    }
    if (multi) {
        result["rules"]["multi_field_agriculture"]=simulation::multi_field_agriculture_contract;
        result["rules"]["multi_field_agriculture_revision"]=simulation::multi_field_agriculture_revision;
    }
    if(reconfiguration) {
        result["rules"]["agriculture_reconfiguration"]=simulation::agriculture_reconfiguration_contract;
        result["rules"]["agriculture_reconfiguration_revision"]=simulation::agriculture_reconfiguration_revision;
    }
    if(warehouses) {
        result["rules"]["food_warehouses"]=simulation::food_warehouses_contract;
        result["rules"]["food_warehouses_revision"]=simulation::food_warehouses_revision;
    }
    if(processing) {
        result["rules"]["food_processing"]=simulation::food_processing_contract;
        result["rules"]["food_processing_revision"]=simulation::food_processing_revision;
    }
    if(productivity) {
        result["rules"]["irrigation_productivity"]=simulation::irrigation_productivity_contract;
        result["rules"]["irrigation_productivity_revision"]=simulation::irrigation_productivity_revision;
    }
    if(seasonal) {
        result["rules"]["seasonal_agriculture"]=simulation::seasonal_agriculture_contract;
        result["rules"]["seasonal_agriculture_revision"]=simulation::seasonal_agriculture_revision;
    }
    if (physical) {
        result["rules"]["physical_food_growers"]=simulation::physical_food_growers_contract;
        result["rules"]["physical_food_growers_revision"]=simulation::physical_food_growers_revision;
    }
    if (hunting) {
        result["rules"]["playable_hunting"]=simulation::playable_hunting_contract;
        result["rules"]["playable_hunting_revision"]=simulation::playable_hunting_revision;
    }
    if (fishing) {
        result["rules"]["playable_fishing"]=simulation::playable_fishing_contract;
        result["rules"]["playable_fishing_revision"]=simulation::playable_fishing_revision;
    }
    if (culture) result["rules"]["music_venues"]=simulation::original_music_venues_contract;
    if (venue_service) result["rules"]["venue_house_service"]=simulation::original_venue_house_service_contract;
    if (schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) result["rules"]["market_logistics"]=simulation::original_market_logistics_contract;
    if (schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) result["rules"]["cardinal_water"]=simulation::original_cardinal_water_contract;
    if (schema==31) result["rules"]["inspector_coordination"]=simulation::original_inspector_coordination_contract;
    if (schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) result["rules"]["deadline_fire_dispatch"]=simulation::original_deadline_fire_dispatch_contract;
    if (schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) result["rules"]["deadline_service_dispatch"]=simulation::original_deadline_service_dispatch_contract;
    if (schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) result["rules"]["deadline_food_dispatch"]=simulation::original_deadline_food_dispatch_contract;
    if (schema==35) result["rules"]["safe_demolition"]=simulation::original_safe_demolition_contract;
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
        {"services",production_services_json(production.services,venue_service)},
        {"water",production_water_json(production.water)},{"housing",production_housing_json(production.housing,venue_service)},
        {"industry",production_industry_json(production.industry)},
        {"finance",production_finance_json(production.finance)}};
    if (fire) result["world"]["original_production"]["fire_protection"]=fire_protection_json(*production.fire_protection,d.world.width);
    if (schema==35) result["world"]["original_production"]["safe_demolition"]=production_safe_demolition_json(*production.safe_demolition);
    if (culture) result["world"]["original_production"]["culture"]=production_culture_json(*production.culture,venue_service);
    if (agriculture && !fishing) result["world"]["agriculture"]=agriculture_json(*d.world.agriculture);
    if(reconfiguration) result["world"]["agriculture_reconfiguration"]=agriculture_reconfiguration_json(*d.world.agriculture_reconfiguration);
    if(warehouses) result["world"]["food_warehouses"]=food_warehouses_json(*d.world.food_warehouses);
    if(processing) {
        result["world"]["processed_food"]=agriculture_json(*d.world.processed_food);
        result["world"]["food_processing"]=food_processing_json(*d.world.food_processing);
    }
    if (hunting) {
        if(!processing) result["world"]["hunting_food"]=agriculture_json(*d.world.hunting_food);
        result["world"]["hunting"]=hunting_json(*d.world.hunting);
    }
    if (multi) result["world"]["multi_field_agriculture"]=multi_field_agriculture_json(*d.world.multi_field_agriculture);
    if (physical) result["world"]["physical_food_growers"]=physical_food_growers_json(*d.world.physical_food_growers,seasonal,productivity);
    if(seasonal) result["world"]["seasonal_agriculture"]=seasonal_agriculture_json(*d.world.seasonal_agriculture,productivity);
    if(productivity) result["world"]["irrigation_productivity"]=irrigation_productivity_json(*d.world.irrigation_productivity);
    if (fishing) {
        if(!hunting) result["world"]["fishing_food"]=agriculture_json(*d.world.fishing_food);
        result["world"]["fishing"]=fishing_json(*d.world.fishing);
    }
    if (plantations) result["world"]["plantations"]=plantations_json(*d.world.plantations,seasonal,productivity);
    if (irrigation) result["world"]["irrigation"]=irrigation_json(*d.world.irrigation);
    return result;
}
SaveDocument parse_production_document(const json& j) {
    const auto schema=number(field(j,"schema_version"));
    const bool reconfiguration=schema==52;
    const bool warehouses=schema==51 || reconfiguration;
    const bool processing=schema==50 || warehouses;
    const bool productivity=schema==49 || processing;
    const bool seasonal=schema==48 || productivity;
    const bool physical=schema==47 || seasonal;
    const bool multi=schema==46 || physical;
    const bool hunting=schema==45 || multi;
    const bool fishing=schema==44 || hunting;
    const bool playable=schema==43 || fishing;
    const bool irrigation=schema==42 || playable;
    const bool plantations=schema==41 || irrigation;
    const bool agriculture=schema==40 || plantations;
    if(reconfiguration) require(
        str(field(field(j,"rules"),"agriculture_reconfiguration"))==simulation::agriculture_reconfiguration_contract &&
        number(field(field(j,"rules"),"agriculture_reconfiguration_revision"),UINT32_MAX)==simulation::agriculture_reconfiguration_revision,
        "schema52 requires its exact AUTHORED agriculture reconfiguration contract and revision");
    if(warehouses) require(
        str(field(field(j,"rules"),"food_warehouses"))==simulation::food_warehouses_contract &&
        number(field(field(j,"rules"),"food_warehouses_revision"),UINT32_MAX)==simulation::food_warehouses_revision,
        "schema51 requires its exact AUTHORED Food Warehouse contract and revision");
    if(processing) require(
        str(field(field(j,"rules"),"food_processing"))==simulation::food_processing_contract &&
        number(field(field(j,"rules"),"food_processing_revision"),UINT32_MAX)==simulation::food_processing_revision,
        "schema50 requires its exact AUTHORED food processing contract and revision");
    if(productivity) require(
        str(field(field(j,"rules"),"irrigation_productivity"))==simulation::irrigation_productivity_contract &&
        number(field(field(j,"rules"),"irrigation_productivity_revision"),UINT32_MAX)==simulation::irrigation_productivity_revision,
        "schema49 requires its exact AUTHORED irrigation productivity contract and revision");
    if(seasonal) require(
        str(field(field(j,"rules"),"seasonal_agriculture"))==simulation::seasonal_agriculture_contract &&
        number(field(field(j,"rules"),"seasonal_agriculture_revision"),UINT32_MAX)==simulation::seasonal_agriculture_revision,
        "schema48 requires its exact AUTHORED seasonal Agriculture contract and revision");
    if(physical) require(
        str(field(field(j,"rules"),"physical_food_growers"))==simulation::physical_food_growers_contract &&
        number(field(field(j,"rules"),"physical_food_growers_revision"),UINT32_MAX)==simulation::physical_food_growers_revision,
        "schema47 requires its exact AUTHORED physical Grower contract and revision");
    if(multi) require(
        str(field(field(j,"rules"),"multi_field_agriculture"))==simulation::multi_field_agriculture_contract &&
        number(field(field(j,"rules"),"multi_field_agriculture_revision"),UINT32_MAX)==simulation::multi_field_agriculture_revision,
        "schema46 requires its exact AUTHORED multi-field contract and revision");
    if(hunting) require(
        str(field(field(j,"rules"),"playable_hunting"))==simulation::playable_hunting_contract &&
        number(field(field(j,"rules"),"playable_hunting_revision"),UINT32_MAX)==simulation::playable_hunting_revision,
        "schema45 requires its exact AUTHORED Hunting contract and revision");
    if(fishing) require(
        str(field(field(j,"rules"),"playable_fishing"))==simulation::playable_fishing_contract &&
        number(field(field(j,"rules"),"playable_fishing_revision"),UINT32_MAX)==simulation::playable_fishing_revision,
        "schema44 requires its exact AUTHORED Fishing contract and revision");
    if (playable) require(
        str(field(field(j,"rules"),"playable_agriculture"))==simulation::playable_agriculture_contract &&
        number(field(field(j,"rules"),"playable_agriculture_revision"),UINT32_MAX)==simulation::playable_agriculture_revision,
        "schema43 requires its exact AUTHORED Playable Agriculture contract and revision");
    if (irrigation) require(str(field(field(j,"rules"),"irrigation"))==simulation::irrigation_contract,
        "schema42 requires its exact irrigation contract");
    if (plantations) require(str(field(field(j,"rules"),"plantations"))==simulation::original_plantations_contract,
        "schema41 requires its exact plantation contract");
    if (agriculture) require(str(field(field(j,"rules"),"agriculture"))==simulation::original_agriculture_contract,
        "schema40 requires its exact Agriculture contract");
    const bool venue_service=schema==39;
    if(venue_service)require(str(field(field(j,"rules"),"venue_house_service"))==simulation::original_venue_house_service_contract,
        "schema39 requires its exact authored venue House-service contract");
    if (schema==38 || schema==39) require(str(field(field(j,"rules"),"music_venues"))==simulation::original_music_venues_contract,
        "schema38 requires its exact authored music-venue contract");
    const bool fire=schema==27 || schema==28 || schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52;
    if (schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) require(str(field(field(j,"rules"),"market_logistics"))==simulation::original_market_logistics_contract,
        "schema29 requires its exact authored market-logistics contract");
    if (schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) require(str(field(field(j,"rules"),"cardinal_water"))==simulation::original_cardinal_water_contract,
        "schema30 requires its exact verified cardinal-water contract");
    if (schema==31) require(str(field(field(j,"rules"),"inspector_coordination"))==simulation::original_inspector_coordination_contract,
        "schema31 requires its exact authored Inspector-coordination contract");
    if (schema==32 || schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) require(str(field(field(j,"rules"),"deadline_fire_dispatch"))==simulation::original_deadline_fire_dispatch_contract,
        "schema32 requires its exact authored deadline-fire-dispatch contract");
    if (schema==33 || schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) require(str(field(field(j,"rules"),"deadline_service_dispatch"))==simulation::original_deadline_service_dispatch_contract,
        "schema33 requires its exact authored deadline-service-dispatch contract");
    if (schema==34 || schema==35 || schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) require(str(field(field(j,"rules"),"deadline_food_dispatch"))==simulation::original_deadline_food_dispatch_contract,
        "schema34 requires its exact authored deadline-food-dispatch contract");
    if (schema==35) require(str(field(field(j,"rules"),"safe_demolition"))==simulation::original_safe_demolition_contract,
        "schema35 requires its exact authored safe-demolition contract");
    const auto rule_version=reconfiguration ? simulation::kAgricultureReconfigurationRuleVersion:warehouses ? simulation::kFoodWarehousesRuleVersion:processing ? simulation::kFoodProcessingRuleVersion:productivity ? simulation::kIrrigationProductivityRuleVersion:seasonal ? simulation::kSeasonalAgricultureRuleVersion:physical ? simulation::kPhysicalFoodGrowersRuleVersion:multi ? simulation::kMultiFieldAgricultureRuleVersion:hunting ? simulation::kPlayableHuntingRuleVersion:fishing ? simulation::kPlayableFishingRuleVersion:
        playable ? simulation::kPlayableAgricultureRuleVersion:
        irrigation ? simulation::kOriginalIrrigationRuleVersion:
        plantations ? simulation::kOriginalPlantationsRuleVersion:
        agriculture ? simulation::kOriginalAgricultureRuleVersion:
        schema==39 ? simulation::kOriginalVenueHouseServiceRuleVersion:
        schema==38 ? simulation::kOriginalMusicVenuesRuleVersion:
        schema==35 ? simulation::kOriginalSafeDemolitionRuleVersion:
        schema==34 ? simulation::kOriginalDeadlineFoodDispatchRuleVersion:
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
    state.services=parse_production_services(field(production,"services"),venue_service);
    state.water=parse_production_water(field(production,"water"));
    state.housing=parse_production_housing(field(production,"housing"),venue_service);
    state.industry=parse_production_industry(field(production,"industry"));
    if (schema==38 || schema==39) state.culture=parse_production_culture(field(production,"culture"),venue_service);
    if (schema==35) state.safe_demolition=parse_production_safe_demolition(field(production,"safe_demolition"));
    auto projected=j;
    projected["schema_version"]=22;projected["rules"]["version"]=6;
    for (const char* key:{"original_economy","original_influence","original_goods","original_production","agriculture","plantations","irrigation","fishing_food","fishing","hunting_food","hunting","multi_field_agriculture","physical_food_growers","seasonal_agriculture","irrigation_productivity","processed_food","food_processing","food_warehouses","agriculture_reconfiguration"})
        projected["world"].erase(key);
    auto& buildings=projected["world"]["buildings"];
    require(buildings.is_array() && buildings.size()<=simulation::Rules::original_production_building_limit,
            "schema26 building collection exceeds bound");
    for (auto& building:buildings) {
        if(reconfiguration) building["placed"]=true;
        const auto kind=number(field(building,"kind"),static_cast<unsigned>(processing ? simulation::Object::OriginalFoodShop66:hunting ? simulation::Object::OriginalHuntersTent:fishing ? simulation::Object::OriginalFishingQuay:irrigation ? simulation::Object::OriginalIrrigationPump:agriculture ? simulation::Object::OriginalSoybeanField:
            (schema==38 || schema==39) ? simulation::Object::OriginalTheatre75:simulation::Object::OriginalEliteHouse));
        if(multi && (kind==static_cast<unsigned>(simulation::Object::OriginalWheatField) ||
            kind==static_cast<unsigned>(simulation::Object::OriginalMilletField) ||
            kind==static_cast<unsigned>(simulation::Object::OriginalRiceField) ||
            kind==static_cast<unsigned>(simulation::Object::OriginalCabbageField) ||
            kind==static_cast<unsigned>(simulation::Object::OriginalSoybeanField))) {
            // Frozen Well syntax never gains field operation authority. Only
            // schema46's local projection is neutral; exact source values are
            // parsed and reapplied below before versioned validation.
            building["operating_enabled"]=true;
            building["workforce_priority"]=static_cast<int>(simulation::WorkforcePriority::Normal);
        }
        if (processing && (kind==static_cast<unsigned>(simulation::Object::OriginalMill53) || kind==static_cast<unsigned>(simulation::Object::OriginalFoodShop66)))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if (hunting && kind==static_cast<unsigned>(simulation::Object::OriginalHuntersTent))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if (fishing && kind==static_cast<unsigned>(simulation::Object::OriginalFishingQuay))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if (irrigation && kind==static_cast<unsigned>(simulation::Object::OriginalIrrigationPump))
            building["kind"]=static_cast<int>(simulation::Object::Pottery);
        else if ((schema==38 || schema==39) && kind==static_cast<unsigned>(simulation::Object::OriginalTheatre75))
            building["kind"]=static_cast<int>(simulation::Object::ServicePost);
        else if (simulation::original_elite_house_kind(static_cast<simulation::Object>(kind)))
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
                 kind==static_cast<unsigned>(simulation::Object::OriginalMilletField) ||
                 (agriculture && (kind==static_cast<unsigned>(simulation::Object::OriginalRiceField) ||
                  kind==static_cast<unsigned>(simulation::Object::OriginalCabbageField) ||
                  kind==static_cast<unsigned>(simulation::Object::OriginalSoybeanField))))
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
        const auto role=number(field(courier,"role"),static_cast<unsigned>(warehouses ? simulation::CourierRole::OriginalWarehouseFood:processing ? simulation::CourierRole::OriginalMealsToHouse:seasonal ? simulation::CourierRole::OriginalSeasonalHempGrower:physical ? simulation::CourierRole::OriginalFoodGrower:plantations ? simulation::CourierRole::OriginalPlantationGrower:venue_service ? simulation::CourierRole::OriginalVenueMusic:schema==38 ? simulation::CourierRole::OriginalMusicPerformer:simulation::CourierRole::OriginalAuxSupplier));
        if (warehouses && role==static_cast<unsigned>(simulation::CourierRole::OriginalWarehouseFood))
            courier["role"]=static_cast<int>(simulation::CourierRole::MarketFoodDistribution);
        else if (processing && (role==static_cast<unsigned>(simulation::CourierRole::OriginalMealsToShop) || role==static_cast<unsigned>(simulation::CourierRole::OriginalMealsToHouse)))
            courier["role"]=static_cast<int>(simulation::CourierRole::MarketFoodDistribution);
        else if (physical && role==static_cast<unsigned>(simulation::CourierRole::OriginalFoodGrower))
            courier["role"]=static_cast<int>(simulation::CourierRole::Service);
        else if (seasonal && role==static_cast<unsigned>(simulation::CourierRole::OriginalSeasonalHempGrower))
            courier["role"]=static_cast<int>(simulation::CourierRole::Service);
        else if (plantations && role==static_cast<unsigned>(simulation::CourierRole::OriginalPlantationGrower))
            courier["role"]=static_cast<int>(simulation::CourierRole::Service);
        else if (((schema==38 || schema==39) && role==static_cast<unsigned>(simulation::CourierRole::OriginalMusicPerformer)) ||
            (venue_service && role==static_cast<unsigned>(simulation::CourierRole::OriginalVenueMusic)))
            courier["role"]=static_cast<int>(simulation::CourierRole::Service);
        else if (role==static_cast<unsigned>(simulation::CourierRole::OriginalHempToWarehouse) ||
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
    for (std::size_t i=0;i<d.world.buildings.size();++i) {
        auto& building=d.world.buildings[i];const auto& source=field(world,"buildings")[i];
        building.kind=static_cast<simulation::Object>(number(field(source,"kind")));
        if(reconfiguration) building.placed=boolean(field(source,"placed"));
        if(multi && (building.kind==simulation::Object::OriginalWheatField || building.kind==simulation::Object::OriginalMilletField ||
            building.kind==simulation::Object::OriginalRiceField || building.kind==simulation::Object::OriginalCabbageField ||
            building.kind==simulation::Object::OriginalSoybeanField)) {
            building.operating_enabled=boolean(field(source,"operating_enabled"));
            building.workforce_priority=static_cast<simulation::WorkforcePriority>(number(field(source,"workforce_priority"),2));
        }
    }
    for (std::size_t i=0;i<d.world.couriers.size();++i)
        d.world.couriers[i].role=static_cast<simulation::CourierRole>(number(field(field(world,"couriers")[i],"role")));
    d.world.original_economy=parse_original_economy(field(world,"original_economy"));
    d.world.original_influence=parse_original_influence(field(world,"original_influence"));
    d.world.original_goods=parse_production_goods(field(world,"original_goods"));
    if (fire) state.fire_protection=parse_fire_protection(field(production,"fire_protection"),d.world.width,d.world.height);
    d.world.original_production=state;
    if(reconfiguration) d.world.agriculture_reconfiguration=parse_agriculture_reconfiguration(field(world,"agriculture_reconfiguration"));
    if(warehouses) d.world.food_warehouses=parse_food_warehouses(field(world,"food_warehouses"));
    if(processing) {
        d.world.processed_food=parse_agriculture_width<simulation::processed_food_count>(field(world,"processed_food"));
        d.world.food_processing=parse_food_processing(field(world,"food_processing"));
    }
    if(productivity) d.world.irrigation_productivity=parse_irrigation_productivity(field(world,"irrigation_productivity"));
    if(seasonal) d.world.seasonal_agriculture=parse_seasonal_agriculture(field(world,"seasonal_agriculture"),productivity);
    if(multi) d.world.multi_field_agriculture=parse_multi_field_agriculture(field(world,"multi_field_agriculture"));
    if(physical) d.world.physical_food_growers=parse_physical_food_growers(field(world,"physical_food_growers"),seasonal,productivity);
    if (agriculture && !fishing) d.world.agriculture=parse_agriculture(field(world,"agriculture"));
    if(hunting) {
        if(!processing) d.world.hunting_food=parse_agriculture_width<simulation::hunting_food_count>(field(world,"hunting_food"));
        d.world.hunting=parse_hunting(field(world,"hunting"));
    }
    if(fishing) {
        if(!hunting) d.world.fishing_food=parse_fishing_food(field(world,"fishing_food"));
        d.world.fishing=parse_fishing(field(world,"fishing"));
        simulation::World::validate_fishing_snapshot(d.world);
    }
    if(hunting) simulation::World::validate_hunting_snapshot(d.world);
    if (plantations) d.world.plantations=parse_plantations(field(world,"plantations"),seasonal,productivity);
    if (irrigation) d.world.irrigation=parse_irrigation(field(world,"irrigation"));
    if (irrigation) simulation::World::validate_irrigation_snapshot(d.world);
    require(d.world.agriculture_reconfiguration.has_value()==reconfiguration,"Agriculture reconfiguration authority requires exact Rule36/schema52");
    if(reconfiguration) simulation::World::validate_agriculture_reconfiguration_snapshot(d.world);
    require(d.world.food_warehouses.has_value()==warehouses,"Food Warehouse authority requires exact Rule35/schema51");
    if(warehouses) simulation::World::validate_food_warehouses_snapshot(d.world);
    if(processing) simulation::World::validate_food_processing_snapshot(d.world);
    if(productivity) simulation::World::validate_irrigation_productivity_snapshot(d.world);
    if(seasonal) simulation::World::validate_seasonal_agriculture_snapshot(d.world);
    if(multi) simulation::World::validate_multi_field_agriculture_snapshot(d.world);
    if(physical) simulation::World::validate_physical_food_growers_snapshot(d.world);
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
bool land_trade_world(const simulation::WorldSnapshot& world) {
    return world.profile==simulation::RulesProfile::CityV16 &&
        world.rule_version==simulation::kAuthoredLandTradeRuleVersion;
}
json land_trade_json(const simulation::AuthoredLandTradeState& state) {
    using namespace simulation;
    const auto& source=state.model_source;
    require(source.format_version==original::model_schema_version && valid_hash(source.sha256) &&
            source.byte_count>0 && source.byte_count<=original::max_model_bytes &&
            static_cast<std::size_t>(source.difficulty)<original::difficulty_count &&
            valid_hash(state.source_map_sha256),"invalid schema37 source identity");
    require(state.orders.size()<=land_trade_order_limit,"schema37 order collection exceeds bound");
    json binding=nullptr,orders=json::array();
    if (state.binding) {
        const auto& b=*state.binding;
        binding={{"warehouse",static_cast<std::uint32_t>(b.warehouse)},
            {"gateway",static_cast<std::uint32_t>(b.gateway)},
            {"courier",static_cast<std::uint32_t>(b.courier)},{"configured_tick",b.configured_tick}};
    }
    for (const auto& row:state.orders) orders.push_back({{"id",static_cast<std::uint32_t>(row.id)},
        {"direction",static_cast<unsigned>(row.direction)},{"ordered_tick",row.ordered_tick},
        {"boundary_tick",row.boundary_tick ? json(*row.boundary_tick):json(nullptr)},
        {"completed_tick",row.completed_tick ? json(*row.completed_tick):json(nullptr)}});
    return {{"model_source",{{"format_version",source.format_version},{"sha256",source.sha256},
        {"byte_count",source.byte_count},{"difficulty",static_cast<unsigned>(source.difficulty)}}},
        {"source_map_sha256",state.source_map_sha256},{"binding",binding},{"orders",orders},
        {"next_order_id",state.next_order_id}};
}
simulation::AuthoredLandTradeState parse_land_trade(const json& j) {
    using namespace simulation;
    AuthoredLandTradeState state;const auto& source=field(j,"model_source");
    state.model_source={static_cast<std::uint32_t>(number(field(source,"format_version"),UINT32_MAX)),
        str(field(source,"sha256")),number(field(source,"byte_count"),original::max_model_bytes),
        static_cast<original::Difficulty>(number(field(source,"difficulty"),original::difficulty_count-1))};
    state.source_map_sha256=str(field(j,"source_map_sha256"));
    state.next_order_id=static_cast<std::uint32_t>(number(field(j,"next_order_id"),UINT32_MAX));
    const auto& binding=field(j,"binding");
    if (!binding.is_null()) state.binding=LandTradeBinding{
        static_cast<BuildingId>(number(field(binding,"warehouse"),UINT32_MAX)),
        static_cast<BuildingId>(number(field(binding,"gateway"),UINT32_MAX)),
        static_cast<CourierId>(number(field(binding,"courier"),UINT32_MAX)),number(field(binding,"configured_tick"))};
    const auto& rows=field(j,"orders");
    require(rows.is_array() && rows.size()<=land_trade_order_limit,"schema37 order collection exceeds bound");
    for (const auto& value:rows) {
        LandTradeOrder row;
        row.id=static_cast<LandTradeOrderId>(number(field(value,"id"),UINT32_MAX));
        row.direction=static_cast<LandTradeDirection>(number(field(value,"direction"),1));
        row.ordered_tick=number(field(value,"ordered_tick"));
        const auto& boundary=field(value,"boundary_tick");if (!boundary.is_null()) row.boundary_tick=number(boundary);
        const auto& completed=field(value,"completed_tick");if (!completed.is_null()) row.completed_tick=number(completed);
        state.orders.push_back(row);
    }
    require(canonical_fields(j,land_trade_json(state)),"schema37 contains unknown or noncanonical trade fields");
    return state;
}
json land_trade_document_json(const SaveDocument& d) {
    using namespace simulation;
    require(land_trade_world(d.world),"schema37 requires exact City-v16 Rule21");
    require(d.source_schema_version==0 || d.source_schema_version==37,
            "Rule21 requires schema37; historical saves are not migrated");
    require(d.map_permissions_policy_version==kBridgesMapPermissionsPolicyVersion && valid_hash(d.map_permissions_sha256),
            "schema37 requires complete immutable policy3 map binding");
    require(!d.world.original_economy && !d.world.original_influence && !d.world.original_goods && !d.world.original_production &&
            d.world.source_cell_ramps.empty() && d.world.next_source_cell_ramp_id==1,
            "schema37 cannot carry Original-economy or Rule20 authority");
    require(d.world.land_trade && d.world.land_trade->source_map_sha256==d.map_sha256,
            "schema37 requires its complete source-bound trade state");
    World::validate_authored_land_trade_snapshot(d.world);
    auto projected=d;projected.source_schema_version=22;projected.world.rule_version=6;
    projected.world.land_trade.reset();
    auto result=document_json(projected);
    result["schema_version"]=37;result["rules"]["version"]=kAuthoredLandTradeRuleVersion;
    result["rules"]["authored_land_trade"]=authored_land_trade_contract;
    result["world"]["land_trade"]=land_trade_json(*d.world.land_trade);
    return result;
}
SaveDocument parse_land_trade_document(const json& j) {
    using namespace simulation;
    const auto& rules=field(j,"rules");
    require(number(field(j,"schema_version"))==37 && str(field(j,"format"))=="openemperor-sandbox-save" &&
        str(field(rules,"id"))==city_v16_profile_name && number(field(rules,"version"))==kAuthoredLandTradeRuleVersion &&
        str(field(rules,"authored_land_trade"))==authored_land_trade_contract,
        "schema37 requires its exact Rule21 authored land trade marker");
    const auto state=parse_land_trade(field(field(j,"world"),"land_trade"));
    auto projected=j;projected["schema_version"]=22;projected["rules"]["version"]=6;
    projected["rules"].erase("authored_land_trade");projected["world"].erase("land_trade");
    // Syntax-only projection: the historical parser never admits role26. The
    // actual role and complete classic state are validated only by Rule21.
    for (auto& courier:projected["world"]["couriers"])
        if (number(field(courier,"role"))==static_cast<unsigned>(CourierRole::AuthoredLandTrader))
            courier["role"]=static_cast<unsigned>(CourierRole::MarketPotteryInbound);
    auto d=parse_document(projected);d.source_schema_version=37;d.world.rule_version=kAuthoredLandTradeRuleVersion;
    d.world.land_trade=state;
    const auto& original_couriers=field(field(j,"world"),"couriers");
    for (std::size_t i=0;i<d.world.couriers.size();++i)
        d.world.couriers[i].role=static_cast<CourierRole>(number(field(original_couriers[i],"role"),
            static_cast<unsigned>(CourierRole::AuthoredLandTrader)));
    require(canonical_fields(j,land_trade_document_json(d)),"schema37 contains unknown or noncanonical fields");
    return d;
}
json source_cell_document_json(const SaveDocument& d) {
    using namespace simulation;
    require(source_cell_ramps_enabled(d.world.profile,d.world.rule_version),
            "schema36 requires exact City-v16 rule20");
    require(d.source_schema_version==0 || d.source_schema_version==36,
            "rule20 requires schema36; historical saves are not migrated");
    require(d.map_permissions_policy_version==kBridgesMapPermissionsPolicyVersion && valid_hash(d.map_permissions_sha256),
            "schema36 requires complete immutable policy3 map binding");
    require(!d.world.original_economy && !d.world.original_influence && !d.world.original_goods && !d.world.original_production,
            "schema36 cannot carry Original-economy authority");
    require(d.world.source_cell_ramps.size()<=source_cell_ramp_max_links &&
            d.world.ramps.size()<=source_cell_ramp_max_links-d.world.source_cell_ramps.size(),
            "schema36 combined ramp collection exceeds64");
    auto projected=d;
    projected.source_schema_version=22;projected.world.rule_version=6;
    projected.world.source_cell_ramps.clear();projected.world.next_source_cell_ramp_id=1;
    auto result=document_json(projected);
    result["schema_version"]=36;result["rules"]["version"]=d.world.rule_version;
    result["rules"]["source_cell_ramps"]=authored_source_cell_ramp_contract;
    result["world"]["next_source_cell_ramp_id"]=d.world.next_source_cell_ramp_id;
    result["world"]["source_cell_ramps"]=json::array();
    for (const auto& r:d.world.source_cell_ramps)
        result["world"]["source_cell_ramps"].push_back({{"id",r.id.value},{"source_cell",cell_json(r.source_cell)},
            {"high_neighbor",cell_json(r.high_neighbor)},{"low_height",int(r.low_height)},
            {"high_height",int(r.high_height)},{"paid_cost",r.paid_cost},{"placed_tick",r.placed_tick}});
    return result;
}
SaveDocument parse_source_cell_document(const json& j) {
    using namespace simulation;
    require(number(field(j,"schema_version"))==36 && str(field(j,"format"))=="openemperor-sandbox-save",
            "invalid authored source-cell ramp save identity");
    const auto& rules=field(j,"rules");
    require(str(field(rules,"id"))==city_v16_profile_name && number(field(rules,"version"))==kAuthoredSourceCellRampRuleVersion &&
            str(field(rules,"source_cell_ramps"))==authored_source_cell_ramp_contract,
            "schema36 requires its exact Rule20 authored source-cell ramp marker");
    auto projected=j;projected["schema_version"]=22;projected["rules"]["version"]=6;
    projected["rules"].erase("source_cell_ramps");
    projected["world"].erase("source_cell_ramps");projected["world"].erase("next_source_cell_ramp_id");
    auto d=parse_document(projected);
    d.source_schema_version=36;d.world.rule_version=kAuthoredSourceCellRampRuleVersion;
    const auto& world=field(j,"world");
    d.world.next_source_cell_ramp_id=static_cast<std::uint32_t>(number(field(world,"next_source_cell_ramp_id"),UINT32_MAX));
    const auto& rows=field(world,"source_cell_ramps");
    require(rows.is_array() && rows.size()<=source_cell_ramp_max_links &&
            d.world.ramps.size()<=source_cell_ramp_max_links-rows.size(),"schema36 combined ramp collection exceeds64");
    std::uint32_t previous=0;
    for (const auto& row:rows) {
        require(row.is_object() && row.size()==7,"source-cell ramp requires exactly seven canonical fields");
        SourceCellRamp r;r.id={static_cast<std::uint32_t>(number(field(row,"id"),UINT32_MAX))};
        require(r.id.value>previous,"source-cell ramp IDs must be positive, unique and sorted");previous=r.id.value;
        r.source_cell=cell(field(row,"source_cell"));r.high_neighbor=cell(field(row,"high_neighbor"));
        const auto low=signed_number(field(row,"low_height")),high=signed_number(field(row,"high_height"));
        require(low>=-128 && low<=127 && high>=-128 && high<=127,"source-cell ramp height exceeds signed byte");
        require(high==low+1,"source-cell ramp requires an exact positive one-level rise");
        r.low_height=static_cast<std::int8_t>(low);r.high_height=static_cast<std::int8_t>(high);
        r.paid_cost=signed_number(field(row,"paid_cost"));r.placed_tick=number(field(row,"placed_tick"));
        require(r.paid_cost==source_cell_ramp_cost && r.placed_tick<=d.world.ticks,
                "source-cell ramp cost or placement tick invalid");
        d.world.source_cell_ramps.push_back(r);
    }
    require(canonical_fields(j,source_cell_document_json(d)),"schema36 contains unknown or noncanonical fields");
    return d;
}
json document_json(const SaveDocument& d) {
    require(!d.world.agriculture_reconfiguration || simulation::agriculture_reconfiguration_enabled(d.world.profile,d.world.rule_version),
        "Agriculture reconfiguration authority requires exact Rule36/schema52");
    require(!d.world.food_warehouses || simulation::food_warehouses_enabled(d.world.profile,d.world.rule_version),
        "Food Warehouse authority requires exact Rule35/schema51");
    require((!d.world.processed_food && !d.world.food_processing) || simulation::food_processing_enabled(d.world.profile,d.world.rule_version),
        "Food processing authority requires exact Rule34/schema50");
    require(!d.world.irrigation_productivity || simulation::irrigation_productivity_enabled(d.world.profile,d.world.rule_version),
        "Irrigation productivity authority requires exact Rule33/schema49");
    require(!d.world.seasonal_agriculture || simulation::seasonal_agriculture_enabled(d.world.profile,d.world.rule_version),
        "Seasonal Agriculture authority requires exact Rule32/schema48");
    require(!d.world.physical_food_growers || simulation::physical_food_growers_enabled(d.world.profile,d.world.rule_version),
        "Physical Grower authority requires exact Rule31/schema47");
    require(!d.world.multi_field_agriculture || simulation::multi_field_agriculture_enabled(d.world.profile,d.world.rule_version),
        "Multi-field authority requires exact Rule30/schema46");
    require((!d.world.hunting_food && !d.world.hunting) || simulation::playable_hunting_enabled(d.world.profile,d.world.rule_version),
        "Hunting authority requires exact Rule29/schema45");
    require((!d.world.fishing_food && !d.world.fishing) ||
        simulation::playable_fishing_enabled(d.world.profile,d.world.rule_version),
        "Fishing authority requires exact Rule28/schema44");
    require(!d.world.irrigation || simulation::original_irrigation_enabled(d.world.profile,d.world.rule_version),
            "Irrigation authority requires exact Rule26/schema42");
    require(!d.world.agriculture || simulation::original_agriculture_family_enabled(d.world.profile,d.world.rule_version),
            "Agriculture authority requires its exact versioned generation");
    require(!d.world.plantations || simulation::original_plantations_enabled(d.world.profile,d.world.rule_version),
            "Plantation authority requires exact Rule25/schema41");
    if (land_trade_world(d.world)) return land_trade_document_json(d);
    require(!d.world.land_trade,"land trade authority requires exact Rule21/schema37");
    if (simulation::source_cell_ramps_enabled(d.world.profile,d.world.rule_version)) return source_cell_document_json(d);
    require(d.world.source_cell_ramps.empty() && d.world.next_source_cell_ramp_id==1,
            "source-cell ramp authority requires exact Rule20/schema36");
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
    require(schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("agriculture_reconfiguration") && !field(j,"rules").contains("agriculture_reconfiguration_revision") &&
             !field(j,"world").contains("agriculture_reconfiguration") && !j.contains("agriculture_reconfiguration") && !j.contains("agriculture_reconfiguration_revision")),
            "Agriculture reconfiguration marker and authority require exact schema52");
    require(schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("food_warehouses") && !field(j,"rules").contains("food_warehouses_revision") &&
             !field(j,"world").contains("food_warehouses") && !j.contains("food_warehouses") && !j.contains("food_warehouses_revision")),
            "Food Warehouse marker and authority require exact schema51");
    require(schema==50 || schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("food_processing") && !field(j,"rules").contains("food_processing_revision") &&
             !field(j,"world").contains("processed_food") && !field(j,"world").contains("food_processing") &&
             !j.contains("processed_food") && !j.contains("food_processing") && !j.contains("food_processing_revision")),
            "Food processing marker and authority require exact schema50");
    require(schema==49 || schema==50 || schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("irrigation_productivity") &&
             !field(j,"rules").contains("irrigation_productivity_revision") &&
             !field(j,"world").contains("irrigation_productivity") &&
             !j.contains("irrigation_productivity") && !j.contains("irrigation_productivity_revision")),
            "Irrigation productivity marker and authority require exact schema49");
    require(schema==48 || schema==49 || schema==50 || schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("seasonal_agriculture") &&
             !field(j,"rules").contains("seasonal_agriculture_revision") &&
             !field(j,"world").contains("seasonal_agriculture") &&
             !j.contains("seasonal_agriculture") && !j.contains("seasonal_agriculture_revision")),
            "Seasonal Agriculture marker and authority require exact schema48");
    require(schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("physical_food_growers") &&
             !field(j,"rules").contains("physical_food_growers_revision") &&
             !field(j,"world").contains("physical_food_growers") &&
             !j.contains("physical_food_growers") && !j.contains("physical_food_growers_revision")),
            "Physical Grower marker and authority require exact schema47");
    require(schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52 || syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("multi_field_agriculture") &&
             !field(j,"rules").contains("multi_field_agriculture_revision") &&
             !field(j,"world").contains("multi_field_agriculture") &&
             !j.contains("multi_field_agriculture") && !j.contains("multi_field_agriculture_revision")),
            "Multi-field marker and authority require exact schema46");
    if (schema==38 || schema==39 || schema==40 || schema==41 || schema==42 || schema==43 || schema==44 || schema==45 || schema==46 || schema==47 || schema==48 || schema==49 || schema==50 || schema==51 || schema==52) {
        require(syntax==SnapshotSyntax::Frozen,"schema38/39/40/41 cannot select projected syntax");
        return parse_production_document(j);
    }
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("playable_hunting") && !field(j,"rules").contains("playable_hunting_revision") &&
             !field(j,"world").contains("hunting_food") && !field(j,"world").contains("hunting") &&
             !j.contains("hunting_food") && !j.contains("hunting") && !j.contains("playable_hunting")),
            "Hunting marker and authority require exact schema45");
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("playable_fishing") && !field(j,"rules").contains("playable_fishing_revision") &&
             !field(j,"world").contains("fishing_food") && !field(j,"world").contains("fishing") &&
             !j.contains("fishing_food") && !j.contains("fishing") && !j.contains("playable_fishing")),
            "Fishing marker and authority require exact schema44");
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("playable_agriculture") &&
             !field(j,"rules").contains("playable_agriculture_revision") &&
             !field(j,"world").contains("playable_agriculture") && !j.contains("playable_agriculture")),
            "Playable Agriculture marker requires exact schema43");
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("irrigation") && !field(j,"world").contains("irrigation") && !j.contains("irrigation")),
            "Irrigation marker and authority require exact schema42");
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("plantations") && !field(j,"world").contains("plantations") && !j.contains("plantations")),
            "Plantation marker and authority require exact schema41");
    require(syntax==SnapshotSyntax::OriginalProduction ||
            (!field(j,"rules").contains("agriculture") && !field(j,"world").contains("agriculture") && !j.contains("agriculture")),
            "Agriculture marker and authority require exact schema40");
    require(syntax==SnapshotSyntax::OriginalProduction || !field(j,"rules").contains("venue_house_service"),
            "venue House-service marker requires exact schema39");
    require(syntax==SnapshotSyntax::OriginalProduction || !field(j,"rules").contains("music_venues"),
            "music-venue marker requires exact schema38");
    if (schema==37) {
        require(syntax==SnapshotSyntax::Frozen,"schema37 cannot select Original-production syntax");
        return parse_land_trade_document(j);
    }
    require(!field(j,"rules").contains("authored_land_trade") && !field(j,"world").contains("land_trade"),
            "authored land trade marker and authority require exact schema37");
    if (schema==36) {
        require(syntax==SnapshotSyntax::Frozen,"schema36 cannot select Original-production syntax");
        return parse_source_cell_document(j);
    }
    require(!field(j,"rules").contains("source_cell_ramps"),"source-cell ramp marker requires exact schema36");
    require(syntax==SnapshotSyntax::Frozen || schema==22,"active syntax is restricted to the local schema22 projection");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==31 ||
            !field(j,"rules").contains("inspector_coordination"),
            "Inspector coordination marker requires exact schema31");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==32 || schema==33 || schema==34 || schema==35 ||
            !field(j,"rules").contains("deadline_fire_dispatch"),
            "Deadline fire-dispatch marker requires exact schema32,33 or34");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==33 || schema==34 || schema==35 ||
            !field(j,"rules").contains("deadline_service_dispatch"),
            "Deadline service-dispatch marker requires exact schema33 or34");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==34 || schema==35 ||
            !field(j,"rules").contains("deadline_food_dispatch"),
            "Deadline Food-dispatch marker requires exact schema34");
    require(syntax==SnapshotSyntax::OriginalProduction || schema==35 ||
            !field(j,"rules").contains("safe_demolition"),"Safe-demolition marker requires exact schema35");
    if (schema==26 || schema==27 || schema==28 || schema==29 || schema==30 || schema==31 || schema==32 || schema==33 || schema==34 || schema==35) return parse_production_document(j);
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
    require(!world.has_grower_navigation_session(),"Detach transient grower navigation before saving");
    safe_relative(relative);
    const auto map=maps::resolve_map_path(root,relative);
    SaveDocument d;
    d.map_relative=relative;d.map_sha256=hash_file(map);
    d.buildable_sha256=digest(mask.data(),mask.size());d.world=world.snapshot();
    if (land_trade_world(d.world)) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        d.prepared_land_trade_boundary_facts=prepare_land_trade_boundary_facts(root,relative);
        require(d.prepared_permissions && d.prepared_original_rules_model,
                "schema37 requires prepared policy3 permissions and model identity");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        (void)restore_authored_land_trade(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
            d.prepared_land_trade_boundary_facts);
        return d;
    }
    if (simulation::original_production_enabled(d.world.profile,d.world.rule_version)) {
        d.prepared_permissions=world.map_permissions();
        d.prepared_original_rules_model=world.original_rules_model();
        require(static_cast<bool>(d.prepared_original_rules_model),"schema26 requires a prepared original rules model");
        require(d.prepared_permissions && d.prepared_permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion,
                "schema26 requires complete immutable policy3 map permissions");
        d.map_permissions_policy_version=d.prepared_permissions->policy_version();
        d.map_permissions_sha256=map_permissions_fingerprint(*d.prepared_permissions,d.map_sha256);
        if (simulation::original_agriculture_family_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_music_venues_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_safe_demolition_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version) ||
            simulation::original_cardinal_water_enabled(d.world.profile,d.world.rule_version)) {
            d.prepared_original_terrain_facts=world.original_terrain_facts();
            require(d.prepared_original_terrain_facts && d.prepared_original_terrain_facts->source_map_sha256()==d.map_sha256,
                simulation::original_plantations_enabled(d.world.profile,d.world.rule_version) ?
                    "schema41 World terrain context differs from complete source-map SHA-256":
                simulation::original_agriculture_enabled(d.world.profile,d.world.rule_version) ?
                    "schema40 World terrain context differs from complete source-map SHA-256":
                simulation::original_venue_house_service_enabled(d.world.profile,d.world.rule_version) ?
                    "schema39 World terrain context differs from complete source-map SHA-256":
                simulation::original_music_venues_enabled(d.world.profile,d.world.rule_version) ?
                    "schema38 World terrain context differs from complete source-map SHA-256":
                simulation::original_safe_demolition_enabled(d.world.profile,d.world.rule_version) ?
                    "schema35 World terrain context differs from complete source-map SHA-256":
                simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema34 World terrain context differs from complete source-map SHA-256":
                simulation::original_deadline_service_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema33 World terrain context differs from complete source-map SHA-256":
                simulation::original_deadline_fire_dispatch_enabled(d.world.profile,d.world.rule_version) ?
                    "schema32 World terrain context differs from complete source-map SHA-256":
                simulation::original_inspector_coordination_enabled(d.world.profile,d.world.rule_version) ?
                    "schema31 World terrain context differs from complete source-map SHA-256":
                    "schema30 World terrain context differs from complete source-map SHA-256");
            if (simulation::agriculture_reconfiguration_enabled(d.world.profile,d.world.rule_version))
                (void)restore_agriculture_reconfiguration(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::food_warehouses_enabled(d.world.profile,d.world.rule_version))
                (void)restore_food_warehouses(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::food_processing_enabled(d.world.profile,d.world.rule_version))
                (void)restore_food_processing(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::irrigation_productivity_enabled(d.world.profile,d.world.rule_version))
                (void)restore_irrigation_productivity(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::seasonal_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_seasonal_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::physical_food_growers_enabled(d.world.profile,d.world.rule_version))
                (void)restore_physical_food_growers(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::multi_field_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_multi_field_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::playable_hunting_enabled(d.world.profile,d.world.rule_version))
                (void)restore_playable_hunting(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::playable_fishing_enabled(d.world.profile,d.world.rule_version))
                (void)restore_playable_fishing(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::playable_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_playable_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_irrigation_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_irrigation(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_plantations_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_plantations(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_venue_house_service_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_venue_house_service(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_music_venues_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_music_venues(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_safe_demolition_enabled(d.world.profile,d.world.rule_version))
                (void)restore_original_safe_demolition(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                    d.prepared_original_terrain_facts);
            else if (simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version))
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
        ((d.world.rule_version>=3 && d.world.rule_version<=6) ||
         simulation::source_cell_ramps_enabled(d.world.profile,d.world.rule_version))) {
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
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==36)
        require(!duplicate_key,"schema36 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && (j.at("schema_version")==38 || j.at("schema_version")==39 || j.at("schema_version")==40 || j.at("schema_version")==41 || j.at("schema_version")==42 || j.at("schema_version")==43))
        require(!duplicate_key,"schema38/39/40/41 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==47)
        require(!duplicate_key,"schema47 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==48)
        require(!duplicate_key,"schema48 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==52)
        require(!duplicate_key,"schema52 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==51)
        require(!duplicate_key,"schema51 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==50)
        require(!duplicate_key,"schema50 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==49)
        require(!duplicate_key,"schema49 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==46)
        require(!duplicate_key,"schema46 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==45)
        require(!duplicate_key,"schema45 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==44)
        require(!duplicate_key,"schema44 duplicate save key");
    if (j.is_object() && j.contains("schema_version") && j.at("schema_version")==37)
        require(!duplicate_key,"schema37 duplicate save key");
    if (j.is_object() && j.contains("schema_version") &&
        (j.at("schema_version")==26 || j.at("schema_version")==27 || j.at("schema_version")==28 || j.at("schema_version")==29 || j.at("schema_version")==30 || j.at("schema_version")==31 || j.at("schema_version")==32 || j.at("schema_version")==33 || j.at("schema_version")==34 || j.at("schema_version")==35))
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
std::shared_ptr<const original::OriginalRulesModel> prepare_original_culture_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid original culture model difficulty");
    auto loaded=original::load_model_data(root);
    if (!loaded) throw std::runtime_error("original culture model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_culture(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_original_agriculture_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid Agriculture model difficulty");
    auto loaded=original::load_model_data(root);
    if (!loaded) throw std::runtime_error("Agriculture model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_agriculture(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_original_irrigation_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid Irrigation model difficulty");
    auto loaded=original::load_model_data(root);
    if (!loaded) throw std::runtime_error("Irrigation model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_irrigation(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_playable_fishing_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid Fishing model difficulty");
    auto loaded=original::load_model_data(root);
    if(!loaded) throw std::runtime_error("Fishing model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_fishing(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_playable_hunting_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid Hunting model difficulty");
    auto loaded=original::load_model_data(root);
    if(!loaded) throw std::runtime_error("Hunting model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_hunting(std::move(*loaded.model),difficulty);
}
std::shared_ptr<const original::OriginalRulesModel> prepare_food_processing_rules_model(
        const fs::path& root,original::Difficulty difficulty) {
    require(static_cast<std::size_t>(difficulty)<original::difficulty_count,"invalid Food processing model difficulty");
    auto loaded=original::load_model_data(root);
    if(!loaded) throw std::runtime_error("Food processing model preparation failed: "+
        (loaded.error ? loaded.error->reason:std::string("model source unavailable")));
    return original::OriginalRulesModel::prepare_food_processing(std::move(*loaded.model),difficulty);
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
    require(!land_trade_world(d.world),"Rule21 requires explicit schema37 restore with prepared model and boundary facts");
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
        ((d.world.rule_version>=3 && d.world.rule_version<=6) ||
         simulation::source_cell_ramps_enabled(d.world.profile,d.world.rule_version))) {
        const auto expected_schema=simulation::source_cell_ramps_enabled(d.world.profile,d.world.rule_version) ? 36U:
            d.world.rule_version==3 ? 19U:d.world.rule_version==4 ? 20U:d.world.rule_version==5 ? 21U:22U;
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
simulation::World restore_authored_source_cell_ramps(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions) {
    require(simulation::source_cell_ramps_enabled(d.world.profile,d.world.rule_version),
            "source-cell ramp restore requires exact City-v16 Rule20");
    require(d.source_schema_version==0 || d.source_schema_version==36,
            "source-cell ramp restore requires exact schema36; no historical migration");
    (void)source_cell_document_json(d);
    return restore_save(d,root,std::move(mask),std::move(permissions));
}
std::shared_ptr<const simulation::LandTradeBoundaryFacts> prepare_land_trade_boundary_facts(
        const fs::path& root,const fs::path& relative) {
    safe_relative(relative);
    const auto path=maps::resolve_map_path(root,relative);
    const auto container=maps::EmperorContainer::open(path);
    const auto source_hash=maps::map_input_sha256(container);
    require(!container.multipart() && container.parts().size()==1,
            "land trade boundary requires one complete standalone source map");
    const auto map=maps::read_emperor_map(container,0);
    const maps::MapGeometry geometry(map.declared_map_size);
    require(geometry.supported && map.stored_width==maps::stored_grid_width && map.stored_height==maps::stored_grid_height &&
            geometry.candidate.size()==static_cast<std::size_t>(map.stored_width)*map.stored_height,
            "land trade boundary requires complete supported shared map geometry");
    require(hash_file(path)==source_hash,"source map changed while preparing land trade boundary facts");
    return std::make_shared<const simulation::LandTradeBoundaryFacts>(static_cast<int>(map.stored_width),
        static_cast<int>(map.stored_height),geometry.candidate,source_hash);
}
simulation::World restore_authored_land_trade(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::LandTradeBoundaryFacts> boundary) {
    (void)land_trade_document_json(d);
    const auto& source=d.world.land_trade->model_source;
    if (!model) model=prepare_original_rules_model(root,source.difficulty);
    const auto& provenance=model->provenance();
    require(source.format_version==provenance.format_version && source.sha256==provenance.sha256 &&
        source.byte_count==provenance.byte_count && source.difficulty==model->difficulty(),
        "schema37 prepared model identity differs from save");
    const auto inspected=original::inspect_model_source(root);
    require(inspected.fingerprint && inspected.fingerprint->sha256==source.sha256 &&
            inspected.fingerprint->byte_count==source.byte_count,"schema37 live model identity differs from save");
    safe_relative(d.map_relative);const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"source map SHA-256 differs from schema37 save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,"sandbox buildable mask SHA-256 differs from schema37 save");
    if (!permissions) permissions=maps::read_sandbox_map_permissions(root,d.map_relative,mask,
        simulation::kBridgesMapPermissionsPolicyVersion);
    require(permissions && permissions->policy_version()==simulation::kBridgesMapPermissionsPolicyVersion &&
        d.map_permissions_policy_version==simulation::kBridgesMapPermissionsPolicyVersion &&
        map_permissions_fingerprint(*permissions,d.map_sha256)==d.map_permissions_sha256,
        "schema37 complete policy3 permission/source fingerprint differs from save");
    // Source geometry is prepared again from the shared reader. A caller's
    // claimed hash or independently authored bitset cannot grant a gateway.
    auto live=prepare_land_trade_boundary_facts(root,d.map_relative);
    require(live->source_map_sha256()==d.map_sha256,"schema37 live boundary source differs from save");
    if (boundary) {
        require(boundary->source_map_sha256()==live->source_map_sha256() && boundary->width()==live->width() &&
                boundary->height()==live->height(),"schema37 prepared boundary identity differs from live source");
        for (int y=0;y<live->height();++y) for (int x=0;x<live->width();++x)
            require(boundary->contains({x,y})==live->contains({x,y}),
                    "schema37 prepared boundary geometry differs from live source");
    }
    return simulation::World::restore_authored_land_trade(d.world,std::move(permissions),std::move(model),std::move(live));
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
    const bool reconfiguration=rule_version==simulation::kAgricultureReconfigurationRuleVersion;
    const bool warehouses=rule_version==simulation::kFoodWarehousesRuleVersion || reconfiguration;
    const bool processing=rule_version==simulation::kFoodProcessingRuleVersion || warehouses;
    const bool productivity=rule_version==simulation::kIrrigationProductivityRuleVersion || processing;
    const bool seasonal=rule_version==simulation::kSeasonalAgricultureRuleVersion || productivity;
    const bool physical=rule_version==simulation::kPhysicalFoodGrowersRuleVersion || seasonal;
    const bool multi=rule_version==simulation::kMultiFieldAgricultureRuleVersion || physical;
    const bool hunting=rule_version==simulation::kPlayableHuntingRuleVersion || multi;
    const bool fishing=rule_version==simulation::kPlayableFishingRuleVersion || hunting;
    const bool playable=rule_version==simulation::kPlayableAgricultureRuleVersion || fishing;
    const bool irrigation=rule_version==simulation::kOriginalIrrigationRuleVersion || playable;
    const bool plantations=rule_version==simulation::kOriginalPlantationsRuleVersion || irrigation;
    const bool agriculture=rule_version==simulation::kOriginalAgricultureRuleVersion || plantations;
    const bool venue_service=rule_version==simulation::kOriginalVenueHouseServiceRuleVersion;
    const bool culture=rule_version==simulation::kOriginalMusicVenuesRuleVersion || venue_service;
    const bool safe=rule_version==simulation::kOriginalSafeDemolitionRuleVersion;
    const bool food=rule_version==simulation::kOriginalDeadlineFoodDispatchRuleVersion;
    const bool service=rule_version==simulation::kOriginalDeadlineServiceDispatchRuleVersion;
    const bool deadline=rule_version==simulation::kOriginalDeadlineFireDispatchRuleVersion;
    const bool coordination=rule_version==simulation::kOriginalInspectorCoordinationRuleVersion;
    const bool cardinal=rule_version==simulation::kOriginalCardinalWaterRuleVersion;
    const bool logistics=rule_version==simulation::kOriginalMarketLogisticsRuleVersion;
    const bool sustainable=rule_version==simulation::kOriginalSustainableRuleVersion;
    const auto schema=reconfiguration ? 52U:warehouses ? 51U:processing ? 50U:productivity ? 49U:seasonal ? 48U:physical ? 47U:multi ? 46U:hunting ? 45U:fishing ? 44U:playable ? 43U:irrigation ? 42U:plantations ? 41U:agriculture ? 40U:venue_service ? 39U:culture ? 38U:safe ? 35U:food ? 34U:service ? 33U:deadline ? 32U:coordination ? 31U:cardinal ? 30U:logistics ? 29U:sustainable ? 28U:27U;
    require(d.world.profile==simulation::RulesProfile::CityV16 && d.world.rule_version==rule_version,
            "original fire restore requires its exact explicit City-v16 rule generation");
    require(d.source_schema_version==0 || d.source_schema_version==schema,
            "original fire restore requires its exact schema; existing saves are not migrated");
    require(d.world.original_economy && d.world.original_economy->model_source,
            "schema27 requires an original model source identity");
    (void)production_document_json(d);
    const auto& source=*d.world.original_economy->model_source;
    if (!model) model=processing ? prepare_food_processing_rules_model(root,source.difficulty):hunting ? prepare_playable_hunting_rules_model(root,source.difficulty):fishing ? prepare_playable_fishing_rules_model(root,source.difficulty):
        irrigation ? prepare_original_irrigation_rules_model(root,source.difficulty):
        agriculture ? prepare_original_agriculture_rules_model(root,source.difficulty):
        culture ? prepare_original_culture_rules_model(root,source.difficulty):
        prepare_original_production_rules_model(root,source.difficulty);
    require(!culture || model->supports_building(original::ModelBuilding::TheatrePavilion),
            venue_service ? "schema39 requires explicitly prepared Theatre Pavilion model capability":
                "schema38 requires explicitly prepared Theatre Pavilion model capability");
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
    if (cardinal || coordination || deadline || service || food || safe || culture || agriculture) {
        // Re-read source facts even when a session dependency was provided.
        // Claimed source hashes or JSON-derived bitsets never become authority.
        auto live=hunting ? prepare_playable_hunting_terrain_facts(root,d.map_relative):fishing ? prepare_playable_fishing_terrain_facts(root,d.map_relative):
            irrigation ? prepare_original_irrigation_terrain_facts(root,d.map_relative):
            agriculture ? prepare_original_agriculture_terrain_facts(root,d.map_relative):
            prepare_original_terrain_facts(root,d.map_relative);
        require(live->source_map_sha256()==d.map_sha256,
                plantations ? "schema41 live terrain source SHA-256 differs from save":
                agriculture ? "schema40 live terrain source SHA-256 differs from save":
                venue_service ? "schema39 live terrain source SHA-256 differs from save":
                culture ? "schema38 live terrain source SHA-256 differs from save":
                safe ? "schema35 live terrain source SHA-256 differs from save":
                food ? "schema34 live terrain source SHA-256 differs from save":
                service ? "schema33 live terrain source SHA-256 differs from save":
                deadline ? "schema32 live terrain source SHA-256 differs from save":
                coordination ? "schema31 live terrain source SHA-256 differs from save":
                    "schema30 live terrain source SHA-256 differs from save");
        if (terrain) require(*terrain==*live,plantations ? "schema41 prepared terrain facts differ from live original source":agriculture ? "schema40 prepared terrain facts differ from live original source":venue_service ? "schema39 prepared terrain facts differ from live original source":
            culture ? "schema38 prepared terrain facts differ from live original source":
            safe ? "schema35 prepared terrain facts differ from live original source":
            food ? "schema34 prepared terrain facts differ from live original source":
            service ? "schema33 prepared terrain facts differ from live original source":
            deadline ? "schema32 prepared terrain facts differ from live original source":
            coordination ? "schema31 prepared terrain facts differ from live original source":
            "schema30 prepared terrain facts differ from live original source");
        if (d.prepared_original_terrain_facts)
            require(*d.prepared_original_terrain_facts==*live,plantations ? "schema41 document terrain facts differ from live original source":agriculture ? "schema40 document terrain facts differ from live original source":venue_service ? "schema39 document terrain facts differ from live original source":
                culture ? "schema38 document terrain facts differ from live original source":
                safe ? "schema35 document terrain facts differ from live original source":
                food ? "schema34 document terrain facts differ from live original source":
                service ? "schema33 document terrain facts differ from live original source":
                deadline ? "schema32 document terrain facts differ from live original source":
                coordination ? "schema31 document terrain facts differ from live original source":
                "schema30 document terrain facts differ from live original source");
        if (reconfiguration)
            return simulation::World::restore_agriculture_reconfiguration(d.world,std::move(permissions),std::move(model),std::move(live));
        if (warehouses)
            return simulation::World::restore_food_warehouses(d.world,std::move(permissions),std::move(model),std::move(live));
        if (processing)
            return simulation::World::restore_food_processing(d.world,std::move(permissions),std::move(model),std::move(live));
        if (productivity)
            return simulation::World::restore_irrigation_productivity(d.world,std::move(permissions),std::move(model),std::move(live));
        if (seasonal)
            return simulation::World::restore_seasonal_agriculture(d.world,std::move(permissions),std::move(model),std::move(live));
        if (physical)
            return simulation::World::restore_physical_food_growers(d.world,std::move(permissions),std::move(model),std::move(live));
        if (multi)
            return simulation::World::restore_multi_field_agriculture(d.world,std::move(permissions),std::move(model),std::move(live));
        if (hunting)
            return simulation::World::restore_playable_hunting(d.world,std::move(permissions),std::move(model),std::move(live));
        if (fishing)
            return simulation::World::restore_playable_fishing(d.world,std::move(permissions),std::move(model),std::move(live));
        if (playable)
            return simulation::World::restore_playable_agriculture(d.world,std::move(permissions),std::move(model),std::move(live));
        if (irrigation)
            return simulation::World::restore_original_irrigation(d.world,std::move(permissions),std::move(model),std::move(live));
        if (plantations)
            return simulation::World::restore_original_plantations(d.world,std::move(permissions),std::move(model),std::move(live));
        if (agriculture)
            return simulation::World::restore_original_agriculture(d.world,std::move(permissions),std::move(model),std::move(live));
        if (venue_service)
            return simulation::World::restore_original_venue_house_service(d.world,std::move(permissions),std::move(model),std::move(live));
        if (culture)
            return simulation::World::restore_original_music_venues(d.world,std::move(permissions),std::move(model),std::move(live));
        if (safe)
            return simulation::World::restore_original_safe_demolition(d.world,std::move(permissions),std::move(model),std::move(live));
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
simulation::World restore_original_safe_demolition(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalSafeDemolitionRuleVersion,std::move(terrain));
}
simulation::World restore_original_music_venues(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalMusicVenuesRuleVersion,std::move(terrain));
}
simulation::World restore_original_venue_house_service(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalVenueHouseServiceRuleVersion,std::move(terrain));
}
simulation::World restore_original_agriculture(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalAgricultureRuleVersion,std::move(terrain));
}
simulation::World restore_original_plantations(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalPlantationsRuleVersion,std::move(terrain));
}
simulation::World restore_original_irrigation(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kOriginalIrrigationRuleVersion,std::move(terrain));
}
simulation::World restore_playable_agriculture(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kPlayableAgricultureRuleVersion,std::move(terrain));
}
simulation::World restore_playable_fishing(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kPlayableFishingRuleVersion,std::move(terrain));
}
simulation::World restore_playable_hunting(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kPlayableHuntingRuleVersion,std::move(terrain));
}
simulation::World restore_multi_field_agriculture(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kMultiFieldAgricultureRuleVersion,std::move(terrain));
}
simulation::World restore_physical_food_growers(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kPhysicalFoodGrowersRuleVersion,std::move(terrain));
}
simulation::World restore_seasonal_agriculture(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kSeasonalAgricultureRuleVersion,std::move(terrain));
}
simulation::World restore_irrigation_productivity(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kIrrigationProductivityRuleVersion,std::move(terrain));
}
simulation::World restore_food_processing(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kFoodProcessingRuleVersion,std::move(terrain));
}
simulation::World restore_food_warehouses(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kFoodWarehousesRuleVersion,std::move(terrain));
}
simulation::World restore_agriculture_reconfiguration(const SaveDocument& d,const fs::path& root,
        std::vector<std::uint8_t> mask,std::shared_ptr<const simulation::MapPermissions> permissions,
        std::shared_ptr<const original::OriginalRulesModel> model,
        std::shared_ptr<const simulation::OriginalTerrainFacts> terrain) {
    return restore_original_fire_generation(d,root,std::move(mask),std::move(permissions),std::move(model),
        simulation::kAgricultureReconfigurationRuleVersion,std::move(terrain));
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
    require(!d.world.navigation_session_present,"Transient grower navigation has no save schema");
    performance::increment(performance::Counter::FileWrites);
    validate_save_target(path,root);
    const bool physical=simulation::physical_food_growers_enabled(d.world.profile,d.world.rule_version);
    const bool multi=simulation::multi_field_agriculture_enabled(d.world.profile,d.world.rule_version);
    const bool seasonal=simulation::seasonal_agriculture_enabled(d.world.profile,d.world.rule_version);
    const bool productivity=simulation::irrigation_productivity_enabled(d.world.profile,d.world.rule_version);
    const bool reconfiguration=simulation::agriculture_reconfiguration_enabled(d.world.profile,d.world.rule_version);
    const bool warehouses=simulation::food_warehouses_enabled(d.world.profile,d.world.rule_version);
    const bool processing=simulation::food_processing_enabled(d.world.profile,d.world.rule_version);
    const auto own_schema=reconfiguration ? 52U:warehouses ? 51U:processing ? 50U:productivity ? 49U:seasonal ? 48U:physical ? 47U:multi ? 46U:0U;
    const auto validate_generation_target=[&](const fs::path& target) {
        const auto protected_schema=protected_agriculture_save_target(target);
        require(!protected_schema || protected_schema==own_schema,
                "Writers cannot overwrite another schema46/47/48/49/50/51/52 generation or an oversized unknown target");
        if(own_schema && fs::exists(target)) {
            const auto existing=read_save(target);
            const auto expected_rule=reconfiguration ? simulation::kAgricultureReconfigurationRuleVersion:warehouses ? simulation::kFoodWarehousesRuleVersion:processing ? simulation::kFoodProcessingRuleVersion:productivity ? simulation::kIrrigationProductivityRuleVersion:seasonal ? simulation::kSeasonalAgricultureRuleVersion:physical ? simulation::kPhysicalFoodGrowersRuleVersion:simulation::kMultiFieldAgricultureRuleVersion;
            require(existing.source_schema_version==own_schema && existing.world.profile==simulation::RulesProfile::CityV16 &&
                    existing.world.rule_version==expected_rule,
                    "Rule30/31/32/33/34/35/36 requires its own exact schema46/47/48/49/50/51/52 save target");
        }
    };
    validate_generation_target(path);
    if (simulation::original_agriculture_enabled(d.world.profile,d.world.rule_version) && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==40 &&
                simulation::original_agriculture_enabled(existing.world.profile,existing.world.rule_version),
                "Agriculture cannot overwrite a historical save; choose a separate schema40 target");
    }
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        d.world.rule_version==simulation::kPlayableAgricultureRuleVersion && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==43 &&
                simulation::playable_agriculture_enabled(existing.world.profile,existing.world.rule_version),
                "Playable Agriculture requires a separate schema43 save target");
    }
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        d.world.rule_version==simulation::kPlayableHuntingRuleVersion && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==45 && simulation::playable_hunting_enabled(existing.world.profile,existing.world.rule_version),
            "Playable Hunting requires a separate schema45 save target");
    }
    if (d.world.profile==simulation::RulesProfile::CityV16 && d.world.rule_version==simulation::kPlayableFishingRuleVersion && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==44 &&
                simulation::playable_fishing_enabled(existing.world.profile,existing.world.rule_version),
                "Playable Fishing requires a separate schema44 save target");
    }
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        d.world.rule_version==simulation::kOriginalIrrigationRuleVersion && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==42 &&
                simulation::original_irrigation_enabled(existing.world.profile,existing.world.rule_version),
                "Irrigation requires a separate schema42 save target");
    }
    if (d.world.profile==simulation::RulesProfile::CityV16 &&
        d.world.rule_version==simulation::kOriginalPlantationsRuleVersion && fs::exists(path)) {
        const auto existing=read_save(path);
        require(existing.source_schema_version==41 &&
                simulation::original_plantations_enabled(existing.world.profile,existing.world.rule_version),
                "Plantations cannot overwrite a historical save; choose a separate schema41 target");
    }
    // Recheck original inputs and all world invariants before writing any bytes.
    if (land_trade_world(d.world))
        (void)restore_authored_land_trade(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
            d.prepared_land_trade_boundary_facts);
    else if (simulation::original_production_enabled(d.world.profile,d.world.rule_version))
        if (simulation::agriculture_reconfiguration_enabled(d.world.profile,d.world.rule_version))
                (void)restore_agriculture_reconfiguration(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
        else if (simulation::food_warehouses_enabled(d.world.profile,d.world.rule_version))
                (void)restore_food_warehouses(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
        else if (simulation::food_processing_enabled(d.world.profile,d.world.rule_version))
                (void)restore_food_processing(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
        else if (simulation::irrigation_productivity_enabled(d.world.profile,d.world.rule_version))
                (void)restore_irrigation_productivity(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::seasonal_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_seasonal_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::physical_food_growers_enabled(d.world.profile,d.world.rule_version))
                (void)restore_physical_food_growers(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::multi_field_agriculture_enabled(d.world.profile,d.world.rule_version))
                (void)restore_multi_field_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::playable_hunting_enabled(d.world.profile,d.world.rule_version))
                (void)restore_playable_hunting(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,d.prepared_original_terrain_facts);
            else if (simulation::playable_fishing_enabled(d.world.profile,d.world.rule_version))
            (void)restore_playable_fishing(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::playable_agriculture_enabled(d.world.profile,d.world.rule_version))
            (void)restore_playable_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_irrigation_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_irrigation(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_plantations_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_plantations(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_agriculture_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_agriculture(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_venue_house_service_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_venue_house_service(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_music_venues_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_music_venues(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_safe_demolition_enabled(d.world.profile,d.world.rule_version))
            (void)restore_original_safe_demolition(d,root,mask,d.prepared_permissions,d.prepared_original_rules_model,
                d.prepared_original_terrain_facts);
        else if (simulation::original_deadline_food_dispatch_enabled(d.world.profile,d.world.rule_version))
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
    platform::atomic_replace(target,bytes,[&] {
        validate_save_target(target,root);
        validate_generation_target(target);
    },fault);
}
} // namespace openemperor::persistence
