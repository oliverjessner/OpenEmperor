#include "assets/AssetCatalog.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include "maps/MapGraphicCandidates.h"
#include "maps/DirectGraphicCandidate.h"
#include "maps/GraphicsIdHypothesis.h"
#include "maps/ResourceGroupLookup.h"
#include "maps/TerrainInterpretation.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace assets = openemperor::assets;
namespace maps = openemperor::maps;
using Json = nlohmann::json;
constexpr std::uint64_t cells = static_cast<std::uint64_t>(maps::stored_grid_width) * maps::stored_grid_height;

std::uint32_t number(std::string_view value) {
    std::uint32_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
        throw std::invalid_argument("invalid unsigned number");
    return result;
}
std::uint32_t key_number(std::string_view value) {
    if (value.starts_with("0x") || value.starts_with("0X")) {
        value.remove_prefix(2);
        std::uint32_t result = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, 16);
        if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw std::invalid_argument("invalid hexadecimal group key");
        return result;
    }
    return number(value);
}
fs::path safe_file(const fs::path& root, const fs::path& relative) {
    if (relative.empty() || relative.is_absolute()) throw std::invalid_argument("expected relative map path");
    for (const auto& component : relative)
        if (component == "..") throw std::invalid_argument("map path escapes data root");
    const auto path = fs::canonical(root / relative);
    const auto checked = path.lexically_relative(root);
    if (checked.empty() || checked.is_absolute() || !fs::is_regular_file(path))
        throw std::invalid_argument("map path is unavailable");
    for (const auto& component : checked)
        if (component == "..") throw std::invalid_argument("map path escapes data root");
    return path;
}

struct Stats {
    std::map<std::uint32_t, std::uint64_t> frequency;
    void add(std::uint32_t value) { ++frequency[value]; }
    Json json() const {
        std::uint64_t count = 0;
        for (const auto& [value, n] : frequency) { (void)value; count += n; }
        std::vector<std::pair<std::uint32_t,std::uint64_t>> sorted(frequency.begin(), frequency.end());
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second > b.second : a.first < b.first;
        });
        Json top = Json::array();
        for (std::size_t i = 0; i < std::min<std::size_t>(sorted.size(), 12); ++i)
            top.push_back({{"value",sorted[i].first},{"count",sorted[i].second}});
        return {{"count",count},{"min",frequency.empty() ? 0 : frequency.begin()->first},
                {"max",frequency.empty() ? 0 : frequency.rbegin()->first},
                {"zeros",frequency.contains(0) ? frequency.at(0) : 0},
                {"distinct",frequency.size()},{"top",top}};
    }
};

Json resolution(std::uint32_t value, const assets::AssetCatalog& catalog) {
    const auto result = maps::resolve_direct_candidate(catalog,value);
    const auto* record = result.record;
    if (!record) return {{"status","index_out_of_range"},{"numeric_in_range",false}};
    Json out{{"status",maps::direct_candidate_status_name(result.status)},
             {"numeric_in_range",result.numeric_in_range},
             {"metadata_nonempty",result.metadata_nonempty},
             {"image_index",value},{"archive",record->id.archive_relative_path.generic_string()},
             {"type",record->image_type},{"kind",assets::sg3_image_kind_name(record->image_kind)},
             {"width",record->width},{"height",record->height},
             {"data_length",record->data_length},{"group_id",record->group_id},
             {"group_description",record->group_description},
             {"group_filename",record->group_filename},
             {"external_flag",record->external_flag},
             {"alpha_length",record->alpha_length},
             {"alpha_policy",assets::alpha_policy_name(record->alpha_policy)},
             {"color_bounds",assets::asset_range_status_name(record->color_bounds)},
             {"alpha_bounds",assets::asset_range_status_name(record->alpha_bounds)},
             {"payload_available",result.payload_available},
             {"decoder_supported",record->decoder_supported},
             {"flat_78x40_preview_compatible",
                record->image_type==30 && record->width==78 && record->height==40 &&
                record->uncompressed_length==3200 && record->data_length==3200 &&
                record->isometric_size_flag==1 && record->horizontal_mirror_offset==0 &&
                record->alpha_length==0 && result.payload_available && record->decoder_supported}};
    return out;
}

using Registrations = std::map<std::uint32_t, maps::GraphicsArchiveRegistration>;
constexpr std::string_view graphics_profile = "exe-6373328b-14bit-hypothesis";
constexpr std::string_view terrain_probe_profile = "exe-6373328b-first-terrain-probe";
constexpr std::string_view group_profile = "exe-6373328b-v213-resource-group";

