#include "assets/Sg3AlphaAudit.h"

#include "assets/Sg3ImageLoader.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace openemperor::assets {
namespace {

namespace fs = std::filesystem;

bool checked_add(std::uint64_t left, std::uint64_t right, std::uint64_t& result) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) return false;
    result = left + right;
    return true;
}

AuditRangeStatus check_range(std::uint64_t start, std::uint64_t length,
                             std::uint64_t size, bool available) {
    std::uint64_t end = 0;
    if (!checked_add(start, length, end)) return AuditRangeStatus::Overflow;
    if (!available) return AuditRangeStatus::SourceUnavailable;
    return end <= size ? AuditRangeStatus::InBounds : AuditRangeStatus::OutOfBounds;
}

void add_counts(AlphaAuditCounts& counts, const AlphaAuditRecord& record) {
    ++counts.records;
    for (std::size_t index = 0; index < 3; ++index) {
        const AlphaCandidate& candidate = record.candidates[index];
        if (candidate.color == AuditRangeStatus::InBounds &&
            candidate.alpha == AuditRangeStatus::InBounds) {
            ++counts.in_bounds[index];
            if (candidate.syntax.valid) {
                ++counts.valid[index];
                if (candidate.syntax.final_pixel_cursor == candidate.syntax.image_pixel_count) {
                    ++counts.complete[index];
                }
            }
            else ++counts.malformed[index];
        } else if (candidate.color == AuditRangeStatus::Overflow ||
                   candidate.alpha == AuditRangeStatus::Overflow) ++counts.overflow[index];
        else if (candidate.color == AuditRangeStatus::NegativeOffset ||
                 candidate.alpha == AuditRangeStatus::NegativeOffset) ++counts.negative[index];
        else if (candidate.color == AuditRangeStatus::SourceUnavailable ||
                 candidate.alpha == AuditRangeStatus::SourceUnavailable) ++counts.source_unavailable[index];
        else ++counts.out_of_bounds[index];
    }
}

std::vector<std::uint8_t> read_range(std::ifstream& input, std::uint64_t offset,
                                     std::uint64_t length) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()) ||
        length > std::numeric_limits<std::size_t>::max() ||
        length > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        throw Sg3LoadError("alpha audit range cannot be read on this platform");
    }
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    if (!input) throw Sg3LoadError("alpha audit seek failed");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (input.gcount() != static_cast<std::streamsize>(bytes.size())) {
        throw Sg3LoadError("alpha audit short read");
    }
    return bytes;
}

} // namespace

const char* alpha_addressing_name(AlphaAddressing addressing) {
    switch (addressing) {
    case AlphaAddressing::Spec: return "SPEC";
    case AlphaAddressing::Contiguous: return "CONTIGUOUS";
    case AlphaAddressing::Legacy: return "LEGACY";
    }
    return "UNKNOWN";
}

const char* audit_range_status_name(AuditRangeStatus status) {
    switch (status) {
    case AuditRangeStatus::InBounds: return "in_bounds";
    case AuditRangeStatus::SourceUnavailable: return "source_unavailable";
    case AuditRangeStatus::OutOfBounds: return "out_of_bounds";
    case AuditRangeStatus::Overflow: return "overflow";
    case AuditRangeStatus::NegativeOffset: return "negative_offset";
    }
    return "unknown";
}

AlphaCandidate evaluate_alpha_candidate(std::uint64_t data_offset, std::uint64_t data_length,
                                        std::uint64_t alpha_offset, std::uint64_t alpha_length,
                                        std::uint8_t external_flag, std::uint64_t file_size,
                                        bool source_available, AlphaAddressing strategy) {
    AlphaCandidate candidate;
    if (strategy == AlphaAddressing::Legacy && data_offset < external_flag) {
        candidate.color = AuditRangeStatus::NegativeOffset;
        candidate.alpha = AuditRangeStatus::NegativeOffset;
        return candidate;
    }
    candidate.color_start = strategy == AlphaAddressing::Legacy
        ? data_offset - external_flag : data_offset;
    candidate.color = check_range(candidate.color_start, data_length, file_size, source_available);
    if (strategy == AlphaAddressing::Spec) {
        candidate.alpha_start = alpha_offset;
    } else if (!checked_add(candidate.color_start, data_length, candidate.alpha_start)) {
        candidate.alpha = AuditRangeStatus::Overflow;
        return candidate;
    }
    candidate.alpha = check_range(candidate.alpha_start, alpha_length, file_size, source_available);
    return candidate;
}

