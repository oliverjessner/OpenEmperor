#include "maps/LandscapeInstances.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "scene/WorldDrawOrder.h"

#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

using namespace openemperor::maps;
namespace {
constexpr std::size_t count = stored_grid_width * stored_grid_height;
constexpr GridCell rear{80, 90};
std::size_t index(GridCell cell) { return std::size_t(cell.y) * stored_grid_width + cell.x; }
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    std::vector<std::uint32_t> terrain = std::vector<std::uint32_t>(count, 0);
    std::vector<std::uint32_t> objects = std::vector<std::uint32_t>(count, 0);
    std::vector<std::uint8_t> variation = std::vector<std::uint8_t>(count, 190);
    std::vector<std::uint8_t> fertility = std::vector<std::uint8_t>(count, 100);
    std::vector<std::uint8_t> eligible = std::vector<std::uint8_t>(count, 0);
    void square(GridCell origin, unsigned side, std::uint32_t kind = 2) {
        for (unsigned y = 0; y < side; ++y) for (unsigned x = 0; x < side; ++x) {
            const auto i = index({origin.x + x, origin.y + y});
            terrain[i] = kind | 0x80U;
            eligible[i] = 1;
        }
    }
    LandscapeSelectorInput input(unsigned orientation = 0) const {
        return {terrain, objects, variation, fertility, std::nullopt, orientation};
    }
    std::vector<LandscapeInstanceSpec> derive() const {
        return derive_rock_instances(input(), eligible);
    }
};
void unique_ownership(const std::vector<LandscapeInstanceSpec>& instances,
                      const Fixture& fixture, std::size_t expected) {
    std::set<std::size_t> owned;
    for (const auto& instance : instances) {
        check(instance.owned_cells.size() == instance.side * instance.side,
            "one rectangular image owns its complete square");
        check(instance.draw_cell == GridCell{instance.origin.x, instance.origin.y + instance.side - 1},
            "orientation-zero draw marker is on the west/front edge");
        for (const auto cell : instance.owned_cells) {
            check(cell.x < 228 && cell.y < 228 && fixture.eligible[index(cell)],
                "ownership never crosses storage or the preserved-layer mask");
            check(owned.insert(index(cell)).second, "no cell can have two regenerated owners");
        }
    }
    check(owned.size() == expected, "every eligible rock cell has one owner");
}
StoredArchiveRegistrations registrations() {
    StoredArchiveRegistration registration;
    registration.layout.emplace();
    auto& layout = *registration.layout;
    layout.slot = 3;
    layout.verified_registration = true;
    layout.sg3_version = 213;
    layout.first_physical_record = 1;
    layout.image_capacity = 66;
    layout.runtime_image_count = 65;
    for (unsigned position = 0; position < 9; ++position) {
        const unsigned base = position < 6 ? 1 : position == 6 ? 17 : position == 7 ? 33 : 49;
        layout.groups.push_back({position, 80 + position * 2, static_cast<std::uint16_t>(base), base});
    }
    registration.catalog.emplace();
    for (unsigned physical = 0; physical < layout.image_capacity; ++physical) {
        openemperor::assets::AssetRecord record;
        record.id = {"DATA/synthetic-rock.sg3", physical};
        record.image_type = 30;
        const unsigned local = physical == 0 ? 0 : physical - 1;
        const unsigned variant = local == 0 ? 0 : (local - 1) % 16;
        const unsigned side = variant < 8 ? 1 : variant < 12 ? 2 : variant < 14 ? 3 : 1;
        record.width = static_cast<std::int16_t>(80 * side - 2);
        record.height = static_cast<std::int16_t>(40 * side + 20);
        record.isometric_size_flag = static_cast<std::uint8_t>(side);
        record.uncompressed_length = 3200 * side * side;
        record.data_length = record.uncompressed_length + 300;
        record.payload_in_bounds = true;
        record.decoder_supported = true;
        record.color_decoder_supported = true;
        record.color_bounds = openemperor::assets::AssetRangeStatus::InBounds;
        registration.catalog->records.push_back(record);
    }
    return {{3, std::move(registration)}};
}
StoredGraphicsPlan historical_plan(const Fixture& fixture) {
    StoredGraphicsPlan plan;
    plan.landscape_layers_available = true;
    plan.raw_terrain = fixture.terrain;
    plan.raw_objects = fixture.objects;
    plan.variation_bytes = fixture.variation;
    plan.fertility_bytes = fixture.fertility;
    plan.raw_saved_ids.assign(count, 123);
    plan.raw_candidate_bytes.assign(count, 0x40);
    plan.draw_properties.assign(count, 0);
    plan.height_bytes.assign(count, 1);
    plan.cell_by_storage.resize(count);
    plan.status_by_storage.assign(count, StoredStatus::Excluded);
    const auto registration = registrations();
    plan.assets.push_back({registration.at(3).catalog->records[0], StoredStatus::Rendered, true, true, {}});
    for (unsigned y = 0; y < 228; ++y) for (unsigned x = 0; x < 228; ++x) {
        const auto raw = index({x, y});
        if (!fixture.eligible[raw]) continue;
        const auto member = plan.cells.size();
        StoredCell cell;
        cell.storage = {x, y};
        cell.cell_index = raw;
        cell.terrain_raw = plan.raw_terrain[raw];
        cell.slot = 3;
        cell.stored_id = 123;
        cell.asset_index = 0;
        cell.footprint_index = member;
        cell.status = StoredStatus::Rendered;
        plan.cell_by_storage[raw] = member;
        plan.status_by_storage[raw] = cell.status;
        plan.cells.push_back(cell);
        PlacedFootprint footprint;
        footprint.id = member;
        footprint.asset_index = 0;
        footprint.origin = cell.storage;
        footprint.cell_indices = {member};
        footprint.status = StoredStatus::Rendered;
        plan.footprints.push_back(footprint);
    }
    return plan;
}
bool same_instances(const RegeneratedMapRenderPlan& a, const RegeneratedMapRenderPlan& b) {
    if (a.instances.size() != b.instances.size()) return false;
    for (std::size_t i = 0; i < a.instances.size(); ++i) {
        const auto& x = a.instances[i];
        const auto& y = b.instances[i];
        if (x.graphic.value != y.graphic.value || x.geometry.origin != y.geometry.origin ||
            x.geometry.side != y.geometry.side || x.geometry.owned_cells != y.geometry.owned_cells ||
            !(x.geometry.selection == y.geometry.selection)) return false;
    }
    return true;
}
} // namespace

