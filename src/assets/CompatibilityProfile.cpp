#include "assets/CompatibilityProfile.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace openemperor::assets {
namespace fs = std::filesystem;
namespace {
constexpr std::uintmax_t max_manifest_bytes = 64U * 1024U;
constexpr std::size_t max_profiles = 16;
constexpr std::size_t max_fingerprints = 64;
std::atomic<std::uint64_t> detection_count{0};

bool safe_relative(const fs::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_name()) return false;
    const auto text = path.generic_string();
    if (text.find('\\') != std::string::npos) return false;
    for (const auto& part : path)
        if (part.empty() || part == "." || part == "..") return false;
    return true;
}

bool contained(const fs::path& path, const fs::path& root) {
    auto child = path.begin();
    auto parent = root.begin();
    for (; parent != root.end(); ++parent, ++child)
        if (child == path.end() || *child != *parent) return false;
    return true;
}

std::string read_bounded_text(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error || size > max_manifest_bytes)
        throw std::runtime_error("compatibility manifest is missing or too large");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("compatibility manifest could not be opened");
    std::string text(static_cast<std::size_t>(size), '\0');
    stream.read(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream || stream.gcount() != static_cast<std::streamsize>(text.size()))
        throw std::runtime_error("compatibility manifest could not be read completely");
    return text;
}

std::string required_string(const nlohmann::json& object, const char* key) {
    if (!object.contains(key) || !object.at(key).is_string())
        throw std::runtime_error(std::string("compatibility manifest lacks ") + key);
    return object.at(key).get<std::string>();
}

fs::path required_relative(const nlohmann::json& object, const char* key) {
    const fs::path result(required_string(object, key));
    if (!safe_relative(result))
        throw std::runtime_error(std::string("compatibility manifest has unsafe ") + key);
    return result;
}

bool sha256_text(const std::string& value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) || (c >= 'a' && c <= 'f');
    });
}

std::string sha256_file(const fs::path& path) {
    using Context = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
    Context context(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1)
        throw std::runtime_error("SHA-256 initialization failed");
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("fingerprinted file could not be opened");
    std::array<char, 1024U * 1024U> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = stream.gcount();
        if (count > 0 && EVP_DigestUpdate(context.get(), buffer.data(),
                                          static_cast<std::size_t>(count)) != 1)
            throw std::runtime_error("SHA-256 update failed");
    }
    if (!stream.eof()) throw std::runtime_error("fingerprinted file could not be read");
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int size = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &size) != 1 || size != 32)
        throw std::runtime_error("SHA-256 finalization failed");
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < size; ++i) output << std::setw(2) << static_cast<unsigned>(digest[i]);
    return output.str();
}

std::optional<fs::path> checked_file(const fs::path& root, const fs::path& relative) {
    fs::path current = root;
    std::error_code error;
    for (const auto& part : relative) {
        current /= part;
        const auto status = fs::symlink_status(current, error);
        if (error || fs::is_symlink(status)) return std::nullopt;
    }
    const auto canonical = fs::canonical(current, error);
    if (error || !contained(canonical, root) || !fs::is_regular_file(canonical, error) || error)
        return std::nullopt;
    return canonical;
}
}

const char* compatibility_status_name(CompatibilityStatus status) {
    switch (status) {
    case CompatibilityStatus::Compatible: return "compatible";
    case CompatibilityStatus::Unknown: return "unknown";
    case CompatibilityStatus::MissingFile: return "missing_file";
    case CompatibilityStatus::FingerprintMismatch: return "fingerprint_mismatch";
    }
    return "unknown";
}

