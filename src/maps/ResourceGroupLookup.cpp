#include "maps/ResourceGroupLookup.h"

#include <limits>

namespace openemperor::maps {

const char* group_lookup_status_name(GroupLookupStatus status) {
    switch (status) {
    case GroupLookupStatus::Resolved: return "resolved";
    case GroupLookupStatus::UnsupportedHighBit: return "unsupported_high_bit";
    case GroupLookupStatus::NoGroupPosition: return "no_group_position";
    case GroupLookupStatus::UnregisteredSlot: return "unregistered_slot";
    case GroupLookupStatus::UnverifiedRegistration: return "unverified_registration";
    case GroupLookupStatus::GroupOutOfRange: return "group_out_of_range";
    case GroupLookupStatus::ImageOutOfRange: return "image_out_of_range";
    }
    return "invalid_status";
}

GroupResolution resolve_resource_group(
    ResourceGroupKey key, const std::map<std::uint32_t, GroupRegistration>& registrations) {
    GroupResolution result;
    if ((key.value & 0x80000000U) != 0) {
        result.status = GroupLookupStatus::UnsupportedHighBit;
        return result;
    }
    result.slot = key.value / 512U;
    const auto one_based_position = key.value % 512U;
    if (one_based_position == 0) {
        result.status = GroupLookupStatus::NoGroupPosition;
        return result;
    }
    result.group_position = one_based_position - 1U;
    const auto found = registrations.find(result.slot);
    if (found == registrations.end() || (result.slot != 3U && result.slot != 16U))
        return result;
    const auto* archive = found->second.archive;
    if (archive == nullptr || archive->header.version != 213 ||
        archive->header.image_capacity != archive->images.size() ||
        archive->header.reported_images_in_use >= archive->header.image_capacity) {
        result.status = GroupLookupStatus::UnverifiedRegistration;
        return result;
    }
    // The studied loader keeps only signed-positive index words, prepends each
    // to a temporary list, then copies that list to its eight-byte group table.
    std::uint32_t remaining = result.group_position;
    for (std::uint32_t position = static_cast<std::uint32_t>(archive->index.size());
         position > 0;) {
        --position;
        const std::uint16_t raw = archive->index[position];
        if (raw == 0 || raw > static_cast<std::uint16_t>(std::numeric_limits<std::int16_t>::max())) continue;
        if (remaining != 0) {
            --remaining;
            continue;
        }
        result.sg3_index_position = position;
        result.sg3_file_offset = 80ULL + 2ULL * position;
        result.sg3_raw_value = raw;
        result.local_base = static_cast<std::uint32_t>(raw - 1U);
        if (*result.local_base >= archive->header.reported_images_in_use ||
            *result.local_base >= 0x4000U) {
            result.status = GroupLookupStatus::ImageOutOfRange;
            return result;
        }
        result.packed_base = PackedGraphicId{result.slot * 0x4000U + *result.local_base};
        result.status = GroupLookupStatus::Resolved;
        return result;
    }
    result.status = GroupLookupStatus::GroupOutOfRange;
    return result;
}

std::optional<PackedGraphicId> group_variant(PackedGraphicId base, std::uint32_t variant) {
    if (variant > 7U || (base.value & 0x80000000U) != 0 ||
        (base.value & 0x3fffU) > 0x3fffU - variant ||
        base.value > std::numeric_limits<std::uint32_t>::max() - variant) return std::nullopt;
    return PackedGraphicId{base.value + variant};
}

} // namespace openemperor::maps
