#pragma once

#include "assets/Sg3Archive.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::maps {

// The examined EXE's v213 loader for explicitly registered Terrain/Elevation.
// Asset catalog IDs remain physical SG3 record positions.
struct RuntimeGroupEntry {
    std::uint32_t sg3_index_position = 0;
    std::uint64_t sg3_file_offset = 0;
    std::uint16_t raw_value = 0;
    std::uint32_t local_base = 0;
};

struct RuntimeArchiveLayout {
    std::uint32_t slot = 0;
    std::uint32_t sg3_version = 0;
    std::uint32_t image_capacity = 0;
    std::uint32_t reported_images_in_use = 0;
    std::string first_group_filename;
    bool system_branch_activated = false;
    std::uint32_t system_record_skip = 0;
    std::uint32_t runtime_image_count = 0;
    std::uint32_t first_physical_record = 0;
    std::vector<RuntimeGroupEntry> groups;

    std::optional<std::uint32_t> physical_record_for_local(std::uint32_t local) const;
};

// Returns null for unsupported slot/version or inconsistent metadata. The
// first SG3 bitmap-group filename, not the outer archive name, selects the
// studied 200-record branch. Retained index words keep file order.
std::optional<RuntimeArchiveLayout> build_runtime_archive_layout(
    std::uint32_t slot, const assets::Sg3Archive& archive);

} // namespace openemperor::maps
