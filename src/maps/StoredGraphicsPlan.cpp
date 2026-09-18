#include "maps/StoredGraphicsPlan.h"

#include "assets/Sg3ImageLoader.h"

#include <algorithm>
#include <array>
#include <deque>
#include <set>
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
    case GraphicsIdStatus::ArchiveMissing: return StoredStatus::ArchiveMissing;
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

bool square_geometry(const assets::AssetRecord& record, std::uint32_t side) {
    if (side != 1 && side != 2 && side != 4) return false;
    const auto n=static_cast<std::uint64_t>(side);
    const auto width=80U*n-2U, height=40U*n, bytes=3200U*n*n;
    return record.image_type==30 && record.width>0 && record.height>0 &&
           static_cast<std::uint64_t>(record.width)==width &&
           static_cast<std::uint64_t>(record.height)>=height &&
           record.uncompressed_length==bytes && record.isometric_size_flag==side &&
           record.horizontal_mirror_offset==0;
}

bool two_by_two_geometry(const assets::AssetRecord& record) {
    return square_geometry(record,2);
}

void set_status(StoredGraphicsPlan& plan, std::size_t member, StoredStatus status) {
    plan.cells[member].status=status;
    plan.status_by_storage[plan.cells[member].cell_index]=status;
}

void place_edge_byte_footprints(StoredGraphicsPlan& plan) {
    using Key = std::tuple<std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::size_t>;
    std::map<Key,std::vector<std::size_t>> groups;
    const bool extended=plan.footprint_policy==FootprintPolicy::EdgeByte4x4Preview;
    for (std::size_t i=0;i<plan.cells.size();++i) {
        auto& cell=plan.cells[i];
        if (cell.status!=StoredStatus::MultiTilePlacementUnverified || !cell.asset_index) continue;
        const auto& record=plan.assets[*cell.asset_index].record;
        const std::uint32_t side=record.isometric_size_flag==4 && extended ? 4U : 2U;
        if (!square_geometry(record,side)) {
            set_status(plan,i,record.horizontal_mirror_offset ? StoredStatus::MirrorUnverified :
                       StoredStatus::UnsupportedFootprintSize);
            continue;
        }
        const auto metadata=decode_map_subtile_byte(cell.candidate_byte);
        cell.subtile=metadata;
        if (metadata.unknown_bits) ++plan.unknown_bit_cells;
        if (metadata.part_x>=side || metadata.part_y>=side) {
            set_status(plan,i,StoredStatus::SubtilePositionInvalid);
            continue;
        }
        cell.subtile_origin=map_subtile_origin(cell.storage,metadata);
        if (!cell.subtile_origin) {
            set_status(plan,i,StoredStatus::SubtilePositionInvalid);
            continue;
        }
        groups[{cell.subtile_origin->x,cell.subtile_origin->y,side,cell.stored_id,*cell.asset_index}].push_back(i);
    }
    struct CheckedGroup { Key key; std::vector<std::size_t> members; StoredStatus issue=StoredStatus::DecodePending; };
    std::vector<CheckedGroup> checked;
    for (const auto& [key,members] : groups) {
        const auto [ox,oy,side,id,asset]=key;
        (void)id; (void)asset;
        CheckedGroup group{key,members};
        std::set<std::pair<std::uint8_t,std::uint8_t>> parts;
        if (members.size()!=static_cast<std::size_t>(side)*side)
            group.issue=StoredStatus::IncompleteFootprint;
        for (const auto i:members) {
            const auto& sub=*plan.cells[i].subtile;
            if (!parts.emplace(sub.part_x,sub.part_y).second)
                group.issue=StoredStatus::ConflictingFootprint;
        }
        for (std::uint32_t dy=0;dy<side;++dy) for (std::uint32_t dx=0;dx<side;++dx) {
            if (static_cast<std::uint64_t>(ox)+dx>=stored_grid_width ||
                static_cast<std::uint64_t>(oy)+dy>=stored_grid_height) {
                group.issue=StoredStatus::AnchorUnresolved;
                continue;
            }
            const auto raw=static_cast<std::size_t>(oy+dy)*stored_grid_width+ox+dx;
            if (!plan.cell_by_storage[raw]) {
                group.issue=StoredStatus::AnchorUnresolved;
                continue;
            }
            const auto other=*plan.cell_by_storage[raw];
            if (std::find(members.begin(),members.end(),other)==members.end())
                group.issue=StoredStatus::ConflictingFootprint;
        }
        checked.push_back(std::move(group));
    }
    // Validate all claims before assigning ownership. A cell claimed by a
    // different origin invalidates both groups, independent of visit order.
    std::vector<std::optional<std::size_t>> group_by_member(plan.cells.size());
    for (std::size_t group_index=0;group_index<checked.size();++group_index)
        for (const auto member:checked[group_index].members)
            group_by_member[member]=group_index;
    for (std::size_t group_index=0;group_index<checked.size();++group_index) {
        auto& group=checked[group_index];
        const auto [ox,oy,side,id,asset]=group.key;
        (void)id; (void)asset;
        for (std::uint32_t dy=0;dy<side;++dy) for (std::uint32_t dx=0;dx<side;++dx) {
            if (static_cast<std::uint64_t>(ox)+dx>=stored_grid_width ||
                static_cast<std::uint64_t>(oy)+dy>=stored_grid_height) continue;
            const auto other=plan.cell_by_storage[static_cast<std::size_t>(oy+dy)*stored_grid_width+ox+dx];
            if (!other) continue;
            if (group_by_member[*other] && *group_by_member[*other]!=group_index) {
                group.issue=StoredStatus::ConflictingFootprint;
                checked[*group_by_member[*other]].issue=StoredStatus::ConflictingFootprint;
            }
        }
    }
    for (const auto& group:checked) {
        if (group.issue!=StoredStatus::DecodePending) {
            for (const auto i:group.members) set_status(plan,i,group.issue);
            continue;
        }
        const auto [ox,oy,side,id,asset]=group.key;
        (void)id;
        PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=asset;
        footprint.origin={ox,oy};
        footprint.width_cells=side; footprint.height_cells=side;
        footprint.rule=side==4 ? "edge_byte_4x4_origin_preview" : "edge_byte_origin_preview";
        footprint.cell_indices=group.members;
        const auto& record=plan.assets[asset].record;
        plan.assets[asset].status=StoredStatus::DecodePending;
        footprint.image_origin=stored_square_image_origin(
            terrain_world(footprint.origin,plan.border),static_cast<std::uint32_t>(record.width),
            static_cast<std::uint32_t>(record.height),side);
        std::size_t markers=0;
        bool expected_marker=false;
        for (const auto i:group.members) {
            const auto& cell=plan.cells[i];
            if (cell.subtile->draw_marker_candidate) {
                ++markers;
                footprint.draw_cell_candidate=cell.storage;
                expected_marker=cell.subtile->part_x==0 && cell.subtile->part_y==side-1;
            }
        }
        if (markers!=1 || !expected_marker) ++plan.marker_deviations;
        if (markers!=1) footprint.draw_cell_candidate.reset();
        for (const auto i:group.members) {
            auto& cell=plan.cells[i];
            cell.footprint_index=footprint.id;
            cell.footprint_supported=true;
            cell.image_origin=footprint.image_origin;
            set_status(plan,i,StoredStatus::DecodePending);
        }
        plan.footprints.push_back(std::move(footprint));
    }
}

