#include "persistence/SandboxSave.h"

#include "maps/MapCatalog.h"
#include "maps/SandboxPlacement.h"
#include "maps/StoredGraphicsPlan.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <array>
#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace openemperor::persistence {
namespace {
using json=nlohmann::json;
namespace fs=std::filesystem;
constexpr std::uintmax_t max_save_bytes=8U*1024U*1024U;

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
    const bool industry=s.profile==simulation::RulesProfile::IndustryV5;
    json bs=json::array(),cs=json::array();
    for (std::size_t i=0;i<(industry ? 9U:7U);++i) {
        const auto& b=s.buildings[i];
        json entry={{"id",static_cast<int>(b.id)},
        {"kind",static_cast<int>(b.kind)},{"cell",cell_json(b.cell)},{"placed",b.placed},
        {"input_clay",b.input_clay},{"output",b.output},{"pottery_stock",b.pottery_stock},
        {"reserved_incoming",b.reserved_incoming},{"progress",b.progress},
        {"active_recipe_clay",b.active_recipe_clay},{"recipes_completed",b.recipes_completed},
        {"placed_tick",b.placed_tick},{"demand_progress",b.demand_progress},
        {"fulfilled_demand",b.fulfilled_demand},{"missed_demand",b.missed_demand},
        {"consumed_total",b.consumed_total},{"last_demand_status",b.last_demand_status}};
        if (industry) entry["clay_extracted"]=b.clay_extracted;
        bs.push_back(std::move(entry));
    }
    for (std::size_t i=0;i<(industry ? 5U:3U);++i) {
        const auto& c=s.couriers[i];
        json entry={{"id",static_cast<int>(c.id)},
        {"owner",static_cast<int>(c.owner)},{"target",static_cast<int>(c.target)},
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
    return result;
}
simulation::WorldSnapshot parse_snapshot(const json& j,std::uint64_t schema,
                                         simulation::RulesProfile profile) {
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
    const std::size_t expected_buildings=schema==5 ? 9U:schema==4 ? 7U:schema==3 ? 4U:3U;
    const std::size_t expected_couriers=schema==5 ? 5U:schema>=3 ? 3U:2U;
    require(bs.is_array() && bs.size()==expected_buildings &&
            cs.is_array() && cs.size()==expected_couriers,
            "invalid building or courier array length");
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
    std::array<bool,9> seen{};
    for (std::size_t i=0;i<expected_buildings;++i) {
        const auto& b=bs[i];
        const auto id=small(field(b,"id"),schema==5 ? 9:schema==4 ? 7:4);
        require(id>0 && (schema<4 || !seen[static_cast<std::size_t>(id-1)]),
                "duplicate or invalid building ID");
        const auto slot=schema>=4 ? static_cast<std::size_t>(id-1):i;
        seen[slot]=true;
        auto& x=s.buildings[slot];
        x.id=static_cast<BuildingId>(id);
        x.kind=static_cast<Object>(small(field(b,"kind"),6));
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
        if (schema==5) x.clay_extracted=number(field(b,"clay_extracted"));
    }
    if (schema<5) s.buildings[0].clay_extracted=s.clay_extracted_total;
    if (schema>=4) {
        for (std::size_t i=0;i<expected_buildings;++i)
            require(seen[i],"missing building ID");
        s.next_household_id=static_cast<std::uint8_t>(small(field(j,"next_household_id"),household_id_end));
        const auto& last=field(j,"last_dispatched_household");
        if (!last.is_null()) s.last_dispatched_household=static_cast<BuildingId>(
            small(last,household_id_end-1));
        if (schema==5) {
            s.next_production_id=static_cast<std::uint8_t>(small(field(j,"next_production_id"),10));
            s.next_courier_id=static_cast<std::uint8_t>(small(field(j,"next_courier_id"),6));
        }
    } else if (schema==3 && s.buildings[3].placed) s.next_household_id=5;
    std::array<bool,5> seen_couriers{};
    for (std::size_t i=0;i<expected_couriers;++i) {
        const auto& c=cs[i];
        const auto id=small(field(c,"id"),schema==5 ? 5:3);
        require(id>0 && (schema!=5 || !seen_couriers[static_cast<std::size_t>(id-1)]),
            "duplicate or invalid courier ID");
        const auto slot=schema==5 ? static_cast<std::size_t>(id-1):i;
        seen_couriers[slot]=true;
        auto& x=s.couriers[slot];
        x.id=static_cast<CourierId>(id);
        x.owner=static_cast<BuildingId>(small(field(c,"owner"),schema==5 ? 9:schema==4 ? 7:4));
        x.target=static_cast<BuildingId>(small(field(c,"target"),schema==5 ? 9:schema==4 ? 7:4));
        if (schema==5) {
            x.role=static_cast<CourierRole>(small(field(c,"role"),3));
            const auto& last=field(c,"last_dispatched_pottery");
            if (!last.is_null()) x.last_dispatched_pottery=static_cast<BuildingId>(small(last,9));
        }
        x.good=static_cast<Good>(small(field(c,"good"),2));
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
    if (schema==5) for (bool present:seen_couriers) require(present,"missing courier ID");
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
json document_json(const SaveDocument& d) {
    return {{"format","openemperor-sandbox-save"},{"schema_version",
             d.world.profile==simulation::RulesProfile::IndustryV5 ? 5:4},
        {"map",{{"relative_path",d.map_relative.generic_string()},{"sha256",d.map_sha256},
                {"part_index",0},{"grid_width",d.world.width},{"grid_height",d.world.height}}},
        {"profiles",{{"graphics",maps::stored_graphics_slot8_profile},
                     {"footprint","edge-byte-4x4"},{"buildability",maps::sandbox_buildable_profile},
                     {"buildable_sha256",d.buildable_sha256}}},
        {"rules",{{"id",rules_profile_name(d.world.profile)},
                  {"version",d.world.rule_version}}},
        {"world",snapshot_json(d.world)}};
}
SaveDocument parse_document(const json& j) {
    require(str(field(j,"format"))=="openemperor-sandbox-save","unknown save format");
    const auto schema=number(field(j,"schema_version"));
    require(schema>=1 && schema<=5,"unsupported save schema version");
    const auto& m=field(j,"map"); const auto& p=field(j,"profiles");
    const auto& r=field(j,"rules");
    SaveDocument d;
    d.source_schema_version=static_cast<std::uint32_t>(schema);
    d.map_relative=fs::path(str(field(m,"relative_path")));
    safe_relative(d.map_relative);
    d.map_sha256=str(field(m,"sha256"));
    d.buildable_sha256=str(field(p,"buildable_sha256"));
    require(valid_hash(d.map_sha256) && valid_hash(d.buildable_sha256),"invalid save fingerprint");
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
    else throw std::runtime_error("unknown sandbox rule ID");
    d.world.rule_version=static_cast<std::uint32_t>(number(field(r,"version"),UINT32_MAX));
    require(d.world.rule_version==(d.world.profile==simulation::RulesProfile::ProductionV2 && schema>=2 ? 2U:1U) &&
            (d.world.profile!=simulation::RulesProfile::HouseholdV3 || schema>=3) &&
            (d.world.profile!=simulation::RulesProfile::SettlementV4 || schema==4) &&
            (d.world.profile!=simulation::RulesProfile::IndustryV5 || schema==5) &&
            (schema!=5 || d.world.profile==simulation::RulesProfile::IndustryV5),
            "unsupported sandbox rule version");
    auto parsed=parse_snapshot(field(j,"world"),schema,d.world.profile);
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
    SaveDocument d{relative,hash_file(map),digest(mask.data(),mask.size()),world.snapshot()};
    // Validate the snapshot against the actual mask before publishing it.
    (void)simulation::World::restore(d.world,mask);
    return d;
}
SaveDocument read_save(const fs::path& path) {
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
    const auto j=json::parse(bytes,[&](int depth,json::parse_event_t event,json&) {
        if ((event==json::parse_event_t::object_start || event==json::parse_event_t::array_start) &&
            depth>32) throw std::runtime_error("save JSON nesting exceeds 32");
        return true;
    });
    return parse_document(j);
}
simulation::World restore_save(const SaveDocument& d,const fs::path& root,
                               std::vector<std::uint8_t> mask) {
    safe_relative(d.map_relative);
    const auto map=maps::resolve_map_path(root,d.map_relative);
    require(hash_file(map)==d.map_sha256,"original map SHA-256 differs from save");
    require(digest(mask.data(),mask.size())==d.buildable_sha256,
            "sandbox buildable mask SHA-256 differs from save");
    return simulation::World::restore(d.world,std::move(mask));
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
    validate_save_target(path,root);
    // Recheck original inputs and all world invariants before writing any bytes.
    (void)restore_save(d,root,mask);
    const auto bytes=document_json(d).dump(2);
    require(bytes.size()<=max_save_bytes,"save JSON exceeds 8 MiB");
    const auto target=fs::absolute(path).lexically_normal();
    fs::create_directories(target.parent_path());
    validate_save_target(target,root);
    platform::atomic_replace(target,bytes,[&] { validate_save_target(target,root); },fault);
}
} // namespace openemperor::persistence
