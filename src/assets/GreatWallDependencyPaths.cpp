#include "assets/GreatWallDependencyPaths.h"

#include <stdexcept>
#include <vector>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
constexpr std::size_t maximum_data_entries=4096;
std::string ascii_lower(std::string_view value) {
    std::string out(value);
    for (auto& c:out) if (c>='A' && c<='Z') c=char(c-'A'+'a');
    return out;
}
bool inside(const fs::path& root,const fs::path& target) {
    const auto relative=target.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return false;
    for (const auto& component:relative) if (component=="..") return false;
    return true;
}
bool archive_name(std::string_view folded) {
    for (const auto prefix:{std::string_view{"china_mon_greatwall_"},
                            std::string_view{"china_mon_earthen_greatwall_"}}) {
        if (!folded.starts_with(prefix)) continue;
        const auto suffix=folded.substr(prefix.size());
        if (suffix=="10") return true;
        if (suffix.size()==1 && suffix[0]>='1' && suffix[0]<='9') return true;
        if (prefix=="china_mon_greatwall_" && suffix=="ruined") return true;
    }
    return false;
}
}

bool known_great_wall_dependency_filename(std::string_view filename) {
    const auto folded=ascii_lower(filename);
    const auto dot=folded.rfind('.');
    if (dot==std::string::npos) return false;
    const auto extension=std::string_view(folded).substr(dot);
    if (extension!=".sg3" && extension!=".555") return false;
    const auto stem=std::string_view(folded).substr(0,dot);
    return archive_name(stem) ||
        (extension==".555" && (stem=="zeus_system" || stem=="china_mon_greatwall_ruins"));
}
bool known_great_wall_archive_path(const fs::path& archive_path) {
    return ascii_lower(archive_path.parent_path().filename().string())=="data" &&
        ascii_lower(archive_path.extension().string())==".sg3" &&
        known_great_wall_dependency_filename(archive_path.filename().string());
}
std::optional<std::string> select_great_wall_dependency_filename(
    std::string_view requested,std::span<const std::string> names) {
    if (!known_great_wall_dependency_filename(requested))
        throw std::runtime_error("unsupported Great Wall dependency filename");
    if (names.size()>maximum_data_entries)
        throw std::runtime_error("Great Wall DATA directory exceeds dependency lookup bound");
    for (const auto& name:names) if (name==requested) return name;
    const auto wanted=ascii_lower(requested);
    std::optional<std::string> selected;
    for (const auto& name:names) if (ascii_lower(name)==wanted) {
        if (selected) throw std::runtime_error("ambiguous Great Wall dependency filename: "+std::string(requested));
        selected=name;
    }
    return selected;
}
std::optional<fs::path> resolve_great_wall_dependency_path(
    const fs::path& data_root,const fs::path& relative) {
    if (relative.parent_path()!=fs::path{"DATA"} ||
        !known_great_wall_dependency_filename(relative.filename().string()))
        throw std::runtime_error("Great Wall dependency must be a known DATA leaf path");
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root))
        throw std::runtime_error("Great Wall data directory cannot be resolved");
    if (fs::is_symlink(fs::symlink_status(root/"DATA",error)))
        throw std::runtime_error("Great Wall DATA directory must not use a symlink");
    if (error==std::errc::no_such_file_or_directory) return {};
    if (error) throw std::runtime_error("Great Wall DATA directory is inaccessible");
    const auto data=fs::canonical(root/"DATA",error);
    if (error || !inside(root,data) || !fs::is_directory(data))
        throw std::runtime_error("Great Wall DATA directory is missing or escapes data directory");
    std::vector<std::string> names;
    fs::directory_iterator it(data,error),end;
    if (error) throw std::runtime_error("Great Wall DATA directory cannot be enumerated");
    while (it!=end) {
        if (names.size()==maximum_data_entries)
            throw std::runtime_error("Great Wall DATA directory exceeds dependency lookup bound");
        names.push_back(it->path().filename().string());
        it.increment(error);
        if (error) throw std::runtime_error("Great Wall DATA dependency enumeration failed");
    }
    const auto name=select_great_wall_dependency_filename(relative.filename().string(),names);
    if (!name) return {};
    const auto candidate=data / *name;
    // scan_asset_archive already rejects archive symlinks. Keep that policy
    // before reading metadata; bitmap symlinks still require DATA containment.
    if (ascii_lower(candidate.extension().string())==".sg3" &&
        fs::is_symlink(fs::symlink_status(candidate,error)))
        throw std::runtime_error("Great Wall archive must not use a symlink");
    if (error) throw std::runtime_error("Great Wall dependency is inaccessible");
    const auto selected=fs::canonical(candidate,error);
    if (error || !inside(root,selected) || !inside(data,selected) || !fs::is_regular_file(selected))
        throw std::runtime_error("Great Wall dependency is missing, not regular or escapes DATA: "+*name);
    return selected;
}
} // namespace openemperor::assets
