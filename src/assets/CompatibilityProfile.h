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
    // Independently fingerprinted optional presentation. Failure never changes
    // the existing walker/building/road compatibility contract.
    std::filesystem::path fire_profile;
    std::vector<CompatibilityFingerprint> fire_files;
    std::string fire_metadata_error;
    std::filesystem::path fire_inspector_profile;
    std::vector<CompatibilityFingerprint> fire_inspector_files;
    std::string fire_inspector_metadata_error;
};

struct CompatibilityResult {
    CompatibilityStatus status = CompatibilityStatus::Unknown;
    std::optional<CompatibilityProfile> profile;
    std::size_t files_hashed = 0;
    std::string detail;
    bool compatible() const { return status == CompatibilityStatus::Compatible && profile.has_value(); }
    CompatibilityStatus fire_status = CompatibilityStatus::Unknown;
    std::size_t fire_files_hashed = 0;
    std::string fire_detail = "optional fire animation is not declared";
    bool fire_compatible() const {
        return compatible() && fire_status == CompatibilityStatus::Compatible &&
            !profile->fire_profile.empty();
    }
    CompatibilityStatus fire_inspector_status = CompatibilityStatus::Unknown;
    std::size_t fire_inspector_files_hashed = 0;
    std::string fire_inspector_detail = "optional FireInspector animation is not declared";
    bool fire_inspector_compatible() const {
        return compatible() && fire_inspector_status == CompatibilityStatus::Compatible &&
            !profile->fire_inspector_profile.empty();
    }
};

CompatibilityProfile load_compatibility_profile(const std::filesystem::path& manifest);
CompatibilityResult detect_compatibility(const std::filesystem::path& data_root,
                                         const std::filesystem::path& compatibility_root);

std::uint64_t compatibility_detection_count();
void reset_compatibility_detection_count_for_tests();

} // namespace openemperor::assets