Json graphics_resolution(std::uint32_t raw, const Registrations& registrations,
                         const fs::path& root, bool decode) {
    const auto result = maps::resolve_graphics_id_hypothesis(raw, registrations);
    Json out{{"profile",graphics_profile},{"raw",result.raw},{"slot",result.slot},
             {"local_index",result.local_index},{"status",maps::graphics_id_status_name(result.status)},
             {"physical_record_index",result.physical_record_index},
             {"evidence",{{"generic_lookup_observed",true},
                          {"registration_observed",result.slot==3 || result.slot==16},
                          {"runtime_to_file_record_observed",result.physical_record_index.has_value()},
                          {"map_to_lookup_dataflow_observed",true},
                          {"sample_value_at_draw_unverified",true},
                          {"corpus_decode_checked",false},
                          {"original_game_visual_match_unverified",true}}},
             {"metadata_status",result.record ?
                 (result.record->width>0 && result.record->height>0 && result.record->data_length>0 ?
                     "nonempty" : "empty") : "not_found"},
             {"payload_status",result.record ?
                 (result.status==maps::GraphicsIdStatus::SourceUnavailable ? "unavailable" :
                     result.status==maps::GraphicsIdStatus::EmptyRecord ? "not_checked" : "available") :
                 "not_checked"},
             {"decode_status","not_attempted"}};
    if (result.record) {
        const auto& record=*result.record;
        const auto found=registrations.find(result.slot);
        if (found!=registrations.end()) {
            out["sg3_version"]=found->second.sg3_version;
            out["sg3_record_stride"]=found->second.sg3_version==213 ? 64 : 72;
            out["runtime_record_count"]=found->second.reported_images_in_use;
            out["sg3_image_capacity"]=found->second.image_capacity;
        }
        out["archive"]=record.id.archive_relative_path.generic_string();
        out["image_index"]=record.id.image_index;
        out["type"]=record.image_type;
        out["kind"]=assets::sg3_image_kind_name(record.image_kind);
        out["width"]=record.width;
        out["height"]=record.height;
        out["data_length"]=record.data_length;
        out["data_offset"]=record.data_offset;
        out["uncompressed_length"]=record.uncompressed_length;
        out["group_id"]=record.group_id;
        out["group_filename"]=record.group_filename;
        out["group_description"]=record.group_description;
        out["asset_id"]={{"archive",record.id.archive_relative_path.generic_string()},
                         {"physical_image_index",record.id.image_index}};
        if (record.sg3_version==213 && result.physical_record_index)
            out["sg3_record_file_offset"]=40680ULL+64ULL*(*result.physical_record_index);
        out["color_bounds"]=assets::asset_range_status_name(record.color_bounds);
        out["alpha_bounds"]=assets::asset_range_status_name(record.alpha_bounds);
    }
    if (decode && result.status==maps::GraphicsIdStatus::DecodeCandidate) {
        try {
            const auto image=assets::load_sg3_image(
                {safe_file(root,result.record->id.archive_relative_path),result.record->id.image_index});
            out["decode_status"]="success";
            out["evidence"]["corpus_decode_checked"]=true;
            out["decoded_width"]=image.width;
            out["decoded_height"]=image.height;
        } catch (const std::exception& error) {
            out["decode_status"]="failed";
            out["evidence"]["corpus_decode_checked"]=true;
            out["decode_error"]=error.what();
        }
    }
    return out;
}