void place_footprints(StoredGraphicsPlan& plan, const MapGraphicCandidates& candidates) {
    if (plan.footprint_policy==FootprintPolicy::EdgeBytePreview ||
        plan.footprint_policy==FootprintPolicy::EdgeByte4x4Preview)
        place_edge_byte_footprints(plan);
    // A two-cell component is never expanded across a mask boundary. Only an
    // isolated, complete 2x2 component of identical saved IDs is a preview
    // placement; neither candidate_byte nor an original draw anchor is inferred.
    std::vector<bool> visited(plan.cells.size(),false);
    for (std::size_t i=0;plan.footprint_policy==FootprintPolicy::IsolatedPreview && i<plan.cells.size();++i) {
        auto& cell=plan.cells[i];
        if (cell.status != StoredStatus::MultiTilePlacementUnverified || !cell.asset_index) continue;
        const auto& record=plan.assets[*cell.asset_index].record;
        if (!two_by_two_geometry(record)) {
            cell.status = record.horizontal_mirror_offset ? StoredStatus::MirrorUnverified :
                          StoredStatus::UnsupportedFootprintSize;
            plan.status_by_storage[cell.cell_index]=cell.status;
            continue;
        }
        if (visited[i]) continue;
        std::vector<std::size_t> component;
        std::deque<std::size_t> pending{i};
        visited[i]=true;
        while (!pending.empty()) {
            const auto current=pending.front(); pending.pop_front();
            component.push_back(current);
            const auto& here=plan.cells[current];
            const auto x=static_cast<int>(here.storage.x), y=static_cast<int>(here.storage.y);
            for (const auto [dx,dy] : std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}) {
                const int nx=x+dx, ny=y+dy;
                if (nx<0 || ny<0 || nx>=static_cast<int>(stored_grid_width) ||
                    ny>=static_cast<int>(stored_grid_height)) continue;
                const auto other=plan.cell_by_storage[static_cast<std::size_t>(ny)*stored_grid_width+
                                                      static_cast<std::size_t>(nx)];
                if (!other || visited[*other]) continue;
                const auto& next=plan.cells[*other];
                if (next.status==StoredStatus::MultiTilePlacementUnverified &&
                    next.asset_index==here.asset_index && next.stored_id==here.stored_id) {
                    visited[*other]=true; pending.push_back(*other);
                }
            }
        }
        std::uint32_t min_x=stored_grid_width,min_y=stored_grid_height,max_x=0,max_y=0;
        for (const auto member : component) {
            const auto p=plan.cells[member].storage;
            min_x=std::min(min_x,p.x); min_y=std::min(min_y,p.y);
            max_x=std::max(max_x,p.x); max_y=std::max(max_y,p.y);
        }
        const bool exact=component.size()==4 && max_x==min_x+1 && max_y==min_y+1;
        if (!exact) {
            const auto reason=component.size()<4 ? StoredStatus::IncompleteFootprint :
                              StoredStatus::AmbiguousFootprint;
            for (const auto member : component) {
                plan.cells[member].status=reason;
                plan.status_by_storage[plan.cells[member].cell_index]=reason;
            }
            continue;
        }
        bool crosses_mask=false;
        for (const auto member : component) {
            const auto p=plan.cells[member].storage;
            for (const auto [dx,dy] : std::array<std::pair<int,int>,4>{{{1,0},{-1,0},{0,1},{0,-1}}}) {
                const int nx=static_cast<int>(p.x)+dx, ny=static_cast<int>(p.y)+dy;
                if (nx<0 || ny<0 || nx>=static_cast<int>(stored_grid_width) ||
                    ny>=static_cast<int>(stored_grid_height)) continue;
                const auto raw_index=static_cast<std::size_t>(ny)*stored_grid_width+
                                     static_cast<std::size_t>(nx);
                if (!plan.cell_by_storage[raw_index] &&
                    candidates.candidate_word_layer[raw_index]==cell.stored_id) crosses_mask=true;
            }
        }
        if (crosses_mask) {
            for (const auto member : component) {
                plan.cells[member].status=StoredStatus::AnchorUnresolved;
                plan.status_by_storage[plan.cells[member].cell_index]=StoredStatus::AnchorUnresolved;
            }
            continue;
        }
        PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=*cell.asset_index;
        footprint.origin={min_x,min_y};
        footprint.width_cells=2; footprint.height_cells=2;
        footprint.rule="isolated_complete_2x2_preview";
        footprint.cell_indices=component;
        footprint.image_origin=stored_two_by_two_image_origin(
            terrain_world(footprint.origin,plan.border),static_cast<std::uint32_t>(record.width),
            static_cast<std::uint32_t>(record.height));
        plan.assets[footprint.asset_index].status=StoredStatus::DecodePending;
        for (const auto member : component) {
            auto& owned=plan.cells[member];
            owned.footprint_index=footprint.id;
            owned.footprint_supported=true;
            owned.status=StoredStatus::DecodePending;
            owned.image_origin=footprint.image_origin;
            plan.status_by_storage[owned.cell_index]=owned.status;
        }
        plan.footprints.push_back(std::move(footprint));
    }
    for (std::size_t i=0;i<plan.cells.size();++i) {
        auto& cell=plan.cells[i];
        if (cell.status!=StoredStatus::DecodePending || cell.footprint_index || !cell.asset_index) continue;
        PlacedFootprint footprint;
        footprint.id=plan.footprints.size();
        footprint.asset_index=*cell.asset_index;
        footprint.origin=cell.storage;
        footprint.cell_indices={i};
        footprint.image_origin=cell.image_origin;
        cell.footprint_index=footprint.id;
        plan.footprints.push_back(std::move(footprint));
    }
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
    case StoredStatus::ArchiveMissing: return "archive_missing";
    case StoredStatus::IndexOutOfRange: return "index_out_of_range";
    case StoredStatus::EmptyRecord: return "empty_record";
    case StoredStatus::SourceUnavailable: return "source_unavailable";
    case StoredStatus::UnsupportedLayout: return "unsupported_layout";
    case StoredStatus::MultiTilePlacementUnverified: return "multi_tile_placement_unverified";
    case StoredStatus::AmbiguousFootprint: return "ambiguous_footprint";
    case StoredStatus::IncompleteFootprint: return "incomplete_footprint";
    case StoredStatus::AnchorUnresolved: return "anchor_unresolved";
    case StoredStatus::UnsupportedFootprintSize: return "unsupported_footprint_size";
    case StoredStatus::MirrorUnverified: return "mirror_unverified";
    case StoredStatus::DecodeFailed: return "decode_failed";
    case StoredStatus::SubtilePositionInvalid: return "subtile_position_invalid";
    case StoredStatus::ConflictingFootprint: return "conflicting_footprint";
    }
    return "invalid_status";
}

