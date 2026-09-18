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
    if (found == registrations.end())
        return result;
    const auto* layout = found->second.layout;
    if (layout == nullptr || !layout->verified_registration ||
        layout->slot != result.slot || layout->sg3_version != 213) {
        result.status = GroupLookupStatus::UnverifiedRegistration;
        return result;
    }
    if (result.group_position >= layout->groups.size()) {
        result.status = GroupLookupStatus::GroupOutOfRange;
        return result;
    }
    const auto& entry = layout->groups[result.group_position];
    result.sg3_index_position = entry.sg3_index_position;
    result.sg3_file_offset = entry.sg3_file_offset;
    result.sg3_raw_value = entry.raw_value;
    result.local_base = entry.local_base;
    if (entry.local_base >= layout->runtime_image_count || entry.local_base >= 0x4000U) {
        result.status = GroupLookupStatus::ImageOutOfRange;
        return result;
    }
    result.packed_base = PackedGraphicId{result.slot * 0x4000U + entry.local_base};
    result.status = GroupLookupStatus::Resolved;
    return result;
}

std::optional<PackedGraphicId> group_variant(PackedGraphicId base, std::uint32_t variant) {
    if (variant > 7U || (base.value & 0x80000000U) != 0 ||
        (base.value & 0x3fffU) > 0x3fffU - variant ||
        base.value > std::numeric_limits<std::uint32_t>::max() - variant) return std::nullopt;
    return PackedGraphicId{base.value + variant};
}

} // namespace openemperor::maps
