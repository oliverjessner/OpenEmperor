#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace fs = std::filesystem;
namespace maps = openemperor::maps;
using Json = nlohmann::json;

void usage(const char* program) {
    std::cerr << "Usage: " << program << " --data <directory> --list [--json]\n"
              << "       " << program << " <file> [--part <index>] [--json]\n";
}
bool candidate_extension(const fs::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".map" || ext == ".pak" || ext == ".sav" || ext == ".sve" || ext == ".gam";
}
std::vector<fs::path> candidates(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec) || ec) throw std::runtime_error("data directory is missing");
    std::vector<fs::path> paths;
    fs::recursive_directory_iterator it{root, fs::directory_options::skip_permission_denied, ec};
    const fs::recursive_directory_iterator end;
    if (ec) throw std::runtime_error("cannot scan data directory: " + ec.message());
    for (; it != end; it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const auto status = it->symlink_status(ec);
        if (ec) { ec.clear(); it.disable_recursion_pending(); continue; }
        if (fs::is_symlink(status)) { it.disable_recursion_pending(); continue; }
        if (fs::is_regular_file(status) && candidate_extension(it->path())) paths.push_back(it->path());
    }
    std::sort(paths.begin(), paths.end(), [&](const fs::path& a, const fs::path& b) {
        return a.lexically_relative(root).generic_string() < b.lexically_relative(root).generic_string();
    });
    return paths;
}
Json frequency(const maps::MapLayer& layer) {
    std::map<std::uint32_t, std::uint64_t> counts;
    for (const auto value : layer.values) ++counts[value];
    std::vector<std::pair<std::uint32_t, std::uint64_t>> ordered(counts.begin(), counts.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    Json top = Json::array();
    for (std::size_t i = 0; i < std::min<std::size_t>(ordered.size(), 16); ++i)
        top.push_back({{"value", ordered[i].first}, {"count", ordered[i].second}});
    return {{"distinct_values", counts.size()}, {"top_values", top}};
}
Json part_json(const maps::EmperorContainer& container, std::size_t index) {
    const auto& part = container.parts()[index];
    std::uint64_t compressed = 0;
    std::map<std::uint32_t, std::size_t> unknown_words;
    for (const auto& block : part.blocks) {
        compressed += block.compressed_size;
        ++unknown_words[block.unknown_word];
    }
    const auto probe = maps::probe_map_part(container, index);
    Json words = Json::array();
    for (const auto& [word, count] : unknown_words)
        words.push_back({{"raw_value", word}, {"blocks", count}});
    return {{"index", index}, {"physical_start", part.physical_start},
            {"physical_end_exclusive", part.physical_end}, {"block_count", part.blocks.size()},
            {"compressed_payload_bytes", compressed},
            {"uncompressed_bytes", part.uncompressed_size},
            {"unknown_block_words", words},
            {"map_profile", maps::part_profile_name(probe.profile)},
            {"map_profile_reason", probe.reason},
            {"declared_map_size", probe.profile == maps::PartProfile::Map
                                       ? Json(probe.declared_map_size) : Json(nullptr)}};
}
Json inspect_file(const fs::path& path, const std::string& display,
                  std::optional<std::uint32_t> chosen_part) {
    Json result{{"relative_path", display}, {"extension", path.extension().string()},
                {"container_valid", false}, {"map_read_success", false}};
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (!ec) result["physical_bytes"] = size;
    try {
        const auto container = maps::EmperorContainer::open(path);
        result["container_valid"] = true;
        result["container_profile"] = "emperor_block_zlib_v1";
        result["multipart"] = container.multipart();
        result["part_count"] = container.parts().size();
        result["ignored_part_table_words"] = container.ignored_part_table_words();
        result["parts"] = Json::array();
        for (std::size_t i = 0; i < container.parts().size(); ++i)
            result["parts"].push_back(part_json(container, i));
        if (!chosen_part && container.parts().size() == 1) chosen_part = 0;
        if (chosen_part) {
            if (*chosen_part >= container.parts().size()) throw maps::ContainerError("selected part index is out of range");
            result["selected_part"] = *chosen_part;
            try {
                const auto map = maps::read_emperor_map(container, *chosen_part);
                result["map_read_success"] = true;
                result["map"] = {
                    {"profile", "emperor_map_v1_storage_grid"},
                    {"stored_width", map.stored_width}, {"stored_height", map.stored_height},
                    {"declared_map_size", map.declared_map_size},
                    {"active_cells", "unknown"},
                    {"terrain_raw", {{"logical_offset", map.terrain_raw.logical_offset},
                                      {"element_type", "uint32_le"},
                                      {"element_count", map.terrain_raw.values.size()},
                                      {"byte_length", maps::grid_byte_length},
                                      {"bounds_valid", true},
                                      {"statistics", frequency(map.terrain_raw)}}},
                    {"objects_raw", {{"logical_offset", map.objects_raw.logical_offset},
                                      {"element_type", "uint32_le"},
                                      {"element_count", map.objects_raw.values.size()},
                                      {"byte_length", maps::grid_byte_length},
                                      {"bounds_valid", true},
                                      {"statistics", frequency(map.objects_raw)}}}
                };
            } catch (const maps::UnsupportedMapProfile& error) {
                result["map_error"] = error.what();
            }
        }
    } catch (const std::exception& error) {
        result["error"] = error.what();
    }
    return result;
}
void print_human(const Json& result) {
    std::cout << result.value("relative_path", "?") << " (" << result.value("physical_bytes", 0U)
              << " bytes, " << result.value("extension", "") << "): ";
    if (!result.value("container_valid", false)) {
        std::cout << "container invalid: " << result.value("error", "unknown error") << '\n';
        return;
    }
    std::cout << (result.value("multipart", false) ? "multipart" : "single-part")
              << " container, " << result.value("part_count", 0U) << " parts";
    if (result.value("map_read_success", false)) {
        const auto& map = result.at("map");
        std::cout << ", map read: storage " << map.at("stored_width") << 'x'
                  << map.at("stored_height") << ", declared size " << map.at("declared_map_size")
                  << ", terrain values " << map.at("terrain_raw").at("statistics").at("distinct_values")
                  << ", object values " << map.at("objects_raw").at("statistics").at("distinct_values")
                  << ", active cells unknown";
    } else if (result.contains("map_error")) std::cout << ", map unsupported: " << result.at("map_error");
    if (result.contains("error")) std::cout << ", error: " << result.at("error");
    std::cout << '\n';
    for (const auto& part : result.at("parts")) {
        std::cout << "  part " << part.at("index") << ": " << part.at("block_count")
                  << " blocks, " << part.at("compressed_payload_bytes") << " compressed, "
                  << part.at("uncompressed_bytes") << " uncompressed; "
                  << part.at("map_profile") << '\n';
    }
}
} // namespace

