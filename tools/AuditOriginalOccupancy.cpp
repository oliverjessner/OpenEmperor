// Read-only, on-demand corpus research. All file-format and admission decisions
// are delegated to the existing production parsers and permissions producer.
#include "maps/LandscapeProvenance.h"
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"
#include "maps/SandboxPlacement.h"
#include "maps/StoredMapSession.h"
#include "persistence/SandboxSave.h"
#include "renderer/StoredGraphicsRenderer.h"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#if defined(__APPLE__) || defined(__unix__)
#include <sys/resource.h>
#endif

namespace fs=std::filesystem;
using json=nlohmann::json;
using namespace openemperor;
namespace {
constexpr std::size_t cells=maps::stored_grid_width*maps::stored_grid_height;
constexpr std::size_t maximum_interaction_pairs=65536;

std::string digest(std::span<const std::uint8_t> bytes) {
    std::array<unsigned char,EVP_MAX_MD_SIZE> output{};unsigned length=0;
    if (EVP_Digest(bytes.data(),bytes.size(),output.data(),&length,EVP_sha256(),nullptr)!=1 || length!=32)
        throw std::runtime_error("SHA-256 failed");
    constexpr char hex[]="0123456789abcdef";std::string result;
    for (unsigned i=0;i<length;++i) {result.push_back(hex[output[i]>>4U]);result.push_back(hex[output[i]&15U]);}
    return result;
}
std::string digest(const std::string& text) {
    return digest(std::span{reinterpret_cast<const std::uint8_t*>(text.data()),text.size()});
}
std::string hash_file(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("cannot read inventory input");
    std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)> ctx{EVP_MD_CTX_new(),EVP_MD_CTX_free};
    if (!ctx || EVP_DigestInit_ex(ctx.get(),EVP_sha256(),nullptr)!=1) throw std::runtime_error("SHA-256 init failed");
    std::array<char,65536> buffer{};
    while (input) {
        input.read(buffer.data(),std::streamsize(buffer.size()));
        if (input.gcount()>0 && EVP_DigestUpdate(ctx.get(),buffer.data(),std::size_t(input.gcount()))!=1)
            throw std::runtime_error("SHA-256 update failed");
    }
    if (!input.eof()) throw std::runtime_error("inventory input read failed");
    std::array<unsigned char,EVP_MAX_MD_SIZE> output{};unsigned length=0;
    if (EVP_DigestFinal_ex(ctx.get(),output.data(),&length)!=1 || length!=32) throw std::runtime_error("SHA-256 finish failed");
    constexpr char hex[]="0123456789abcdef";std::string result;
    for (unsigned i=0;i<length;++i) {result.push_back(hex[output[i]>>4U]);result.push_back(hex[output[i]&15U]);}
    return result;
}
void write_text(const fs::path& path,const std::string& text) {
    fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
    if (!out || !(out<<text)) throw std::runtime_error("cannot write private audit output");
}
void write_json(const fs::path& path,const json& value) {write_text(path,value.dump(2)+"\n");}
bool beneath(const fs::path& child,const fs::path& parent) {
    auto c=child.begin();for (auto p=parent.begin();p!=parent.end();++p,++c) if (c==child.end() || *c!=*p) return false;
    return true;
}
json inventory(const fs::path& root) {
    std::vector<fs::path> files;
    for (const auto& entry:fs::recursive_directory_iterator(root)) {
        if (entry.is_symlink()) throw std::runtime_error("inventory symlink is unsupported");
        if (entry.is_regular_file()) files.push_back(entry.path());
    }
    std::sort(files.begin(),files.end(),[&](const auto& a,const auto& b){return a.lexically_relative(root).generic_string()<b.lexically_relative(root).generic_string();});
    json result={{"files",json::array()},{"total_files",files.size()},{"total_bytes",0}};
    std::uint64_t total=0;std::string manifest;
    for (const auto& path:files) {
        const auto relative=path.lexically_relative(root).generic_string();const auto size=fs::file_size(path);const auto sha=hash_file(path);
        result["files"].push_back({{"relative_path",relative},{"bytes",size},{"sha256",sha}});
        total+=size;manifest+=relative+"\t"+std::to_string(size)+"\t"+sha+"\n";
    }
    result["total_bytes"]=total;result["manifest_encoding"]="UTF-8 relative path TAB decimal size TAB SHA-256 LF, lexicographic path order";
    result["manifest_sha256"]=digest(manifest);return result;
}
json coord(simulation::Cell c) {return json::array({c.x,c.y});}
json source(const maps::OriginalEntityRecord& e,const maps::ParsedEmperorMap& map,const maps::MapGeometry& geometry,std::span<const std::uint8_t> heights) {
    const auto& p=e.provenance;const int x=int(e.local_x)+int(geometry.border),y=int(e.local_y)+int(geometry.border);
    json result={{"entity_class",maps::original_entity_class_name(e.entity_class)},{"type",e.type},
        {"original_id",e.serialized_original_id},{"manager_index",e.manager_index},{"status",e.status},
        {"footprint_side",e.footprint_side},{"subindex",e.subindex},{"base_schema",p.base_schema},
        {"wrapper_schema",p.wrapper_schema},{"extended_schema",p.extended_schema},{"class_wrapper_schema",p.class_wrapper_schema},
        {"local_coordinates",json::array({e.local_x,e.local_y})},{"storage_coordinates",json::array({x,y})},
        {"serialized_cell_reference",e.serialized_cell_reference},{"logical_record_position",p.logical_record_offset},
        {"logical_base_position",p.logical_base_offset},{"record_byte_length",p.record_byte_length},
        {"mfc_object_reference",p.mfc_object_reference}};
    if (p.logical_extended_offset) result["logical_extended_position"]=*p.logical_extended_offset;
    if (p.logical_class_wrapper_offset) result["logical_class_wrapper_position"]=*p.logical_class_wrapper_offset;
    if (e.gate_house) result["gate_layout"]=e.gate_house->layout;
    if (x>=0 && y>=0 && x<int(maps::stored_grid_width) && y<int(maps::stored_grid_height)) {
        const auto at=std::size_t(y)*maps::stored_grid_width+std::size_t(x);
        result["raw_terrain"]=map.terrain_raw.values[at];result["raw_objects"]=map.objects_raw.values[at];
        result["signed_height"]=heights[at]<128 ? int(heights[at]):int(heights[at])-256;
        result["geometry_on_map"]=geometry.contains({unsigned(x),unsigned(y)});
    }
    return result;
}
std::string check(const maps::ParsedEmperorMap& map,const maps::MapGeometry& g,const maps::OriginalMapEntities& entities,std::span<const std::uint8_t> heights) {
    try {maps::validate_sandbox_original_map_rules(map,g,entities,heights);return {};}
    catch (const std::exception& error) {return error.what();}
}
std::string category(const std::string& reason) {
    if (reason.empty()) return "prepared";
    if (reason.find("unsupported active original occupancy:")!=std::string::npos) return "unknown_occupancy";
    if (reason.find("conflict")!=std::string::npos) return "known_conflict";
    return "other_original_rules_failure";
}
// This enumerates only candidate interactions from already production-accepted
// source geometries; the production validator decides whether a pair conflicts.
std::vector<std::size_t> interaction_cells(const maps::OriginalEntityRecord& e,const maps::MapGeometry& g) {
    const int x=int(e.local_x)+int(g.border),y=int(e.local_y)+int(g.border);
    unsigned width=e.footprint_side,height=width;
    if (e.gate_house) {width=e.gate_house->layout==0 ? 5U:3U;height=e.gate_house->layout==0 ? 3U:5U;}
    else if (width==0) width=height=1; // Only called after actual production acceptance.
    std::vector<std::size_t> result;
    auto add=[&](int cx,int cy) {if (cx>=0 && cy>=0 && cx<int(maps::stored_grid_width) && cy<int(maps::stored_grid_height)) result.push_back(std::size_t(cy)*maps::stored_grid_width+std::size_t(cx));};
    for (unsigned dy=0;dy<height;++dy) for (unsigned dx=0;dx<width;++dx) add(x+int(dx),y+int(dy));
    if (e.gate_house) {
        if (e.gate_house->layout==0) {add(x+2,y-1);add(x+2,y+3);} else {add(x-1,y+2);add(x+3,y+2);}
    }
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
json save_permissions(const simulation::MapPermissions& p,const fs::path& directory,const std::string& map_sha,std::span<const std::uint8_t> legacy) {
    fs::create_directories(directory);write_text(directory/"canonical.txt",p.canonical_state());
    write_text(directory/"legacy_mask.bin",std::string(reinterpret_cast<const char*>(legacy.data()),legacy.size()));
    const std::array<std::string,6> names={"roads","buildings","protected","signed_heights","road_blockers","building_blockers"};
    std::array<std::ofstream,6> output;
    for (std::size_t i=0;i<output.size();++i) output[i].open(directory/(names[i]+".bin"),std::ios::binary);
    for (const auto& c:p.cells()) {
        const std::array<std::uint8_t,6> bytes={std::uint8_t(c.road_allowed),std::uint8_t(c.building_allowed),std::uint8_t(c.protected_original),std::uint8_t(c.height),std::uint8_t(c.road_blocker),std::uint8_t(c.building_blocker)};
        for (std::size_t i=0;i<bytes.size();++i) {output[i].put(char(bytes[i]));if (!output[i]) throw std::runtime_error("component write failed");}
    }
    json gates=json::array();
    for (const auto& gate:p.gates()) {
        json value={{"id",gate.id.value},{"protected_footprint",json::array()},{"corridor",json::array()},{"openings",json::array()}};
        for (const auto c:gate.protected_footprint) value["protected_footprint"].push_back(coord(c));
        for (const auto c:gate.corridor) value["corridor"].push_back(coord(c));
        for (const auto c:gate.openings) value["openings"].push_back(coord(c));gates.push_back(std::move(value));
    }
    write_json(directory/"gates.json",gates);
    std::ofstream edges(directory/"edge_blockers.bin",std::ios::binary);
    constexpr std::array<simulation::Cell,4> directions={simulation::Cell{0,-1},{1,0},{0,1},{-1,0}};
    for (int y=0;y<p.height();++y) for (int x=0;x<p.width();++x) for (const auto d:directions) {
        edges.put(char(p.transport_edge_blocker({x,y},{x+d.x,y+d.y})));if (!edges) throw std::runtime_error("edge component write failed");
    }
    json result={{"policy_version",p.policy_version()},{"policy_fingerprint",persistence::map_permissions_fingerprint(p,map_sha)},
        {"canonical_sha256",digest(p.canonical_state())},{"legacy_mask_sha256",digest(legacy)},
        {"roads",std::count(p.road_mask().begin(),p.road_mask().end(),std::uint8_t(1))},
        {"buildings",std::count(p.building_mask().begin(),p.building_mask().end(),std::uint8_t(1))},{"gate_count",p.gates().size()},
        {"edge_encoding","row-major storage cells, cardinal order north/east/south/west, literal production BuildBlocker bytes"}};
    write_json(directory/"metadata.json",result);return result;
}

json audit_map(const maps::MapCatalogEntry& entry,const fs::path& root,const fs::path& output) {
    json row={{"relative_path",entry.relative_path.generic_string()},{"manager_status","not_parsed"},
        {"active_object_count",nullptr},{"unknown_records",json::array()},{"known_record_errors",json::array()},
        {"occupancy_conflicts",json::array()},{"policy_prepared",false},{"normal_start_checked",false}};
    const auto container=maps::EmperorContainer::open(maps::resolve_map_path(root,entry.relative_path));
    row["input_sha256"]=maps::map_input_sha256(container);row["physical_bytes"]=container.physical_size();
    const auto map=maps::read_emperor_map(container,0);const maps::MapGeometry geometry{map.declared_map_size};
    row["declared_map_size"]=map.declared_map_size;row["border"]=geometry.border;
    maps::OriginalMapEntities entities;
    try {entities=maps::read_original_map_entities(container,0);}
    catch (const std::exception& error) {
        row["manager_status"]="atomic_failure";row["parser_error"]=error.what();row["first_preparation_error"]=error.what();
        row["first_category"]=std::string(error.what()).find("cResWall")!=std::string::npos ? "unsupported_cResWall":"parser_failure";
        row["no_partial_or_following_records_claimed"]=true;return row;
    }
    row["manager_status"]="complete";row["manager_records"]=entities.records.size();row["manager_schema"]=entities.manager_schema;
    row["manager_logical_position"]=entities.logical_offset;row["manager_byte_length"]=entities.byte_length;
    const auto heights=container.read_range(0,maps::landscape_height_offset,cells);
    const auto first=check(map,geometry,entities,heights);row["first_preparation_error"]=first;row["first_category"]=category(first);
    maps::OriginalMapEntities probe=entities;probe.records.clear();std::vector<maps::OriginalEntityRecord> supported;
    std::set<std::size_t> unknown_indices;row["active_object_count"]=0;
    for (const auto& record:entities.records) if (record.active()) {
        row["active_object_count"]=row["active_object_count"].get<std::size_t>()+1;
        probe.records={record};const auto reason=check(map,geometry,probe,heights);
        if (reason.empty()) supported.push_back(record);
        else {
            auto value=source(record,map,geometry,heights);value["production_error"]=reason;
            if (category(reason)=="unknown_occupancy") {row["unknown_records"].push_back(std::move(value));unknown_indices.insert(record.manager_index);}
            else row["known_record_errors"].push_back(std::move(value));
        }
    }
    // Bound candidate pairs by shared source-claim/opening cells, not all pairs.
    std::map<std::size_t,std::vector<std::size_t>> cell_records;std::set<std::pair<std::size_t,std::size_t>> pairs;
    bool pair_limit_reached=false;
    for (std::size_t i=0;i<supported.size() && !pair_limit_reached;++i) for (const auto at:interaction_cells(supported[i],geometry)) {
        auto& prior=cell_records[at];
        for (const auto j:prior) {
            const auto pair=std::make_pair(j,i);
            if (!pairs.contains(pair) && pairs.size()==maximum_interaction_pairs) {pair_limit_reached=true;break;}
            pairs.insert(pair);
        }
        if (pair_limit_reached) break;
        prior.push_back(i);
    }
    row["production_pair_checks"]=pairs.size();
    row["known_pair_conflict_diagnosis_complete"]=!pair_limit_reached;
    row["candidate_pair_limit"]=maximum_interaction_pairs;
    if (pair_limit_reached) row["conflict_diagnosis_notice"]="Additional pair diagnosis stopped at the fixed bound. The full production first-preparation decision remains complete; this partial pair list must not be used as a complete conflict inventory.";
    for (const auto [a,b]:pairs) {
        probe.records={supported[a],supported[b]};const auto reason=check(map,geometry,probe,heights);
        if (!reason.empty()) row["occupancy_conflicts"].push_back({{"first_manager_index",supported[a].manager_index},
            {"second_manager_index",supported[b].manager_index},{"first_original_id",supported[a].serialized_original_id},
            {"second_original_id",supported[b].serialized_original_id},{"production_error",reason}});
    }
    // Strictly labelled counterfactual: replacing only this unknown group's
    // classification with the already accepted conservative origin-marker
    // contract measures possible map impact. It proves no original semantics,
    // never publishes MapPermissions and preserves all IDs/coordinates/schemas.
    std::set<std::pair<std::string,int>> groups;
    for (const auto& record:entities.records) if (unknown_indices.contains(record.manager_index)) groups.emplace(maps::original_entity_class_name(record.entity_class),record.type);
    row["conditional_origin_marker_probes"]=json::array();
    for (const auto& [class_name,type]:groups) {
        probe=entities;
        for (auto& record:probe.records) if (unknown_indices.contains(record.manager_index) && maps::original_entity_class_name(record.entity_class)==class_name && record.type==type) {
            record.entity_class=maps::OriginalEntityClass::Industrial;record.type=162;
        }
        const auto reason=check(map,geometry,probe,heights);
        row["conditional_origin_marker_probes"].push_back({{"entity_class",class_name},{"type",type},
            {"would_pass_original_rules",reason.empty()},{"remaining_production_error",reason}});
    }
    if (first.empty()) {
        const auto directory=output/"permissions"/entry.relative_path;
        std::vector<std::uint8_t> zero(cells,0),one(cells,1);
        const auto minimal=maps::prepare_sandbox_map_permissions(map,geometry,zero,entities,heights);
        const auto maximal=maps::prepare_sandbox_map_permissions(map,geometry,one,entities,heights);
        row["policy_prepared"]=true;row["policies"]={{"zero_legacy",save_permissions(*minimal,directory/"zero-legacy",row["input_sha256"],zero)},
            {"maximal_legacy",save_permissions(*maximal,directory/"maximal-legacy",row["input_sha256"],one)}};
    }
    return row;
}

json priorities(const json& rows) {
    std::map<std::pair<std::string,int>,json> groups;
    for (const auto& row:rows) {
        std::set<std::pair<std::string,int>> types;
        for (const auto& record:row["unknown_records"]) types.emplace(record["entity_class"].get<std::string>(),record["type"].get<int>());
        for (const auto& key:types) {
            auto& group=groups[key];
            if (group.is_null()) group={{"entity_class",key.first},{"type",key.second},{"records",0},{"maps",json::array()},
                {"first_failure_maps",json::array()},{"other_unknown_types",json::array()},{"known_conflict_maps",json::array()},
                {"other_known_error_maps",json::array()},{"conditional_origin_marker_preparable_maps",json::array()},
                {"source_state_combinations",json::array()}};
            group["maps"].push_back(row["relative_path"]);
            for (const auto& record:row["unknown_records"]) if (record["entity_class"]==key.first && record["type"]==key.second) {
                group["records"]=group["records"].get<std::size_t>()+1;
                if (row["first_preparation_error"]==record["production_error"]) group["first_failure_maps"].push_back(row["relative_path"]);
                json state=json::object();for (const auto* field:{"status","footprint_side","subindex","base_schema","wrapper_schema","extended_schema","class_wrapper_schema"}) state[field]=record[field];
                if (std::find(group["source_state_combinations"].begin(),group["source_state_combinations"].end(),state)==group["source_state_combinations"].end()) group["source_state_combinations"].push_back(std::move(state));
            }
            for (const auto& other:types) if (other!=key) {
                const json value={{"entity_class",other.first},{"type",other.second}};
                if (std::find(group["other_unknown_types"].begin(),group["other_unknown_types"].end(),value)==group["other_unknown_types"].end()) group["other_unknown_types"].push_back(value);
            }
            if (!row["occupancy_conflicts"].empty()) group["known_conflict_maps"].push_back(row["relative_path"]);
            if (!row["known_record_errors"].empty()) group["other_known_error_maps"].push_back(row["relative_path"]);
            for (const auto& probe:row["conditional_origin_marker_probes"]) if (probe["entity_class"]==key.first && probe["type"]==key.second && probe["would_pass_original_rules"].get<bool>()) group["conditional_origin_marker_preparable_maps"].push_back(row["relative_path"]);
        }
    }
    json result=json::array();
    for (auto& [key,group]:groups) {
        (void)key;for (const auto* field:{"maps","first_failure_maps","other_unknown_types","known_conflict_maps","other_known_error_maps","conditional_origin_marker_preparable_maps","source_state_combinations"}) {
            auto& values=group[field];std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());
        }
        group["map_count"]=group["maps"].size();group["first_failure_map_count"]=group["first_failure_maps"].size();
        group["conditional_origin_marker_preparable_count"]=group["conditional_origin_marker_preparable_maps"].size();result.push_back(std::move(group));
    }
    std::sort(result.begin(),result.end(),[](const json& a,const json& b) {
        if (a["conditional_origin_marker_preparable_count"]!=b["conditional_origin_marker_preparable_count"]) return a["conditional_origin_marker_preparable_count"]>b["conditional_origin_marker_preparable_count"];
        if (a["first_failure_map_count"]!=b["first_failure_map_count"]) return a["first_failure_map_count"]>b["first_failure_map_count"];
        return a["type"]<b["type"];
    });return result;
}
} // namespace