Json cell_json(maps::GridCell cell, const maps::ParsedEmperorMap& map,
               const maps::MapGraphicCandidates& candidates,
               const maps::MapGeometry& geometry, const assets::AssetCatalog& catalog,
               const fs::path& archive_path, const Registrations* registrations,
               const fs::path& root, bool terrain_probe) {
    const auto word = candidates.word_at(cell.x, cell.y);
    const auto byte = candidates.byte_at(cell.x, cell.y);
    const auto interpretation = maps::interpret_terrain(map.terrain_at(cell.x, cell.y),
                                                         map.object_at(cell.x, cell.y));
    Json result{{"storage_x",cell.x},{"storage_y",cell.y},
                {"candidate_mask",geometry.contains(cell)},
                {"candidate_word",word},{"candidate_word_offset",candidates.word_offset(cell.x,cell.y)},
                {"candidate_byte",byte},{"candidate_byte_offset",candidates.byte_offset(cell.x,cell.y)},
                {"terrain_raw",interpretation.terrain_raw},{"objects_raw",interpretation.objects_raw},
                {"terrain_category",maps::category_name(interpretation.category)},
                {"hypothesis","candidate_word_is_direct_index_in_selected_archive"},
                {"resolution",resolution(word,catalog)}};
    if (result["resolution"].value("status","") == "decode_candidate") {
        try {
            const auto image = assets::load_sg3_image({archive_path,word});
            result["resolution"]["decode_status"] = "success";
            result["resolution"]["decoded_width"] = image.width;
            result["resolution"]["decoded_height"] = image.height;
        } catch (const std::exception& error) {
            result["resolution"]["status"] = "decode_failed";
            result["resolution"]["decode_status"] = "failed";
            result["resolution"]["decode_error"] = error.what();
        }
    } else result["resolution"]["decode_status"] = "not_attempted";
    if (registrations) result["graphics_id_hypothesis"] =
        graphics_resolution(word,*registrations,root,true);
    if (registrations) result["graphics_id_hypothesis"]["candidate_word_logical_offset"] =
        candidates.word_offset(cell.x,cell.y);
    if (terrain_probe) {
        const bool simple_ground = interpretation.terrain_raw == 0x80U &&
                                   interpretation.objects_raw == 0;
        result["terrain_selection_probe"] = {
            {"profile",terrain_probe_profile},
            {"status",simple_ground ? "unresolved_post_read_selection" : "outside_simple_ground_case"},
            {"stored_graphic_id",word},
            {"computed_graphic_id",nullptr},
            {"terrain_raw",interpretation.terrain_raw},
            {"objects_raw",interpretation.objects_raw},
            {"adjacent_byte_raw",byte},
            {"stored_id_resolution",result["graphics_id_hypothesis"]},
            {"evidence",{{"observed_path_reset_before_read",true},
                         {"pre_read_range_writer_routine_observed",true},
                         {"cell_in_writer_span_unverified",true},
                         {"saved_graphic_array_read_observed",true},
                         {"post_read_simple_ground_rule_observed",false},
                         {"value_at_first_draw_observed",false},
                         {"native_rule_implemented",false},
                         {"original_game_visual_match_unverified",true}}},
            {"missing_link",simple_ground ?
                "No verified post-read path establishes whether this cell's stored graphic ID is retained or replaced before its first draw; the observed range writer runs before the map read, and the selected image's composition/placement is unknown." :
                "The exact 0x80/0 simple-ground case does not apply to this cell."}
        };
    }
    return result;
}

