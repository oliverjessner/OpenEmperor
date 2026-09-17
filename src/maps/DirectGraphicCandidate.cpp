#include "maps/DirectGraphicCandidate.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <stdexcept>

namespace openemperor::maps {

const char* direct_candidate_status_name(DirectCandidateStatus status) {
    switch (status) {
    case DirectCandidateStatus::IndexOutOfRange: return "index_out_of_range";
    case DirectCandidateStatus::EmptyRecord: return "empty_record";
    case DirectCandidateStatus::SourceUnavailable: return "source_unavailable";
    case DirectCandidateStatus::UnsupportedLayout: return "unsupported_layout";
    case DirectCandidateStatus::DecodeCandidate: return "decode_candidate";
    }
    return "invalid_status";
}

DirectCandidateResolution resolve_direct_candidate(const assets::AssetCatalog& catalog,
                                                   std::uint32_t raw_word) {
    DirectCandidateResolution result;
    if (raw_word >= catalog.records.size()) return result;
    result.numeric_in_range = true;
    result.record = &catalog.records[raw_word];
    const auto& record = *result.record;
    result.metadata_nonempty = record.width > 0 && record.height > 0 && record.data_length > 0;
    if (!result.metadata_nonempty) { result.status = DirectCandidateStatus::EmptyRecord; return result; }
    result.payload_available = record.color_bounds == assets::AssetRangeStatus::InBounds &&
        (record.alpha_bounds == assets::AssetRangeStatus::InBounds ||
         record.alpha_bounds == assets::AssetRangeStatus::NotPresent);
    if (!result.payload_available) {
        result.status = DirectCandidateStatus::SourceUnavailable;
    } else if (!record.decoder_supported) {
        result.status = DirectCandidateStatus::UnsupportedLayout;
    } else result.status = DirectCandidateStatus::DecodeCandidate;
    return result;
}

std::optional<std::uint32_t> shifted_control_candidate(std::uint32_t raw_word) {
    const auto shifted = static_cast<std::uint64_t>(raw_word) + 1024U;
    if (shifted > UINT32_MAX) return std::nullopt;
    return static_cast<std::uint32_t>(shifted);
}

CandidateSpatialControl compare_candidate_structure(std::span<const std::uint32_t> words,
                                                    std::span<const std::uint32_t> categories) {
    if (words.size() != categories.size() ||
        (!words.empty() && std::gcd<std::size_t>(11,words.size()) != 1))
        throw std::invalid_argument("candidate control requires equal arrays and bijective stride");
    auto majority = [&](bool permuted) {
        std::map<std::uint32_t,std::map<std::uint32_t,std::size_t>> counts;
        for (std::size_t i=0;i<words.size();++i) {
            const auto word=words[permuted ? (i*11U+1U)%words.size() : i];
            ++counts[word][categories[i]];
        }
        std::size_t total=0;
        for (const auto& [word,values] : counts) {
            (void)word;
            std::size_t maximum=0;
            for (const auto& [category,count] : values) {
                (void)category;
                maximum=std::max(maximum,count);
            }
            total+=maximum;
        }
        return total;
    };
    return {words.size(),majority(false),majority(true)};
}

} // namespace openemperor::maps
