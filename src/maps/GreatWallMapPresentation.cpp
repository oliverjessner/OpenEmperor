#include "maps/GreatWallMapPresentation.h"
#include <set>

namespace openemperor::maps {
namespace {
constexpr std::size_t count=stored_grid_width*stored_grid_height;
std::size_t index(GridCell cell) { return std::size_t(cell.y)*stored_grid_width+cell.x; }
}
GreatWallMapPresentation prepare_great_wall_presentation(const OriginalMapEntities& entities,
    const std::map<std::int16_t,GreatWallModelResult>& models,GreatWallRestoreContext context) {
    GreatWallMapPresentation out;
    out.manager_offset=entities.logical_offset;out.manager_bytes=entities.byte_length;
    out.manager_records=entities.records.size();out.restore_context=context;
    const MapGeometry map{entities.declared_map_size};
    if (!map.supported) {out.error="unsupported original entity map geometry";return out;}
    out.piece_by_storage.resize(count);
    std::map<std::uint32_t,std::filesystem::path> registry;
    std::set<std::size_t> claims;
    for (const auto& entity:entities.records) {
        if (!entity.active() || entity.entity_class!=OriginalEntityClass::Monument ||
            entity.type<253 || entity.type>268) continue;
        GreatWallPresentationPiece piece;piece.source=entity;
        const auto model=models.find(entity.type);
        if (model==models.end() || !model->second.model) {
            piece.fallback=model==models.end() ? "required original Great Wall model unavailable":model->second.reason;
        } else if (entity.subindex<0 || std::size_t(entity.subindex)>=model->second.model->pieces.size()) {
            piece.fallback="original Great Wall subindex outside validated model";
        } else if (!entity.monument) {
            piece.fallback="original monument extended state unavailable";
        } else {
            piece.model_piece=model->second.model->pieces[std::size_t(entity.subindex)];
            piece.selection=select_great_wall({*piece.model_piece,entity.monument->phase,
                entity.monument->orientation,context});
            if (!piece.selection.supported) piece.fallback=piece.selection.reason;
        }
        if (piece.selection.supported && piece.selection.required_registration) {
            const auto& registration=*piece.selection.required_registration;
            registry[registration.slot]=std::filesystem::path("DATA")/
                (std::string(registration.archive_basename)+".sg3");
        }
        // Model offsets describe the monument's authored layout, not another
        // translation to add to already-serialized per-piece coordinates.
        if (context.camera_view!=0) piece.fallback="Great Wall placement is evidenced only for camera view zero";
        else if (piece.model_piece && valid_great_wall_model_piece(*piece.model_piece) &&
            entity.local_x>=0 && entity.local_y>=0) {
            const auto side=piece.model_piece->side;
            const GridCell origin{unsigned(entity.local_x)+map.border,unsigned(entity.local_y)+map.border};
            LandscapeInstanceSpec geometry;
            geometry.origin=origin;geometry.side=side;
            geometry.draw_cell={origin.x,origin.y+side-1};
            const GridCell front{origin.x+side-1,origin.y+side-1};
            geometry.depth_cell=front;
            geometry.explicit_height=LandscapeInstanceHeight{LandscapeInstanceHeightSource::SerializedCellHeight,geometry.draw_cell};
            geometry.explicit_anchor=LandscapeInstanceAnchor{front,int(40U*side-1U),20};
            geometry.original_entity_index=out.pieces.size();
            geometry.placement_evidence=piece.model_piece->kind==GreatWallPieceKind::Road ?
                "EXE-OBSERVED: 5724e0 singleton marker; ordinary Type30 caller anchor":
                "EXE-OBSERVED: 563dd0 marker; 470100/4704e0 full Type30 caller anchor";
            geometry.composition_evidence="bounded static Type30 base/overlay; original runtime context required";
            bool valid=side==1 || side==2 || side==4;
            std::vector<std::size_t> members;
            for (unsigned y=0;y<side && valid;++y) for (unsigned x=0;x<side;++x) {
                const GridCell cell{origin.x+x,origin.y+y};
                if (!map.contains(cell) || claims.contains(index(cell))) {valid=false;break;}
                geometry.owned_cells.push_back(cell);members.push_back(index(cell));
            }
            if (!valid || members.size()!=side*side) piece.fallback="original Great Wall claim outside map or conflicts with another piece";
            else {
                for (const auto member:members) {claims.insert(member);out.piece_by_storage[member]=out.pieces.size();}
                if (piece.selection.supported) {
                    const auto& s=piece.selection;
                    geometry.selection={LandscapeFamily::GreatWall,s.evidence,
                        "52f030 / 563fd0 / 57bba0",s.reason,s.group,s.variant,{}};
                    if (s.evidence==SelectorEvidence::Preview) {
                        geometry.placement_evidence="EXE-observed Type30 marker/anchor; OPENEMPEROR PREVIEW material context";
                        geometry.composition_evidence="OPENEMPEROR PREVIEW: static Type30 base/overlay; original restore context unverified";
                    }
                    if (s.slot==3) piece.archive_relative="DATA/China_Terrain.sg3";
                    else if (registry.contains(s.slot)) piece.archive_relative=registry.at(s.slot);
                    else piece.fallback="Great Wall inherited registration unavailable in restored entity order";
                }
                // Even unresolved pieces retain source-based geometry for F1.
                // They do not publish renderer ownership or suppress old images.
                piece.geometry=std::move(geometry);
            }
        }
        out.pieces.push_back(std::move(piece));
    }
    return out;
}
void read_great_wall_presentation(StoredGraphicsPlan& plan,const EmperorContainer& container,
    std::size_t part,GreatWallRestoreContext context) {
    if (!plan.landscape_layers_available || plan.original_great_wall) return;
    auto result=std::make_shared<GreatWallMapPresentation>();
    result->restore_context=context;
    try {
        const auto entities=read_original_map_entities(container,part);
        std::map<std::int16_t,GreatWallModelResult> models;
        for (const auto& e:entities.records) if (e.active() && e.entity_class==OriginalEntityClass::Monument &&
            e.type>=253 && e.type<=268 && !models.contains(e.type))
            models.emplace(e.type,load_great_wall_model(plan.data_root,e.type));
        // A standalone .map provides no proven mission/player restore context.
        // Keep absent material absent, including when every saved byte says 3.
        *result=prepare_great_wall_presentation(entities,models,context);
        for (const auto& piece:result->pieces) {
            const auto key=std::pair{piece.selection.slot,piece.archive_relative.generic_string()};
            if (!piece.selection.supported || !piece.fallback.empty() || piece.selection.slot==3 ||
                result->archives.contains(key)) continue;
            auto registration=load_regenerated_great_wall_registration(plan.data_root,piece.selection.slot,piece.archive_relative);
            if (context.source==GreatWallContextSource::ExplicitPreview &&
                (registration.archive_missing || !registration.optional_error.empty() ||
                 !registration.layout || !registration.catalog)) {
                if (result->error.empty()) result->error="required Great Wall preview archive unavailable or invalid: "+
                    piece.archive_relative.generic_string()+
                    (registration.optional_error.empty() ? std::string{}:"; "+registration.optional_error);
            }
            result->archives.emplace(key,std::move(registration));
        }
    } catch (const std::exception& error) {result->error=error.what();}
    plan.original_great_wall=std::move(result);
}
} // namespace openemperor::maps
