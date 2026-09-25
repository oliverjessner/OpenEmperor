#include "app/MenuStorage.h"
#include "maps/StoredGraphicsPlan.h"
#include "persistence/SandboxSave.h"
#include "platform/AtomicReplace.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <random>
#include <stdexcept>

namespace openemperor::menu {
namespace fs=std::filesystem;
namespace {
bool inside(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for (;a!=root.end();++a,++b) if (b==path.end() || *a!=*b) return false;
    return true;
}
std::string bounded_string(const nlohmann::json& j,const char* key) {
    if (!j.contains(key) || !j.at(key).is_string()) throw std::runtime_error(std::string("invalid settings field: ")+key);
    const auto value=j.at(key).get<std::string>();
    if (value.size()>4096) throw std::runtime_error("settings path too long");
    return value;
}
fs::path checked_archive(const fs::path& root,const fs::path& relative) {
    std::error_code ec;
    const auto path=fs::canonical(root/relative,ec);
    if (ec || !inside(root,path) || !fs::is_regular_file(path))
        throw std::runtime_error("required archive missing or escapes data root: "+relative.generic_string());
    std::ifstream input(path,std::ios::binary);
    if (!input) throw std::runtime_error("required archive is unreadable: "+relative.generic_string());
    return path;
}
}
SettingsRead read_settings(const fs::path& root) {
    SettingsRead result;
    const auto path=root/"settings.json";
    std::error_code ec;
    if (!fs::exists(path,ec)) return result;
    try {
        if (ec || fs::is_symlink(path) || !fs::is_regular_file(path) || fs::file_size(path)>16384)
            throw std::runtime_error("settings file invalid or too large");
        std::ifstream input(path,std::ios::binary);
        if (!input) throw std::runtime_error("settings file unreadable");
        auto j=nlohmann::json::parse(input);
        if (!j.is_object() || !j.contains("version") || !j.at("version").is_number_integer() ||
            j.at("version")!=1) throw std::runtime_error("unknown settings version");
        result.value.data_root=bounded_string(j,"data_root");
        result.value.last_map=bounded_string(j,"last_map");
        result.value.last_save=bounded_string(j,"last_save");
        const auto profile=bounded_string(j,"profile");
        bool found=false;
        for (const auto p:{simulation::RulesProfile::LogisticsV1,simulation::RulesProfile::ProductionV2,
             simulation::RulesProfile::HouseholdV3,simulation::RulesProfile::SettlementV4,
             simulation::RulesProfile::IndustryV5,simulation::RulesProfile::CityV6,
             simulation::RulesProfile::CityV7,simulation::RulesProfile::CityV8,
             simulation::RulesProfile::CityV9,simulation::RulesProfile::CityV10})
            if (profile==simulation::rules_profile_name(p)) { result.value.profile=p; found=true; }
        if (!found) throw std::runtime_error("unknown settings profile");
    } catch (const std::exception& e) {
        result.needs_reset=true; result.message=e.what(); result.value={};
    }
    return result;
}
void write_settings(const fs::path& root,const Settings& settings) {
    if (!settings.data_root.empty()) {
        std::error_code ec;
        const auto data=fs::canonical(settings.data_root,ec);
        if (!ec) {
            const auto app=fs::weakly_canonical(root,ec);
            if (!ec && inside(data,app))
                throw std::runtime_error("preferences folder must be outside original data");
        }
    }
    fs::create_directories(root);
    const auto file=root/"settings.json";
    if (fs::is_symlink(file)) throw std::runtime_error("settings symlink rejected");
    const nlohmann::json j={{"version",1},{"data_root",settings.data_root.generic_string()},
        {"last_map",settings.last_map.generic_string()},
        {"last_save",settings.last_save.generic_string()},
        {"profile",simulation::rules_profile_name(settings.profile)}};
    platform::atomic_replace(file,j.dump(2),[&]{ if (fs::is_symlink(file))
        throw std::runtime_error("settings symlink rejected"); });
}
maps::MapCatalog validate_data_root(const fs::path& path) {
    std::error_code ec;
    const auto root=fs::canonical(path,ec);
    if (ec || !fs::is_directory(root)) throw std::runtime_error("invalid original data directory");
    checked_archive(root,"DATA/China_Terrain.sg3");
    checked_archive(root,"DATA/China_Elevation.sg3");
    checked_archive(root,"DATA/China_Terrain.555");
    checked_archive(root,"DATA/China_Elevation.555");
    auto catalog=maps::discover_standalone_maps(root);
    if (std::none_of(catalog.entries.begin(),catalog.entries.end(),[](const auto& e){return e.map_profile;}))
        throw std::runtime_error("no supported standalone map found");
    return catalog;
}
SaveList list_saves(const fs::path& root) {
    SaveList result;
    const auto requested=root/"saves";
    std::error_code ec;
    if (!fs::exists(requested,ec)) return result;
    if (ec || !fs::is_directory(requested) || fs::is_symlink(requested))
        throw std::runtime_error("save directory invalid");
    const auto dir=fs::canonical(requested,ec);
    if (ec) throw std::runtime_error("save directory cannot be resolved");
    std::vector<fs::path> paths;
    std::size_t scanned=0;
    for (fs::directory_iterator it(dir,fs::directory_options::none,ec),end;it!=end && !ec;it.increment(ec)) {
        if (++scanned>4096) { result.truncated=true; break; }
        if (it->path().extension()==".json") paths.push_back(it->path());
    }
    if (ec) throw std::runtime_error("cannot enumerate saves: "+ec.message());
    std::sort(paths.begin(),paths.end());
    if (paths.size()>64) { paths.resize(64); result.truncated=true; }
    for (const auto& path:paths) {
        SaveEntry e; e.path=path; e.name=path.filename().string();
        try {
            if (fs::is_symlink(path) || !fs::is_regular_file(path)) throw std::runtime_error("unsafe save file");
            const auto doc=persistence::read_save(path);
            e.map=doc.map_relative.generic_string();
            e.profile=simulation::rules_profile_name(doc.world.profile);
            e.tick=doc.world.ticks;
            e.schema=doc.source_schema_version; e.rule_version=doc.world.rule_version;
        } catch (const std::exception& error) { e.error=error.what(); }
        result.entries.push_back(std::move(e));
    }
    return result;
}
fs::path new_save_target(const fs::path& app_root,const fs::path& data_root) {
    std::error_code ec;
    const auto data=fs::canonical(data_root,ec);
    if (ec) throw std::runtime_error("original data root cannot be resolved");
    const auto intended=fs::weakly_canonical(app_root,ec);
    if (ec || inside(data,intended)) throw std::runtime_error("app folder must be outside original data");
    fs::create_directories(app_root/"saves");
    const auto saves=fs::canonical(app_root/"saves",ec);
    if (ec || inside(data,saves)) throw std::runtime_error("save folder must be outside original data");
    std::random_device random;
    for (int i=0;i<64;++i) {
        const auto name="sandbox-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+
            "-"+std::to_string(random())+".json";
        const auto path=saves/name;
        if (!fs::exists(path)) { persistence::validate_save_target(path,data); return path; }
    }
    throw std::runtime_error("cannot reserve unique save filename");
}
}
