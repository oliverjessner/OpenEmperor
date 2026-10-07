#include "assets/FireVisualProfile.h"

#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
constexpr std::uintmax_t manifest_limit=64U*1024U;

bool under(const fs::path& root,const fs::path& path) {
    auto first=root.begin(),second=path.begin();
    for (;first!=root.end();++first,++second)
        if (second==path.end() || *first!=*second) return false;
    return true;
}

fs::path checked_file(const fs::path& root,const fs::path& requested) {
    std::error_code error;
    const auto canonical=fs::canonical(requested,error);
    if (error || !under(root,canonical) || !fs::is_regular_file(canonical))
        throw std::runtime_error("fire asset missing or escapes game-data root");
    return canonical;
}

std::string bounded_string(const nlohmann::json& json,const char* key,std::size_t limit) {
    if (!json.contains(key) || !json.at(key).is_string())
        throw std::runtime_error(std::string("fire field missing or not a string: ")+key);
    const auto value=json.at(key).get<std::string>();
    if (value.empty() || value.size()>limit)
        throw std::runtime_error(std::string("fire field length invalid: ")+key);
    return value;
}

void known_keys(const nlohmann::json& json,std::initializer_list<const char*> allowed) {
    for (const auto& [key,value]:json.items()) {
        (void)value;
        if (std::none_of(allowed.begin(),allowed.end(),[&](const char* name){return key==name;}))
            throw std::runtime_error("unknown fire profile field: "+key);
    }
}

fs::path relative_archive(const std::string& value) {
    const fs::path path{value};
    if (path.empty() || path.is_absolute() || path.has_root_name() ||
        path.extension()!=".sg3" || value.find('\\')!=std::string::npos)
        throw std::runtime_error("fire archive path must be a relative .sg3 path");
    for (const auto& component:path)
        if (component=="." || component=="..")
            throw std::runtime_error("fire archive path traversal rejected");
    return path;
}

double anchor(const nlohmann::json& value) {
    if (!value.is_number()) throw std::runtime_error("fire anchor is not numeric");
    const auto result=value.get<double>();
    if (!std::isfinite(result) || std::abs(result)>fire_dimension_limit)
        throw std::runtime_error("fire anchor outside finite dimension limit");
    return result;
}

std::uint64_t image_bytes(const RgbaImage& image) {
    if (image.width==0 || image.height==0 || image.width>fire_dimension_limit ||
        image.height>fire_dimension_limit)
        throw std::runtime_error("fire image dimensions outside 1..256");
    return static_cast<std::uint64_t>(image.width)*image.height*4U;
}
} // namespace

std::uint64_t fire_rgba_bytes(const FireVisualProfile& profile) {
    std::uint64_t bytes=0;
    if (profile.unique_images.size()>fire_frame_limit)
        throw std::runtime_error("fire unique asset limit exceeded");
    for (const auto& image:profile.unique_images) {
        const auto next=image_bytes(image);
        if (next>fire_rgba_budget-bytes) throw std::runtime_error("fire RGBA budget exceeded");
        bytes+=next;
    }
    return bytes;
}

