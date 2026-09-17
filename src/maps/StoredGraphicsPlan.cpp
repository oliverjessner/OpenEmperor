#include "maps/StoredGraphicsPlan.h"

#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace openemperor::maps {
namespace {
namespace fs = std::filesystem;
bool under(const fs::path& root, const fs::path& path) {
    const auto relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& part : relative) if (part == "..") return false;
    return true;
}

void require_under(const fs::path& root, const fs::path& path) {
    std::error_code error;
    const auto resolved = fs::weakly_canonical(path,error);
    if (error || !under(root,resolved))
        throw std::runtime_error("stored graphics SG3/.555 path escapes the data directory");
}

StoredStatus from_resolution(GraphicsIdStatus status) {
    switch (status) {
    case GraphicsIdStatus::UnsupportedHighBit: return StoredStatus::UnsupportedHighBit;
    case GraphicsIdStatus::UnregisteredSlot: return StoredStatus::UnregisteredSlot;
    case GraphicsIdStatus::UnverifiedRegistration: return StoredStatus::UnverifiedRegistration;
    case GraphicsIdStatus::IndexOutOfRange: return StoredStatus::IndexOutOfRange;
    case GraphicsIdStatus::EmptyRecord: return StoredStatus::EmptyRecord;
    case GraphicsIdStatus::SourceUnavailable: return StoredStatus::SourceUnavailable;
    case GraphicsIdStatus::UnsupportedLayout: return StoredStatus::UnsupportedLayout;
    case GraphicsIdStatus::DecodeCandidate: return StoredStatus::DecodePending;
    }
    return StoredStatus::UnsupportedLayout;
}

StoredStatus supported_footprint(const assets::AssetRecord& record) {
    if (record.image_type != 30) return StoredStatus::UnsupportedLayout;
    if (record.width > 78 || record.uncompressed_length > 3200 ||
        record.isometric_size_flag > 1) return StoredStatus::MultiTilePlacementUnverified;
    if (record.width != 78 || record.uncompressed_length != 3200 || record.height < 40)
        return StoredStatus::UnsupportedLayout;
    if (record.horizontal_mirror_offset != 0) return StoredStatus::MirrorUnverified;
    return StoredStatus::DecodePending;
}
} // namespace

std::filesystem::path validate_stored_archive_sources(
    const fs::path& data_root, const fs::path& archive_relative) {
    std::error_code error;
    const auto root = fs::canonical(data_root,error);
    if (error || !fs::is_directory(root))
        throw std::runtime_error("stored graphics data directory cannot be resolved");
    if (archive_relative.is_absolute())
        throw std::runtime_error("stored graphics archive must be relative to data directory");
    const auto archive_path = fs::canonical(root/archive_relative,error);
    if (error || !under(root,archive_path) || !fs::is_regular_file(archive_path))
        throw std::runtime_error("stored graphics required archive is missing or escapes data directory: " +
                                 archive_relative.generic_string());
    const auto archive = assets::read_sg3_archive(archive_path);
    auto internal = archive_path;
    internal.replace_extension(".555");
    require_under(root,internal);
    for (std::size_t i=0;i<archive.groups.size();++i) {
        const auto location = assets::resolve_sg3_group_bitmap(archive_path,archive.groups[i],i);
        if (location.status == assets::Sg3BitmapStatus::UnsafeRelativeName)
            throw std::runtime_error("stored graphics has unsafe external .555 name");
        if (location.status == assets::Sg3BitmapStatus::Resolved)
            require_under(root,location.path);
    }
    return archive_path;
}

const char* stored_status_name(StoredStatus status) {
    switch (status) {
    case StoredStatus::Excluded: return "excluded";
    case StoredStatus::DecodePending: return "decode_pending";
    case StoredStatus::Rendered: return "rendered";
    case StoredStatus::UnsupportedHighBit: return "unsupported_high_bit";
    case StoredStatus::UnregisteredSlot: return "unregistered_slot";
    case StoredStatus::UnverifiedRegistration: return "unverified_registration";
    case StoredStatus::IndexOutOfRange: return "index_out_of_range";
    case StoredStatus::EmptyRecord: return "empty_record";
    case StoredStatus::SourceUnavailable: return "source_unavailable";
    case StoredStatus::UnsupportedLayout: return "unsupported_layout";
    case StoredStatus::MultiTilePlacementUnverified: return "multi_tile_placement_unverified";
    case StoredStatus::MirrorUnverified: return "mirror_unverified";
    case StoredStatus::DecodeFailed: return "decode_failed";
    }
    return "invalid_status";
}

scene::Point stored_image_origin(scene::Point world, std::uint32_t width, std::uint32_t height) {
    return {world.x - static_cast<double>(width) / 2.0,
            world.y - (static_cast<double>(height) - 40.0)};
}

bool stored_rect_visible(scene::Point origin, std::uint32_t width, std::uint32_t height,
                         const scene::Camera2D& camera) {
    const auto top_left = camera.world_to_screen(origin);
    return top_left.x + width * camera.zoom > 0 && top_left.y + height * camera.zoom > 0 &&
           top_left.x < camera.viewport_width && top_left.y < camera.viewport_height;
}