int main(int argc, char* argv[]) {
    bool list = false, json = false, data_supplied = false, file_supplied = false;
    fs::path data_root, file;
    std::optional<std::uint32_t> part;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg == "--list" && !list) list = true;
        else if (arg == "--json" && !json) json = true;
        else if (arg == "--data" && !data_supplied && i + 1 < argc) {
            data_root = argv[++i]; data_supplied = true;
        } else if (arg == "--part" && !part && i + 1 < argc) {
            const std::string_view value{argv[++i]};
            std::uint32_t parsed_value = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), parsed_value);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                usage(argv[0]); return 2;
            }
            part = parsed_value;
        } else if (!file_supplied && !arg.starts_with("--")) {
            file = argv[i]; file_supplied = true;
        } else { usage(argv[0]); return 2; }
    }
    if ((list && (!data_supplied || file_supplied || part)) ||
        (!list && (!file_supplied || data_supplied))) { usage(argv[0]); return 2; }
    try {
        if (list) {
            const fs::path root = fs::canonical(data_root);
            Json entries = Json::array();
            for (const auto& path : candidates(root))
                entries.push_back(inspect_file(path, path.lexically_relative(root).generic_string(), std::nullopt));
            if (json) std::cout << entries.dump() << '\n';
            else {
                for (const auto& entry : entries) print_human(entry);
                std::cout << entries.size() << " candidate files\n";
            }
            return 0;
        }
        const auto result = inspect_file(file, file.generic_string(), part);
        if (json) std::cout << result.dump() << '\n';
        else print_human(result);
        return result.value("container_valid", false) &&
            !result.contains("error") &&
            (!part || result.value("map_read_success", false)) ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Map inspector failed: " << error.what() << '\n';
        return 1;
    }
}
