#include "maps/RegeneratedMapRenderPlan.h"
#include "maps/PinnacleSelector.h"
#include "maps/WallTopology.h"
#include "maps/GreatWallMapPresentation.h"
#include <chrono>
#include <memory>
#include <algorithm>

namespace openemperor::maps {
std::optional<PackedGraphicId> resolve_landscape_variant(ResourceGroupKey key,
    unsigned variant,const std::map<std::uint32_t,GroupRegistration>& registrations) {
    const auto g=resolve_resource_group(key,registrations);
    if (g.status!=GroupLookupStatus::Resolved || !g.packed_base || !g.local_base) return {};
    const auto& layout=*registrations.at(g.slot).layout;
    auto end=layout.runtime_image_count;
    for (const auto& row:layout.groups)
        if (row.local_base>*g.local_base && row.local_base<end) end=row.local_base;
    if (end<=*g.local_base || variant>=end-*g.local_base ||
        variant>0x3fffU-*g.local_base) return {};
    return PackedGraphicId{g.packed_base->value+variant};
}
void build_regenerated_map_render_plan(StoredGraphicsPlan& plan,
    const StoredArchiveRegistrations& registrations) {
    if (!plan.landscape_layers_available || plan.regenerated) return;
    const auto start=std::chrono::steady_clock::now();
    auto generated=std::make_shared<RegeneratedMapRenderPlan>();
    generated->historical_asset_count=plan.assets.size();
    generated->cells.reserve(plan.cells.size());
    generated->footprint_assets.resize(plan.footprints.size());
    std::map<std::uint32_t,GroupRegistration> groups;
    std::map<std::uint32_t,GraphicsArchiveRegistration> images;
    for (const auto& [slot,r]:registrations) {
        groups[slot]={r.layout ? &*r.layout:nullptr};
        images[slot]={r.catalog ? &*r.catalog:nullptr,r.layout ? &*r.layout:nullptr,r.archive_missing};
    }
    using AssetKey=std::pair<std::string,std::uint32_t>;
    std::map<AssetKey,std::size_t> pool;
    for (std::size_t i=0;i<plan.assets.size();++i) {
        const auto& id=plan.assets[i].record.id;
        pool[{id.archive_relative_path.generic_string(),id.image_index}]=i;
    }
    const LandscapeSelectorInput input{plan.raw_terrain,plan.raw_objects,
        plan.variation_bytes,plan.fertility_bytes,std::nullopt,0};
    for (const auto& c:plan.cells) {
        RegeneratedCell next;
        next.selection=select_landscape(input,c.storage);
        // The existing elevation/large-footprint placement remains frozen.
        if (c.slot==16 || !c.footprint_index ||
            plan.footprints[*c.footprint_index].width_cells!=1) {
            next.fallback="preserved elevation/footprint preview";
        } else if (next.selection.evidence!=SelectorEvidence::Unresolved) {
            next.graphic=resolve_landscape_variant(next.selection.group,next.selection.variant,groups);
            if (next.graphic) {
                const auto found=resolve_graphics_id_hypothesis(next.graphic->value,images);
                const auto* r=found.record;
                if (found.status==GraphicsIdStatus::DecodeCandidate && r &&
                    r->image_type==30 && r->width==78 && r->height>=40 &&
                    r->uncompressed_length==3200 && r->isometric_size_flag==1 &&
                    r->horizontal_mirror_offset==0 && r->animation_sprites==0) {
                    const AssetKey key{r->id.archive_relative_path.generic_string(),r->id.image_index};
                    auto existing=pool.find(key);
                    if (existing==pool.end() && plan.assets.size()<stored_max_assets) {
                        const auto index=plan.assets.size();
                        plan.assets.push_back({*r,StoredStatus::DecodePending,false,false,{}});
                        existing=pool.emplace(key,index).first;
                    }
                    if (existing!=pool.end()) {
                        next.asset_index=existing->second;
                        generated->footprint_assets[*c.footprint_index]=next.asset_index;
                        next.fallback="historical preview only if eager decode fails";
                    } else next.fallback="physical asset budget exceeded";
                } else next.fallback="regenerated record unsupported; historical preview retained";
            } else next.fallback="group/variant unresolved; historical preview retained";
        }
        generated->cells.push_back(next);
    }
    // New geometry has its own ownership and generation order. It does not
    // reuse a saved graphic's origin, size or footprint partition.
    constexpr auto count=stored_grid_width*stored_grid_height;
    std::vector<std::uint8_t> eligible(count,0);
    for (const auto& c:plan.cells) {
        // Preserve the current elevation presentation at this bounded pass.
        // This is a preview boundary, not evidence that a saved ID survived.
        if (c.slot!=16) eligible[c.cell_index]=1;
    }
    const auto append_instance=[&](LandscapeInstanceSpec spec,
        const StoredArchiveRegistration* original_registration=nullptr,
        const GreatWallRestoreContext* wall_context=nullptr,
        LandscapeCompositionPolicy composition_policy=LandscapeCompositionPolicy::EarlyBaseSpatialOverlay) {
        const bool original_wall=spec.original_entity_index.has_value();
        std::vector<std::size_t> members;
        for (const auto cell:spec.owned_cells) {
            if (cell.x>=stored_grid_width || cell.y>=stored_grid_height) return false;
            const auto raw=std::size_t(cell.y)*stored_grid_width+cell.x;
            if ((!eligible[raw] && !original_wall) || plan.cell_by_storage.size()!=count || !plan.cell_by_storage[raw]) return false;
            const auto index=*plan.cell_by_storage[raw];
            if (generated->cells[index].instance_index) return false;
            members.push_back(index);
        }
        auto instance_groups=groups;
        auto instance_images=images;
        if (original_registration) {
            const auto& r=*original_registration;
            instance_groups[r.slot]={r.layout ? &*r.layout:nullptr};
            instance_images[r.slot]={r.catalog ? &*r.catalog:nullptr,r.layout ? &*r.layout:nullptr,r.archive_missing};
        }
        const auto graphic=resolve_landscape_variant(spec.selection.group,spec.selection.variant,instance_groups);
        if (!graphic) return false;
        const auto resolution=resolve_graphics_id_hypothesis(graphic->value,instance_images);
        const auto* r=resolution.record;
        const auto side=spec.side;
        if (resolution.status!=GraphicsIdStatus::DecodeCandidate || !r ||
            r->image_type!=30 || r->width!=int(80U*side-2U) || r->height<int(40U*side) ||
            r->uncompressed_length!=3200U*side*side || r->isometric_size_flag!=side ||
            r->horizontal_mirror_offset!=0 || r->animation_sprites!=0) return false;
        const AssetKey key{r->id.archive_relative_path.generic_string(),r->id.image_index};
        auto existing=pool.find(key);
        if (existing==pool.end()) {
            if (plan.assets.size()>=stored_max_assets) return false;
            const auto index=plan.assets.size();
            plan.assets.push_back({*r,StoredStatus::DecodePending,false,false,{}});
            existing=pool.emplace(key,index).first;
        } else if (plan.assets[existing->second].status!=StoredStatus::Rendered) {
            // Metadata was previously refused only by the historical footprint
            // policy. The unchanged decoder supports this evidenced geometry.
            plan.assets[existing->second].status=StoredStatus::DecodePending;
        }
        const auto id=generated->instances.size();
        for (const auto member:members) {
            auto& c=generated->cells[member];c.selection=spec.selection;
            c.graphic=graphic;c.asset_index=existing->second;c.instance_index=id;
            c.fallback="whole regenerated instance, or atomic historical preview after decode failure";
        }
        const auto composition=spec.original_entity_index;
        if (original_wall) for (const auto cell:spec.owned_cells) eligible[std::size_t(cell.y)*stored_grid_width+cell.x]=0;
        generated->instances.push_back({std::move(spec),*graphic,existing->second,std::move(members),composition,
            wall_context ? std::optional{*wall_context}:std::nullopt,composition_policy});
        return true;
    };
    // Object restore precedes landscape generation. Only complete supported
    // objects claim renderer ownership; unresolved metadata retains old draws.
    if (plan.original_great_wall) for (const auto& piece:plan.original_great_wall->pieces) {
        if (!piece.selection.supported || !piece.geometry || !piece.fallback.empty()) continue;
        const StoredArchiveRegistration* r=nullptr;
        if (piece.selection.slot!=3) {
            const auto found=plan.original_great_wall->archives.find(
                {piece.selection.slot,piece.archive_relative.generic_string()});
            if (found==plan.original_great_wall->archives.end()) continue;
            r=&found->second;
        }
        // A validated wall/tower/gate is a spatial object even when its SG3
        // Base contains some of the masonry. Model roads keep their existing
        // ground composition; no asset ID or pixel color chooses this role.
        const auto policy=piece.model_piece &&
            (piece.model_piece->kind==GreatWallPieceKind::Wall ||
             piece.model_piece->kind==GreatWallPieceKind::Tower ||
             piece.model_piece->kind==GreatWallPieceKind::Gate) ?
                LandscapeCompositionPolicy::SpatialCombined:
                LandscapeCompositionPolicy::EarlyBaseSpatialOverlay;
        if (!append_instance(*piece.geometry,r,&piece.selection.restore_context,policy)) {
            std::string reason="selected Great Wall complete geometry or image unavailable; historical preview retained";
            if (r && r->layout) {
                const std::map<std::uint32_t,GroupRegistration> selected_groups{{r->slot,{&*r->layout}}};
                const auto graphic=resolve_landscape_variant(piece.selection.group,piece.selection.variant,selected_groups);
                if (!graphic) reason="Great Wall variant "+std::to_string(piece.selection.variant)+
                    " outside registered archive group "+piece.archive_relative.generic_string()+"; historical preview retained";
                else if (r->catalog) {
                    const std::map<std::uint32_t,GraphicsArchiveRegistration> selected_images{
                        {r->slot,{&*r->catalog,&*r->layout,r->archive_missing}}};
                    const auto found=resolve_graphics_id_hypothesis(graphic->value,selected_images);
                    if (found.record && found.record->image_type==1)
                        reason="selected Great Wall Type-1 image layout unsupported; historical preview retained";
                }
            }
            generated->original_wall_fallbacks.emplace(*piece.geometry->original_entity_index,std::move(reason));
        }
    }
    for (auto spec:derive_rock_instances(input,eligible)) {
        if (!append_instance(spec)) for (const auto cell:spec.owned_cells) {
            const auto raw=std::size_t(cell.y)*stored_grid_width+cell.x;
            if (plan.cell_by_storage.size()==count && plan.cell_by_storage[raw]) {
                auto& c=generated->cells[*plan.cell_by_storage[raw]];
                c.selection=spec.selection;c.selection.evidence=SelectorEvidence::Unresolved;
                c.fallback="selected rock instance geometry/asset unavailable; historical preview retained";
            }
        }
    }
    for (const auto& c:plan.cells) {
        if (!(c.terrain_raw&0x2000000U)) continue;
        const auto pin=select_pinnacle(input,c.storage,plan.draw_properties,plan.raw_candidate_bytes);
        if (!pin || pin->origin!=c.storage) continue;
        LandscapeInstanceSpec spec;
        spec.selection={LandscapeFamily::Mountain,SelectorEvidence::Verified,"53fa20 / 4b7bd0 / 4b72b0",
            pin->reason,pin->group,0,{}};
        spec.origin=pin->origin;spec.side=pin->side;
        spec.draw_cell={pin->origin.x,pin->origin.y+pin->side-1};
        spec.placement_evidence="EXE-OBSERVED: canonical five-cell ownership; OPENEMPEROR PREVIEW: caller anchor";
        for (unsigned y=0;y<pin->side;++y) for (unsigned x=0;x<pin->side;++x)
            spec.owned_cells.push_back({pin->origin.x+x,pin->origin.y+y});
        append_instance(std::move(spec));
    }
    std::vector<std::optional<unsigned>> wall_variants(count);
    constexpr std::array<int,8> dx{0,1,1,1,0,-1,-1,-1},dy{-1,-1,0,1,1,1,0,-1};
    for (unsigned y=0;y<stored_grid_height;++y) for (unsigned x=0;x<stored_grid_width;++x) {
        const auto at=std::size_t(y)*stored_grid_width+x;
        if (!eligible[at] || !(plan.raw_terrain[at]&0x4000U)) continue;
        WallTopologyInput wall;wall.center_terrain=plan.raw_terrain[at];wall.orientation=input.orientation;
        for (unsigned d=0;d<8;++d) {
            const int nx=int(x)+dx[d],ny=int(y)+dy[d];
            if (nx<0 || ny<0 || nx>=int(stored_grid_width) || ny>=int(stored_grid_height)) continue;
            const auto neighbor=std::size_t(ny)*stored_grid_width+unsigned(nx);
            wall.neighboring_terrain[d]=plan.raw_terrain[neighbor];
            if (!(d&1U)) wall.generated_cardinal_variants[d/2]=wall_variants[neighbor];
        }
        const auto selected=select_normal_wall(wall);
        const auto candidate=*plan.cell_by_storage[at];
        generated->cells[candidate].selection.reason=selected.reason;
        if (!selected.verified || !selected.variant) continue;
        if (!registrations.contains(2U) || !registrations.at(2U).layout || !registrations.at(2U).catalog) {
            generated->cells[candidate].fallback="optional General wall metadata unavailable; historical preview retained";
            continue;
        }
        wall_variants[at]=selected.variant;
        LandscapeInstanceSpec spec;
        spec.selection={LandscapeFamily::Wall,SelectorEvidence::Verified,"4b67b0 / 4b8f70 / 4bbdc0 / 4bee90",
            selected.reason,{normal_wall_resource_key},*selected.variant,{}};
        spec.origin={x,y};spec.draw_cell=spec.origin;spec.owned_cells={spec.origin};
        spec.composition_evidence="EXE-OBSERVED: static wall identity; extra model caps/stairs and gates unresolved";
        append_instance(std::move(spec));
    }
    // Great Wall identity belongs to restored original object/piece state, not
    // a raw-terrain or saved-ID selector. Retain only the labeled old preview.
    for (std::size_t i=0;i<plan.cells.size();++i) if (plan.cells[i].slot==8 && !generated->cells[i].instance_index) {
        auto& c=generated->cells[i];
        c.selection={LandscapeFamily::GreatWall,SelectorEvidence::Unresolved,"52f030 / 4b11f0 / 57bba0",
            "restored monument stage, orientation and piece state unavailable in landscape inputs",{0x1001},0,{}};
        c.fallback="historical slot-8 footprint preview; no regenerated Great Wall instance";
        if (plan.original_great_wall && plan.original_great_wall->piece_by_storage.size()==count &&
            plan.original_great_wall->piece_by_storage[plan.cells[i].cell_index]) {
            c.selection.reason="original entity and model read; external material context or whole composition unresolved";
            c.fallback="historical slot-8 preview retained; raw saved material is not restored authority";
        }
    }
    generated->build_milliseconds=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count();
    plan.regenerated=std::move(generated);
}
} // namespace openemperor::maps
