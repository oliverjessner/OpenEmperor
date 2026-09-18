#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::maps {

struct MapCatalogEntry {
    std::filesystem::path relative_path;
    std::optional<std::uint64_t> file_bytes;
    bool container_valid = false;
    bool map_profile = false;
    std::optional<std::uint32_t> declared_size;
    std::string error;
};
struct MapCatalog {
    std::filesystem::path data_root;
    std::vector<MapCatalogEntry> entries;
    std::vector<std::string> scan_errors;
};

std::filesystem::path resolve_map_path(const std::filesystem::path& root,
                                       const std::filesystem::path& relative);
MapCatalog discover_standalone_maps(const std::filesystem::path& data_root);

} // namespace openemperor::maps
