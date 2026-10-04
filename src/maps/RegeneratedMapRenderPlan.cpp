#include "maps/RegeneratedMapRenderPlan.h"
#include <chrono>
#include <memory>

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
    generated->build_milliseconds=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count();
    plan.regenerated=std::move(generated);
}
} // namespace openemperor::maps
