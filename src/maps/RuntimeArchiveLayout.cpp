#include "maps/RuntimeArchiveLayout.h"

#include <limits>

namespace openemperor::maps {

std::optional<std::uint32_t> RuntimeArchiveLayout::physical_record_for_local(
    std::uint32_t local) const {
    if (local >= runtime_image_count) return std::nullopt;
    return first_physical_record + local;
}

std::optional<RuntimeArchiveLayout> build_runtime_archive_layout(
    std::uint32_t slot, const assets::Sg3Archive& archive, RuntimeLayoutEvidence evidence) {
    if ((slot != 3U && slot != 16U &&
         !(slot == 8U && evidence == RuntimeLayoutEvidence::TerrainElevationAndSlot8)) ||
        archive.header.version != 213 ||
        archive.header.image_capacity != archive.images.size() ||
        archive.header.reported_images_in_use >= archive.header.image_capacity)
        return std::nullopt;
    if (slot == 8U && (archive.groups.empty() ||
        archive.groups.front().filename != "Zeus_system.bmp" ||
        archive.groups.front().image_count != 200U ||
        archive.groups.front().first_image_index != 1U ||
        archive.groups.front().last_image_index != 200U)) return std::nullopt;

    RuntimeArchiveLayout layout;
    layout.slot = slot;
    layout.verified_registration = true;
    layout.sg3_version = archive.header.version;
    layout.image_capacity = archive.header.image_capacity;
    layout.reported_images_in_use = archive.header.reported_images_in_use;
    if (!archive.groups.empty()) layout.first_group_filename = archive.groups.front().filename;
    layout.system_branch_activated = layout.first_group_filename == "Zeus_system.bmp";
    layout.system_record_skip = layout.system_branch_activated ? 200U : 0U;
    if (layout.reported_images_in_use < layout.system_record_skip)
        return std::nullopt;
    layout.runtime_image_count = layout.reported_images_in_use - layout.system_record_skip;
    layout.first_physical_record = layout.system_record_skip + 1U;

    for (std::uint32_t i=0;i<archive.index.size();++i) {
        const auto raw = archive.index[i];
        if (raw > static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max()) ||
            raw <= layout.system_record_skip) continue;
        layout.groups.push_back({i,80ULL+2ULL*i,raw,
                                 static_cast<std::uint32_t>(raw-layout.system_record_skip-1U)});
    }
    return layout;
}

} // namespace openemperor::maps
