#pragma once

#include "assets/AssetCatalog.h"
#include "assets/Sg3AlphaDecoder.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace openemperor::assets {

enum class AlphaAddressing { Spec, Contiguous, Legacy };
const char* alpha_addressing_name(AlphaAddressing addressing);

enum class AuditRangeStatus { InBounds, SourceUnavailable, OutOfBounds, Overflow, NegativeOffset };
const char* audit_range_status_name(AuditRangeStatus status);

struct AlphaCandidate {
    AuditRangeStatus color = AuditRangeStatus::SourceUnavailable;
    AuditRangeStatus alpha = AuditRangeStatus::SourceUnavailable;
    std::uint64_t color_start = 0;
    std::uint64_t alpha_start = 0;
    AlphaSyntaxResult syntax;
};

struct AlphaAuditRecord {
    AssetId id;
    std::uint16_t image_type = 0;
    std::uint8_t external_flag = 0;
    std::int16_t width = 0;
    std::int16_t height = 0;
    std::int64_t delta_from_data = 0;
    std::int64_t delta_from_contiguous = 0;
    std::int64_t delta_from_legacy_alpha = 0;
    std::array<AlphaCandidate, 3> candidates;
};

struct AlphaAuditCounts {
    std::uint64_t records = 0;
    std::array<std::uint64_t, 3> in_bounds{};
    std::array<std::uint64_t, 3> valid{};
    std::array<std::uint64_t, 3> complete{};
    std::array<std::uint64_t, 3> malformed{};
    std::array<std::uint64_t, 3> source_unavailable{};
    std::array<std::uint64_t, 3> out_of_bounds{};
    std::array<std::uint64_t, 3> overflow{};
    std::array<std::uint64_t, 3> negative{};
};

struct AlphaAudit {
    std::vector<AlphaAuditRecord> records;
    AlphaAuditCounts total;
    std::map<std::string, AlphaAuditCounts> by_source;
    std::map<std::uint16_t, AlphaAuditCounts> by_type;
    std::map<std::string, AlphaAuditCounts> by_archive;
    std::map<std::int64_t, std::uint64_t> delta_from_data;
    std::map<std::int64_t, std::uint64_t> delta_from_contiguous;
    std::map<std::int64_t, std::uint64_t> delta_from_legacy_alpha;
    std::uint64_t alpha_offset_equals_data_plus_twice_length = 0;
};

// Independent addressing hypotheses. Checks both ranges before reading any bytes.
AlphaCandidate evaluate_alpha_candidate(std::uint64_t data_offset, std::uint64_t data_length,
                                        std::uint64_t alpha_offset, std::uint64_t alpha_length,
                                        std::uint8_t external_flag, std::uint64_t file_size,
                                        bool source_available, AlphaAddressing strategy);
AlphaAudit audit_alpha_catalog(const AssetCatalog& catalog);

} // namespace openemperor::assets
