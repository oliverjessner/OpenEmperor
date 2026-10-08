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
enum class WalkerVisualRole : std::uint8_t {
    Clay=0, Pottery=1, Household=2, FireInspector=3, Supplier=4, Distributor=5
};
inline constexpr std::size_t walker_core_role_count=3;
inline constexpr std::size_t walker_schema3_role_count=4;
inline constexpr std::size_t walker_visual_role_count=6;
inline constexpr std::size_t walker_max_frame_aliases=256;
inline constexpr std::size_t walker_max_unique_assets=256;
inline constexpr std::uint64_t walker_max_rgba_bytes=64U*1024U*1024U;
constexpr std::size_t walker_role_index(WalkerVisualRole role) {
    return static_cast<std::size_t>(role);
}
const char* walker_role_name(WalkerVisualRole role);
struct WalkerFrame {
    std::string alias;
    AssetId id; // Physical SG3 record, never a packed runtime image ID.
    double foot_x=0, foot_y=0; // Explicit display anchor, after any frame transform.
    std::size_t image_index=0; // Index into unique_images.
    bool flip_x=false; // Schema-4 Supplier/Distributor display only; pixels stay native.
};
struct WalkerRoleVisual {
    std::uint32_t ticks_per_frame=1;
    std::string evidence;
    std::string clip_id; // Required for FireInspector and schema-4 supplier/distributor families.
    std::vector<WalkerFrame> frames;
    std::array<std::vector<std::size_t>,4> clips; // Indices into frames; order is manifest order.
    std::size_t idle_frame=0;
};
struct WalkerVisualProfile {
    std::uint32_t schema_version=1;
    std::array<std::optional<WalkerRoleVisual>,walker_visual_role_count> roles;
    std::vector<RgbaImage> unique_images; // Deduplicated across all configured roles.
    const WalkerRoleVisual* find(WalkerVisualRole role) const {
        const auto index=walker_role_index(role);
        if (index>=roles.size()) return nullptr;
        const auto& value=roles[index];
        return value ? &*value:nullptr;
    }
};
constexpr std::size_t direction_index(StorageDirection direction) {
    return static_cast<std::size_t>(direction);
}
// Manifest may be outside data_root; every referenced SG3 and derived .555 must stay within it.
WalkerVisualProfile load_walker_visual_profile(const std::filesystem::path& data_root,
                                               const std::filesystem::path& manifest);
// A built-in supplement must contain only a complete schema-3 FireInspector.
// Global deduplication and existing alias/asset/RGBA budgets include the core.
// Any error leaves the supplied core profile unchanged.
void append_fire_inspector_visual_profile(const std::filesystem::path& data_root,
                                         const std::filesystem::path& manifest,
                                         WalkerVisualProfile& profile);
// A built-in schema-4 supplement contains exactly the complete supplier and
// distributor families. Both roles share the existing global budgets and
// physical assets; any failure retains all existing roles and prepared images.
void append_market_visual_profile(const std::filesystem::path& data_root,
                                  const std::filesystem::path& manifest,
                                  WalkerVisualProfile& profile);
std::uint64_t walker_rgba_bytes(const WalkerVisualProfile& profile);
}
