#include "maps/SandboxPlacement.h"

#include "maps/EmperorMap.h"
#include "maps/LandscapeProvenance.h"
#include "maps/MapCatalog.h"
#include "maps/MapRulesCheck.h"

#include <set>
#include <stdexcept>

namespace openemperor::maps {

std::vector<std::uint8_t> make_sandbox_buildable_mask(const StoredGraphicsPlan& plan,
                                                      const MapGeometry& geometry) {
    if (!geometry.supported || plan.border!=geometry.border)
        throw std::invalid_argument("sandbox map geometry mismatch");
    std::vector<std::uint8_t> mask(static_cast<std::size_t>(stored_grid_width)*stored_grid_height,0);
    for (const auto& cell:plan.cells) {
        if (cell.cell_index>=mask.size() ||
            cell.cell_index!=static_cast<std::size_t>(cell.storage.y)*stored_grid_width+cell.storage.x)
            throw std::invalid_argument("invalid stored cell for sandbox placement");
        if (!cell.footprint_index) continue;
        if (*cell.footprint_index>=plan.footprints.size())
            throw std::invalid_argument("invalid stored footprint for sandbox placement");
        const auto& footprint=plan.footprints[*cell.footprint_index];
        if (geometry.contains(cell.storage) && cell.status==StoredStatus::Rendered &&
            footprint.status==StoredStatus::Rendered &&
            footprint.width_cells==1 && footprint.height_cells==1 &&
            !cell.offmap_bit && cell.terrain_raw==0x80U && cell.objects_raw==0)
            mask[cell.cell_index]=1;
    }
    return mask;
}

namespace {
constexpr std::size_t permission_cells=stored_grid_width*stored_grid_height;
constexpr std::uint32_t offmap=0x80000U;
using simulation::BuildBlocker;
using simulation::Cell;
std::size_t storage(GridCell cell) {return std::size_t(cell.y)*stored_grid_width+cell.x;}
Cell simulation_cell(GridCell cell) {return {int(cell.x),int(cell.y)};}
enum class OriginalOccupancy { SavedSquare, MarkerOrigin, Unsupported };
OriginalOccupancy original_occupancy(const OriginalEntityRecord& entity) {
    if (entity.footprint_side>0 && entity.footprint_side<=16)
        return OriginalOccupancy::SavedSquare;
    if (entity.entity_class!=OriginalEntityClass::Industrial || entity.footprint_side!=0)
        return OriginalOccupancy::Unsupported;
    // Preserve the existing five marker contracts exactly.
    switch (entity.type) {
    case 162: case 163: case 174: case 180: case 185:
        return OriginalOccupancy::MarkerOrigin;
    case 175:
        // The evidenced saved exit point has no physical rectangle: the
        // original editor stores a coordinate pair and restore skips physical
        // reconstruction. We still protect its exact origin conservatively.
        // See docs/map-permissions-compatibility.md; zero side alone is never
        // sufficient, and other source states remain unsupported.
        if (entity.status==3 && entity.subindex==0 &&
            (entity.provenance.base_schema==3 || entity.provenance.base_schema==4) &&
            entity.provenance.wrapper_schema==1 && entity.provenance.extended_schema==1 &&
            entity.provenance.class_wrapper_schema==0)
            return OriginalOccupancy::MarkerOrigin;
        break;
    case 165:
        // The Water editor tool writes terrain at one point, not a building
        // owner rectangle. Its saved marker is also skipped by physical
        // restore. Reserve the origin; do not import the editor operation or
        // relax any raw terrain permission. These three base schemas are
        // independently observed with this exact saved state.
        if (entity.status==3 && entity.subindex==0 &&
            (entity.provenance.base_schema==3 || entity.provenance.base_schema==4 ||
             entity.provenance.base_schema==5) && entity.provenance.wrapper_schema==1 &&
            entity.provenance.extended_schema==1 && entity.provenance.class_wrapper_schema==0)
            return OriginalOccupancy::MarkerOrigin;
        break;
    case 183:
        // The evidenced editor/undo paths store a coordinate point; physical
        // standalone restore skips this type. Protect each saved origin under
        // the existing marker contract, without importing fauna behavior or
        // treating side zero alone as permission. Only these saved states are
        // evidenced. See docs/reverse/original-occupancy-pass-next.md.
        if (entity.status==3 && entity.subindex==0 &&
            (entity.provenance.base_schema==3 || entity.provenance.base_schema==4) &&
            entity.provenance.wrapper_schema==1 && entity.provenance.extended_schema==1 &&
            entity.provenance.class_wrapper_schema==0)
            return OriginalOccupancy::MarkerOrigin;
        break;
    default: break;
    }
    return OriginalOccupancy::Unsupported;
}
void permission_require(bool condition,const char* reason) {
    if (!condition) throw std::invalid_argument(std::string("map permissions: ")+reason);
}
void original_rules_require(bool condition,const char* reason) {
    if (!condition) throw SandboxMapRulesUnsupported(std::string("map permissions: ")+reason);
}
[[noreturn]] void unsupported_occupancy(const OriginalEntityRecord& entity,GridCell origin,
                                      const ParsedEmperorMap& map) {
    const auto at=storage(origin);
    throw SandboxMapRulesUnsupported("map permissions: unsupported active original occupancy: ID "+
        std::to_string(entity.serialized_original_id)+", manager "+std::to_string(entity.manager_index)+
        ", "+original_entity_class_name(entity.entity_class)+" type "+std::to_string(entity.type)+
        ", status "+std::to_string(entity.status)+", side "+std::to_string(entity.footprint_side)+
        ", subindex "+std::to_string(entity.subindex)+", base/wrapper/state "+
        std::to_string(entity.provenance.base_schema)+"/"+std::to_string(entity.provenance.wrapper_schema)+
        "/"+std::to_string(entity.provenance.extended_schema)+", local ("+
        std::to_string(entity.local_x)+","+std::to_string(entity.local_y)+"), storage ("+
        std::to_string(origin.x)+","+std::to_string(origin.y)+"), reference "+
        std::to_string(entity.serialized_cell_reference)+", record logical "+
        std::to_string(entity.provenance.logical_record_offset)+", terrain "+
        std::to_string(map.terrain_raw.values[at])+", objects "+std::to_string(map.objects_raw.values[at]));
}
std::int8_t signed_height(std::uint8_t byte) {
    return std::int8_t(byte<128U ? int(byte):int(byte)-256);
}
struct OriginalRulePreparation {
    std::vector<simulation::MapCellPermission> cells;
    std::vector<simulation::FixedGatePassage> passages;
};
OriginalRulePreparation prepare_original_rules(
    const ParsedEmperorMap& map,const MapGeometry& geometry,
    const OriginalMapEntities& entities,
    std::span<const std::uint8_t> height_bytes,std::uint32_t policy_version) {
    permission_require(policy_version==simulation::kMapPermissionsPolicyVersion,"unsupported policy version");
    permission_require(geometry.supported && map.declared_map_size==geometry.declared_size &&
        map.stored_width==stored_grid_width && map.stored_height==stored_grid_height &&
        map.terrain_raw.values.size()==permission_cells && map.objects_raw.values.size()==permission_cells &&
        height_bytes.size()==permission_cells,
        "incomplete or mismatched original map/height inputs");
    permission_require(entities.manager_schema==1 && entities.declared_map_size==map.declared_map_size &&
        entities.records.size()<=maximum_original_entity_records,"complete supported original manager required");
    std::vector<simulation::MapCellPermission> cells(permission_cells);
    for (unsigned y=0;y<stored_grid_height;++y) for (unsigned x=0;x<stored_grid_width;++x) {
        const GridCell cell{x,y};const auto at=storage(cell);auto& out=cells[at];
        out.height=signed_height(height_bytes[at]);
        if (!geometry.contains(cell) || (map.terrain_raw.values[at]&offmap)) {
            out.road_blocker=out.building_blocker=BuildBlocker::OutsideMap;continue;
        }
        // Exact raw ground/ground-road markings diagnosed on the gate maps.
        // This permits a paid sandbox road, not a free imported World road.
        const auto terrain=map.terrain_raw.values[at];
        out.road_allowed=(terrain==0x80U || terrain==0xc0U) && map.objects_raw.values[at]==0;
        out.road_blocker=out.road_allowed ? BuildBlocker::None:BuildBlocker::UnsupportedTerrain;
        // Building authority is deliberately absent here. Only the actual
        // producer below can apply its renderer-derived legacy mask.
        out.building_blocker=BuildBlocker::LegacyBuildabilityRestriction;
        // Raw structure terrain remains protected without inventing an entity
        // or an original body from a saved graphic/visible grass pixel.
        if (map.terrain_raw.values[at]&0xc008U) {
            out.protected_original=true;out.road_allowed=out.building_allowed=false;
            out.road_blocker=out.building_blocker=BuildBlocker::OriginalStructure;
        }
    }
    std::set<std::uint32_t> ids,manager_indices;
    std::vector<std::optional<std::uint32_t>> owner(permission_cells);
    std::vector<std::uint8_t> marker_reserved(permission_cells,0);
    std::vector<const OriginalEntityRecord*> gates;
    // Reserve all complete original occupancy before admitting any passage.
    // Non-Gate saved square footprints are a conservative reservation, not an
    // assertion that their complete original model composition is reproduced.
    for (const auto& entity:entities.records) {
        if (!entity.active()) continue;
        original_rules_require(static_cast<unsigned>(entity.entity_class)<=static_cast<unsigned>(OriginalEntityClass::Tower) &&
            (entity.provenance.base_schema==3 || entity.provenance.base_schema==4 || entity.provenance.base_schema==5),
            "unknown active original class/base schema");
        if (entity.entity_class==OriginalEntityClass::GateHouse)
            original_rules_require(entity.provenance.wrapper_schema==1 && entity.provenance.extended_schema==1 &&
                (entity.provenance.class_wrapper_schema==1 || entity.provenance.class_wrapper_schema==2),
                "unknown active GateHouse wrapper schema");
        if (entity.entity_class==OriginalEntityClass::Tower)
            original_rules_require(entity.provenance.wrapper_schema==1 && entity.provenance.extended_schema==1 &&
                entity.provenance.class_wrapper_schema==0,"unknown active Tower wrapper schema");
        permission_require(entity.original_id && entity.original_id->value>0 &&
            entity.original_id->value<=maximum_original_entity_records &&
            entity.serialized_original_id==std::int32_t(entity.original_id->value) &&
            ids.insert(entity.original_id->value).second &&
            entity.manager_index<maximum_original_entity_records && manager_indices.insert(entity.manager_index).second,
            "invalid or duplicate active original identity");
        permission_require(entity.local_x>=0 && entity.local_y>=0 &&
            unsigned(entity.local_x)<map.declared_map_size && unsigned(entity.local_y)<map.declared_map_size,
            "unbounded active original origin");
        const GridCell origin{unsigned(entity.local_x)+geometry.border,unsigned(entity.local_y)+geometry.border};
        permission_require(entity.serialized_cell_reference==std::int32_t(storage(origin)),
            "active original origin/reference mismatch");
        unsigned width=entity.footprint_side,height=entity.footprint_side;
        const bool gate=entity.entity_class==OriginalEntityClass::GateHouse;
        const auto occupancy=original_occupancy(entity);
        const bool marker=!gate && occupancy==OriginalOccupancy::MarkerOrigin;
        if (gate) {
            original_rules_require(entity.type==130 && entity.gate_house &&
                (entity.gate_house->layout==0 || entity.gate_house->layout==1),"unsupported original GateHouse type/layout");
            width=entity.gate_house->layout==0 ? 5U:3U;height=entity.gate_house->layout==0 ? 3U:5U;
            gates.push_back(&entity);
        } else if (marker) width=height=1;
        else if (occupancy==OriginalOccupancy::Unsupported) unsupported_occupancy(entity,origin,map);
        for (unsigned dy=0;dy<height;++dy) for (unsigned dx=0;dx<width;++dx) {
            const GridCell cell{origin.x+dx,origin.y+dy};
            permission_require(geometry.contains(cell),"active original footprint outside map");
            const auto at=storage(cell);
            permission_require(!(map.terrain_raw.values[at]&offmap),"active original footprint on off-map terrain");
            original_rules_require(!owner[at] && (marker || !marker_reserved[at]),"conflicting complete original occupancy");
            if (marker) marker_reserved[at]=1;
            else owner[at]=entity.original_id->value;
            if (gate) permission_require((map.terrain_raw.values[at]&0x8008U)==0x8008U &&
                !(map.terrain_raw.values[at]&0x4000U),"incomplete/conflicting original GateHouse raw footprint");
            auto& out=cells[at];out.protected_original=true;out.road_allowed=out.building_allowed=false;
            out.road_blocker=out.building_blocker=gate ? BuildBlocker::GateSolidPart:BuildBlocker::OriginalStructure;
        }
    }
    std::vector<simulation::FixedGatePassage> passages;passages.reserve(gates.size());
    for (const auto* entity:gates) {
        const GridCell origin{unsigned(entity->local_x)+geometry.border,unsigned(entity->local_y)+geometry.border};
        const bool layout1=entity->gate_house->layout==1;
        simulation::FixedGatePassage passage;passage.id={entity->original_id->value};
        const unsigned width=layout1 ? 3U:5U,height=layout1 ? 5U:3U;
        const auto level=cells[storage(origin)].height;
        for (unsigned dy=0;dy<height;++dy) for (unsigned dx=0;dx<width;++dx) {
            const GridCell cell{origin.x+dx,origin.y+dy};
            original_rules_require(cells[storage(cell)].height==level,"unsupported height variation in original GateHouse");
            passage.protected_footprint.push_back(simulation_cell(cell));
        }
        // OpenEmperor-authored short-axis corridor, perpendicular to the
        // evidenced wall axis. No alpha scan or reconstructed military rule.
        for (unsigned step=0;step<3;++step) {
            const GridCell cell{origin.x+(layout1 ? step:2U),origin.y+(layout1 ? 2U:step)};
            passage.corridor.push_back(simulation_cell(cell));
        }
        const Cell direction=layout1 ? Cell{1,0}:Cell{0,1};
        const auto first=passage.corridor.front(),last=passage.corridor.back();
        passage.openings={Cell{first.x-direction.x,first.y-direction.y},Cell{last.x+direction.x,last.y+direction.y}};
        for (const auto opening:passage.openings) {
            permission_require(opening.x>=0 && opening.y>=0 && geometry.contains({unsigned(opening.x),unsigned(opening.y)}),
                "original GateHouse opening outside map");
            const auto at=storage({unsigned(opening.x),unsigned(opening.y)});
            original_rules_require(!owner[at] && !cells[at].protected_original,"original GateHouse opening conflicts with original structure");
            original_rules_require(cells[at].height==level,"unsupported height transition at original GateHouse opening");
        }
        for (const auto cell:passage.corridor) {
            auto& out=cells[storage({unsigned(cell.x),unsigned(cell.y)})];
            out.road_allowed=true;out.road_blocker=BuildBlocker::None;
        }
        passages.push_back(std::move(passage));
    }
    return {std::move(cells),std::move(passages)};
}
} // namespace

void validate_sandbox_original_map_rules(
    const ParsedEmperorMap& map,const MapGeometry& geometry,const OriginalMapEntities& entities,
    std::span<const std::uint8_t> height_bytes,std::uint32_t policy_version) {
    (void)prepare_original_rules(map,geometry,entities,height_bytes,policy_version);
}

std::shared_ptr<const simulation::MapPermissions> prepare_sandbox_map_permissions(
    const ParsedEmperorMap& map,const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask,const OriginalMapEntities& entities,
    std::span<const std::uint8_t> height_bytes,std::uint32_t policy_version) {
    permission_require(legacy_mask.size()==permission_cells,"incomplete or mismatched legacy input");
    for (const auto byte:legacy_mask) permission_require(byte<=1,"invalid legacy mask byte");
    auto original=prepare_original_rules(map,geometry,entities,height_bytes,policy_version);
    for (std::size_t at=0;at<permission_cells;++at) {
        auto& out=original.cells[at];
        if (out.protected_original || out.building_blocker==BuildBlocker::OutsideMap) continue;
        out.building_allowed=legacy_mask[at]!=0 && map.terrain_raw.values[at]==0x80U &&
            map.objects_raw.values[at]==0;
        out.building_blocker=out.building_allowed ? BuildBlocker::None:BuildBlocker::LegacyBuildabilityRestriction;
    }
    return std::make_shared<const simulation::MapPermissions>(int(stored_grid_width),int(stored_grid_height),
        policy_version,std::move(original.cells),std::move(original.passages));
}

std::shared_ptr<const simulation::MapPermissions> make_sandbox_map_permissions(
    const ParsedEmperorMap& map,const StoredGraphicsPlan& plan,const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask,const OriginalMapEntities& entities,std::uint32_t policy_version) {
    permission_require(plan.border==geometry.border,"presentation/map geometry mismatch");
    return prepare_sandbox_map_permissions(map,geometry,legacy_mask,entities,plan.height_bytes,policy_version);
}
std::shared_ptr<const simulation::MapPermissions> load_sandbox_map_permissions(
    const ParsedEmperorMap& map,const StoredGraphicsPlan& plan,const MapGeometry& geometry,
    std::span<const std::uint8_t> legacy_mask,std::uint32_t policy_version,
    const std::string& expected_input_sha256) {
    permission_require(policy_version==simulation::kMapPermissionsPolicyVersion,"unsupported policy version");
    const auto container=EmperorContainer::open(resolve_map_path(plan.data_root,plan.map_relative));
    if (!expected_input_sha256.empty() && map_input_sha256(container)!=expected_input_sha256)
        throw std::runtime_error("Map changed since the map-rules check. Check the selected map again.");
    permission_require(!container.multipart(),"requires standalone original map");
    const auto entities=read_original_map_entities(container,0);
    return make_sandbox_map_permissions(map,plan,geometry,legacy_mask,entities,policy_version);
}
std::shared_ptr<const simulation::MapPermissions> read_sandbox_map_permissions(
    const std::filesystem::path& data_root,const std::filesystem::path& map_relative,
    std::span<const std::uint8_t> legacy_mask,std::uint32_t policy_version) {
    permission_require(policy_version==simulation::kMapPermissionsPolicyVersion,"unsupported policy version");
    const auto container=EmperorContainer::open(resolve_map_path(data_root,map_relative));
    permission_require(!container.multipart(),"requires standalone original map");
    const auto map=read_emperor_map(container,0);const MapGeometry geometry{map.declared_map_size};
    const auto entities=read_original_map_entities(container,0);
    const auto heights=container.read_range(0,landscape_height_offset,permission_cells);
    return prepare_sandbox_map_permissions(map,geometry,legacy_mask,entities,heights,policy_version);
}

} // namespace openemperor::maps