int main() {
    try {
        Fixture three;
        three.square(rear, 3);
        auto instances = three.derive();
        check(instances.size() == 1 && instances[0].origin == rear && instances[0].side == 3 &&
            instances[0].selection.variant == 12, "3x3 wins before overlapping 2x2 candidates");
        unique_ownership(instances, three, 9);
        check(std::none_of(instances.begin(), instances.end(), [](const auto& f) { return f.side == 1; }),
            "members claimed by the earlier owner never become singletons");
        three.variation[index(rear)] = 191;
        check(three.derive()[0].selection.variant == 13, "3x3 uses origin variation parity");
        three.variation[index({rear.x + 2, rear.y + 2})] = 0;
        check(three.derive()[0].selection.variant == 13, "member variation cannot change an owner's bank");

        Fixture two;
        two.square(rear, 2);
        two.variation[index(rear)] = 191;
        check(two.derive().size() == 1 && two.derive()[0].side == 2 &&
            two.derive()[0].selection.variant == 11, "2x2 uses origin modulo four after failed 3x3 fit");
        Fixture singleton;
        singleton.square(rear, 1);
        singleton.variation[index(rear)] = 255;
        check(singleton.derive()[0].selection.variant == 7, "singleton uses origin modulo eight");
        for (const auto [kind, key] : {std::pair{2U, 0x606U}, {0x100002U, 0x607U}, {0x200002U, 0x608U}}) {
            Fixture ore;
            ore.square(rear, 3, kind);
            check(ore.derive()[0].selection.group.value == key, "rock and ore families use distinct resource keys");
        }
        Fixture overlap;
        overlap.square(rear, 5);
        const auto packed = overlap.derive();
        check(packed[0].side == 3 && packed[1].origin == GridCell{83, 90} && packed[1].side == 2,
            "row-major traversal commits the earlier 3x3 before the next 2x2");
        unique_ownership(packed, overlap, 25);
        overlap.eligible[index({81, 91})] = 0;
        unique_ownership(overlap.derive(), overlap, 24);
        Fixture boundary;
        boundary.square({226, 226}, 2);
        check(boundary.derive().size() == 1 && boundary.derive()[0].side == 2,
            "storage-edge 2x2 is accepted without a wrapped 3x3 probe");
        unique_ownership(boundary.derive(), boundary, 4);
        check(derive_rock_instances(three.input(), std::span(three.eligible).first(3)).empty() &&
            derive_rock_instances(three.input(1), three.eligible).empty(),
            "short masks and unsupported view inputs fail closed");
        Fixture water;
        water.square(rear, 1, 6);
        check(water.derive().empty(), "water with a rock bit never enters this branch");
        for (const unsigned side : {1U, 2U, 3U}) {
            Fixture flood_rock;
            flood_rock.square(rear, side, 0x102U);
            check(flood_rock.derive().empty(),
                "flood with a rock bit cannot become a singleton or a packed rock owner");
            auto flood_plan = historical_plan(flood_rock);
            build_regenerated_map_render_plan(flood_plan, registrations());
            check(flood_plan.regenerated->instances.empty(),
                "the load plan preserves flood instead of promoting it through the instance pass");
            for (const auto& cell : flood_plan.regenerated->cells)
                check(cell.selection.family == LandscapeFamily::Ground &&
                    cell.selection.evidence == SelectorEvidence::Unresolved &&
                    !cell.instance_index && !cell.asset_index,
                    "frozen unresolved flood selection survives for every member");
        }
        Fixture mixed_flood;
        mixed_flood.square(rear, 3);
        mixed_flood.terrain[index({81, 91})] |= 0x100U;
        const auto non_flood_owners = mixed_flood.derive();
        unique_ownership(non_flood_owners, mixed_flood, 8);
        for (const auto& owner : non_flood_owners)
            check(std::find(owner.owned_cells.begin(), owner.owned_cells.end(), GridCell{81, 91}) ==
                owner.owned_cells.end(), "a mixed flood member is never claimed by an adjacent rock owner");

        const auto ordinary = instances[0];
        const auto h0 = regenerated_instance_origin(ordinary, 68, 238, 170, 0);
        const auto h1 = regenerated_instance_origin(ordinary, 68, 238, 170, 1);
        const auto hn = regenerated_instance_origin(ordinary, 68, 238, 170, -1);
        check(h0.x == -519 && h0.y == 630 && h1.x == h0.x && h1.y == 590 && hn.y == 670,
            "instance anchor retains the frozen signed height times forty displacement");
        const auto front = terrain_ground({82, 92}, 68);
        const auto marker = terrain_ground(ordinary.draw_cell, 68);
        check(front.y == 780 && marker.y == 740,
            "diagonal front-cell painter depth stays separate from west/front draw marker");
        bool rejected = false;
        try { regenerated_instance_origin(ordinary, 68, 158, 170, 0); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "mismatched physical footprint geometry is rejected");

        for (const unsigned side : {1U, 2U, 4U}) {
            LandscapeInstanceSpec wall;
            wall.origin={84,61};wall.side=side;
            wall.draw_cell={wall.origin.x,wall.origin.y+side-1};
            const GridCell wall_front{wall.origin.x+side-1,wall.origin.y+side-1};
            wall.explicit_height=LandscapeInstanceHeight{LandscapeInstanceHeightSource::SerializedCellHeight,
                wall.draw_cell};
            wall.explicit_anchor=LandscapeInstanceAnchor{wall_front,int(40U*side-1U),20};
            wall.depth_cell=wall_front;
            wall.original_entity_index=7;
            for (unsigned y=0;y<side;++y) for (unsigned x=0;x<side;++x)
                wall.owned_cells.push_back({wall.origin.x+x,wall.origin.y+y});
            const unsigned image_height=40U*side+78U;
            const auto wall_h0=regenerated_instance_origin(wall,29,80U*side-2U,image_height,0);
            const auto wall_h4=regenerated_instance_origin(wall,29,80U*side-2U,image_height,4);
            const auto wall_hn=regenerated_instance_origin(wall,29,80U*side-2U,image_height,-2);
            const auto wall_marker_ground=terrain_ground(wall.draw_cell,29);
            check(wall_h0.x==wall_marker_ground.x-39 &&
                wall_h0.y==wall_marker_ground.y-double(image_height)+20.0*side,
                "explicit front anchor reconstructs the full original marker component chain");
            check(wall_h4.x==wall_h0.x && wall_h4.y==wall_h0.y-160 && wall_hn.y==wall_h0.y+80,
                "explicit monument anchor applies signed marker height times forty exactly once");
            check(wall.explicit_height->cell==wall.draw_cell && *wall.depth_cell==wall_front &&
                wall.original_entity_index==7,
                "serialized height marker, front painter depth, and original entity reference stay distinct");
            if (side>1) check(terrain_ground(*wall.depth_cell,29).y>wall_marker_ground.y,
                "monument front painter depth is independent of the earlier original draw marker");
            auto wrong_marker=wall;
            wrong_marker.explicit_height->cell={wall.draw_cell.x+1,wall.draw_cell.y};
            rejected=false;
            try { regenerated_instance_origin(wrong_marker,29,80U*side-2U,image_height,0); }
            catch (const std::invalid_argument&) { rejected=true; }
            check(rejected,"explicit monument placement refuses height from a different cell");
            auto wrong_anchor=wall;
            ++wrong_anchor.explicit_anchor->x;
            rejected=false;
            try { regenerated_instance_origin(wrong_anchor,29,80U*side-2U,image_height,0); }
            catch (const std::invalid_argument&) { rejected=true; }
            check(rejected,"unobserved whole-image anchor is rejected instead of applied as a preview correction");
        }

        const auto registered = registrations();
        auto plan = historical_plan(three);
        const auto before = plan;
        build_regenerated_map_render_plan(plan, registered);
        check(plan.regenerated && plan.regenerated->instances.size() == 1 &&
            plan.regenerated->instances[0].geometry.side == 3 &&
            plan.regenerated->instances[0].cell_indices.size() == 9,
            "load plan publishes one complete derived 3x3 instance");
        for (const auto& cell : plan.regenerated->cells)
            check(cell.instance_index == 0 && cell.asset_index == plan.regenerated->instances[0].asset_index,
                "all members share one physical asset and instance owner");
        check(plan.assets.size() == before.assets.size() + 1 && plan.raw_saved_ids == before.raw_saved_ids &&
            plan.raw_candidate_bytes == before.raw_candidate_bytes && plan.raw_terrain == before.raw_terrain &&
            plan.status_by_storage == before.status_by_storage && plan.cell_by_storage == before.cell_by_storage,
            "declaring new visuals cannot alter historical raw layers or buildability status");
        for (std::size_t i = 0; i < plan.cells.size(); ++i)
            check(plan.cells[i].stored_id == before.cells[i].stored_id &&
                plan.cells[i].status == before.cells[i].status &&
                plan.footprints[i].asset_index == before.footprints[i].asset_index &&
                plan.footprints[i].origin == before.footprints[i].origin,
                "old cells and footprint partition remain untouched");
        auto independent = before;
        independent.raw_saved_ids.assign(count, 0xffffffffU);
        independent.height_bytes.assign(count, 255);
        independent.raw_candidate_bytes.assign(count, 7);
        for (auto& cell : independent.cells) cell.stored_id = 0xffffffffU;
        build_regenerated_map_render_plan(independent, registered);
        check(same_instances(*plan.regenerated, *independent.regenerated),
            "rock ownership and identity are independent of historic IDs, candidate bytes, and heights");
        const auto published = plan.regenerated;
        build_regenerated_map_render_plan(plan, registered);
        check(plan.regenerated == published && plan.assets.size() == before.assets.size() + 1,
            "the immutable load plan cannot be rebuilt or duplicate uploads");

        auto animated = registered;
        animated.at(3).catalog->records[15].animation_sprites = 2;
        auto refused = before;
        build_regenerated_map_render_plan(refused, animated);
        check(refused.regenerated->instances.empty(), "unsupported animation rejects the whole selected owner");
        for (const auto& cell : refused.regenerated->cells)
            check(!cell.instance_index && cell.selection.evidence == SelectorEvidence::Unresolved,
                "whole-instance failure never publishes partial member ownership");
        check(refused.status_by_storage == before.status_by_storage && refused.raw_terrain == before.raw_terrain,
            "whole-instance failure preserves historical fallback authority");
        auto wrong_geometry = registered;
        wrong_geometry.at(3).catalog->records[15].width = 158;
        auto unsupported = before;
        build_regenerated_map_render_plan(unsupported, wrong_geometry);
        check(unsupported.regenerated->instances.empty(),
            "a physical record with the wrong side geometry cannot replace the selected 3x3");
        auto mirrored = registered;
        mirrored.at(3).catalog->records[15].horizontal_mirror_offset = 1;
        auto unverified = before;
        build_regenerated_map_render_plan(unverified, mirrored);
        check(unverified.regenerated->instances.empty(),
            "unverified mirroring rejects the entire derived instance");

        std::cout << "landscape instance tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