const StoredCell* StoredGraphicsPlan::at(GridCell cell) const {
    if (cell.x >= stored_grid_width || cell.y >= stored_grid_height) return nullptr;
    const auto index = cell_by_storage[static_cast<std::size_t>(cell.y) * stored_grid_width + cell.x];
    return index ? &cells[*index] : nullptr;
}

std::map<std::string,std::size_t> StoredGraphicsPlan::status_counts() const {
    std::map<std::string,std::size_t> counts;
    for (const auto& cell : cells) ++counts[stored_status_name(cell.status)];
    return counts;
}

StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout) {
    constexpr auto storage_count = static_cast<std::size_t>(stored_grid_width) * stored_grid_height;
    if (!geometry.supported || geometry.candidate.size() != storage_count ||
        map.terrain_raw.values.size() != storage_count || map.objects_raw.values.size() != storage_count ||
        candidates.candidate_word_layer.size() != storage_count ||
        candidates.candidate_byte_layer.size() != storage_count)
        throw std::invalid_argument("stored graphics require complete supported map grids");
    if (terrain.data_root.empty() || terrain.data_root != elevation.data_root ||
        terrain_layout.slot != 3 || elevation_layout.slot != 16)
        throw std::invalid_argument("stored graphics require verified Terrain/Elevation registrations");
    StoredGraphicsPlan plan;
    plan.data_root = terrain.data_root;
    plan.border = geometry.border;
    plan.mask_comparison = compare_masks(map,geometry);
    plan.status_by_storage.assign(storage_count, StoredStatus::Excluded);
    plan.cell_by_storage.resize(storage_count);
    const std::map<std::uint32_t,GraphicsArchiveRegistration> registrations{
        {3,{&terrain,&terrain_layout}}, {16,{&elevation,&elevation_layout}}};
    std::map<std::tuple<std::string,std::uint32_t>,std::size_t> distinct;
    for (std::uint32_t y=0; y<stored_grid_height; ++y) {
        for (std::uint32_t x=0; x<stored_grid_width; ++x) {
            const GridCell storage{x,y};
            const auto index = candidates.cell_index(x,y);
            if (!geometry.contains(storage)) { ++plan.excluded; continue; }
            StoredCell cell;
            cell.storage = storage;
            cell.cell_index = index;
            cell.stored_id = candidates.word_at(x,y);
            cell.logical_offset = candidates.word_offset(x,y);
            cell.terrain_raw = map.terrain_at(x,y);
            cell.objects_raw = map.object_at(x,y);
            cell.candidate_byte = candidates.byte_at(x,y);
            cell.offmap_bit = (cell.terrain_raw & 0x80000U) != 0;
            cell.world = terrain_world(storage,geometry.border);
            cell.image_origin = terrain_image_origin(cell.world);
            const auto resolved = resolve_graphics_id_hypothesis(cell.stored_id,registrations);
            cell.slot = resolved.slot;
            cell.local_index = resolved.local_index;
            cell.physical_record = resolved.physical_record_index;
            cell.lookup_status = resolved.status;
            cell.record_present = resolved.record != nullptr;
            if (resolved.slot == 3) cell.system_record_skip = terrain_layout.system_record_skip;
            if (resolved.slot == 16) cell.system_record_skip = elevation_layout.system_record_skip;
            cell.status = from_resolution(resolved.status);
            if (resolved.record) {
                const auto& record = *resolved.record;
                cell.source_ranges_valid = record.color_bounds == assets::AssetRangeStatus::InBounds &&
                    (record.alpha_bounds == assets::AssetRangeStatus::InBounds ||
                     record.alpha_bounds == assets::AssetRangeStatus::NotPresent);
                const auto key = std::make_tuple(record.id.archive_relative_path.generic_string(),
                                                 record.id.image_index);
                auto [it, inserted] = distinct.try_emplace(key,plan.assets.size());
                if (inserted) {
                    if (plan.assets.size() >= stored_max_assets)
                        throw std::runtime_error("stored graphics exceeds 2048 distinct referenced assets");
                    plan.assets.push_back({record,cell.status,false,false,{}});
                }
                cell.asset_index = it->second;
                if (cell.status == StoredStatus::DecodePending) {
                    cell.status = supported_footprint(record);
                    cell.footprint_supported = cell.status == StoredStatus::DecodePending;
                    if (inserted) plan.assets.back().status = cell.status;
                    if (cell.status == StoredStatus::DecodePending)
                        cell.image_origin = stored_image_origin(cell.world,
                            static_cast<std::uint32_t>(record.width),
                            static_cast<std::uint32_t>(record.height));
                }
            }
            plan.status_by_storage[index] = cell.status;
            plan.cells.push_back(std::move(cell));
        }
    }
    std::sort(plan.cells.begin(),plan.cells.end(),[](const StoredCell& a,const StoredCell& b) {
        if (a.world.y != b.world.y) return a.world.y < b.world.y;
        if (a.world.x != b.world.x) return a.world.x < b.world.x;
        return a.cell_index < b.cell_index;
    });
    for (std::size_t i=0;i<plan.cells.size();++i)
        plan.cell_by_storage[plan.cells[i].cell_index] = i;
    if (plan.cells.size() + plan.excluded != storage_count)
        throw std::logic_error("stored graphics candidate accounting mismatch");
    return plan;
}

} // namespace openemperor::maps
