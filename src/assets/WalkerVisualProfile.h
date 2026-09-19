#pragma once
#include "assets/AssetCatalog.h"
#include "assets/Sg3RgbaDecoder.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::assets {
enum class StorageDirection { PosX, NegX, PosY, NegY };
enum class WalkerVisualRole : std::uint8_t { Clay, Pottery, Household };
constexpr std::size_t walker_role_index(WalkerVisualRole role) {
    return static_cast<std::size_t>(role);
}
const char* walker_role_name(WalkerVisualRole role);
struct WalkerFrame {
    std::string alias;
    AssetId id; // Physical SG3 record, never a packed runtime image ID.
    double foot_x=0, foot_y=0;
    std::size_t image_index=0; // Index into unique_images.
};
struct WalkerRoleVisual {
    std::uint32_t ticks_per_frame=1;
    std::string evidence;
    std::vector<WalkerFrame> frames;
    std::array<std::vector<std::size_t>,4> clips; // Indices into frames; order is manifest order.
    std::size_t idle_frame=0;
};
struct WalkerVisualProfile {
    std::uint32_t schema_version=1;
    std::array<std::optional<WalkerRoleVisual>,3> roles;
    std::vector<RgbaImage> unique_images; // Deduplicated across all configured roles.
    const WalkerRoleVisual* find(WalkerVisualRole role) const {
        const auto& value=roles[walker_role_index(role)];
        return value ? &*value:nullptr;
    }
};
constexpr std::size_t direction_index(StorageDirection direction) {
    return static_cast<std::size_t>(direction);
}
// Manifest may be outside data_root; every referenced SG3 and derived .555 must stay within it.
WalkerVisualProfile load_walker_visual_profile(const std::filesystem::path& data_root,
                                               const std::filesystem::path& manifest);
}
