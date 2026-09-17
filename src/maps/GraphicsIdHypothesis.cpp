#include "maps/GraphicsIdHypothesis.h"

namespace openemperor::maps {

const char* graphics_id_status_name(GraphicsIdStatus status) {
    switch (status) {
    case GraphicsIdStatus::UnsupportedHighBit: return "unsupported_high_bit";
    case GraphicsIdStatus::UnregisteredSlot: return "unregistered_slot";
    case GraphicsIdStatus::IndexOutOfRange: return "index_out_of_range";
    case GraphicsIdStatus::EmptyRecord: return "empty_record";
    case GraphicsIdStatus::SourceUnavailable: return "source_unavailable";
    case GraphicsIdStatus::UnsupportedLayout: return "unsupported_layout";
    case GraphicsIdStatus::DecodeCandidate: return "decode_candidate";
    }
    return "invalid_status";
}

GraphicsIdResolution resolve_graphics_id_hypothesis(
    std::uint32_t raw, const std::map<std::uint32_t, const assets::AssetCatalog*>& registrations) {
    GraphicsIdResolution result;
    result.raw = raw;
    if ((raw & 0x80000000U) != 0) {
        result.status = GraphicsIdStatus::UnsupportedHighBit;
        return result;
    }
    result.slot = raw >> 14U;
    result.local_index = raw & 0x3fffU;
    const auto found = registrations.find(result.slot);
    if (found == registrations.end() || found->second == nullptr) return result;
    const auto direct = resolve_direct_candidate(*found->second, result.local_index);
    result.record = direct.record;
    switch (direct.status) {
    case DirectCandidateStatus::IndexOutOfRange: result.status = GraphicsIdStatus::IndexOutOfRange; break;
    case DirectCandidateStatus::EmptyRecord: result.status = GraphicsIdStatus::EmptyRecord; break;
    case DirectCandidateStatus::SourceUnavailable: result.status = GraphicsIdStatus::SourceUnavailable; break;
    case DirectCandidateStatus::UnsupportedLayout: result.status = GraphicsIdStatus::UnsupportedLayout; break;
    case DirectCandidateStatus::DecodeCandidate: result.status = GraphicsIdStatus::DecodeCandidate; break;
    }
    return result;
}

} // namespace openemperor::maps
