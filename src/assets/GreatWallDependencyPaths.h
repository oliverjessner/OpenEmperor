#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace openemperor::assets {
// Only the original Great Wall archive families (phases 1..10 and Ruined)
// and their documented bitmap leaf names are eligible. No nested path search.
bool known_great_wall_dependency_filename(std::string_view filename);
bool known_great_wall_archive_path(const std::filesystem::path& archive_path);

// Exact spelling wins. Otherwise a unique ASCII-case match is required.
// Separate from filesystem lookup so ambiguous directory listings can be
// validated on hosts whose volumes do not permit two case-only filenames.
std::optional<std::string> select_great_wall_dependency_filename(
    std::string_view requested,std::span<const std::string> directory_filenames);

// Enumerates the immediate DATA directory only, with a fixed entry bound.
// The selected regular file must remain inside canonical DATA and data_root.
// Missing files return nullopt; unsafe or ambiguous selections throw.
std::optional<std::filesystem::path> resolve_great_wall_dependency_path(
    const std::filesystem::path& data_root,const std::filesystem::path& relative);
} // namespace openemperor::assets
