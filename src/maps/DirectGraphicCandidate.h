#pragma once

#include "assets/AssetCatalog.h"

#include <cstdint>
#include <optional>
#include <span>
#include <cstddef>

namespace openemperor::maps {

enum class DirectCandidateStatus {
    IndexOutOfRange, EmptyRecord, SourceUnavailable, UnsupportedLayout, DecodeCandidate
};
const char* direct_candidate_status_name(DirectCandidateStatus status);
struct DirectCandidateResolution {
    DirectCandidateStatus status = DirectCandidateStatus::IndexOutOfRange;
    bool numeric_in_range = false;
    bool metadata_nonempty = false;
    bool payload_available = false;
    const assets::AssetRecord* record = nullptr;
};

// Explicit diagnosis of one archive's exact index space. No implicit archive
// search, bit masking, modulo, or fallback. Does not itself decode pixels.
DirectCandidateResolution resolve_direct_candidate(const assets::AssetCatalog& catalog,
                                                   std::uint32_t raw_word);
// Negative control only; never used to choose an image for display.
std::optional<std::uint32_t> shifted_control_candidate(std::uint32_t raw_word);

struct CandidateSpatialControl {
    std::size_t denominator = 0;
    std::size_t direct_majority_cells = 0;
    std::size_t permuted_majority_cells = 0;
};
// Candidate-mask-order negative control. Category codes are diagnostic labels,
// not assertions that the reference-derived classification is ground truth.
CandidateSpatialControl compare_candidate_structure(std::span<const std::uint32_t> words,
                                                    std::span<const std::uint32_t> categories);

} // namespace openemperor::maps