Json group_query(const fs::path& root, std::uint32_t key, std::uint32_t variant_count,
                 const fs::path& map_relative, std::uint32_t part,
                 const std::vector<maps::GridCell>& requested) {
    const std::uint32_t slot = key >> 9U;
    const fs::path archive_relative = slot == 3U ? "DATA/China_Terrain.sg3" :
                                      slot == 16U ? "DATA/China_Elevation.sg3" : fs::path{};
    Json output{{"profile",group_profile},{"group_key",key},{"slot",slot},
                {"status",maps::group_lookup_status_name(
                    maps::resolve_resource_group({key},{}).status)},
                {"evidence",{{"group_lookup_statically_observed",true},
                             {"sg3_table_value_read",false},
                             {"native_resolution_tested",true},
                             {"selected_image_decodes_checked",false},
                             {"map_correlation_checked",false},
                             {"images_visually_viewed_by_tool",false},
                             {"original_game_execution_observed",false},
                             {"original_game_visual_match_observed",false}}}};
    if (archive_relative.empty()) return output;
    const auto archive_path = safe_file(root,archive_relative);
    const auto archive = assets::read_sg3_archive(archive_path);
    const auto catalog = assets::scan_asset_archive(root,archive_relative);
    const auto resolved = maps::resolve_resource_group({key},{{slot,{&archive}}});
    output["status"] = maps::group_lookup_status_name(resolved.status);
    output["archive"] = archive_relative.generic_string();
    output["runtime_group_position"] = resolved.group_position;
    output["sg3_index_position"] = resolved.sg3_index_position;
    output["sg3_index_file_offset"] = resolved.sg3_file_offset;
    output["sg3_index_raw_value"] = resolved.sg3_raw_value;
    output["runtime_local_base"] = resolved.local_base;
    output["packed_graphic_base"] = resolved.packed_base ?
        Json(resolved.packed_base->value) : Json(nullptr);
    output["index_table_source"] = "SG3 header+index prefix, 300 little-endian uint16 words";
    output["runtime_transform"] = "signed-positive entries, reverse order, local base=raw-1, packed base=slot*16384+local base";
    output["variants"] = Json::array();
    output["map_comparison"] = Json::array();
    if (resolved.status != maps::GroupLookupStatus::Resolved) return output;
    output["evidence"]["sg3_table_value_read"] = true;
    Registrations registrations{{slot,{&catalog,archive.header.version,
                                            archive.header.image_capacity,
                                            archive.header.reported_images_in_use}}};
    for (std::uint32_t variant=0;variant<variant_count;++variant) {
        const auto packed = maps::group_variant(*resolved.packed_base,variant);
        Json item{{"variant",variant},{"packed_graphic_id",packed ? Json(packed->value) : Json(nullptr)}};
        item["image_resolution"] = packed ? graphics_resolution(packed->value,registrations,root,true) :
                                           Json{{"status","variant_out_of_range"}};
        item["image_visually_viewed_by_tool"] = false;
        output["variants"].push_back(std::move(item));
    }
    output["evidence"]["selected_image_decodes_checked"] = variant_count > 0;
    if (!map_relative.empty()) {
        const auto container = maps::EmperorContainer::open(safe_file(root,map_relative));
        const auto map = maps::read_emperor_map(container,part);
        const auto candidates = maps::read_map_graphic_candidates(container,part);
        output["map"] = map_relative.generic_string();
        output["input_byte_logical_offset"] = maps::auxiliary_byte_logical_offset;
        for (const auto cell : requested) {
            const auto index = candidates.cell_index(cell.x,cell.y);
            const auto input = maps::read_auxiliary_map_byte(container,part,cell.x,cell.y);
            const std::uint32_t stored = candidates.word_at(cell.x,cell.y);
            const std::int64_t difference = static_cast<std::int64_t>(stored) -
                                            static_cast<std::int64_t>(resolved.packed_base->value);
            output["map_comparison"].push_back({
                {"storage_x",cell.x},{"storage_y",cell.y},
                {"terrain_raw",map.terrain_at(cell.x,cell.y)},
                {"objects_raw",map.object_at(cell.x,cell.y)},
                {"stored_graphic_id",stored},
                {"candidate_byte_raw",candidates.byte_at(cell.x,cell.y)},
                {"generator_input_byte_raw",input},
                {"generator_input_byte_logical_offset",maps::auxiliary_byte_logical_offset+index},
                {"packed_group_base",resolved.packed_base->value},
                {"signed_difference",difference},
                {"difference_in_0_to_7",difference>=0 && difference<=7},
                {"matches_observed_pre_read_expression",
                    difference==static_cast<std::int64_t>(input & 7U)}
            });
        }
        output["map_comparison_scope"] = "stored data correlation only; pre-read generator is not a post-load rule";
        output["evidence"]["map_correlation_checked"] = !requested.empty();
    }
    return output;
}