CompatibilityProfile load_compatibility_profile(const fs::path& manifest) {
    std::error_code error;
    if (fs::is_symlink(fs::symlink_status(manifest, error)) || error)
        throw std::runtime_error("compatibility manifest may not be a symlink");
    const auto canonical_manifest = fs::canonical(manifest, error);
    if (error) throw std::runtime_error("compatibility manifest is missing");
    const auto document = nlohmann::json::parse(read_bounded_text(canonical_manifest));
    if (!document.is_object() || document.value("schema_version", 0U) != 1U)
        throw std::runtime_error("unsupported compatibility manifest schema");
    CompatibilityProfile result;
    result.schema_version = 1;
    result.id = required_string(document, "id");
    if (result.id.empty() || result.id.size() > 96 ||
        !std::all_of(result.id.begin(), result.id.end(), [](unsigned char c) {
            return std::isalnum(c) || c == '-' || c == '_' || c == '.';
        })) throw std::runtime_error("compatibility manifest has invalid id");
    result.evidence = required_string(document, "evidence");
    result.pack_root = canonical_manifest.parent_path();
    if (!document.contains("files") || !document.at("files").is_array() ||
        document.at("files").empty() || document.at("files").size() > max_fingerprints)
        throw std::runtime_error("compatibility manifest has invalid fingerprint list");
    for (const auto& entry : document.at("files")) {
        if (!entry.is_object()) throw std::runtime_error("compatibility fingerprint is not an object");
        CompatibilityFingerprint fingerprint;
        fingerprint.relative_path = required_relative(entry, "path");
        fingerprint.sha256 = required_string(entry, "sha256");
        if (!sha256_text(fingerprint.sha256))
            throw std::runtime_error("compatibility fingerprint is not lowercase SHA-256");
        if (std::any_of(result.files.begin(), result.files.end(), [&](const auto& existing) {
            return existing.relative_path == fingerprint.relative_path;
        })) throw std::runtime_error("compatibility fingerprint path is duplicated");
        result.files.push_back(std::move(fingerprint));
    }
    if (!document.contains("profiles") || !document.at("profiles").is_object())
        throw std::runtime_error("compatibility manifest lacks profiles");
    const auto& profiles = document.at("profiles");
    const auto resolve = [&](const char* key) {
        const auto relative = required_relative(profiles, key);
        const auto path = fs::weakly_canonical(result.pack_root / relative, error);
        if (error || !contained(path, result.pack_root))
            throw std::runtime_error("compatibility profile path escapes its pack");
        return path;
    };
    result.walker_profile = resolve("walker");
    result.building_profile = resolve("building");
    result.road_profile = resolve("road");
    return result;
}

CompatibilityResult detect_compatibility(const fs::path& data_root,
                                         const fs::path& compatibility_root) {
    ++detection_count;
    CompatibilityResult fallback;
    std::error_code error;
    const auto root = fs::canonical(data_root, error);
    if (error || !fs::is_directory(root, error) || error) {
        fallback.detail = "game-data root is unavailable";
        return fallback;
    }
    if (compatibility_root.empty() || !fs::is_directory(compatibility_root, error) || error) {
        fallback.detail = "built-in compatibility resources are unavailable";
        return fallback;
    }
    std::vector<fs::path> manifests;
    for (fs::directory_iterator it(compatibility_root, fs::directory_options::skip_permission_denied,
                                   error), end;
         !error && it != end; it.increment(error)) {
        if (it->is_symlink(error) || error) continue;
        if (it->is_directory(error) && !error && fs::is_regular_file(it->path() / "manifest.json", error) && !error)
            manifests.push_back(it->path() / "manifest.json");
        if (manifests.size() > max_profiles) {
            fallback.detail = "too many compatibility packs";
            return fallback;
        }
    }
    if (error) {
        fallback.detail = "compatibility resources could not be enumerated";
        return fallback;
    }
    std::sort(manifests.begin(), manifests.end());
    if (manifests.empty()) {
        fallback.detail = "no compatibility packs are installed";
        return fallback;
    }
    for (const auto& manifest : manifests) {
        CompatibilityProfile profile;
        try { profile = load_compatibility_profile(manifest); }
        catch (const std::exception& exception) {
            fallback.status = CompatibilityStatus::Unknown;
            fallback.detail = std::string("compatibility pack invalid: ") + exception.what();
            continue;
        }
        bool missing = false;
        bool unsafe = false;
        bool mismatch = false;
        std::size_t hashed = 0;
        for (const auto& expected : profile.files) {
            const auto candidate = checked_file(root, expected.relative_path);
            if (!candidate) {
                bool symlink = false;
                fs::path current = root;
                for (const auto& part : expected.relative_path) {
                    current /= part;
                    std::error_code link_error;
                    if (fs::is_symlink(fs::symlink_status(current, link_error)) && !link_error) {
                        symlink = true;
                        break;
                    }
                }
                std::error_code exists_error;
                missing = !symlink && !fs::exists(root / expected.relative_path, exists_error) &&
                    !exists_error;
                unsafe = !missing;
                break;
            }
            ++hashed;
            if (sha256_file(*candidate) != expected.sha256) { mismatch = true; break; }
        }
        fallback.files_hashed = std::max(fallback.files_hashed, hashed);
        if (!missing && !unsafe && !mismatch) {
            return {CompatibilityStatus::Compatible, std::move(profile), hashed,
                    "Compatible original data detected"};
        }
        fallback.profile.reset();
        fallback.status = missing ? CompatibilityStatus::MissingFile :
            CompatibilityStatus::FingerprintMismatch;
        fallback.detail = missing ? "required compatibility file is missing" :
            unsafe ? "required compatibility file path is unsafe" :
                     "game-data fingerprint does not match";
    }
    return fallback;
}

std::uint64_t compatibility_detection_count() { return detection_count.load(); }
void reset_compatibility_detection_count_for_tests() { detection_count.store(0); }

} // namespace openemperor::assets