int main(int argc,char** argv) {
    try {
        if (argc!=3) throw std::invalid_argument("usage: openemperor-audit-original-occupancy DATA-ROOT .local/OUTPUT-ROOT");
        const auto root=fs::canonical(argv[1]);const auto output=fs::weakly_canonical(argv[2]);
        const auto private_root=fs::canonical(fs::current_path()/".local");
        if (!beneath(output,private_root) || beneath(output,root) || output==private_root || (fs::exists(output) && !fs::is_empty(output)))
            throw std::invalid_argument("audit output requires a fresh ignored .local directory outside original inputs");
        fs::create_directories(output);
        const auto started=std::chrono::steady_clock::now();const auto initial_inventory=inventory(root);write_json(output/"input-inventory.json",initial_inventory);
        const auto catalog=maps::discover_standalone_maps(root);
        json report={{"schema","openemperor-original-occupancy-audit-v1"},{"input_revision",{{"files",initial_inventory["total_files"]},{"bytes",initial_inventory["total_bytes"]},{"manifest_sha256",initial_inventory["manifest_sha256"]}}},
            {"catalog_entries",catalog.entries.size()},{"catalog_scan_errors",catalog.scan_errors},{"maps",json::array()},
            {"diagnostic_semantics","Complete atomic production manager first; individual source records and bounded interacting pairs validated by the existing producer. No records asserted after a parser error."},
            {"candidate_probe_semantics","Conditional conservative MarkerOrigin impact only: a copied unknown Class/Type group is replaced with existing Industrial162 solely for validate_sandbox_original_map_rules. All other records and input fields retained; no policy is published and no original semantics or actual support is claimed."},
            {"legacy_mask_semantics","Zero and one explicit extremes plus real normal-session mask after EdgeByte4x4Preview/Slot8/Automatic preparation through StoredGraphicsRenderer under dummy/software; real mask is not guessed."}};
        std::map<std::string,std::size_t> counts;
        for (const auto& entry:catalog.entries) if (entry.map_profile) {
            auto row=audit_map(entry,root,output);counts[row["first_category"].get<std::string>()]++;
            std::cerr<<entry.relative_path.generic_string()<<": "<<row["first_category"].get<std::string>()<<'\n';report["maps"].push_back(std::move(row));
        }
        report["first_failure_distribution"]=counts;report["prioritization"]=priorities(report["maps"]);
        write_json(output/"corpus.json",report);write_json(output/"prioritization.json",report["prioritization"]);
        // The normal-session legacy mask requires the actual decoder readiness.
        // Sequential preparation bounds live resources and releases each map.
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER,"dummy");
        if (!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
        SDL_Window* window=SDL_CreateWindow("Private corpus preparation",1280,720,SDL_WINDOW_HIDDEN);
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_Renderer* renderer=SDL_CreateRenderer(window,"software");
        if (!renderer) {SDL_DestroyWindow(window);throw std::runtime_error(SDL_GetError());}
        for (auto& row:report["maps"]) if (row["policy_prepared"].get<bool>()) {
            const fs::path relative=row["relative_path"].get<std::string>();
            try {
                auto session=maps::load_stored_map_session(root,relative,maps::FootprintPolicy::EdgeByte4x4Preview,maps::StoredGraphicsProfile::Slot8,maps::GreatWallPresentationMode::Automatic,row["input_sha256"]);
                const maps::MapGeometry geometry{session.map.declared_map_size};
                StoredGraphicsRenderer background{std::move(session.plan)};background.initialize(renderer);
                const auto mask=maps::make_sandbox_buildable_mask(background.plan(),geometry);
                const auto policy=maps::load_sandbox_map_permissions(session.map,background.plan(),geometry,mask,simulation::kMapPermissionsPolicyVersion,row["input_sha256"]);
                row["policies"]["production_legacy"]=save_permissions(*policy,output/"permissions"/relative/"production-legacy",row["input_sha256"],mask);
                row["production_legacy_prepared"]=true;row["presentation_preparation"]={{"decoded_assets",background.plan().decoded_assets},{"texture_uploads",background.plan().texture_uploads},{"logical_texture_bytes",background.plan().logical_texture_bytes}};
                std::cerr<<relative.generic_string()<<": production legacy mask prepared\n";
            } catch (const std::exception& error) {
                row["production_legacy_prepared"]=false;row["production_legacy_error"]=error.what();
                std::cerr<<relative.generic_string()<<": production legacy preparation failed\n";
            }
            if (StoredGraphicsRenderer::live_texture_count()!=0) throw std::runtime_error("private corpus texture ownership leak");
        }
        SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
        const auto final_inventory=inventory(root);const bool unchanged=initial_inventory==final_inventory;
        report["original_input_inventory_unchanged"]=unchanged;write_json(output/"corpus.json",report);
        const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
        json metrics={{"elapsed_seconds",elapsed},{"peak_rss_native_units",nullptr},{"peak_rss_bytes",nullptr},
            {"peak_rss_available",false},{"maps",report["maps"].size()},{"initialization_only",true},
            {"rendered_frames",0},{"worlds_created",0},{"original_input_inventory_unchanged",unchanged}};
#if defined(__APPLE__) || defined(__unix__)
        struct rusage usage{};
        if (getrusage(RUSAGE_SELF,&usage)==0) {
            metrics["peak_rss_available"]=true;metrics["peak_rss_native_units"]=usage.ru_maxrss;
#if defined(__APPLE__)
            metrics["peak_rss_bytes"]=usage.ru_maxrss;
#else
            metrics["peak_rss_bytes"]=std::uint64_t(usage.ru_maxrss)*1024U;
#endif
        }
#endif
        write_json(output/"metrics.json",metrics);
        std::cout<<json(counts).dump()<<'\n';return unchanged ? 0:1;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
