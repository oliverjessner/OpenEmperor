#pragma once

#include "assets/Sg3Archive.h"

#include <cstdint>
#include <map>
#include <optional>

namespace openemperor::maps {

// The examined EXE's group keys are a different index space from packed image IDs.
struct ResourceGroupKey { std::uint32_t value = 0; };
struct PackedGraphicId { std::uint32_t value = 0; };

enum class GroupLookupStatus {
    Resolved, UnsupportedHighBit, NoGroupPosition, UnregisteredSlot,
    UnverifiedRegistration, GroupOutOfRange, ImageOutOfRange
};
const char* group_lookup_status_name(GroupLookupStatus status);

struct GroupRegistration {
    const assets::Sg3Archive* archive = nullptr;
};

struct GroupResolution {
    GroupLookupStatus status = GroupLookupStatus::UnregisteredSlot;
    std::uint32_t slot = 0;
    std::uint32_t group_position = 0; // Zero-based in the filtered runtime table.
    std::optional<std::uint32_t> sg3_index_position;
    std::optional<std::uint64_t> sg3_file_offset;
    std::optional<std::uint16_t> sg3_raw_value;
    std::optional<std::uint32_t> local_base;
    std::optional<PackedGraphicId> packed_base;
};

// Explicit snapshot of the studied v213 resource manager. No archive is selected
// by filesystem order. The caller owns each parsed archive.
GroupResolution resolve_resource_group(
    ResourceGroupKey key, const std::map<std::uint32_t, GroupRegistration>& registrations);

// The observed range writer adds a caller-supplied low-three-bit variant.
// This does not reproduce its byte generator or a loaded map's first draw.
std::optional<PackedGraphicId> group_variant(PackedGraphicId base, std::uint32_t variant);

} // namespace openemperor::maps