void validate_fire_visual_profile(const FireVisualProfile& profile) {
    if (profile.schema_version!=1 || profile.clip_id.empty() || profile.clip_id.size()>64 ||
        profile.evidence.empty() || profile.evidence.size()>2048)
        throw std::runtime_error("fire clip identity/evidence invalid");
    if (profile.ticks_per_frame==0 || profile.ticks_per_frame>1000)
        throw std::runtime_error("fire ticks_per_frame outside 1..1000");
    if (profile.frames.size()<2 || profile.frames.size()>fire_frame_limit ||
        profile.unique_images.size()<2 || profile.unique_images.size()>profile.frames.size())
        throw std::runtime_error("fire clip must contain 2..64 prepared frames and different assets");
    (void)fire_rgba_bytes(profile);
    std::set<std::size_t> referenced;
    std::map<std::pair<std::string,std::uint32_t>,std::size_t> physical;
    for (const auto& frame:profile.frames) {
        (void)relative_archive(frame.id.archive_relative_path.generic_string());
        if (frame.id.image_index==0 || frame.image_index>=profile.unique_images.size())
            throw std::runtime_error("fire frame references an invalid physical/prepared image");
        if (!std::isfinite(frame.anchor_x) || !std::isfinite(frame.anchor_y) ||
            std::abs(frame.anchor_x)>fire_dimension_limit ||
            std::abs(frame.anchor_y)>fire_dimension_limit)
            throw std::runtime_error("fire anchor outside finite dimension limit");
        const auto key=std::make_pair(frame.id.archive_relative_path.generic_string(),frame.id.image_index);
        const auto [found,inserted]=physical.emplace(key,frame.image_index);
        if (!inserted && found->second!=frame.image_index)
            throw std::runtime_error("fire physical asset is not deduplicated");
        referenced.insert(frame.image_index);
    }
    if (referenced.size()!=profile.unique_images.size() ||
        physical.size()!=profile.unique_images.size())
        throw std::runtime_error("fire prepared assets do not match physical frame references");
    for (const auto& image:profile.unique_images) {
        if (image.pixels.size()!=image_bytes(image))
            throw std::runtime_error("fire decoded dimensions differ from pixel buffer");
        bool visible=false;
        for (std::size_t at=3;at<image.pixels.size();at+=4)
            visible=visible || image.pixels[at]!=0;
        if (!visible) throw std::runtime_error("fire frame has no visible pixels");
    }
    const auto& first=profile.unique_images.front();
    const bool different=std::any_of(profile.unique_images.begin()+1,profile.unique_images.end(),
        [&](const RgbaImage& image) {
            return image.width!=first.width || image.height!=first.height || image.pixels!=first.pixels;
        });
    if (!different) throw std::runtime_error("fire clip has no different decoded frames");
}

std::optional<std::size_t> fire_frame_index(const FireVisualProfile& profile,
                                           std::uint64_t world_tick,
                                           std::uint64_t building_id) {
    if (profile.ticks_per_frame==0 || profile.ticks_per_frame>1000 ||
        profile.frames.size()<2 || profile.frames.size()>fire_frame_limit) return std::nullopt;
    // Reduce both inputs before adding: at most 63+6, including UINT64_MAX inputs.
    const auto count=profile.frames.size();
    const auto loop=(world_tick/profile.ticks_per_frame)%count;
    const auto phase=(building_id%7U)%count;
    return static_cast<std::size_t>((loop+phase)%count);
}

