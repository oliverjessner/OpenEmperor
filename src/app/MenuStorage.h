#pragma once
#include "maps/MapCatalog.h"
#include "simulation/World.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::menu {
struct Settings {
    std::filesystem::path data_root, last_map, last_save;
    simulation::RulesProfile profile=simulation::RulesProfile::CityV6;
};
struct SettingsRead {
    Settings value;
    bool needs_reset=false;
    std::string message;
};
struct SaveEntry {
    std::filesystem::path path;
    std::string name, map, profile, error;
    std::uint64_t tick=0;
    std::uint32_t schema=0,rule_version=0;
};
struct SaveList { std::vector<SaveEntry> entries; bool truncated=false; };
SettingsRead read_settings(const std::filesystem::path& app_root);
void write_settings(const std::filesystem::path& app_root,const Settings& settings);
maps::MapCatalog validate_data_root(const std::filesystem::path& root);
SaveList list_saves(const std::filesystem::path& app_root);
std::filesystem::path new_save_target(const std::filesystem::path& app_root,
                                      const std::filesystem::path& data_root);
}
