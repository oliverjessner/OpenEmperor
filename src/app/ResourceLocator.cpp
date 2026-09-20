#include "app/ResourceLocator.h"

namespace openemperor {
namespace fs = std::filesystem;

fs::path locate_resource_root(const fs::path& base_directory, const fs::path& injected_root) {
    std::error_code error;
    if (!injected_root.empty()) {
        const auto root = fs::canonical(injected_root, error);
        return !error && fs::is_directory(root, error) && !error ? root : fs::path{};
    }
    if (base_directory.empty()) return {};
    const auto base = fs::canonical(base_directory, error);
    if (error || !fs::is_directory(base, error) || error) return {};
    const fs::path candidates[] = {
        base.filename() == "MacOS" ? base.parent_path() / "Resources" : fs::path{},
        base / "resources"
    };
    for (const auto& candidate : candidates) {
        if (candidate.empty()) continue;
        const auto resolved = fs::canonical(candidate, error);
        if (!error && fs::is_directory(resolved, error) && !error) return resolved;
        error.clear();
    }
    return {};
}

} // namespace openemperor
