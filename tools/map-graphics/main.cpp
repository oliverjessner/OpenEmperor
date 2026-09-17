#include "assets/AssetCatalog.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapGeometry.h"
#include "maps/MapGraphicCandidates.h"
#include "maps/DirectGraphicCandidate.h"
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

Json cell_json(maps::GridCell cell, const maps::ParsedEmperorMap& map,
               const maps::MapGraphicCandidates& candidates,
               const maps::MapGeometry& geometry, const assets::AssetCatalog& catalog,
               const fs::path& archive_path) {
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
    return result;
}

Json analyze(const fs::path& root, const fs::path& map_relative, const fs::path& archive_relative,
             std::uint32_t part, const std::vector<maps::GridCell>& requested) {
    const auto map_path = safe_file(root,map_relative);
    const auto container = maps::EmperorContainer::open(map_path);
    const auto map = maps::read_emperor_map(container,part);
    const auto candidate = maps::read_map_graphic_candidates(container,part);
    const maps::MapGeometry geometry{map.declared_map_size};
    if (!geometry.supported) throw std::runtime_error("candidate mask is unavailable for this map size");
    const auto catalog = assets::scan_asset_archive(root,archive_relative);
    const auto archive_path = safe_file(root,archive_relative);
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
    Json examples = Json::array();
    std::set<std::pair<std::uint32_t,std::uint32_t>> selected;
    auto add_example = [&](maps::GridCell cell) {
        if (selected.emplace(cell.x,cell.y).second && selected.size() <= 18)
            examples.push_back(cell_json(cell,map,candidate,geometry,catalog,archive_path));
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
        }
    }
    output["word"]={{"candidate",inside_word.json()},{"outside",outside_word.json()}};
    output["byte"]={{"candidate",inside_byte.json()},{"outside",outside_byte.json()}};
    output["direct_status_counts"]=direct;
    output["control_plus_1024_status_counts"]=shifted;
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
            auto item=cell_json(cell,map,candidate,geometry,catalog,archive_path);
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
        std::vector<maps::GridCell> cells_to_show;
        for (int i=1;i<argc;++i) {
            const std::string_view arg=argv[i];
            if (arg=="--data" && i+1<argc) root=argv[++i];
            else if (arg=="--map" && i+1<argc) map=argv[++i];
            else if (arg=="--archive" && i+1<argc) archive=argv[++i];
            else if (arg=="--part" && i+1<argc) part=number(argv[++i]);
            else if (arg=="--cell" && i+2<argc) {
                const auto x=number(argv[++i]), y=number(argv[++i]);
                if (x>=maps::stored_grid_width || y>=maps::stored_grid_height)
                    throw std::invalid_argument("cell is outside 228x228");
                cells_to_show.push_back({x,y});
            } else throw std::invalid_argument("usage: openemperor-map-graphics --data <root> --map <relative.map> [--part N] [--archive <relative.sg3>] [--cell x y]...");
        }
        if (root.empty() || map.empty()) throw std::invalid_argument("--data and --map are required");
        std::cout << analyze(fs::canonical(root),map,archive,part,cells_to_show).dump(2) << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Map graphics candidate inspection failed: " << error.what() << '\n';
        return 1;
    }
}
