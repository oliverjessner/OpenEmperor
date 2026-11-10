#pragma once
#include "assets/BuildingVisualProfile.h"
#include "assets/Sg3Archive.h"
#include "core/PerformanceDiagnostics.h"
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace openemperor::assets::original_visual_detail {
namespace fs=std::filesystem;
using Json=nlohmann::json;
// Shared bounded metadata, containment and fingerprint reader. The House
// caller retains its exact historical prefix, limits and diagnostics.
struct Reader {
    std::string prefix;
    [[noreturn]] void fail(const std::string& message) { throw std::runtime_error(prefix+message); }
void keys(const Json& value,std::initializer_list<const char*> allowed) {
    if (!value.is_object()) fail("expected an object");
    for (auto it=value.begin();it!=value.end();++it)
        if (std::none_of(allowed.begin(),allowed.end(),[&](const char* name){return it.key()==name;}))
            fail("unknown key "+it.key());
}
std::string string(const Json& value,const char* name,std::size_t limit=512) {
    if (!value.contains(name) || !value.at(name).is_string()) fail(std::string("missing string ")+name);
    const auto result=value.at(name).get<std::string>();
    if (result.empty() || result.size()>limit) fail(std::string("invalid string ")+name);
    return result;
}
std::uint64_t number(const Json& value,const char* name,std::uint64_t maximum) {
    if (!value.contains(name) || !value.at(name).is_number_unsigned()) fail(std::string("invalid unsigned ")+name);
    const auto result=value.at(name).get<std::uint64_t>();
    if (result>maximum) fail(std::string("out of range ")+name);
    return result;
}
fs::path relative(const std::string& name) {
    const fs::path path(name);
    if (path.empty() || path.is_absolute() || path.has_root_name() || name.find('\\')!=std::string::npos)
        fail("unsafe relative asset path");
    for (const auto& part:path) if (part.empty() || part=="." || part=="..") fail("asset path traversal");
    return path;
}
fs::path checked_file(const fs::path& root,const fs::path& path) {
    auto current=root;
    for (const auto& part:path) {
        current/=part;
        std::error_code error;
        const auto status=fs::symlink_status(current,error);
        if (error || fs::is_symlink(status)) fail("missing or linked asset "+path.generic_string());
    }
    std::error_code error;const auto result=fs::canonical(current,error);
    if (error || !fs::is_regular_file(result)) fail("missing asset "+path.generic_string());
    const auto mismatch=std::mismatch(root.begin(),root.end(),result.begin(),result.end());
    if (mismatch.first!=root.end()) fail("asset escapes data root");
    return result;
}
std::string sha256(const fs::path& path) {
    using Context=std::unique_ptr<EVP_MD_CTX,decltype(&EVP_MD_CTX_free)>;
    Context context(EVP_MD_CTX_new(),EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(),EVP_sha256(),nullptr)!=1) fail("SHA256 initialization");
    std::ifstream input(path,std::ios::binary);if (!input) fail("fingerprint source unavailable");
    performance::increment(performance::Counter::FileReads);
    std::array<char,65536> buffer{};
    while (input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count=input.gcount();
        if (count>0 && EVP_DigestUpdate(context.get(),buffer.data(),static_cast<std::size_t>(count))!=1)
            fail("SHA256 update");
    }
    if (!input.eof()) fail("fingerprint source unreadable");
    std::array<unsigned char,EVP_MAX_MD_SIZE> bytes{};unsigned length=0;
    if (EVP_DigestFinal_ex(context.get(),bytes.data(),&length)!=1 || length!=32) fail("SHA256 finalization");
    std::ostringstream result;result<<std::hex<<std::setfill('0');
    for (unsigned i=0;i<length;++i) result<<std::setw(2)<<static_cast<unsigned>(bytes[i]);
    return result.str();
}
Json read_manifest(const fs::path& path) {
    std::error_code error;const auto size=fs::file_size(path,error);
    if (error || !fs::is_regular_file(path) || size>1024U*1024U) fail("metadata missing or exceeds1MiB");
    std::ifstream input(path,std::ios::binary);if (!input) fail("metadata unavailable");
    performance::increment(performance::Counter::FileReads);
    std::vector<std::set<std::string>> names;
    return Json::parse(input,[&](int,Json::parse_event_t event,Json& value) {
        if (event==Json::parse_event_t::object_start) names.emplace_back();
        else if (event==Json::parse_event_t::key && !names.back().insert(value.get<std::string>()).second)
            fail("duplicate metadata key");
        else if (event==Json::parse_event_t::object_end) names.pop_back();
        return true;
    });
}
std::uint64_t image_bytes(const RgbaImage& image) {
    const auto bytes=std::uint64_t(image.width)*image.height*4U;
    if (image.pixels.size()!=bytes) fail("existing image size differs from metadata");
    return bytes;
}
std::vector<std::optional<AssetId>> physical_ids(const BuildingVisualProfile& profile) {
    std::vector<std::optional<AssetId>> ids(profile.unique_images.size());
    const auto collect=[&](const auto& entries) {
        for (const auto& entry:entries) if (entry) {
            if (entry->image_index>=ids.size()) fail("existing role image index invalid");
            if (ids[entry->image_index] && ids[entry->image_index]!=entry->id)
                fail("existing physical identity conflict");
            ids[entry->image_index]=entry->id;
        }
    };
    collect(profile.entries);
    collect(profile.original_house_entries);
    collect(profile.original_building_entries);
    for (const auto& stages:profile.original_field_entries) collect(stages);
    return ids;
}
};
// Full native base/overlay layout, with no invented animation association.
inline bool native_type30_matches(const Sg3Image& record,std::uint64_t width,
    std::uint64_t height,std::uint64_t side,std::uint64_t base,bool allow_animation_metadata) {
    return record.image_type==30 && record.width==static_cast<int>(width) && record.height==static_cast<int>(height) &&
        record.isometric_size_flag==side && record.uncompressed_length==base &&
        !record.horizontal_mirror_offset && !record.alpha_offset && !record.alpha_length && record.data_length &&
        (allow_animation_metadata || (!record.animation_sprites && !record.animation_x_offset &&
         !record.animation_y_offset && !record.reversible_animation_flag && !record.animation_speed_id));
}
} // namespace openemperor::assets::original_visual_detail