AlphaAudit audit_alpha_catalog(const AssetCatalog& catalog) {
    AlphaAudit audit;
    fs::path previous_archive;
    std::optional<Sg3Archive> archive;
    for (const AssetRecord& record : catalog.records) {
        if (record.alpha_length == 0) continue;
        const fs::path archive_path = catalog.data_root / record.id.archive_relative_path;
        if (!archive || archive_path != previous_archive) {
            archive = read_sg3_archive(archive_path);
            previous_archive = archive_path;
        }
        const Sg3Image& image = archive->images.at(record.id.image_index);
        const Sg3BitmapLocation location = resolve_sg3_image_bitmap(archive_path, *archive, image);
        std::error_code error;
        const bool resolved = location.status == Sg3BitmapStatus::Resolved;
        const bool regular = resolved && fs::is_regular_file(location.path, error) && !error;
        const std::uintmax_t raw_size = regular ? fs::file_size(location.path, error) : 0;
        const bool available = regular && !error &&
            raw_size <= std::numeric_limits<std::uint64_t>::max();
        const std::uint64_t size = available ? static_cast<std::uint64_t>(raw_size) : 0;
        AlphaAuditRecord result;
        result.id = record.id;
        result.image_type = record.image_type;
        result.external_flag = record.external_flag;
        result.width = record.width;
        result.height = record.height;
        result.delta_from_data = static_cast<std::int64_t>(record.alpha_offset) - record.data_offset;
        const std::uint64_t contiguous = static_cast<std::uint64_t>(record.data_offset) + record.data_length;
        result.delta_from_contiguous = static_cast<std::int64_t>(record.alpha_offset) -
            static_cast<std::int64_t>(contiguous);
        result.delta_from_legacy_alpha = result.delta_from_contiguous +
            (record.external_flag == 1 ? 1 : 0);
        if (static_cast<std::uint64_t>(record.alpha_offset) ==
            static_cast<std::uint64_t>(record.data_offset) +
                2U * static_cast<std::uint64_t>(record.data_length)) {
            ++audit.alpha_offset_equals_data_plus_twice_length;
        }
        std::ifstream input;
        if (available) input.open(location.path, std::ios::binary);
        const bool readable = available && static_cast<bool>(input);
        for (std::size_t index = 0; index < 3; ++index) {
            auto& candidate = result.candidates[index];
            candidate = evaluate_alpha_candidate(record.data_offset, record.data_length,
                record.alpha_offset, record.alpha_length, record.external_flag, size, readable,
                static_cast<AlphaAddressing>(index));
            if (candidate.color != AuditRangeStatus::InBounds ||
                candidate.alpha != AuditRangeStatus::InBounds) continue;
            const auto bytes = read_range(input, candidate.alpha_start, record.alpha_length);
            if (record.width <= 0 || record.height <= 0) {
                candidate.syntax.failure = "alpha destination dimensions must be nonzero";
            } else {
                candidate.syntax = inspect_omega_alpha_syntax(bytes,
                    static_cast<std::uint16_t>(record.width),
                    static_cast<std::uint16_t>(record.height));
            }
        }
        add_counts(audit.total, result);
        add_counts(audit.by_source[record.external_flag == 0 ? "internal" :
            record.external_flag == 1 ? "external" : "unknown_flag"], result);
        add_counts(audit.by_type[record.image_type], result);
        add_counts(audit.by_archive[record.id.archive_relative_path.generic_string()], result);
        ++audit.delta_from_data[result.delta_from_data];
        ++audit.delta_from_contiguous[result.delta_from_contiguous];
        if (record.external_flag == 1) ++audit.delta_from_legacy_alpha[result.delta_from_legacy_alpha];
        audit.records.push_back(std::move(result));
    }
    return audit;
}

} // namespace openemperor::assets
