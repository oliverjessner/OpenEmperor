#include "maps/MapCatalog.h"

#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace openemperor::maps {
namespace {
namespace fs = std::filesystem;
bool map_extension(const fs::path& path) {
    auto extension=path.extension().string();
    std::transform(extension.begin(),extension.end(),extension.begin(),
        [](unsigned char ch){ return static_cast<char>(std::tolower(ch)); });
    return extension==".map";
}
bool inside(const fs::path& root,const fs::path& path) {
    const auto relative=path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& part:relative) if (part=="..") return false;
    return true;
}
} // namespace

fs::path resolve_map_path(const fs::path& root,const fs::path& relative) {
    std::error_code error;
    const auto canonical_root=fs::canonical(root,error);
    if (error || !fs::is_directory(canonical_root))
        throw std::runtime_error("map data directory cannot be resolved");
    if (relative.empty()) throw std::runtime_error("map path is empty");
    const auto canonical_path=fs::canonical(relative.is_absolute() ? relative : canonical_root/relative,error);
    if (error || !inside(canonical_root,canonical_path) || !fs::is_regular_file(canonical_path))
        throw std::runtime_error("map file is missing or escapes data directory: "+relative.generic_string());
    return canonical_path;
}

MapCatalog discover_standalone_maps(const fs::path& data_root) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root))
        throw std::runtime_error("map data directory is missing or cannot be accessed");
    MapCatalog result;
    result.data_root=root;
    fs::recursive_directory_iterator it(root,fs::directory_options::none,error),end;
    if (error) throw std::runtime_error("cannot scan map directory: "+error.message());
    while (it!=end) {
        const auto path=it->path();
        const auto relative=path.lexically_relative(root);
        const auto status=it->symlink_status(error);
        if (error) {
            result.scan_errors.push_back(relative.generic_string()+": "+error.message());
            error.clear(); it.disable_recursion_pending();
        } else if (fs::is_symlink(status)) {
            it.disable_recursion_pending();
        } else if (fs::is_regular_file(status) && map_extension(path)) {
            MapCatalogEntry entry;
            entry.relative_path=relative;
            entry.file_bytes=fs::file_size(path,error);
            if (error) { entry.file_bytes.reset(); entry.error=error.message(); error.clear(); }
            else {
                try {
                    const auto container=EmperorContainer::open(path);
                    entry.container_valid=true;
                    if (container.multipart() || container.parts().size()!=1)
                        entry.error="standalone browser requires one map part";
                    else {
                        const auto probe=probe_map_part(container,0);
                        entry.map_profile=probe.profile==PartProfile::Map;
                        if (entry.map_profile) entry.declared_size=probe.declared_map_size;
                        else entry.error=probe.reason.empty() ? "unsupported map profile" : probe.reason;
                    }
                } catch (const std::exception& exception) { entry.error=exception.what(); }
            }
            result.entries.push_back(std::move(entry));
        }
        it.increment(error);
        if (error) {
            result.scan_errors.push_back(relative.generic_string()+": "+error.message());
            error.clear();
        }
    }
    std::sort(result.entries.begin(),result.entries.end(),[](const auto& a,const auto& b){
        return a.relative_path.generic_string()<b.relative_path.generic_string();
    });
    return result;
}
} // namespace openemperor::maps