FireVisualProfile load_fire_visual_profile(const fs::path& data_root,const fs::path& manifest) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) throw std::runtime_error("fire data root invalid");
    if (!fs::is_regular_file(manifest) || fs::file_size(manifest)>manifest_limit)
        throw std::runtime_error("fire manifest missing or exceeds 64 KiB");
    std::ifstream input(manifest,std::ios::binary);
    if (!input) throw std::runtime_error("fire manifest cannot be read");
    performance::increment(performance::Counter::FileReads);
    std::vector<std::set<std::string>> object_keys;
    const auto reject_duplicates=[&](int,nlohmann::json::parse_event_t event,nlohmann::json& value) {
        if (event==nlohmann::json::parse_event_t::object_start) object_keys.emplace_back();
        else if (event==nlohmann::json::parse_event_t::object_end) object_keys.pop_back();
        else if (event==nlohmann::json::parse_event_t::key &&
                 !object_keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("duplicate fire manifest key");
        return true;
    };
    const auto json=nlohmann::json::parse(input,reject_duplicates);
    if (!json.is_object()) throw std::runtime_error("fire profile must be an object");
    known_keys(json,{"schema_version","mode","clip_id","evidence","ticks_per_frame","frames"});
    if (!json.contains("schema_version") || !json.at("schema_version").is_number_unsigned() ||
        json.at("schema_version").get<std::uint64_t>()!=1 ||
        json.value("mode",std::string{})!="curated_fire_presentation")
        throw std::runtime_error("unsupported fire profile schema/mode");
    if (!json.contains("ticks_per_frame") || !json.at("ticks_per_frame").is_number_unsigned())
        throw std::runtime_error("fire ticks_per_frame must be positive");
    const auto ticks=json.at("ticks_per_frame").get<std::uint64_t>();
    if (ticks==0 || ticks>1000) throw std::runtime_error("fire ticks_per_frame outside 1..1000");
    if (!json.contains("frames") || !json.at("frames").is_array() ||
        json.at("frames").size()<2 || json.at("frames").size()>fire_frame_limit)
        throw std::runtime_error("fire clip must contain 2..64 explicit frames");
    FireVisualProfile profile;
    profile.clip_id=bounded_string(json,"clip_id",64);
    profile.evidence=bounded_string(json,"evidence",2048);
    profile.ticks_per_frame=static_cast<std::uint32_t>(ticks);
    std::map<std::string,Sg3Archive> archives;
    std::map<std::pair<std::string,std::uint32_t>,std::size_t> unique;
    std::uint64_t bytes=0;
    for (const auto& item:json.at("frames")) {
        if (!item.is_object()) throw std::runtime_error("fire frame must be an object");
        known_keys(item,{"archive","image_index","anchor"});
        FireFrame frame;
        const auto requested=relative_archive(bounded_string(item,"archive",4096));
        const auto archive_path=checked_file(root,root/requested);
        frame.id.archive_relative_path=archive_path.lexically_relative(root);
        if (!item.contains("image_index") || !item.at("image_index").is_number_unsigned())
            throw std::runtime_error("fire image_index must be a physical nonnegative record");
        const auto physical=item.at("image_index").get<std::uint64_t>();
        if (physical==0 || physical>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("fire physical image_index outside uint32 nonzero range");
        frame.id.image_index=static_cast<std::uint32_t>(physical);
        if (!item.contains("anchor") || !item.at("anchor").is_array() || item.at("anchor").size()!=2)
            throw std::runtime_error("fire anchor must contain x,y");
        frame.anchor_x=anchor(item.at("anchor").at(0));
        frame.anchor_y=anchor(item.at("anchor").at(1));
        const auto name=frame.id.archive_relative_path.generic_string();
        auto archive=archives.find(name);
        if (archive==archives.end()) archive=archives.emplace(name,read_sg3_archive(archive_path)).first;
        if (frame.id.image_index>=archive->second.images.size())
            throw std::runtime_error("fire physical image_index outside SG3 table");
        const auto& metadata=archive->second.images[frame.id.image_index];
        if (classify_sg3_image_type(metadata.image_type)!=Sg3ImageKind::Sprite ||
            metadata.data_length==0 || metadata.horizontal_mirror_offset!=0 ||
            metadata.width<=0 || metadata.height<=0 ||
            metadata.width>fire_dimension_limit || metadata.height>fire_dimension_limit)
            throw std::runtime_error("fire frame has unsupported sprite/mirror/dimension layout");
        const auto key=std::make_pair(name,frame.id.image_index);
        if (const auto existing=unique.find(key);existing!=unique.end()) frame.image_index=existing->second;
        else {
            const auto required=static_cast<std::uint64_t>(metadata.width)*
                                static_cast<std::uint64_t>(metadata.height)*4U;
            if (required>fire_rgba_budget-bytes) throw std::runtime_error("fire RGBA budget exceeded");
            const auto bitmap=resolve_sg3_image_bitmap(archive_path,archive->second,metadata);
            if (bitmap.status!=Sg3BitmapStatus::Resolved)
                throw std::runtime_error("fire .555 source unresolved or unsafe");
            (void)checked_file(root,bitmap.path);
            auto image=load_sg3_image({archive_path,frame.id.image_index});
            if (image.width!=metadata.width || image.height!=metadata.height || image.pixels.size()!=required)
                throw std::runtime_error("fire decoded dimensions differ from SG3 metadata");
            // Preserve the shared decoder's original alpha/color. No speculative
            // additive blend, color key or Walker-specific shadow transformation.
            frame.image_index=profile.unique_images.size();
            unique.emplace(key,frame.image_index);
            profile.unique_images.push_back(std::move(image));
            bytes+=required;
        }
        profile.frames.push_back(std::move(frame));
    }
    validate_fire_visual_profile(profile);
    return profile;
}
} // namespace openemperor::assets
