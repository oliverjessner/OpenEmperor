#include "maps/GreatWallMapPresentation.h"
#include "maps/RegeneratedMapRenderPlan.h"
#include "maps/LandscapeProvenance.h"
#include "core/PerformanceDiagnostics.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>
using namespace openemperor::maps;
namespace {
constexpr std::size_t count=stored_grid_width*stored_grid_height;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
OriginalMapEntities entities() {
    OriginalMapEntities result;result.declared_map_size=170;result.logical_offset=1093607;
    for (unsigned n=0;n<3;++n) {
        OriginalEntityRecord r;r.manager_index=n;r.original_id=OriginalEntityId{n+1};
        r.entity_class=OriginalEntityClass::Monument;r.status=3;r.type=257;r.subindex=std::int16_t(n);
        r.local_x=std::int16_t(55+6*n);r.local_y=32;r.monument=OriginalMonumentState{n==1 ? 1:10,3,4,0};
        r.provenance.logical_record_offset=1093808+322*n;r.provenance.logical_base_offset=r.provenance.logical_record_offset+2;
        r.provenance.logical_extended_offset=r.provenance.logical_base_offset+183;
        r.provenance.base_schema=4;r.provenance.extended_schema=10;
        result.records.push_back(r);
    }
    return result;
}
std::map<std::int16_t,GreatWallModelResult> models() {
    GreatWallModel m;m.monument_type=257;
    m.pieces={{0,0,GreatWallPieceKind::Tower,4,0,26,4},
        {6,0,GreatWallPieceKind::Gate,4,2,1,2},{12,0,GreatWallPieceKind::Tower,4,0,26,4}};
    return {{257,{m,"synthetic bounded model"}}};
}
StoredArchiveRegistration archive() {
    StoredArchiveRegistration r;r.slot=8;r.relative_path="DATA/China_Mon_Greatwall_10.sg3";
    r.layout.emplace();auto& l=*r.layout;l.slot=8;l.sg3_version=213;l.verified_registration=true;
    l.first_physical_record=201;l.runtime_image_count=42;l.image_capacity=243;l.system_record_skip=200;
    l.groups={{0,80,201,0}};
    r.catalog.emplace();r.catalog->records.resize(243);
    for (unsigned i=201;i<=242;++i) {
        auto& a=r.catalog->records[i];a.id={r.relative_path,i};a.image_type=30;
        const auto side=i==229 ? 2U:4U;
        a.width=std::int16_t(80*side-2);a.height=std::int16_t(40*side+100);
        a.uncompressed_length=3200*side*side;a.data_length=a.uncompressed_length+300;
        a.isometric_size_flag=std::uint8_t(side);a.payload_in_bounds=true;
        a.decoder_supported=true;a.color_decoder_supported=true;a.color_bounds=openemperor::assets::AssetRangeStatus::InBounds;
    }
    return r;
}
StoredGraphicsPlan plan(const GreatWallMapPresentation& state) {
    StoredGraphicsPlan p;p.border=29;p.landscape_layers_available=true;p.original_great_wall=std::make_shared<GreatWallMapPresentation>(state);
    p.raw_terrain.assign(count,0x82);p.raw_objects.assign(count,0);p.raw_saved_ids.assign(count,123);
    p.raw_candidate_bytes.assign(count,0x40);p.draw_properties.assign(count,0);p.height_bytes.assign(count,4);
    p.variation_bytes.assign(count,0);p.fertility_bytes.assign(count,0);
    p.cell_by_storage.resize(count);p.status_by_storage.assign(count,StoredStatus::Excluded);
    for (std::size_t raw=0;raw<count;++raw) if (state.piece_by_storage[raw]) {
        StoredCell c;c.storage={unsigned(raw%228),unsigned(raw/228)};c.cell_index=raw;c.slot=8;c.stored_id=123;c.terrain_raw=0x82;
        c.footprint_index=p.footprints.size();p.cell_by_storage[raw]=p.cells.size();p.cells.push_back(c);
        PlacedFootprint f;f.id=p.footprints.size();f.origin=c.storage;f.cell_indices={p.cells.size()-1};p.footprints.push_back(f);
    }
    return p;
}
struct Fixture {
    OriginalMapEntities source;
    std::map<std::int16_t,GreatWallModelResult> models;
};
Fixture sequence_fixture(const std::vector<std::pair<GreatWallPieceKind,std::int32_t>>& pieces) {
    Fixture f;f.source.declared_map_size=170;f.source.logical_offset=1093607;
    GreatWallModel model;model.monument_type=257;
    for (std::size_t n=0;n<pieces.size();++n) {
        const auto [kind,phase]=pieces[n];
        auto record=entities().records[0];record.manager_index=std::uint32_t(n);
        record.original_id=OriginalEntityId{std::uint32_t(n+1)};record.subindex=std::int16_t(n);
        record.local_x=std::int16_t(70+5*(n%9));record.local_y=std::int16_t(60+5*(n/9));
        record.monument->phase=phase;record.provenance.logical_record_offset+=322*n;
        record.provenance.logical_base_offset+=322*n;*record.provenance.logical_extended_offset+=322*n;
        f.source.records.push_back(record);
        model.pieces.push_back({int(3*n),int(2*n),kind,4,kind==GreatWallPieceKind::Road ? 2U:0U,
            0,great_wall_piece_side(kind)});
    }
    f.models.emplace(257,GreatWallModelResult{std::move(model),"authored restore-order fixture"});
    return f;
}
StoredArchiveRegistration road_archive(const std::filesystem::path& requested,unsigned length=42) {
    auto r=archive();r.relative_path=requested;r.layout->runtime_image_count=length;
    // Catalog padding is deliberately populated even for the 40-record group.
    // An available physical record cannot enlarge that group's legal domain.
    for (auto& record:r.catalog->records) record.id.archive_relative_path=requested;
    for (const auto physical:{241U,242U}) {
        auto& record=r.catalog->records[physical];record.width=78;record.height=80;
        record.isometric_size_flag=1;record.uncompressed_length=3200;record.data_length=3500;
    }
    return r;
}
void attach_archive(GreatWallMapPresentation& state,StoredArchiveRegistration registration,
                    const std::filesystem::path& requested) {
    state.archives.emplace(std::pair{registration.slot,requested.generic_string()},std::move(registration));
}
void registration_snapshots() {
    const auto f=sequence_fixture({{GreatWallPieceKind::Wall,10},{GreatWallPieceKind::Road,2},
        {GreatWallPieceKind::Wall,1},{GreatWallPieceKind::Road,2},
        {GreatWallPieceKind::Wall,10},{GreatWallPieceKind::Road,2}});
    auto source=f.source;
    for (std::size_t n=0;n<source.records.size();++n) {
        source.records[n].manager_index=std::uint32_t(99-n);
        source.records[n].original_id=OriginalEntityId{std::uint32_t(100-n)};
    }
    auto state=prepare_great_wall_presentation(source,f.models,great_wall_context_from_mode(GreatWallPresentationMode::PreviewStone));
    const auto a=state.pieces[0].archive_relative,b=state.pieces[2].archive_relative;
    check(a!=b && state.pieces[1].archive_relative==a && state.pieces[3].archive_relative==b &&
        state.pieces[5].archive_relative==a,"same-slot A/Road/B/Road/A/Road preserves each restore-order archive snapshot");
    auto a_registration=road_archive(a);const auto actual_a=std::filesystem::path("DATA/China_Mon_GreatWall_10.sg3");
    a_registration.relative_path=actual_a;
    for (auto& record:a_registration.catalog->records) record.id.archive_relative_path=actual_a;
    attach_archive(state,std::move(a_registration),a);attach_archive(state,road_archive(b),b);
    auto p=plan(state);build_regenerated_map_render_plan(p,{{8,road_archive(b)}});
    check(p.regenerated->instances.size()==6 && p.assets.size()==4,
        "per-piece snapshots override a different final global slot without duplicating repeated physical assets");
    const auto& instances=p.regenerated->instances;
    check(instances[1].graphic.value==instances[3].graphic.value &&
        instances[1].asset_index!=instances[3].asset_index && instances[1].asset_index==instances[5].asset_index &&
        instances[0].asset_index==instances[4].asset_index,
        "identical packed slot/local IDs in different archives are distinct while a reused physical archive deduplicates");
    check(p.assets[instances[1].asset_index].record.id.archive_relative_path==actual_a &&
        p.assets[instances[3].asset_index].record.id.archive_relative_path==b &&
        p.assets[instances[1].asset_index].record.id.image_index==241 &&
        p.assets[instances[3].asset_index].record.id.image_index==241,
        "requested registry identity stays separate from canonical physical asset identity");
    const auto a_info=landscape_provenance(p,state.pieces[1].geometry->origin,true)["original_great_wall"];
    const auto b_info=landscape_provenance(p,state.pieces[3].geometry->origin,true)["original_great_wall"];
    check(a_info["selection"]["archive"]==a.generic_string() &&
        a_info["preparation"]["registered_archive"]==actual_a.generic_string() &&
        b_info["selection"]["archive"]==b.generic_string() &&
        b_info["preparation"]["registered_archive"]==b.generic_string(),
        "F1 preserves the requested per-piece snapshot and canonical registered archive independently");
    for (const auto n:{1U,3U,5U})
        check(instances[n].composition_policy==LandscapeCompositionPolicy::EarlyBaseSpatialOverlay &&
            instances[n].cell_indices.size()==1,"inherited roads retain singleton split composition through archive switches");
    std::rotate(source.records.begin(),source.records.begin()+1,source.records.end());
    const auto road_first=prepare_great_wall_presentation(source,f.models,
        great_wall_context_from_mode(GreatWallPresentationMode::PreviewStone));
    check(road_first.pieces[0].selection.supported && road_first.pieces[0].archive_relative.empty() &&
        !road_first.pieces[0].fallback.empty() && road_first.pieces[2].archive_relative==b,
        "a producer restored later does not retroactively supply an earlier road registration");
}
void ruined_transition_diagnostics() {
    std::vector<std::pair<GreatWallPieceKind,std::int32_t>> rows(49,{GreatWallPieceKind::Wall,10});
    rows.insert(rows.end(),4,{GreatWallPieceKind::Road,2});
    auto f=sequence_fixture(rows);
    auto& model=*f.models.at(257).model;
    for (unsigned n=49;n<53;++n) model.pieces[n].position=(n%2)*2;
    for (const auto mode:{GreatWallPresentationMode::PreviewRuined,GreatWallPresentationMode::PreviewEarthen,
                         GreatWallPresentationMode::PreviewStone}) {
        auto state=prepare_great_wall_presentation(f.source,f.models,great_wall_context_from_mode(mode));
        check(state.pieces.size()==53 && std::all_of(state.pieces.begin(),state.pieces.end(),[](const auto& piece) {
            return piece.selection.supported && piece.geometry && piece.fallback.empty();
        }),"53 authored source pieces remain selector-supported with unchanged phase despite preview compatibility limits");
        const bool ruined=mode==GreatWallPresentationMode::PreviewRuined;
        const auto path=state.pieces[0].archive_relative;
        attach_archive(state,road_archive(path,ruined ? 40U:42U),path);
        auto p=plan(state);const auto raw=p.raw_saved_ids;const auto historical_cells=p.cells;
        for (unsigned n=49;n<53;++n) {
            const auto marker=state.pieces[n].geometry->draw_cell;
            p.height_bytes[std::size_t(marker.y)*228+marker.x]=0xfe;
        }
        build_regenerated_map_render_plan(p,{});
        check(p.regenerated->instances.size()==(ruined ? 49U:53U) && p.assets.size()==(ruined ? 1U:3U) &&
            p.raw_saved_ids==raw && p.cells.size()==historical_cells.size() && p.footprints.size()==historical_cells.size(),
            "only complete legal images publish claims; group overflow adds no asset and leaves historical cells/footprints intact");
        for (std::size_t n=0;n<p.cells.size();++n)
            check(p.cells[n].stored_id==historical_cells[n].stored_id && p.cells[n].storage==historical_cells[n].storage &&
                p.cells[n].footprint_index==historical_cells[n].footprint_index,
                "transition preparation never rewrites saved IDs, storage positions or historical footprint membership");
        for (unsigned n=49;n<53;++n) {
            const auto& piece=state.pieces[n];const auto& preparation=p.regenerated->original_wall_preparation.at(n);
            const auto cell=piece.geometry->origin;const auto info=landscape_provenance(p,cell,true)["original_great_wall"];
            check(info["serialized_state"]["phase"]==2 && info["serialized_state"]["material_raw"]==3 &&
                info["selector_phase"]==2 && info["original_context_verified"]==false &&
                info["selection"]["variant"]==piece.selection.variant && info["geometry"]["height_signed"]==-2,
                "F1 preserves raw phase/material and signed cell height independently of selected preview material");
            check(preparation.resource.group.local_base==0 && preparation.resource.local_end==(ruined ? 40U:42U) &&
                info["preparation"]["variant_count"]==(ruined ? 40U:42U),
                "F1 reports the bounded runtime-local group rather than padded physical catalog capacity");
            const auto cell_index=*p.cell_by_storage[std::size_t(cell.y)*228+cell.x];
            if (ruined) {
                check(preparation.status==GreatWallPreparationStatus::UnsupportedPreviewState &&
                    preparation.resource.variant_out_of_range && !preparation.image_status && !preparation.instance_index &&
                    !p.regenerated->cells[cell_index].instance_index && !p.regenerated->cells[cell_index].asset_index &&
                    !p.regenerated->footprint_assets[*p.cells[cell_index].footprint_index] &&
                    info["preparation"]["status"]=="saved_phase_preview_material_unsupported" &&
                    info["fallback"].get<std::string>().find("saved road phase 2")!=std::string::npos,
                    "Ruined phase-two roads fail before image lookup, keep historical ownership and name the exact unsupported state");
            } else {
                check(preparation.status==GreatWallPreparationStatus::Prepared && preparation.image_status==GraphicsIdStatus::DecodeCandidate &&
                    preparation.instance_index && info["preparation"]["status"]=="prepared",
                    "Earthen and Stone phase-two roads prepare the same exact variants without activation claims");
                const auto& instance=p.regenerated->instances[*preparation.instance_index];
                const auto zero=regenerated_instance_origin(instance.geometry,p.border,78,80,0);
                const auto elevated=regenerated_instance_origin(instance.geometry,p.border,78,80,landscape_height(p,cell));
                check(instance.composition_policy==LandscapeCompositionPolicy::EarlyBaseSpatialOverlay &&
                    elevated.x==zero.x && elevated.y==zero.y+80,
                    "road composition stays split and applies the signed saved height times forty exactly once");
            }
        }
        const auto cold=landscape_fidelity_report(p)["original_great_wall"];
        check(cold["selected_pieces"]==53 && cold["descriptor_cells"]==788 && cold["preview_render_instances"]==0 &&
            cold["render_instances"]==0 && cold["rendered_original_objects"]==0,
            "supported source descriptors and prepared metadata never count as eager active or original reproduction");
        if (ruined) check(cold["preparation_status_counts"]["saved_phase_preview_material_unsupported"]==4 &&
            cold["preparation_status_counts"]["prepared"]==49 &&
            cold["preview_notice"].get<std::string>().find("4 road pieces unsupported")!=std::string::npos,
            "cached partial-preview notice counts only the four diagnosed unsupported road pieces");
        else check(cold["preview_notice"]=="","compatible material previews have no ruined-state warning");
        p.regenerated_instance_active.assign(p.regenerated->instances.size(),1);
        for (auto& asset:p.assets) asset.status=StoredStatus::Rendered;
        check(landscape_fidelity_report(p)["original_great_wall"]["preview_render_instances"]==(ruined ? 49U:53U),
            "preview active counts include only complete prepared instances after separate readiness publication");
        if (!ruined) {
            for (unsigned n=49;n<53;++n) {
                const auto index=*p.regenerated->original_wall_preparation.at(n).instance_index;
                p.regenerated_instance_active[index]=0;
                auto& asset=p.assets[p.regenerated->instances[index].asset_index];
                asset.status=state.pieces[n].selection.variant==40 ? StoredStatus::DecodeFailed:StoredStatus::Rendered;
                asset.error="authored eager decoder failure";
                const auto info=landscape_provenance(p,state.pieces[n].geometry->origin,true)["original_great_wall"];
                check(info["preparation"]["status"]==(state.pieces[n].selection.variant==40 ?
                    "decode_failed":"atomic_activation_unavailable"),
                    "decode failure and a decoded inactive composition have separate runtime diagnoses");
            }
            const auto failed=landscape_fidelity_report(p)["original_great_wall"];
            check(failed["preview_render_instances"]==49 && failed["preparation_status_counts"]["decode_failed"]==2 &&
                failed["preparation_status_counts"]["atomic_activation_unavailable"]==2 &&
                failed["preparation_status_counts"]["active"]==49,
                "shared road decode refusal does not inflate active counts or become a source/material incompatibility");
        }
    }
    const auto verified_context=GreatWallRestoreContext{1,0,GreatWallContextSource::VerifiedOriginal};
    auto verified=prepare_great_wall_presentation(f.source,f.models,verified_context);
    const auto path=verified.pieces[0].archive_relative;
    attach_archive(verified,road_archive(path,40),path);
    auto verified_plan=plan(verified);build_regenerated_map_render_plan(verified_plan,{});
    check(verified_plan.regenerated->original_wall_preparation.at(49).status==GreatWallPreparationStatus::VariantOutOfRange &&
        verified_plan.regenerated->great_wall_preview_notice.empty(),
        "independently verified material-one context reports a resource bound without asserting original state impossibility");
    auto missing=prepare_great_wall_presentation(f.source,f.models,great_wall_context_from_mode(GreatWallPresentationMode::PreviewRuined));
    auto missing_plan=plan(missing);build_regenerated_map_render_plan(missing_plan,{});
    check(missing_plan.regenerated->original_wall_preparation.at(49).status==GreatWallPreparationStatus::ArchiveUnavailable &&
        missing_plan.regenerated->great_wall_preview_notice.empty() &&
        landscape_provenance(missing_plan,missing.pieces[49].geometry->origin,true)
            ["original_great_wall"]["preparation"]["group_status"].is_null(),
        "missing required archive is not mislabeled as a diagnosed phase/material mismatch");
    auto compatible=missing;attach_archive(compatible,road_archive(path,42),path);
    auto compatible_plan=plan(compatible);build_regenerated_map_render_plan(compatible_plan,{});
    check(compatible_plan.regenerated->instances.size()==53 &&
        compatible_plan.regenerated->original_wall_preparation.at(49).status==GreatWallPreparationStatus::Prepared &&
        compatible_plan.regenerated->great_wall_preview_notice.empty(),
        "preview-state diagnosis requires an actual bound failure rather than material and phase alone");
    auto bad_group=missing;auto bad_registration=road_archive(path,40);bad_registration.layout->groups.clear();
    attach_archive(bad_group,std::move(bad_registration),path);
    auto bad_plan=plan(bad_group);build_regenerated_map_render_plan(bad_plan,{});
    check(bad_plan.regenerated->original_wall_preparation.at(49).status==GreatWallPreparationStatus::GroupUnavailable &&
        !bad_plan.regenerated->original_wall_preparation.at(49).resource.variant_out_of_range &&
        bad_plan.regenerated->great_wall_preview_notice.empty(),
        "invalid group structure remains distinct from a validated exclusive group boundary");
}
}
int main(){try{
    registration_snapshots();ruined_transition_diagnostics();
    const auto e=entities();const auto m=models();
    auto missing=prepare_great_wall_presentation(e,m);
    check(missing.pieces.size()==3 && !missing.pieces[0].selection.supported &&
        !missing.pieces[0].fallback.empty(),"raw saved material3 never supplies missing restore context");
    auto absent=plan(missing);build_regenerated_map_render_plan(absent,{});
    check(absent.regenerated->instances.empty() && absent.assets.empty(),"unresolved context publishes no ownership/assets");
    auto known=prepare_great_wall_presentation(e,m,{3,0,GreatWallContextSource::VerifiedOriginal});
    check(known.pieces[0].geometry->origin==GridCell{84,61} &&
        known.pieces[0].geometry->draw_cell==GridCell{84,64},"serialized local origin maps through validated border");
    check(known.pieces[0].selection.variant==25 && known.pieces[1].selection.variant==28,
        "phase10 tower remap and effectiveview2 gate selection");
    check(known.pieces[1].archive_relative==known.pieces[0].archive_relative,
        "gate inherits restore-order registration, never its own phase1 archive");
    known.archives.emplace(std::pair{known.pieces[0].selection.slot,known.pieces[0].archive_relative.generic_string()},archive());
    auto p=plan(known);const auto raw_before=p.raw_saved_ids;
    build_regenerated_map_render_plan(p,{});
    check(p.regenerated->instances.size()==3 && p.assets.size()==2,"two towers deduplicate physical asset and gate owns separate image");
    std::size_t members=0;for(const auto& i:p.regenerated->instances){members+=i.cell_indices.size();
        check(i.geometry.selection.family==LandscapeFamily::GreatWall,"original wall claims precede rock packing");
        check(i.composition_policy==LandscapeCompositionPolicy::SpatialCombined,
            "validated towers and gates retain their entire body in the spatial painter");}
    check(members==36 && p.raw_saved_ids==raw_before,"complete4x4/2x2 claims preserve historical raw authority");
    auto road_entities=e;road_entities.records[2].monument->phase=2;
    auto road_models=m;
    road_models.at(257).model->pieces[0].kind=GreatWallPieceKind::Wall;
    road_models.at(257).model->pieces[0].piece=0;
    road_models.at(257).model->pieces[2]={12,0,GreatWallPieceKind::Road,4,2,0,1};
    auto road_state=prepare_great_wall_presentation(road_entities,road_models,{3,0,GreatWallContextSource::VerifiedOriginal});
    auto road_archive=archive();auto& road_record=road_archive.catalog->records[241];
    road_record.width=78;road_record.height=40;road_record.isometric_size_flag=1;
    road_record.uncompressed_length=3200;road_record.data_length=3200;
    road_state.archives.emplace(std::pair{8U,road_state.pieces[0].archive_relative.generic_string()},std::move(road_archive));
    auto road_plan=plan(road_state);build_regenerated_map_render_plan(road_plan,{});
    check(road_plan.regenerated->instances.size()==3 &&
        road_plan.regenerated->instances[0].composition_policy==LandscapeCompositionPolicy::SpatialCombined &&
        road_plan.regenerated->instances[1].composition_policy==LandscapeCompositionPolicy::SpatialCombined &&
        road_plan.regenerated->instances[2].composition_policy==LandscapeCompositionPolicy::EarlyBaseSpatialOverlay,
        "validated model kind makes wall/gate bodies spatial while model roads preserve split ground composition");
    auto altered=plan(known);altered.raw_saved_ids.assign(count,0xffffffffU);
    for(auto& c:altered.cells){c.stored_id=0xffffffffU;c.slot=16;}
    build_regenerated_map_render_plan(altered,{});
    check(altered.regenerated->instances.size()==p.regenerated->instances.size(),"changed saved IDs including elevation slot cannot change restored claims");
    for(std::size_t n=0;n<p.regenerated->instances.size();++n){const auto& a=p.regenerated->instances[n];const auto& b=altered.regenerated->instances[n];
        check(a.graphic.value==b.graphic.value && a.geometry.origin==b.geometry.origin &&
            a.geometry.owned_cells==b.geometry.owned_cells,"restored wall identity/claims independent of saved IDs");}
    auto wrong=known;wrong.archives.begin()->second.catalog->records[226].image_type=1;
    auto unsupported=plan(wrong);build_regenerated_map_render_plan(unsupported,{});
    check(unsupported.regenerated->instances.size()==1 && unsupported.regenerated->instances[0].geometry.side==2,
        "Type1 cannot become a scaled4x4 Type30 or partial original owner");
    check(unsupported.regenerated->original_wall_preparation.at(0).status==GreatWallPreparationStatus::GeometryOrImageUnsupported &&
        !unsupported.regenerated->original_wall_preparation.at(0).resource.variant_out_of_range,
        "a present unsupported image layout is distinct from a group-bound or preview-state failure");
    check(landscape_provenance(unsupported,{84,61},true)["original_great_wall"]["fallback"]==
        "selected Great Wall Type-1 image layout unsupported; historical preview retained",
        "unsupported selected layout retains a concrete per-piece provenance fallback");
    auto unavailable=known;unavailable.archives.begin()->second.layout->runtime_image_count=25;
    auto missing_variant=plan(unavailable);build_regenerated_map_render_plan(missing_variant,{});
    const auto missing_info=landscape_provenance(missing_variant,{84,61},true);
    check(missing_info["original_great_wall"]["fallback"].get<std::string>().find("variant 25 outside registered archive group")!=std::string::npos &&
        missing_info["regenerated"]["fallback"]==missing_info["original_great_wall"]["fallback"],
        "unavailable group variant is named consistently without publishing partial ownership");
    auto conflict_entities=e;conflict_entities.records[1].local_x=55;
    const auto conflict=prepare_great_wall_presentation(conflict_entities,m,{3,0,GreatWallContextSource::VerifiedOriginal});
    check(!conflict.pieces[1].geometry && !conflict.pieces[1].fallback.empty(),"whole conflicting claim rejected without stealing first owner");
    auto edge_entities=e;edge_entities.records[0].local_x=0;edge_entities.records[0].local_y=0;
    const auto edge=prepare_great_wall_presentation(edge_entities,m,{3,0,GreatWallContextSource::VerifiedOriginal});
    check(!edge.pieces[0].geometry,"off-map complete claim rejected atomically");
    check(edge.pieces[1].fallback.empty() && !edge.pieces[1].archive_relative.empty(),
        "a rejected whole claim still establishes its evidenced registration before a following gate");
    const auto rotated=prepare_great_wall_presentation(e,m,{3,2,GreatWallContextSource::VerifiedOriginal});
    check(!rotated.pieces[0].geometry && !rotated.pieces[0].fallback.empty(),
        "selector views never imply an unproved rotated caller anchor");
    auto bad_subindex=e;bad_subindex.records[0].subindex=-1;
    check(!prepare_great_wall_presentation(bad_subindex,m,{3,0,GreatWallContextSource::VerifiedOriginal}).pieces[0].geometry,"negative subindex never converted to valid unsigned index");
    auto bad_model=m;bad_model.at(257).model->pieces[0].side=0;
    check(!prepare_great_wall_presentation(e,bad_model,{3,0,GreatWallContextSource::VerifiedOriginal}).pieces[0].geometry,
        "invalid model side never enters unsigned marker arithmetic");
    auto gate_first=e;gate_first.records.erase(gate_first.records.begin());
    check(!prepare_great_wall_presentation(gate_first,m,{3,0,GreatWallContextSource::VerifiedOriginal}).pieces[0].fallback.empty(),"inherited archive unavailable before first producer");
    auto mixed=e;mixed.records[0].monument->phase=1;mixed.records[2].monument->phase=2;
    auto mixed_state=prepare_great_wall_presentation(mixed,m,{1,0,GreatWallContextSource::VerifiedOriginal});
    for(const auto slot:{8U,9U}) {
        auto registration=archive();registration.slot=slot;registration.layout->slot=slot;
        registration.relative_path=mixed_state.pieces[0].archive_relative;
        for(auto& record:registration.catalog->records) record.id.archive_relative_path=registration.relative_path;
        auto& gate=registration.catalog->records[232];gate.width=158;gate.height=180;gate.isometric_size_flag=2;
        gate.uncompressed_length=12800;gate.data_length=13100;
        mixed_state.archives.emplace(std::pair{slot,registration.relative_path.generic_string()},std::move(registration));
    }
    auto mixed_plan=plan(mixed_state);build_regenerated_map_render_plan(mixed_plan,{});
    check(mixed_plan.regenerated->instances.size()==3 && mixed_plan.assets.size()==2 &&
        mixed_plan.regenerated->instances[0].asset_index==mixed_plan.regenerated->instances[2].asset_index,
        "same physical archive in dynamic slots8/9 retains both lookups with one shared texture identity");
    const auto report=landscape_fidelity_report(absent);
    check(report["original_great_wall"]["serialized_objects"]==3 && report["original_great_wall"]["render_instances"]==0,
        "serialization and rendering counts remain separate");
    const auto info=landscape_provenance(absent,{84,61},true);
    check(info["original_great_wall"]["serialized_state"]["material_raw"]==3 &&
        info["original_great_wall"]["restored_material"].is_null(),"F1 exposes raw versus unresolved derived material");
    check(info["original_great_wall"]["restore_context"]["source"]=="unavailable" &&
        info["original_great_wall"]["original_context_verified"]==false,
        "automatic missing context reports its source and cannot claim original verification");
    const auto historical=prepare_great_wall_presentation(e,m,
        great_wall_context_from_mode(GreatWallPresentationMode::HistoricalFallback,{3,0,GreatWallContextSource::VerifiedOriginal}));
    auto historical_plan=plan(historical);build_regenerated_map_render_plan(historical_plan,{});
    check(historical_plan.regenerated->instances.empty() && historical_plan.assets.empty() &&
        !historical.restore_context.material,"explicit historical choice preserves fallback even when original context is known");
    for (const auto mode:{GreatWallPresentationMode::PreviewRuined,GreatWallPresentationMode::PreviewEarthen,GreatWallPresentationMode::PreviewStone}) {
        const auto context=great_wall_context_from_mode(mode);
        auto preview=prepare_great_wall_presentation(e,m,context);
        auto registration=archive();registration.relative_path=preview.pieces[0].archive_relative;
        for (auto& record:registration.catalog->records) record.id.archive_relative_path=registration.relative_path;
        auto& gate=registration.catalog->records[201+preview.pieces[1].selection.variant];
        gate.width=158;gate.height=180;gate.uncompressed_length=12800;gate.data_length=13100;gate.isometric_size_flag=2;
        preview.archives.emplace(std::pair{8U,preview.pieces[0].archive_relative.generic_string()},std::move(registration));
        auto preview_plan=plan(preview);build_regenerated_map_render_plan(preview_plan,{});
        check(preview_plan.regenerated->instances.size()==3,"explicit preview uses the same complete object claim pipeline");
        for (const auto& instance:preview_plan.regenerated->instances) {
            check(instance.geometry.selection.evidence==SelectorEvidence::Preview && instance.great_wall_context &&
                instance.great_wall_context->mode==mode && instance.great_wall_context->source==GreatWallContextSource::ExplicitPreview,
                "render instances retain explicit preview source and never become verified original selectors");
            preview_plan.assets[instance.asset_index].status=StoredStatus::Rendered;
        }
        preview_plan.regenerated_instance_active.assign(3,1);
        const auto preview_report=landscape_fidelity_report(preview_plan);
        check(preview_report["regeneration"]["verified_identity"]==0 &&
            preview_report["regeneration"]["preview_identity"]==36 &&
            preview_report["regeneration"]["normal_wall_selector_verified"]==0 &&
            preview_report["regeneration"]["great_wall_selector_verified"]==0 &&
            preview_report["regeneration"]["great_wall_instances"]==0 &&
            preview_report["regeneration"]["great_wall_preview_instances"]==3 &&
            preview_report["original_great_wall"]["rendered_original_objects"]==0 &&
            preview_report["original_great_wall"]["render_instances"]==0 &&
            preview_report["original_great_wall"]["preview_render_instances"]==3 &&
            preview_report["original_great_wall"]["original_context_verified"]==false,
            "active previews have independent counts and never inflate original identity or object metrics");
        const auto preview_info=landscape_provenance(preview_plan,{84,61},true);
        check(preview_info["original_great_wall"]["selected_material"]==context.material.value() &&
            preview_info["original_great_wall"]["restored_material"].is_null() &&
            preview_info["regenerated"]["instance"]["restore_context"]["source"]=="explicit_preview" &&
            preview_info["regenerated"]["instance"]["original_context_verified"]==false &&
            preview_info["render_source"]=="regenerated Great Wall explicit material preview",
            "F1 retains raw, selected preview, original authority and active image provenance separately");
        const auto& composition=preview_info["regenerated"]["instance"]["composition"];
        check(composition["policy"]=="spatial_combined" && composition["early_base_pass"]==false &&
            composition["spatial_pass"]=="Combined" &&
            composition["format_components"]==nlohmann::json::array({"Base","Overlay"}) &&
            composition["drawn_components_when_visible"]==nlohmann::json::array({"Combined"}) &&
            preview_info["regenerated"]["instance"]["spatial_reference_cell"]==nlohmann::json{{"x",87},{"y",64}} &&
            preview_info["regenerated"]["instance"]["height_cell"]==nlohmann::json{{"x",84},{"y",64}},
            "F1 separates format components, spatial body policy, front key and signed marker height source");
        preview_plan.regenerated_instance_active[0]=0;
        check(landscape_fidelity_report(preview_plan)["original_great_wall"]["preview_render_instances"]==2,
            "preview metrics follow eager atomic readiness rather than selected descriptor counts");
    }
    openemperor::performance::set_enabled(true);openemperor::performance::reset();
    for (unsigned n=0;n<32;++n) {
        const auto pure=prepare_great_wall_presentation(e,m,great_wall_context_from_mode(GreatWallPresentationMode::PreviewStone));
        (void)landscape_provenance(absent,{84,61},true);
        (void)landscape_fidelity_report(absent);
        check(pure.pieces.size()==3,"pure context and descriptor projections remain bounded");
    }
    for (const auto counter:{openemperor::performance::Counter::FileReads,openemperor::performance::Counter::FileWrites,
        openemperor::performance::Counter::AssetDecodes,openemperor::performance::Counter::TextureUploads,
        openemperor::performance::Counter::WorldCopies,openemperor::performance::Counter::WorldExecutes,
        openemperor::performance::Counter::BfsCalls,openemperor::performance::Counter::RouteRefreshes})
        check(openemperor::performance::counter(counter)==0,"context, inspection and report projections perform no mutable or asset work");
    openemperor::performance::set_enabled(false);
    std::cout<<"Great Wall presentation tests passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