Json analyze(const fs::path& root, const fs::path& map_relative, const fs::path& archive_relative,
             std::uint32_t part, const std::vector<maps::GridCell>& requested,
             bool use_graphics_profile, bool terrain_probe) {
    const auto map_path = safe_file(root,map_relative);
    const auto container = maps::EmperorContainer::open(map_path);
    const auto map = maps::read_emperor_map(container,part);
    const auto candidate = maps::read_map_graphic_candidates(container,part);
    const maps::MapGeometry geometry{map.declared_map_size};
    if (!geometry.supported) throw std::runtime_error("candidate mask is unavailable for this map size");
    const auto catalog = assets::scan_asset_archive(root,archive_relative);
    const auto archive_path = safe_file(root,archive_relative);
    std::optional<assets::AssetCatalog> terrain_catalog;
    std::optional<assets::AssetCatalog> elevation_catalog;
    Registrations registrations;
    if (use_graphics_profile) {
        terrain_catalog.emplace(assets::scan_asset_archive(root,"DATA/China_Terrain.sg3"));
        elevation_catalog.emplace(assets::scan_asset_archive(root,"DATA/China_Elevation.sg3"));
        const auto terrain_meta=assets::read_sg3_archive(safe_file(root,"DATA/China_Terrain.sg3"));
        const auto elevation_meta=assets::read_sg3_archive(safe_file(root,"DATA/China_Elevation.sg3"));
        registrations.emplace(3U,maps::GraphicsArchiveRegistration{
            &*terrain_catalog,terrain_meta.header.version,terrain_meta.header.image_capacity,
            terrain_meta.header.reported_images_in_use});
        registrations.emplace(16U,maps::GraphicsArchiveRegistration{
            &*elevation_catalog,elevation_meta.header.version,elevation_meta.header.image_capacity,
            elevation_meta.header.reported_images_in_use});
    }
    Json output{{"map",map_relative.generic_string()},{"part",part},
                {"archive",archive_relative.generic_string()},
                {"hypothesis","candidate_word_is_direct_index_in_selected_archive"},
                {"candidate_word_logical_offset",maps::candidate_word_logical_offset},
                {"candidate_byte_logical_offset",maps::candidate_byte_logical_offset},
                {"cells",cells},{"image_records",catalog.records.size()}};
    Stats inside_word,outside_word,inside_byte,outside_byte;
    std::map<std::string,Stats> by_category;
    std::map<std::string,Stats> by_category_byte;
    std::map<std::pair<std::uint32_t,std::uint32_t>,Stats> by_raw_pair;
    std::map<std::pair<std::uint32_t,std::uint32_t>,Stats> by_raw_pair_byte;
    std::map<std::string,std::uint64_t> direct, shifted;
    std::map<std::string,std::uint64_t> graphics_counts;
    Json examples = Json::array();
    std::set<std::pair<std::uint32_t,std::uint32_t>> selected;
    auto add_example = [&](maps::GridCell cell) {
        if (selected.emplace(cell.x,cell.y).second && selected.size() <= 18)
            examples.push_back(cell_json(cell,map,candidate,geometry,catalog,archive_path,
                                         use_graphics_profile ? &registrations : nullptr,root,
                                         terrain_probe));
    };
    for (const auto cell : requested) add_example(cell);
    for (std::uint32_t y=0;y<maps::stored_grid_height;++y) {
        for (std::uint32_t x=0;x<maps::stored_grid_width;++x) {
            const maps::GridCell cell{x,y};
            const auto i = candidate.cell_index(x,y);
            const auto word = candidate.candidate_word_layer[i];
            const auto byte = candidate.candidate_byte_layer[i];
            if (!geometry.contains(cell)) { outside_word.add(word); outside_byte.add(byte); continue; }
            inside_word.add(word); inside_byte.add(byte);
            const auto terrain=map.terrain_raw.values[i], objects=map.objects_raw.values[i];
            const auto category=maps::interpret_terrain(terrain,objects).category;
            by_category[maps::category_name(category)].add(word);
            by_category_byte[maps::category_name(category)].add(byte);
            by_raw_pair[{terrain,objects}].add(word);
            by_raw_pair_byte[{terrain,objects}].add(byte);
            ++direct[maps::direct_candidate_status_name(
                maps::resolve_direct_candidate(catalog,word).status)];
            const auto control=maps::shifted_control_candidate(word);
            ++shifted[maps::direct_candidate_status_name(control ?
                maps::resolve_direct_candidate(catalog,*control).status :
                maps::DirectCandidateStatus::IndexOutOfRange)];
            if (use_graphics_profile) ++graphics_counts[maps::graphics_id_status_name(
                maps::resolve_graphics_id_hypothesis(word,registrations).status)];
        }
    }
    output["word"]={{"candidate",inside_word.json()},{"outside",outside_word.json()}};
    output["byte"]={{"candidate",inside_byte.json()},{"outside",outside_byte.json()}};
    output["direct_status_counts"]=direct;
    output["control_plus_1024_status_counts"]=shifted;
    if (use_graphics_profile) {
        output["graphics_id_profile"]=graphics_profile;
        output["graphics_id_status_counts"]=graphics_counts;
        output["graphics_id_registration_hypothesis"]={{"3","DATA/China_Terrain.sg3"},
                                                      {"16","DATA/China_Elevation.sg3"}};
        output["graphics_id_index_rule"]="v213 Terrain/Elevation: local i -> physical SG3 record i+1; runtime count is reported_images_in_use";
    }
    if (terrain_probe) {
        output["terrain_selection_profile"]=terrain_probe_profile;
        output["terrain_selection_status"]="unresolved_no_complete_native_rule";
    }
    output["category_word_stats"]=Json::object();
    for (const auto& [category,stats] : by_category)
        output["category_word_stats"][category]=stats.json();
    output["category_byte_stats"]=Json::object();
    for (const auto& [category,stats] : by_category_byte)
        output["category_byte_stats"][category]=stats.json();
    Json pair_summary=Json::array();
    for (const auto& [pair,stats] : by_raw_pair)
        pair_summary.push_back({{"terrain_raw",pair.first},{"objects_raw",pair.second},
                                {"count",stats.json().at("count")},{"word_distinct",stats.frequency.size()},
                                {"word_top",stats.json().at("top")},
                                {"byte_distinct",by_raw_pair_byte.at(pair).frequency.size()},
                                {"byte_top",by_raw_pair_byte.at(pair).json().at("top")}});
    std::sort(pair_summary.begin(),pair_summary.end(),[](const Json& a,const Json& b) {
        return a.at("count").get<std::uint64_t>() > b.at("count").get<std::uint64_t>();
    });
    if (pair_summary.size()>16) pair_summary.erase(pair_summary.begin()+16,pair_summary.end());
    output["top_raw_pairs"]=pair_summary;
    // Scramble candidate cells only: a fixed bijection for all three observed
    // candidate counts. It preserves the word histogram but breaks placement.
    std::vector<std::size_t> candidate_indices;
    for (std::uint32_t y=0;y<maps::stored_grid_height;++y)
        for (std::uint32_t x=0;x<maps::stored_grid_width;++x)
            if (geometry.contains({x,y})) candidate_indices.push_back(candidate.cell_index(x,y));
    std::vector<std::uint32_t> control_words, control_categories;
    control_words.reserve(candidate_indices.size());
    control_categories.reserve(candidate_indices.size());
    for (const auto at : candidate_indices) {
        control_words.push_back(candidate.candidate_word_layer[at]);
        control_categories.push_back(static_cast<std::uint32_t>(maps::interpret_terrain(
            map.terrain_raw.values[at],map.objects_raw.values[at]).category));
    }
    const auto control = maps::compare_candidate_structure(control_words,control_categories);
    output["spatial_control"]={{"permutation","candidate_position=(position*11+1)%candidate_count"},
                               {"direct_word_category_majority_cells",control.direct_majority_cells},
                               {"permuted_word_category_majority_cells",control.permuted_majority_cells},
                               {"denominator",control.denominator}};

    auto example_with_reason = [&](maps::GridCell cell, std::string reason) {
        if (selected.emplace(cell.x,cell.y).second && selected.size() <= 18) {
            auto item=cell_json(cell,map,candidate,geometry,catalog,archive_path,
                                use_graphics_profile ? &registrations : nullptr,root,
                                terrain_probe);
            item["sample_reason"]=std::move(reason);
            examples.push_back(std::move(item));
        }
    };
    example_with_reason({0,0},"outside mask; inspect raw zero and SG3 record zero");
    std::set<std::string> categories_seen;
    std::optional<std::uint32_t> first_fertile_word;
    bool paired_fertile=false, adjacent_ground=false, water_interior=false, water_edge=false;
    for (std::uint32_t y=0;y<maps::stored_grid_height;++y) {
        for (std::uint32_t x=0;x<maps::stored_grid_width;++x) {
            const maps::GridCell cell{x,y};
            if (!geometry.contains(cell)) continue;
            const auto i=candidate.cell_index(x,y);
            const auto name=std::string{maps::category_name(maps::interpret_terrain(
                map.terrain_raw.values[i],map.objects_raw.values[i]).category)};
            if (categories_seen.insert(name).second)
                example_with_reason(cell,"first candidate in category "+name);
            if (map.terrain_raw.values[i]==128 && map.objects_raw.values[i]==0) {
                if (!first_fertile_word) first_fertile_word=candidate.candidate_word_layer[i];
                else if (!paired_fertile && candidate.candidate_word_layer[i]!=*first_fertile_word) {
                    example_with_reason(cell,"same raw pair (128,0), different candidate word");
                    paired_fertile=true;
                }
                if (!adjacent_ground && x+1<maps::stored_grid_width) {
                    const auto right=i+1;
                    if (map.terrain_raw.values[right]==128 && map.objects_raw.values[right]==0) {
                        example_with_reason(cell,"adjacent pair (128,0) ground cells");
                        example_with_reason({x+1,y},"adjacent pair (128,0) ground cells");
                        adjacent_ground=true;
                    }
                }
            }
            if (name=="water") {
                unsigned neighbors=0;
                for (const auto [dx,dy] : {std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}}) {
                    const int nx=static_cast<int>(x)+dx, ny=static_cast<int>(y)+dy;
                    if (nx<0 || ny<0 || nx>=static_cast<int>(maps::stored_grid_width) ||
                        ny>=static_cast<int>(maps::stored_grid_height)) continue;
                    const auto ni=candidate.cell_index(static_cast<std::uint32_t>(nx),
                                                       static_cast<std::uint32_t>(ny));
                    if (geometry.contains({static_cast<std::uint32_t>(nx),static_cast<std::uint32_t>(ny)}) &&
                        maps::interpret_terrain(map.terrain_raw.values[ni],
                            map.objects_raw.values[ni]).category==maps::TerrainCategory::Water) ++neighbors;
                }
                if (neighbors==4 && !water_interior) {
                    example_with_reason(cell,"water-category interior by four-neighbor diagnostic");
                    water_interior=true;
                } else if (neighbors<4 && !water_edge) {
                    example_with_reason(cell,"water-category edge by four-neighbor diagnostic");
                    water_edge=true;
                }
            }
        }
    }
    output["selected_cells"]=examples;
    return output;
}
}

