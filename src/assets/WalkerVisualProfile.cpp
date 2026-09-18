#include "assets/WalkerVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
constexpr std::uintmax_t max_manifest=1024U*1024U;
constexpr std::uint64_t max_rgba=64U*1024U*1024U;
bool under(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for (;a!=root.end();++a,++b) if (b==path.end() || *a!=*b) return false;
    return true;
}
fs::path checked_file(const fs::path& root,const fs::path& requested) {
    std::error_code error;
    const auto result=fs::canonical(requested,error);
    if (error || !under(root,result) || !fs::is_regular_file(result))
        throw std::runtime_error("walker asset missing or escapes game-data root: "+requested.string());
    return result;
}
std::string bounded_string(const nlohmann::json& j,const char* key,std::size_t limit) {
    if (!j.contains(key) || !j.at(key).is_string())
        throw std::runtime_error(std::string("walker field missing or not a string: ")+key);
    const auto value=j.at(key).get<std::string>();
    if (value.empty() || value.size()>limit)
        throw std::runtime_error(std::string("walker field length invalid: ")+key);
    return value;
}
fs::path relative_archive(const std::string& value) {
    const fs::path path{value};
    if (path.empty() || path.is_absolute() || path.has_root_name() ||
        path.extension()!=".sg3" || value.find('\\')!=std::string::npos)
        throw std::runtime_error("walker archive path must be a relative .sg3 path");
    for (const auto& component:path)
        if (component=="." || component=="..")
            throw std::runtime_error("walker archive path traversal rejected");
    return path;
}
double anchor(const nlohmann::json& value) {
    if (!value.is_number()) throw std::runtime_error("walker foot anchor is not numeric");
    const double result=value.get<double>();
    if (!std::isfinite(result) || std::abs(result)>4096)
        throw std::runtime_error("walker foot anchor outside finite limit");
    return result;
}
}
WalkerVisualProfile load_walker_visual_profile(const fs::path& data_root,
                                               const fs::path& manifest) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) throw std::runtime_error("walker data root invalid");
    if (!fs::is_regular_file(manifest) || fs::file_size(manifest)>max_manifest)
        throw std::runtime_error("walker manifest missing or exceeds 1 MiB");
    std::ifstream input(manifest,std::ios::binary);
    if (!input) throw std::runtime_error("walker manifest cannot be read");
    const auto json=nlohmann::json::parse(input);
    if (!json.is_object() || !json.contains("schema_version") ||
        !json.at("schema_version").is_number_integer() || json.at("schema_version")!=1 ||
        json.value("mode",std::string{})!="curated_walker_preview" ||
        json.value("role",std::string{})!="clay")
        throw std::runtime_error("unsupported walker visual profile schema/mode/role");
    if (!json.contains("ticks_per_frame") || !json.at("ticks_per_frame").is_number_unsigned())
        throw std::runtime_error("walker ticks_per_frame must be positive");
    WalkerVisualProfile result;
    const auto tick_count=json.at("ticks_per_frame").get<std::uint64_t>();
    if (tick_count>1000)
        throw std::runtime_error("walker ticks_per_frame outside 1..1000");
    result.ticks_per_frame=static_cast<std::uint32_t>(tick_count);
    if (result.ticks_per_frame==0 || result.ticks_per_frame>1000)
        throw std::runtime_error("walker ticks_per_frame outside 1..1000");
    result.evidence=bounded_string(json,"evidence",512);
    if (!json.contains("frames") || !json.at("frames").is_array() ||
        json.at("frames").empty() || json.at("frames").size()>256)
        throw std::runtime_error("walker frames must contain 1..256 aliases");
    std::map<std::string,std::size_t> aliases;
    std::map<std::pair<std::string,std::uint32_t>,std::size_t> unique;
    std::map<std::string,Sg3Archive> archives;
    std::uint64_t rgba_bytes=0;
    for (const auto& item:json.at("frames")) {
        if (!item.is_object()) throw std::runtime_error("walker frame must be an object");
        WalkerFrame frame;
        frame.alias=bounded_string(item,"alias",64);
        frame.id.archive_relative_path=relative_archive(bounded_string(item,"archive",4096));
        if (!item.contains("image_index") || !item.at("image_index").is_number_unsigned())
            throw std::runtime_error("walker image_index must be a physical nonnegative record");
        const auto physical_index=item.at("image_index").get<std::uint64_t>();
        if (physical_index>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("walker physical image_index exceeds uint32 range");
        frame.id.image_index=static_cast<std::uint32_t>(physical_index);
        if (frame.id.image_index==0) throw std::runtime_error("walker dummy record zero rejected");
        if (!item.contains("foot_anchor") || !item.at("foot_anchor").is_array() ||
            item.at("foot_anchor").size()!=2)
            throw std::runtime_error("walker foot_anchor must contain x,y");
        frame.foot_x=anchor(item.at("foot_anchor").at(0));
        frame.foot_y=anchor(item.at("foot_anchor").at(1));
        if (!aliases.emplace(frame.alias,result.frames.size()).second)
            throw std::runtime_error("duplicate walker frame alias: "+frame.alias);
        const auto name=frame.id.archive_relative_path.generic_string();
        auto found=archives.find(name);
        if (found==archives.end()) {
            const auto path=checked_file(root,root/frame.id.archive_relative_path);
            found=archives.emplace(name,read_sg3_archive(path)).first;
        }
        const auto& archive=found->second;
        if (frame.id.image_index>=archive.images.size())
            throw std::runtime_error("walker physical image_index outside SG3 table");
        const auto& metadata=archive.images[frame.id.image_index];
        if (classify_sg3_image_type(metadata.image_type)!=Sg3ImageKind::Sprite ||
            metadata.data_length==0 || metadata.horizontal_mirror_offset!=0 ||
            metadata.width<=0 || metadata.height<=0)
            throw std::runtime_error("walker frame has unsupported sprite/mirror layout");
        const auto key=std::make_pair(name,frame.id.image_index);
        if (const auto existing=unique.find(key);existing!=unique.end()) frame.image_index=existing->second;
        else {
            const auto bytes=static_cast<std::uint64_t>(metadata.width)*
                static_cast<std::uint64_t>(metadata.height)*4U;
            if (bytes>max_rgba-rgba_bytes) throw std::runtime_error("walker RGBA budget exceeded");
            const auto archive_path=checked_file(root,root/frame.id.archive_relative_path);
            const auto bitmap=resolve_sg3_image_bitmap(archive_path,archive,metadata);
            if (bitmap.status!=Sg3BitmapStatus::Resolved)
                throw std::runtime_error("walker .555 source unresolved or unsafe");
            checked_file(root,bitmap.path);
            auto rgba=load_sg3_image({archive_path,frame.id.image_index});
            if (rgba.width!=metadata.width || rgba.height!=metadata.height ||
                rgba.pixels.size()!=bytes)
                throw std::runtime_error("walker decoded dimensions differ from SG3 metadata");
            rgba_bytes+=bytes;
            frame.image_index=result.unique_images.size();
            unique.emplace(key,frame.image_index);
            result.unique_images.push_back(std::move(rgba));
        }
        result.frames.push_back(std::move(frame));
    }
    if (!json.contains("clips") || !json.at("clips").is_object())
        throw std::runtime_error("walker clips missing");
    constexpr const char* names[]={"pos_x","neg_x","pos_y","neg_y"};
    for (std::size_t direction=0;direction<4;++direction) {
        if (!json.at("clips").contains(names[direction])) continue; // Explicitly unmapped.
        const auto& clip=json.at("clips").at(names[direction]);
        if (!clip.is_array() || clip.empty() || clip.size()>64)
            throw std::runtime_error("walker clip must contain 1..64 frames");
        for (const auto& value:clip) {
            if (!value.is_string()) throw std::runtime_error("walker clip alias must be a string");
            const auto found=aliases.find(value.get<std::string>());
            if (found==aliases.end()) throw std::runtime_error("walker clip references missing alias");
            result.clips[direction].push_back(found->second);
        }
    }
    if (result.clips[0].empty() && result.clips[1].empty() &&
        result.clips[2].empty() && result.clips[3].empty())
        throw std::runtime_error("walker profile has no mapped moving direction");
    const auto idle=bounded_string(json,"idle",64);
    const auto found=aliases.find(idle);
    if (found==aliases.end()) throw std::runtime_error("walker idle frame alias missing");
    result.idle_frame=found->second;
    return result;
}
}
