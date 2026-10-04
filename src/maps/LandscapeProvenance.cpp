#include "maps/LandscapeProvenance.h"
#include "maps/TerrainInterpretation.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace openemperor::maps {
namespace {
using Json = nlohmann::json;
constexpr std::size_t count = stored_grid_width * stored_grid_height;
std::string hex(std::uint32_t value, int width=8) {
    std::ostringstream stream;
    stream << "0x" << std::hex << std::setfill('0') << std::setw(width) << value;
    return stream.str();
}
Json point(scene::Point p) { return {{"x",p.x},{"y",p.y}}; }
Json cell_json(GridCell p) { return {{"x",p.x},{"y",p.y}}; }
}
void read_landscape_layers(StoredGraphicsPlan& plan, const EmperorContainer& container,
                           std::size_t part) {
    // Short historical/synthetic profiles keep their ordinary snapshot path.
    if (container.multipart() || part >= container.parts().size() ||
        container.parts()[part].uncompressed_size < landscape_height_offset+count) return;
    auto properties=container.read_range(part,landscape_draw_properties_offset,count);
    auto heights=container.read_range(part,landscape_height_offset,count);
    auto variation=container.read_range(part,auxiliary_byte_logical_offset,count);
    // Separate serialized operand read to VA e9f8e0 at 52eaa4. This is not
    // an inferred numerical interpretation of the terrain fertility bit.
    auto fertility=container.read_range(part,937255,count);
    plan.draw_properties=std::move(properties);
    plan.height_bytes=std::move(heights);
    plan.variation_bytes=std::move(variation);
    plan.fertility_bytes=std::move(fertility);
    plan.landscape_layers_available=true;
}
int landscape_height(const StoredGraphicsPlan& plan, GridCell cell) {
    if (!plan.landscape_layers_available || plan.height_bytes.size()!=count ||
        cell.x>=stored_grid_width || cell.y>=stored_grid_height) return 0;
    const int raw=plan.height_bytes[static_cast<std::size_t>(cell.y)*stored_grid_width+cell.x];
    return raw<128 ? raw : raw-256;
}
scene::Point landscape_ground(const StoredGraphicsPlan& plan, GridCell cell) {
    auto p=terrain_ground(cell,plan.border);
    p.y-=landscape_height(plan,cell)*landscape_height_step;
    return p;
}
std::optional<GridCell> pick_landscape_ground(const StoredGraphicsPlan& plan,
    scene::Point world, const MapGeometry& geometry, bool elevated) {
    if (!elevated || !plan.landscape_layers_available) return pick_terrain_cell(world,geometry);
    std::optional<GridCell> found;
    // Bounded inverse of the verified transform. Backmost overlap loses to the
    // front storage cell, consistent with the preview painter; no route work.
    for (int h=-128;h<=127;++h) {
        const auto cell=pick_terrain_cell({world.x,world.y+h*landscape_height_step},geometry);
        if (!cell || landscape_height(plan,*cell)!=h) continue;
        if (!found || std::pair{cell->x+cell->y,cell->x}>
                      std::pair{found->x+found->y,found->x}) found=cell;
    }
    return found;
}
Json landscape_provenance(const StoredGraphicsPlan& plan, GridCell selected,
    bool elevated, const scene::Camera2D* camera) {
    Json j={{"map",plan.map_relative.generic_string()},{"storage",cell_json(selected)},
        {"profile",stored_graphics_profile_name(plan.profile)},
        {"footprint_policy",footprint_policy_name(plan.footprint_policy)},
        {"placement_evidence","preview convention; height operand EXE-observed"},
        {"composition_evidence","saved IDs cleared after load; separated components remain preview"}};
    if (selected.x>=stored_grid_width || selected.y>=stored_grid_height) {
        j["status"]="outside_storage"; return j;
    }
    const auto index=static_cast<std::size_t>(selected.y)*stored_grid_width+selected.x;
    if (plan.cell_by_storage.size()!=count || plan.status_by_storage.size()!=count ||
        plan.raw_terrain.size()!=count || plan.raw_objects.size()!=count ||
        plan.raw_saved_ids.size()!=count || plan.raw_candidate_bytes.size()!=count) {
        j["status"]="incomplete_provenance_metadata";
        return j;
    }
    const auto* c=plan.at(selected);
    if (plan.variation_bytes.size()==count) {
        j["variation_byte"]=plan.variation_bytes[index];
        j["variation_logical_offset"]=auxiliary_byte_logical_offset+index;
        j["variation_semantics"]="separate f1e780 selector operand; never candidate/footprint byte";
    }
    if (plan.fertility_bytes.size()==count) j["fertility_selector_operand"]=plan.fertility_bytes[index];
    j["render_source"]="historical saved-ID preview";
    if (c && plan.regenerated && plan.cell_by_storage[index] &&
        *plan.cell_by_storage[index]<plan.regenerated->cells.size()) {
        const auto& generated=plan.regenerated->cells[*plan.cell_by_storage[index]];
        const auto& s=generated.selection;
        Json detail={{"family",landscape_family_name(s.family)},{"selector",s.selector},
            {"evidence",selector_evidence_name(s.evidence)},{"reason",s.reason},
            {"resource_key",hex(s.group.value,3)},{"variant",s.variant},{"fallback",generated.fallback}};
        if (s.water_match) detail["water_match"]={{"row",s.water_match->row},
            {"orientation_offset",s.water_match->orientation_offset},{"variant_count",s.water_match->variant_count},
            {"row_variant",s.water_match->variant}};
        if (generated.graphic) detail["packed_graphic"]=generated.graphic->value;
        if (generated.asset_index) {
            const auto& a=plan.assets[*generated.asset_index];const auto& r=a.record;
            detail["resolved_asset"]={{"archive",r.id.archive_relative_path.generic_string()},
                {"physical_record",r.id.image_index},{"width",r.width},{"height",r.height},
                {"base_bytes",r.uncompressed_length},{"overlay_bytes",r.data_length-r.uncompressed_length},
                {"decode_status",stored_status_name(a.status)}};
            auto origin=stored_image_origin(terrain_world(c->storage,plan.border),
                static_cast<std::uint32_t>(r.width),static_cast<std::uint32_t>(r.height));
            if (elevated) origin.y-=landscape_height(plan,c->storage)*landscape_height_step;
            detail["preview_image_origin"]=point(origin);
            detail["first_visible_layer"]="Ground base (overlay added by semantic layer)";
            if (elevated && a.status==StoredStatus::Rendered) j["render_source"]=
                s.evidence==SelectorEvidence::Verified ? "regenerated verified static selector":"regenerated preview; transient context unresolved";
        }
        detail["active"]=elevated;
        j["regenerated"]=std::move(detail);
    }
    j["candidate_mask"]=c!=nullptr;
    j["status"]=stored_status_name(plan.status_by_storage.at(index));
    if (plan.raw_terrain.size()==count) {
        const auto terrain=plan.raw_terrain[index], objects=plan.raw_objects[index];
        j["terrain_raw"]=terrain; j["terrain_hex"]=hex(terrain);
        j["objects_raw"]=objects; j["objects_hex"]=hex(objects);
        j["semantic_category"]=category_name(interpret_terrain(terrain,objects).category);
        j["offmap_bit"]=(terrain&0x80000U)!=0;
        j["saved_id"]=plan.raw_saved_ids[index]; j["saved_id_hex"]=hex(plan.raw_saved_ids[index]);
        const auto raw=plan.raw_candidate_bytes[index];
        const auto part=decode_map_subtile_byte(raw);
        j["candidate_byte"]=raw; j["candidate_byte_hex"]=hex(raw,2);
        j["tentative_parts"]={{"part_x",part.part_x},{"part_y",part.part_y},
            {"marker_0x40",part.draw_marker_candidate},{"unknown_0x80",part.unknown_bits}};
    }
    j["height_available"]=plan.landscape_layers_available;
    j["height_signed"]=landscape_height(plan,selected);
    j["height_logical_offset"]=landscape_height_offset+index;
    j["height_step_pixels"]=landscape_height_step;
    if (plan.draw_properties.size()==count) j["draw_properties_raw"]=plan.draw_properties[index];
    const auto ground=elevated ? landscape_ground(plan,selected):terrain_ground(selected,plan.border);
    j["ground_world"]=point(ground);
    if (camera) j["screen_ground"]=point(camera->world_to_screen(ground));
    if (!c) return j;
    j["resource"]={{"slot",c->slot},{"local_index",c->local_index},
        {"physical_record",c->physical_record ? Json(*c->physical_record):Json(nullptr)},
        {"lookup_status",graphics_id_status_name(c->lookup_status)}};
    GridCell front=selected;
    std::size_t tie=static_cast<std::size_t>(c-&plan.cells[0]);
    if (c->asset_index) {
        const auto& r=plan.assets[*c->asset_index].record;
        j["image"]={{"archive",r.id.archive_relative_path.generic_string()},
            {"physical_record",r.id.image_index},{"group_id",r.group_id},
            {"group",r.group_filename},{"type",r.image_type},{"width",r.width},{"height",r.height},
            {"size_flag",r.isometric_size_flag},{"base_bytes",r.uncompressed_length},
            {"overlay_bytes",r.data_length>=r.uncompressed_length ?
                Json(r.data_length-r.uncompressed_length):Json(nullptr)},
            {"mirror_offset",r.horizontal_mirror_offset},
            {"alpha_offset",r.alpha_offset},{"alpha_bytes",r.alpha_length},
            {"decode_error",plan.assets[*c->asset_index].error}};
    }
    auto origin=c->image_origin;
    if (c->footprint_index) {
        const auto& f=plan.footprints[*c->footprint_index];
        const auto anchor_cell=f.draw_cell_candidate.value_or(f.origin);
        if (elevated) origin.y-=landscape_height(plan,anchor_cell)*landscape_height_step;
        front={f.origin.x+f.width_cells-1,f.origin.y+f.height_cells-1};
        tie=f.cell_indices.front();
        const auto anchor_ground=elevated ? landscape_ground(plan,anchor_cell):
            terrain_ground(anchor_cell,plan.border);
        j["placement"]={{"footprint_side",f.width_cells},{"preview_origin",cell_json(f.origin)},
            {"image_origin",point(origin)},
            {"image_anchor",point({anchor_ground.x-origin.x,anchor_ground.y-origin.y})},
            {"anchor_ground_cell",cell_json(anchor_cell)},
            {"draw_marker",f.draw_cell_candidate ? cell_json(*f.draw_cell_candidate):Json(nullptr)},
            {"rule",f.rule}};
    }
    auto depth=terrain_ground(front,plan.border);
    j["painter"]={{"front_ground_cell",cell_json(front)},{"depth_y",depth.y},
        {"depth_x",depth.x},{"source_order_tie_break",tie},{"order_evidence","OpenEmperor preview"}};
    return j;
}
std::vector<std::string> landscape_inspection_lines(const StoredGraphicsPlan& plan,
    GridCell cell, bool elevated, const scene::Camera2D* camera) {
    const auto j=landscape_provenance(plan,cell,elevated,camera);
    std::vector<std::string> lines={"Landscape ("+std::to_string(cell.x)+","+std::to_string(cell.y)+")"};
    for (const auto& key:{"candidate_mask","offmap_bit","terrain_hex","semantic_category","objects_hex",
        "render_source","regenerated","variation_byte","variation_logical_offset","variation_semantics",
        "fertility_selector_operand","saved_id","saved_id_hex","candidate_byte_hex","tentative_parts","resource","image",
        "height_available","height_signed","draw_properties_raw","placement","ground_world",
        "screen_ground","painter","status","placement_evidence","composition_evidence"}) {
        if (!j.contains(key)) continue;
        const auto text=std::string(key)+": "+j[key].dump();
        // Inspector rows are intentionally bounded and scroll through the existing panel.
        for (std::size_t start=0;start<text.size();start+=48) lines.push_back(text.substr(start,48));
    }
    return lines;
}
Json landscape_fidelity_report(const StoredGraphicsPlan& plan) {
    std::size_t resolved=0, supported=0;
    for (const auto& c:plan.cells) {
        if (c.physical_record) ++resolved;
        if (c.status==StoredStatus::Rendered) ++supported;
    }
    Json regeneration={{"available",bool(plan.regenerated)},{"verified_identity",0},
        {"preview_identity",0},{"historical_preview",plan.cells.size()},
        {"water_selector",0},{"ground_selector",0},{"decoration_selector",0},
        {"unresolved_selector",plan.cells.size()},{"full_original_composition_verified",0}};
    if (plan.regenerated) {
        std::size_t verified=0,preview=0,water=0,ground=0,decor=0,unresolved=0;
        for (const auto& c:plan.regenerated->cells) {
            if (c.selection.evidence==SelectorEvidence::Unresolved) ++unresolved;
            if (!c.asset_index || plan.assets[*c.asset_index].status!=StoredStatus::Rendered) continue;
            if (c.selection.evidence==SelectorEvidence::Verified) ++verified;else ++preview;
            switch(c.selection.family) {case LandscapeFamily::Water:++water;break;
            case LandscapeFamily::Ground:++ground;break;case LandscapeFamily::Decoration:++decor;break;
            case LandscapeFamily::Preserved:break;}
        }
        regeneration.update({{"verified_identity",verified},{"preview_identity",preview},
            {"historical_preview",plan.cells.size()-verified-preview},{"water_selector",water},
            {"ground_selector",ground},{"decoration_selector",decor},{"unresolved_selector",unresolved},
            {"load_plan_milliseconds",plan.regenerated->build_milliseconds},
            {"historical_physical_assets",plan.regenerated->historical_asset_count},
            {"shared_physical_assets",plan.assets.size()}});
    }
    return {{"regeneration",regeneration},
        {"coverage",{{"candidate_cells",plan.cells.size()},{"decoded_snapshot_cells",supported}}},
        {"resolved_identity",{{"resolved_saved_graphics",resolved},{"original_first_draw_identity_verified",0}}},
        {"placement_verified",{{"height_transform_available",plan.landscape_layers_available},
            {"height_transform_cells",plan.landscape_layers_available ? plan.cells.size():0},
            {"complete_original_anchor_verified",0}}},
        {"composition_verified",{{"ground_verified",0},{"water_verified",0},{"elevation_verified",0},
            {"preview_only",supported},{"unresolved_composition",plan.cells.size()},
            {"reason","static selectors reported separately; transient contexts, multi-cell packing and complete original composition unresolved"}}}};
}
} // namespace openemperor::maps