const char* footprint_policy_name(FootprintPolicy policy) {
    switch (policy) {
    case FootprintPolicy::Disabled: return "disabled";
    case FootprintPolicy::IsolatedPreview: return "isolated";
    case FootprintPolicy::EdgeBytePreview: return "edge-byte";
    case FootprintPolicy::EdgeByte4x4Preview: return "edge-byte-4x4";
    }
    return "invalid";
}

scene::Point stored_image_origin(scene::Point world, std::uint32_t width, std::uint32_t height) {
    return {world.x - static_cast<double>(width) / 2.0,
            world.y - (static_cast<double>(height) - 40.0)};
}

scene::Point stored_two_by_two_image_origin(scene::Point rear_world, std::uint32_t width,
                                            std::uint32_t height) {
    return stored_square_image_origin(rear_world,width,height,2);
}

scene::Point stored_square_image_origin(scene::Point rear_world, std::uint32_t width,
                                        std::uint32_t height, std::uint32_t side) {
    if (side!=1 && side!=2 && side!=4)
        throw std::invalid_argument("unsupported stored graphics footprint side");
    return {rear_world.x-static_cast<double>(width)/2.0,
            rear_world.y-(static_cast<double>(height)-40.0*side)};
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
std::size_t StoredGraphicsPlan::covered_cells() const {
    std::size_t count=0;
    for (const auto& cell:cells) {
        if (!cell.footprint_index || *cell.footprint_index>=footprints.size()) continue;
        const auto status=footprints[*cell.footprint_index].status;
        if ((status==StoredStatus::DecodePending || status==StoredStatus::Rendered) &&
            cell.status==status) ++count;
    }
    return count;
}
std::size_t StoredGraphicsPlan::footprint_count(std::uint32_t side) const {
    return static_cast<std::size_t>(std::count_if(footprints.begin(),footprints.end(),
        [side](const auto& footprint) { return footprint.width_cells==side; }));
}
std::map<std::uint32_t,std::size_t> StoredGraphicsPlan::footprint_histogram() const {
    std::map<std::uint32_t,std::size_t> counts;
    for (const auto& footprint:footprints) ++counts[footprint.width_cells];
    return counts;
}

namespace {
StoredGraphicsPlan make_plan_impl(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry,
    const std::map<std::uint32_t,GraphicsArchiveRegistration>& registrations,
    const fs::path& data_root, FootprintPolicy policy, StoredGraphicsProfile profile) {
    constexpr auto storage_count = static_cast<std::size_t>(stored_grid_width) * stored_grid_height;
    if (!geometry.supported || geometry.candidate.size() != storage_count ||
        map.terrain_raw.values.size() != storage_count || map.objects_raw.values.size() != storage_count ||
        candidates.candidate_word_layer.size() != storage_count ||
        candidates.candidate_byte_layer.size() != storage_count)
        throw std::invalid_argument("stored graphics require complete supported map grids");
    if (data_root.empty() || !registrations.contains(3U) || !registrations.contains(16U) ||
        !registrations.at(3U).catalog || !registrations.at(3U).layout ||
        !registrations.at(16U).catalog || !registrations.at(16U).layout)
        throw std::invalid_argument("stored graphics require verified Terrain/Elevation registrations");
    StoredGraphicsPlan plan;
    plan.data_root = data_root;
    plan.profile = profile;
    plan.multi_tile_preview = policy!=FootprintPolicy::Disabled;
    plan.footprint_policy = policy;
    plan.border = geometry.border;
    plan.mask_comparison = compare_masks(map,geometry);
    plan.status_by_storage.assign(storage_count, StoredStatus::Excluded);
    plan.cell_by_storage.resize(storage_count);
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
            const auto registration=registrations.find(resolved.slot);
            if (registration!=registrations.end() && registration->second.layout)
                cell.system_record_skip=registration->second.layout->system_record_skip;
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
                if (plan.multi_tile_preview && cell.status==StoredStatus::UnsupportedLayout &&
                    record.image_type==30 && record.width>78) {
                    cell.status=StoredStatus::UnsupportedFootprintSize;
                    if (inserted) plan.assets.back().status=cell.status;
                }
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
    place_footprints(plan,candidates);
    if (plan.cells.size() + plan.excluded != storage_count)
        throw std::logic_error("stored graphics candidate accounting mismatch");
    return plan;
}
} // namespace

StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const StoredArchiveRegistrations& owned,
    FootprintPolicy policy, StoredGraphicsProfile profile) {
    std::map<std::uint32_t,GraphicsArchiveRegistration> registrations;
    fs::path root;
    for (const auto& [slot,entry]:owned) {
        if (slot!=entry.slot || (slot!=3U && slot!=16U &&
            !(slot==8U && profile==StoredGraphicsProfile::Slot8)))
            throw std::invalid_argument("unverified stored graphics registration");
        if (entry.catalog) {
            if (!root.empty() && root!=entry.catalog->data_root)
                throw std::invalid_argument("stored graphics catalogs have different roots");
            root=entry.catalog->data_root;
        }
        registrations.emplace(slot,GraphicsArchiveRegistration{
            entry.catalog ? &*entry.catalog : nullptr,
            entry.layout ? &*entry.layout : nullptr,entry.archive_missing});
    }
    return make_plan_impl(map,candidates,geometry,registrations,root,policy,profile);
}

StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout, FootprintPolicy policy) {
    if (terrain.data_root.empty() || terrain.data_root!=elevation.data_root ||
        terrain_layout.slot!=3U || elevation_layout.slot!=16U)
        throw std::invalid_argument("stored graphics require verified Terrain/Elevation registrations");
    const std::map<std::uint32_t,GraphicsArchiveRegistration> registrations{
        {3,{&terrain,&terrain_layout,false}}, {16,{&elevation,&elevation_layout,false}}};
    return make_plan_impl(map,candidates,geometry,registrations,terrain.data_root,policy,
                          StoredGraphicsProfile::Base);
}

StoredGraphicsPlan make_stored_graphics_plan(
    const ParsedEmperorMap& map, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, const assets::AssetCatalog& terrain,
    const RuntimeArchiveLayout& terrain_layout, const assets::AssetCatalog& elevation,
    const RuntimeArchiveLayout& elevation_layout, bool multi_tile_preview) {
    return make_stored_graphics_plan(map,candidates,geometry,terrain,terrain_layout,
        elevation,elevation_layout,multi_tile_preview ? FootprintPolicy::IsolatedPreview :
                                           FootprintPolicy::Disabled);
}

} // namespace openemperor::maps