int main(int argc,char* argv[]) {
    try {
        fs::path root,map,archive="DATA/China_Terrain.sg3";
        std::uint32_t part=0;
        bool use_graphics_profile=false;
        bool terrain_probe=false;
        std::optional<std::uint32_t> group_key;
        std::uint32_t variant_count=8;
        bool variants_supplied=false;
        std::vector<maps::GridCell> cells_to_show;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            if (arg=="--data" && i+1<argc) root=argv[++i];
            else if (arg=="--map" && i+1<argc) map=argv[++i];
            else if (arg=="--archive" && i+1<argc) archive=argv[++i];
            else if (arg=="--part" && i+1<argc) part=number(argv[++i]);
            else if (arg=="--group-key" && i+1<argc) group_key=key_number(argv[++i]);
            else if (arg=="--variants" && i+1<argc) {
                variant_count=number(argv[++i]);
                variants_supplied=true;
                if (variant_count>8) throw std::invalid_argument("--variants must be between 0 and 8");
            }
            else if (arg=="--profile" && i+1<argc) {
                if (std::string_view{argv[++i]} != graphics_profile)
                    throw std::invalid_argument("unknown graphics profile");
                use_graphics_profile=true;
            }
            else if (arg=="--terrain-selection-profile" && i+1<argc) {
                if (std::string_view{argv[++i]} != terrain_probe_profile)
                    throw std::invalid_argument("unknown terrain-selection profile");
                use_graphics_profile=true;
                terrain_probe=true;
            }
            else if (arg=="--cell" && i+2<argc) {
                const auto x=number(argv[++i]), y=number(argv[++i]);
                if (x>=maps::stored_grid_width || y>=maps::stored_grid_height)
                    throw std::invalid_argument("cell is outside 228x228");
                cells_to_show.push_back({x,y});
            } else throw std::invalid_argument("usage: openemperor-map-graphics --data <root> [--group-key 0x603 --variants 8 [--map <relative.map> --cell x y]...] | --map <relative.map> [--part N] [--archive <relative.sg3>] [--profile exe-6373328b-14bit-hypothesis | --terrain-selection-profile exe-6373328b-first-terrain-probe] [--cell x y]...");
        }
        if (root.empty()) throw std::invalid_argument("--data is required");
        if (group_key) {
            if (use_graphics_profile || terrain_probe || archive != "DATA/China_Terrain.sg3")
                throw std::invalid_argument("group query cannot be combined with another graphics profile or --archive");
            if (map.empty() && !cells_to_show.empty())
                throw std::invalid_argument("--cell requires --map in group query mode");
            std::cout << group_query(fs::canonical(root),*group_key,variant_count,
                                     map,part,cells_to_show).dump(2) << '\n';
            return 0;
        }
        if (variants_supplied) throw std::invalid_argument("--variants requires --group-key");
        if (map.empty()) throw std::invalid_argument("--map is required");
        std::cout << analyze(fs::canonical(root),map,archive,part,cells_to_show,
                             use_graphics_profile,terrain_probe).dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Map graphics candidate inspection failed: " << error.what() << '\n';
        return 1;
    }
}
