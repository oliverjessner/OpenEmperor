#pragma once
#include "maps/LandscapeSelectors.h"
#include "maps/GreatWallSelector.h"
#include "maps/LandscapeInstances.h"
#include "maps/StoredGraphicsPlan.h"
#include "maps/WallTopology.h"

namespace openemperor::maps {
// File-format Base/Overlay components do not determine their scene roles.
// Structural bodies stay together at one spatial painter key; the existing
// landscape path keeps its early base and spatial overlay convention.
enum class LandscapeCompositionPolicy { EarlyBaseSpatialOverlay, SpatialCombined };
inline const char* landscape_composition_policy_name(LandscapeCompositionPolicy policy) {
    switch (policy) {
    case LandscapeCompositionPolicy::EarlyBaseSpatialOverlay: return "split_base_spatial_overlay";
    case LandscapeCompositionPolicy::SpatialCombined: return "spatial_combined";
    }
    return "invalid";
}
struct RegeneratedCell {
    LandscapeSelection selection;
    std::optional<PackedGraphicId> graphic;
    std::optional<std::size_t> asset_index;
    std::optional<std::size_t> instance_index;
    const char* fallback="historical snapshot preview";
};
struct RegeneratedLandscapeInstance {
    LandscapeInstanceSpec geometry;
    PackedGraphicId graphic;
    std::size_t asset_index=0;
    std::vector<std::size_t> cell_indices;
    // Images belonging to one restored composition activate together after
    // eager decoding. Unset preserves existing independent instances.
    std::optional<std::size_t> composition_group;
    std::optional<GreatWallRestoreContext> great_wall_context;
    LandscapeCompositionPolicy composition_policy=LandscapeCompositionPolicy::EarlyBaseSpatialOverlay;
};
// Load-time diagnostics from the same bounded resolver used for activation.
// Group bounds are runtime-local indices; they are never physical SG3 IDs.
struct LandscapeVariantResolution {
    GroupResolution group;
    std::optional<std::uint32_t> local_end;
    bool attempted=false;
    bool variant_out_of_range=false;
};
enum class GreatWallPreparationStatus {
    ArchiveUnavailable, GroupUnavailable, VariantOutOfRange,
    UnsupportedPreviewState, GeometryOrImageUnsupported, Prepared
};
const char* great_wall_preparation_status_name(GreatWallPreparationStatus status);
struct GreatWallPreparationResult {
    GreatWallPreparationStatus status=GreatWallPreparationStatus::GeometryOrImageUnsupported;
    std::filesystem::path registered_archive;
    LandscapeVariantResolution resource;
    std::optional<GraphicsIdStatus> image_status;
    std::optional<std::size_t> instance_index;
};
struct OrdinaryGatePreparationResult {
    std::vector<std::size_t> instance_indices;
    std::string fallback;
};
// Published const once per load. Historical cells, footprints and buildability
// remain untouched. Indices refer to the renderer's shared physical asset pool.
struct RegeneratedMapRenderPlan {
    std::vector<RegeneratedCell> cells; // Same candidate order as historical plan.
    std::vector<std::optional<std::size_t>> footprint_assets;
    std::vector<RegeneratedLandscapeInstance> instances;
    // Bounded by candidate cells and captured in the existing row-major wall
    // pass. Inspection never reconstructs a neighborhood or scans the map.
    std::map<std::size_t,WallTopologySelection> normal_wall_topology;
    std::map<std::size_t,OrdinaryGatePreparationResult> ordinary_gate_preparation;
    // Load-time refusal reason for a selected original piece that cannot
    // publish a complete image. Provenance only; never grants ownership.
    std::map<std::size_t,std::string> original_wall_fallbacks;
    std::map<std::size_t,GreatWallPreparationResult> original_wall_preparation;
    std::string great_wall_preview_notice;
    std::size_t historical_asset_count=0;
    double build_milliseconds=0;
};
void build_regenerated_map_render_plan(StoredGraphicsPlan& historical,
    const StoredArchiveRegistrations& registrations);
std::optional<PackedGraphicId> resolve_landscape_variant(ResourceGroupKey key,
    unsigned variant,const std::map<std::uint32_t,GroupRegistration>& registrations,
    LandscapeVariantResolution* diagnostic=nullptr);
} // namespace openemperor::maps
