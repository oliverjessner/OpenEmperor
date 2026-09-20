#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {

enum class CompatibilityStatus { Compatible, Unknown, MissingFile, FingerprintMismatch };

const char* compatibility_status_name(CompatibilityStatus status);

struct CompatibilityFingerprint {
    std::filesystem::path relative_path;
    std::string sha256;
};

struct CompatibilityProfile {
    std::uint32_t schema_version = 0;
    std::string id;
    std::string evidence;
    std::filesystem::path pack_root;
    std::vector<CompatibilityFingerprint> files;
    std::filesystem::path walker_profile;
    std::filesystem::path building_profile;
    std::filesystem::path road_profile;
};

struct CompatibilityResult {
    CompatibilityStatus status = CompatibilityStatus::Unknown;
    std::optional<CompatibilityProfile> profile;
    std::size_t files_hashed = 0;
    std::string detail;
    bool compatible() const { return status == CompatibilityStatus::Compatible && profile.has_value(); }
};

CompatibilityProfile load_compatibility_profile(const std::filesystem::path& manifest);
CompatibilityResult detect_compatibility(const std::filesystem::path& data_root,
                                         const std::filesystem::path& compatibility_root);

std::uint64_t compatibility_detection_count();
void reset_compatibility_detection_count_for_tests();

} // namespace openemperor::assets
