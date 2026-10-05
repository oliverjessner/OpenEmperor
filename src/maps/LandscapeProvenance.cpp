#include "maps/LandscapeProvenance.h"
#include "maps/TerrainInterpretation.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "maps/GreatWallMapPresentation.h"
#include "maps/OrdinaryGateMapPresentation.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <set>

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
Json wall_context_json(const GreatWallRestoreContext& context) {
    const bool verified=great_wall_original_context_verified(context);
    return {{"mode",great_wall_presentation_mode_name(context.mode)},
        {"source",great_wall_context_source_name(context.source)},
        {"material",context.material ? Json(*context.material):Json(nullptr)},
        {"camera_view",context.camera_view},{"original_context_verified",verified},
        {"evidence",verified ? "EXE-OBSERVED original restore inputs":
            (context.source==GreatWallContextSource::ExplicitPreview ? "OPENEMPEROR PREVIEW":"UNRESOLVED")},
        {"reason",context.reason}};
}
Json wall_preparation_json(const StoredGraphicsPlan& plan,const GreatWallPreparationResult& result) {
    const auto& resource=result.resource;
    const auto& group=resource.group;
    Json out={{"status",great_wall_preparation_status_name(result.status)},
        {"registered_archive",result.registered_archive.generic_string()},
        {"group_status",resource.attempted ? Json(group_lookup_status_name(group.status)):Json(nullptr)},
        {"runtime_local_begin",group.local_base ? Json(*group.local_base):Json(nullptr)},
        {"runtime_local_end_exclusive",resource.local_end ? Json(*resource.local_end):Json(nullptr)},
        {"variant_out_of_range",resource.variant_out_of_range},
        {"image_status",result.image_status ? Json(graphics_id_status_name(*result.image_status)):Json(nullptr)}};
    if (group.local_base && resource.local_end && *resource.local_end>=*group.local_base)
        out["variant_count"]=*resource.local_end-*group.local_base;
    if (result.instance_index && plan.regenerated && *result.instance_index<plan.regenerated->instances.size()) {
        const auto n=*result.instance_index;
        const auto& instance=plan.regenerated->instances[n];
        const auto& asset=plan.assets.at(instance.asset_index);
        out["instance_index"]=n;
        out["decode_status"]=stored_status_name(asset.status);
        if (asset.status==StoredStatus::DecodeFailed) {
            out["status"]="decode_failed";out["decode_error"]=asset.error;
        } else if (n<plan.regenerated_instance_active.size()) {
            out["status"]=plan.regenerated_instance_active[n] ? "active":"atomic_activation_unavailable";
        }
    }
    return out;
}
Json ordinary_gate_json(const StoredGraphicsPlan& plan,std::size_t gate_index) {
    const auto& gate=plan.original_ordinary_gates->gates.at(gate_index);
    const auto& source=gate.source;
    Json out={{"original_id",source.original_id ? Json(source.original_id->value):Json(nullptr)},
        {"manager_index",source.manager_index},{"original_class",original_entity_class_name(source.entity_class)},
        {"type",source.type},{"status_raw",source.status},
        {"layout_raw",source.gate_house ? Json(source.gate_house->layout):Json(nullptr)},
        {"origin",cell_json(gate.origin)},{"width_cells",gate.width},{"height_cells",gate.height},
        {"selected_components",gate.components.size()},{"component_footprint_side",1},
        {"restore_boolean",0},{"camera_view",0},{"resource_key",hex(ordinary_gate_resource_key,3)},
        {"selector_evidence","EXE-OBSERVED: saved GateHouse restore and static component selection"},
        {"composition_evidence","OPENEMPEROR PREVIEW: existing split Type30 painter"},
        {"fallback",gate.fallback},
        {"preparation_status",gate.fallback.empty() ? "unprepared":"complete_claim_unavailable"},
        {"active_components",0}};
    if (const auto field=source.field_source(OriginalEntityField::GateLayout))
        out["layout_source"]={{"logical_offset",field->logical_offset},
            {"record_relative_offset",field->record_relative_offset},{"byte_width",field->byte_width},
            {"signed",field->signed_value},{"base_schema",source.provenance.base_schema},
            {"class_wrapper_schema",source.provenance.class_wrapper_schema}};
    if (plan.regenerated) {
        const auto found=plan.regenerated->ordinary_gate_preparation.find(gate_index);
        if (found!=plan.regenerated->ordinary_gate_preparation.end()) {
            const auto& prepared=found->second;
            out["fallback"]=prepared.fallback;
            if (!prepared.fallback.empty()) out["preparation_status"]=gate.fallback.empty() ?
                "complete_preparation_unavailable":"complete_claim_unavailable";
            else if (prepared.instance_indices.size()==15) {
                std::size_t active=0;bool failed=false,ready_known=true;
                for (const auto n:prepared.instance_indices) {
                    const auto& instance=plan.regenerated->instances.at(n);
                    failed|=plan.assets.at(instance.asset_index).status==StoredStatus::DecodeFailed;
                    if (n>=plan.regenerated_instance_active.size()) ready_known=false;
                    else if (plan.regenerated_instance_active[n]) ++active;
                }
                out["active_components"]=active;
                out["preparation_status"]=failed ? "decode_failed":
                    (!ready_known ? "prepared":(active==15 ? "active":"atomic_activation_unavailable"));
            }
        }
    }
    return out;
}
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
    if (plan.original_ordinary_gates) {
        j["ordinary_gate_reader"]={{"manager_records",plan.original_ordinary_gates->manager_records},
            {"error",plan.original_ordinary_gates->error}};
        if (plan.original_ordinary_gates->gate_by_storage.size()==count &&
            plan.original_ordinary_gates->gate_by_storage[index])
            j["ordinary_gate"]=ordinary_gate_json(plan,*plan.original_ordinary_gates->gate_by_storage[index]);
    }
    std::optional<std::size_t> original_piece_index;
    if (plan.original_great_wall) {
        const auto& wall=*plan.original_great_wall;
        j["original_entity_reader"]={{"manager_logical_offset",wall.manager_offset},
            {"manager_byte_length",wall.manager_bytes},{"manager_records",wall.manager_records},
            {"error",wall.error}};
        if (wall.piece_by_storage.size()==count && wall.piece_by_storage[index]) {
            const auto piece_index=*wall.piece_by_storage[index];
            original_piece_index=piece_index;
            const auto& piece=wall.pieces.at(piece_index);const auto& source=piece.source;
            Json state={{"piece_index",piece_index},{"class",original_entity_class_name(source.entity_class)},
                {"manager_index",source.manager_index},{"original_id",source.original_id ? Json(source.original_id->value):Json(nullptr)},
                {"type",source.type},{"subindex",source.subindex},{"local_x",source.local_x},{"local_y",source.local_y},
                {"record_logical_offset",source.provenance.logical_record_offset},
                {"record_bytes",source.provenance.record_byte_length},{"base_schema",source.provenance.base_schema},
                {"extended_schema",source.provenance.extended_schema},{"fallback",piece.fallback},
                {"restored_material",great_wall_original_context_verified(wall.restore_context) ? Json(*wall.restore_context.material):Json(nullptr)},
                {"selected_material",wall.restore_context.material ? Json(*wall.restore_context.material):Json(nullptr)},
                {"restore_context",wall_context_json(wall.restore_context)},
                {"original_context_verified",great_wall_original_context_verified(wall.restore_context)},
                {"material_authority",great_wall_original_context_verified(wall.restore_context) ?
                    "independently validated original restore context":"preview or unavailable; raw saved material is not authority"},
                {"camera_view",wall.restore_context.camera_view},{"selector_supported",piece.selection.supported}};
            if (plan.regenerated) {
                const auto fallback=plan.regenerated->original_wall_fallbacks.find(piece_index);
                if (fallback!=plan.regenerated->original_wall_fallbacks.end()) state["fallback"]=fallback->second;
                const auto preparation=plan.regenerated->original_wall_preparation.find(piece_index);
                if (preparation!=plan.regenerated->original_wall_preparation.end())
                    state["preparation"]=wall_preparation_json(plan,preparation->second);
            }
            if (source.monument) state["serialized_state"]={{"phase",source.monument->phase},
                {"material_raw",source.monument->serialized_material},{"height_raw",source.monument->height},
                {"orientation",source.monument->orientation}};
            if (source.monument) state["selector_phase"]=source.monument->phase;
            Json fields=Json::object();
            for (const auto [name,field]:{std::pair{"x",OriginalEntityField::LocalX},
                {"y",OriginalEntityField::LocalY},{"type",OriginalEntityField::Type},
                {"subindex",OriginalEntityField::Subindex},{"id",OriginalEntityField::OriginalId},
                {"phase",OriginalEntityField::MonumentPhase},{"material",OriginalEntityField::SerializedMaterial},
                {"height",OriginalEntityField::MonumentHeight},{"orientation",OriginalEntityField::MonumentOrientation}})
                if (const auto f=source.field_source(field)) fields[name]={{"logical_offset",f->logical_offset},
                    {"record_relative_offset",f->record_relative_offset},{"bytes",f->byte_width},
                    {"signed",f->signed_value},{"endianness","little"}};
            state["field_sources"]=std::move(fields);
            if (piece.model_piece) {const auto& p=*piece.model_piece;
                state["model"]={{"filename",great_wall_model_filename(source.type).value_or("")},
                    {"row",source.subindex},{"controller",unsigned(p.kind)},
                    {"piece",p.piece},{"position",p.position},{"elevation",p.elevation},
                    {"offsets_added_to_entity_coordinates",false}};}
            if (piece.geometry) {const auto& g=*piece.geometry;
                state["geometry"]={{"origin",cell_json(g.origin)},{"side",g.side},
                    {"draw_cell",cell_json(g.draw_cell)},{"owned_cells",g.owned_cells.size()},
                    {"height_source","SerializedCellHeight"},{"height_cell",cell_json(g.draw_cell)},
                    {"height_signed",landscape_height(plan,g.draw_cell)}};}
            if (piece.selection.supported) state["selection"]={{"resource_key",hex(piece.selection.group.value)},
                {"variant",piece.selection.variant},{"flags",piece.selection.flags},{"effective_view",piece.selection.effective_view},
                {"evidence",selector_evidence_name(piece.selection.evidence)},
                {"archive",piece.archive_relative.generic_string()}};
            j["original_great_wall"]=std::move(state);
        }
    }
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
        const auto topology=plan.regenerated->normal_wall_topology.find(*plan.cell_by_storage[index]);
        if (topology!=plan.regenerated->normal_wall_topology.end()) {
            const auto& wall=topology->second;
            detail["normal_wall_topology"]={{"gate_connection",wall_gate_connection_status_name(wall.gate_connection)},
                {"gate_context",wall.gate_context},{"center_gate",wall.center_gate},
                {"wall_neighbor_mask",hex(wall.wall_neighbor_mask,2)},
                {"gate_neighbor_mask",hex(wall.gate_neighbor_mask,2)},
                {"combined_neighbor_mask",hex(wall.neighbor_mask,2)},
                {"semantic_row",wall.semantic_row ? Json(*wall.semantic_row):Json(nullptr)},
                {"selected_variant",wall.variant ? Json(*wall.variant):Json(nullptr)},
                {"context_source","raw eight-neighbor terrain; no gate-object or saved-image input"},
                {"gate_body_reproduced",false},{"optional_model_components_reproduced",false}};
        }
        if (original_piece_index) {
            const auto fallback=plan.regenerated->original_wall_fallbacks.find(*original_piece_index);
            if (fallback!=plan.regenerated->original_wall_fallbacks.end()) {
                detail["fallback"]=fallback->second;detail["reason"]=fallback->second;
            }
        }
        if (s.water_match) detail["water_match"]={{"row",s.water_match->row},
            {"orientation_offset",s.water_match->orientation_offset},{"variant_count",s.water_match->variant_count},
            {"row_variant",s.water_match->variant}};
        if (generated.graphic) detail["packed_graphic"]=generated.graphic->value;
        if (generated.asset_index) {
            const auto& a=plan.assets[*generated.asset_index];const auto& r=a.record;
            const bool active_instance=!generated.instance_index ||
                (*generated.instance_index<plan.regenerated_instance_active.size() &&
                 plan.regenerated_instance_active[*generated.instance_index]);
            detail["resolved_asset"]={{"archive",r.id.archive_relative_path.generic_string()},
                {"physical_record",r.id.image_index},{"width",r.width},{"height",r.height},
                {"base_bytes",r.uncompressed_length},{"overlay_bytes",r.data_length-r.uncompressed_length},
                {"decode_status",stored_status_name(a.status)}};
            auto origin=stored_image_origin(terrain_world(c->storage,plan.border),
                static_cast<std::uint32_t>(r.width),static_cast<std::uint32_t>(r.height));
            detail["first_visible_layer"]="Ground base (overlay added by semantic layer)";
            if (generated.instance_index) {
                const auto& instance=plan.regenerated->instances[*generated.instance_index];
                const auto& g=instance.geometry;
                const auto height_cell=g.explicit_height ? g.explicit_height->cell:g.draw_cell;
                const auto front=g.depth_cell.value_or(GridCell{g.origin.x+g.side-1,g.origin.y+g.side-1});
                const auto spatial_ground=terrain_ground(front,plan.border);
                const bool combined=instance.composition_policy==LandscapeCompositionPolicy::SpatialCombined;
                const Json format_components=r.data_length>r.uncompressed_length ?
                    Json::array({"Base","Overlay"}):Json::array({"Base"});
                detail["instance"]={{"id",*generated.instance_index},{"family",landscape_family_name(g.selection.family)},
                    {"origin",cell_json(g.origin)},{"footprint_side",g.side},{"owned_cell",cell_json(c->storage)},
                    {"owned_cell_count",g.owned_cells.size()},{"draw_cell",cell_json(g.draw_cell)},
                    {"height",landscape_height(plan,height_cell)},{"height_cell",cell_json(height_cell)},
                    {"height_source",g.explicit_height ? "SerializedCellHeight":"existing landscape cell height"},
                    {"placement_evidence",g.placement_evidence},{"composition_evidence",g.composition_evidence},
                    {"painter_depth_convention","logical diagonal front ground; height does not reorder"},
                    {"spatial_reference_cell",cell_json(front)},
                    {"spatial_draw_key",{{"depth",spatial_ground.y},{"ground_x",spatial_ground.x},
                        {"layer","StoredMap"},{"stable_id",plan.cells.size()+*generated.instance_index}}},
                    {"composition",{{"policy",landscape_composition_policy_name(instance.composition_policy)},
                        {"evidence","OPENEMPEROR PREVIEW: scene role independent of SG3 format components"},
                        {"format_components",format_components},
                        {"drawn_components_when_visible",combined ? Json::array({"Combined"}):format_components},
                        {"early_base_pass",!combined},
                        {"spatial_pass",combined ? "Combined":
                            (r.data_length>r.uncompressed_length ? "Overlay":"none")}}}};
                if (g.explicit_anchor) {
                    const auto& anchor=*g.explicit_anchor;
                    detail["instance"]["image_anchor"]={{"ground_cell",cell_json(anchor.ground_cell)},
                        {"x",anchor.x},{"y_from_image_bottom",anchor.y_from_image_bottom},
                        {"source",g.placement_evidence}};
                }
                if (combined) detail["first_visible_layer"]="Walls/Monuments: complete spatial object body";
                if (instance.great_wall_context) {
                    detail["instance"]["restore_context"]=wall_context_json(*instance.great_wall_context);
                    detail["instance"]["original_context_verified"]=great_wall_original_context_verified(*instance.great_wall_context);
                }
                origin=regenerated_instance_origin(g,plan.border,unsigned(r.width),unsigned(r.height),
                    elevated ? landscape_height(plan,g.explicit_height ? g.explicit_height->cell:g.draw_cell):0);
            } else if (elevated) origin.y-=landscape_height(plan,c->storage)*landscape_height_step;
            detail["preview_image_origin"]=point(origin);
            detail["atomic_instance_active"]=active_instance;
            if (elevated && active_instance && a.status==StoredStatus::Rendered) j["render_source"]=
                s.evidence==SelectorEvidence::Verified ? "regenerated verified static selector":
                    (s.family==LandscapeFamily::GreatWall ? "regenerated Great Wall explicit material preview":"regenerated preview; transient context unresolved");
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
        "render_source","regenerated","original_entity_reader","original_great_wall","ordinary_gate_reader","ordinary_gate","variation_byte","variation_logical_offset","variation_semantics",
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
    Json original={{"reader_available",bool(plan.original_great_wall)},{"serialized_objects",0},
        {"wall_pieces",0},{"selected_pieces",0},{"descriptor_cells",0},{"render_instances",0},
        {"rendered_original_objects",0},{"rendered_cells",0},{"preview_render_instances",0},
        {"preview_render_objects",0},{"preview_rendered_cells",0},{"original_context_verified",false},
        {"first_draw_acceptance","not established"}};
    if (plan.original_great_wall) {
        const auto& wall=*plan.original_great_wall;
        std::size_t selected=0,cells=0,rendered_cells=0,instances=0,preview_cells=0,preview_instances=0;
        std::set<std::size_t> objects,preview_objects;
        for (const auto& p:wall.pieces) if (p.selection.supported) ++selected;
        for (const auto& p:wall.piece_by_storage) if (p) ++cells;
        if (plan.regenerated) for (std::size_t n=0;n<plan.regenerated->instances.size();++n) {
            const auto& i=plan.regenerated->instances[n];
            if (!i.geometry.original_entity_index || n>=plan.regenerated_instance_active.size() ||
                !plan.regenerated_instance_active[n]) continue;
            if (i.great_wall_context && great_wall_original_context_verified(*i.great_wall_context)) {
                ++instances;objects.insert(*i.geometry.original_entity_index);rendered_cells+=i.cell_indices.size();
            } else if (i.great_wall_context && i.great_wall_context->source==GreatWallContextSource::ExplicitPreview) {
                ++preview_instances;preview_objects.insert(*i.geometry.original_entity_index);preview_cells+=i.cell_indices.size();
            }
        }
        original.update({{"manager_records",wall.manager_records},{"serialized_objects",wall.pieces.size()},
            {"wall_pieces",wall.pieces.size()},{"selected_pieces",selected},{"descriptor_cells",cells},
            {"render_instances",instances},{"rendered_original_objects",objects.size()},{"rendered_cells",rendered_cells},
            {"preview_render_instances",preview_instances},{"preview_render_objects",preview_objects.size()},
            {"preview_rendered_cells",preview_cells},{"restore_context",wall_context_json(wall.restore_context)},
            {"original_context_verified",great_wall_original_context_verified(wall.restore_context)},
            {"restore_material_context_available",wall.restore_context.material.has_value()},{"reader_error",wall.error}});
        if (plan.regenerated) {
            Json statuses=Json::object();
            for (const auto& [piece,result]:plan.regenerated->original_wall_preparation) {
                (void)piece;
                const auto status=wall_preparation_json(plan,result)["status"].get<std::string>();
                if (!statuses.contains(status)) statuses[status]=0;
                statuses[status]=statuses[status].get<std::size_t>()+1;
            }
            original["preparation_status_counts"]=std::move(statuses);
            original["preview_notice"]=plan.regenerated->great_wall_preview_notice;
        }
    }
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
            if (!c.asset_index || plan.assets[*c.asset_index].status!=StoredStatus::Rendered ||
                (c.instance_index && (*c.instance_index>=plan.regenerated_instance_active.size() ||
                 !plan.regenerated_instance_active[*c.instance_index]))) continue;
            if (c.selection.evidence==SelectorEvidence::Verified) ++verified;else ++preview;
            switch(c.selection.family) {case LandscapeFamily::Water:++water;break;
            case LandscapeFamily::Ground:++ground;break;case LandscapeFamily::Decoration:++decor;break;
            case LandscapeFamily::Rock:case LandscapeFamily::Mountain:case LandscapeFamily::Wall:
            case LandscapeFamily::GreatWall:case LandscapeFamily::OrdinaryGate:break;
            case LandscapeFamily::Preserved:break;}
        }
        regeneration.update({{"verified_identity",verified},{"preview_identity",preview},
            {"historical_preview",plan.cells.size()-verified-preview},{"water_selector",water},
            {"ground_selector",ground},{"decoration_selector",decor},{"unresolved_selector",unresolved},
            {"load_plan_milliseconds",plan.regenerated->build_milliseconds},
            {"historical_physical_assets",plan.regenerated->historical_asset_count},
            {"shared_physical_assets",plan.assets.size()}});
        std::size_t mountain_verified=0,wall_verified=0,great_wall_verified=0,unresolved_mountains=0,unresolved_walls=0;
        std::size_t large_rocks=0,pinnacles=0,great_walls=0,preview_great_walls=0;
        for (std::size_t i=0;i<plan.regenerated->cells.size();++i) {
            const auto& c=plan.regenerated->cells[i];const auto f=c.selection.family;
            const bool active=!c.instance_index || (*c.instance_index<plan.regenerated_instance_active.size() &&
                plan.regenerated_instance_active[*c.instance_index]);
            const bool available=active && c.asset_index && plan.assets[*c.asset_index].status==StoredStatus::Rendered;
            const bool selected=c.selection.evidence==SelectorEvidence::Verified && available;
            if (f==LandscapeFamily::Rock || f==LandscapeFamily::Mountain) {
                if (selected) ++mountain_verified;else ++unresolved_mountains;
            }
            if (f==LandscapeFamily::Wall) {
                if (selected) ++wall_verified;else ++unresolved_walls;
            }
            if (f==LandscapeFamily::GreatWall) {
                if (selected) ++great_wall_verified;else if (!available) ++unresolved_walls;
            }
        }
        for (std::size_t n=0;n<plan.regenerated->instances.size();++n) {
            const auto& i=plan.regenerated->instances[n];
            if (n>=plan.regenerated_instance_active.size() || !plan.regenerated_instance_active[n] ||
                plan.assets[i.asset_index].status!=StoredStatus::Rendered) continue;
            if (i.geometry.selection.family==LandscapeFamily::Rock && i.geometry.side>1) ++large_rocks;
            if (i.geometry.selection.family==LandscapeFamily::Mountain) ++pinnacles;
            if (i.geometry.selection.family==LandscapeFamily::GreatWall && i.great_wall_context) {
                if (great_wall_original_context_verified(*i.great_wall_context)) ++great_walls;
                else if (i.great_wall_context->source==GreatWallContextSource::ExplicitPreview) ++preview_great_walls;
            }
        }
        regeneration.update({{"mountain_selector_verified",mountain_verified},{"rock_large_instances",large_rocks},
            {"pinnacle_instances",pinnacles},{"normal_wall_selector_verified",wall_verified},
            {"great_wall_selector_verified",great_wall_verified},{"great_wall_instances",great_walls},
            {"great_wall_preview_instances",preview_great_walls},
            {"unresolved_wall_cells",unresolved_walls},{"unresolved_mountain_cells",unresolved_mountains},
            {"selector_metric_unit","cells; instance metrics count whole selected geometries"}});
        std::size_t adjacent=0,gate_selected=0,gate_active=0,origins=0,unsupported=0;
        for (const auto& [candidate,wall]:plan.regenerated->normal_wall_topology) {
            if (wall.center_gate) ++origins;
            if (wall.gate_neighbor_mask && !wall.center_gate) ++adjacent;
            if (wall.gate_connection==WallGateConnectionStatus::UnsupportedTopology) ++unsupported;
            if (wall.gate_connection!=WallGateConnectionStatus::AdjacentStaticSelected) continue;
            ++gate_selected;
            const auto& c=plan.regenerated->cells[candidate];
            if (c.instance_index && *c.instance_index<plan.regenerated_instance_active.size() &&
                plan.regenerated_instance_active[*c.instance_index] && c.asset_index &&
                plan.assets[*c.asset_index].status==StoredStatus::Rendered) ++gate_active;
        }
        regeneration["normal_wall_gate_connections"]={{"inspected_wall_cells",plan.regenerated->normal_wall_topology.size()},
            {"gate_adjacent_cells",adjacent},{"selected_static_connections",gate_selected},
            {"active_static_connections",gate_active},{"selected_fallback_cells",gate_selected-gate_active},
            {"gate_origin_composition_required",origins},{"unsupported_topology",unsupported},
            {"gate_bodies_reproduced",0},{"optional_model_components_reproduced",0}};
        if (plan.original_ordinary_gates) {
            std::size_t selected_gates=0,active_gates=0,active_components=0,prepared_components=0;
            Json sources=Json::array();
            for (std::size_t n=0;n<plan.original_ordinary_gates->gates.size();++n) {
                const auto detail=ordinary_gate_json(plan,n);
                sources.push_back(detail);
                if (plan.original_ordinary_gates->gates[n].components.size()==15) ++selected_gates;
                const auto found=plan.regenerated->ordinary_gate_preparation.find(n);
                if (found!=plan.regenerated->ordinary_gate_preparation.end())
                    prepared_components+=found->second.instance_indices.size();
                active_components+=detail["active_components"].get<std::size_t>();
                if (detail["preparation_status"]=="active") ++active_gates;
            }
            regeneration["ordinary_gate_rendering"]={{"source_gate_objects",plan.original_ordinary_gates->gates.size()},
                {"selected_gate_objects",selected_gates},{"prepared_components",prepared_components},
                {"active_complete_gate_objects",active_gates},{"active_components",active_components},
                {"active_cells",active_components},{"reader_error",plan.original_ordinary_gates->error},
                {"source_gate_statuses",std::move(sources)},
                {"full_original_composition_verified",0},{"tower_bodies_reproduced",0}};
        }
    }
    return {{"original_great_wall",original},{"regeneration",regeneration},
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
