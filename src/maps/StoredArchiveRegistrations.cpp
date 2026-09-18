#include "maps/StoredGraphicsPlan.h"

#include "assets/Sg3ImageLoader.h"

#include <stdexcept>

namespace openemperor::maps {
namespace {
namespace fs = std::filesystem;

StoredArchiveRegistration load_one(const fs::path& root, std::uint32_t slot,
                                   fs::path relative, bool optional,
                                   RuntimeLayoutEvidence evidence) {
    StoredArchiveRegistration result;
    result.slot=slot;
    result.relative_path=std::move(relative);
    std::error_code error;
    const auto status=fs::symlink_status(root/result.relative_path,error);
    if (optional && (status.type()==fs::file_type::not_found ||
                     (error==std::errc::no_such_file_or_directory))) {
        result.archive_missing=true;
        return result;
    }
    const auto path=validate_stored_archive_sources(root,result.relative_path);
    result.metadata=assets::read_sg3_archive(path);
    result.layout=build_runtime_archive_layout(slot,*result.metadata,evidence);
    if (!result.layout) return result; // Registered, but this layout is unverified.
    result.catalog=assets::scan_asset_archive(root,result.relative_path);
    return result;
}
} // namespace

const char* stored_graphics_profile_name(StoredGraphicsProfile profile) {
    return profile==StoredGraphicsProfile::Slot8 ? stored_graphics_slot8_profile : stored_graphics_profile;
}

std::optional<fs::path> stored_registered_archive_path(
    std::uint32_t slot, StoredGraphicsProfile profile) {
    switch (slot) {
    case 3U: return fs::path{"DATA/China_Terrain.sg3"};
    case 16U: return fs::path{"DATA/China_Elevation.sg3"};
    case 8U: if (profile==StoredGraphicsProfile::Slot8)
                 return fs::path{"DATA/China_Mon_Earthen_Greatwall_1.sg3"};
             return std::nullopt;
    default: return std::nullopt;
    }
}

StoredArchiveRegistrations load_stored_archive_registrations(
    const fs::path& data_root, const MapGraphicCandidates& candidates,
    const MapGeometry& geometry, StoredGraphicsProfile profile) {
    StoredArchiveRegistrations result;
    const auto evidence=profile==StoredGraphicsProfile::Slot8 ?
        RuntimeLayoutEvidence::TerrainElevationAndSlot8 : RuntimeLayoutEvidence::TerrainElevation;
    for (const auto slot : {3U,16U}) {
        auto entry=load_one(data_root,slot,*stored_registered_archive_path(slot,profile),false,evidence);
        if (!entry.layout || !entry.catalog)
            throw std::runtime_error("unsupported required v213 runtime layout: " +
                                     entry.relative_path.generic_string());
        result.emplace(slot,std::move(entry));
    }
    if (profile==StoredGraphicsProfile::Slot8) {
        bool referenced=false;
        for (std::uint32_t y=0;y<stored_grid_height && !referenced;++y)
            for (std::uint32_t x=0;x<stored_grid_width;++x)
                if (geometry.contains({x,y})) {
                    const auto raw=candidates.word_at(x,y);
                    if ((raw&0x80000000U)==0 && (raw>>14U)==8U) {
                        referenced=true;
                        break;
                    }
                }
        if (referenced)
            result.emplace(8U,load_one(data_root,8U,*stored_registered_archive_path(8U,profile),
                                        true,evidence));
    }
    return result;
}
} // namespace openemperor::maps
