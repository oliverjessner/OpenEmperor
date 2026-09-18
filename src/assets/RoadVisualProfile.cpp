#include "assets/RoadVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
constexpr std::uint64_t max_rgba=64U*1024U*1024U;
bool under(const fs::path& root,const fs::path& path) {
    auto a=root.begin(),b=path.begin();
    for (;a!=root.end();++a,++b) if (b==path.end() || *a!=*b) return false;
    return true;
}
fs::path checked_file(const fs::path& root,const fs::path& path) {
    std::error_code error;
    const auto resolved=fs::canonical(path,error);
    if (error || !under(root,resolved) || !fs::is_regular_file(resolved))
        throw std::runtime_error("road asset missing or escapes data root");
    return resolved;
}
double anchor(const nlohmann::json& value) {
    if (!value.is_number()) throw std::runtime_error("road anchor must be numeric");
    const double result=value.get<double>();
    if (!std::isfinite(result) || std::abs(result)>4096)
        throw std::runtime_error("road anchor outside finite limit");
    return result;
}
int mask_key(const std::string& value) {
    if (value.size()!=3 || value[0]!='0' || value[1]!='x') return -1;
    const char digit=value[2];
    if (digit>='0' && digit<='9') return digit-'0';
    if (digit>='a' && digit<='f') return 10+digit-'a';
    return -1;
}
}
std::size_t RoadVisualProfile::configured_count() const {
    return static_cast<std::size_t>(std::count_if(tiles.begin(),tiles.end(),
        [](const auto& tile){return tile.has_value();}));
}
RoadVisualProfile load_road_visual_profile(const fs::path& data_root,const fs::path& manifest) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) throw std::runtime_error("road data root invalid");
    if (!fs::is_regular_file(manifest,error) || error ||
        fs::file_size(manifest,error)>1024U*1024U || error)
        throw std::runtime_error("road manifest missing or exceeds 1 MiB");
    std::ifstream input(manifest,std::ios::binary);
    if (!input) throw std::runtime_error("road manifest cannot be read");
    std::vector<std::set<std::string>> keys;
    const auto reject_duplicates=[&](int,nlohmann::json::parse_event_t event,nlohmann::json& value) {
        if (event==nlohmann::json::parse_event_t::object_start) keys.emplace_back();
        else if (event==nlohmann::json::parse_event_t::key &&
                 !keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("duplicate road manifest key");
        else if (event==nlohmann::json::parse_event_t::object_end) keys.pop_back();
        return true;
    };
    const auto json=nlohmann::json::parse(input,reject_duplicates);
    if (!json.is_object() || !json.contains("schema_version") ||
        !json.at("schema_version").is_number_integer() || json.at("schema_version")!=1 ||
        !json.contains("mode") || json.at("mode")!="curated_road_preview" ||
        !json.contains("tiles") || !json.at("tiles").is_object() ||
        json.at("tiles").empty() || json.at("tiles").size()>16)
        throw std::runtime_error("unsupported road visual schema/mode/tiles");
    RoadVisualProfile result;
    std::vector<AssetId> decoded_ids;
    std::uint64_t total_rgba=0;
    for (auto it=json.at("tiles").begin();it!=json.at("tiles").end();++it) {
        const int mask=mask_key(it.key());
        if (mask<0) throw std::runtime_error("unknown road mask: "+it.key());
        const auto& item=it.value();
        if (!item.is_object() || !item.contains("archive") || !item.at("archive").is_string() ||
            !item.contains("image_index") || !item.at("image_index").is_number_unsigned() ||
            !item.contains("ground_anchor") || !item.at("ground_anchor").is_array() ||
            item.at("ground_anchor").size()!=2 ||
            !item.contains("evidence") || !item.at("evidence").is_string())
            throw std::runtime_error("road visual entry malformed");
        RoadVisualEntry entry;
        const auto archive_name=item.at("archive").get<std::string>();
        const fs::path relative=fs::path(archive_name);
        if (archive_name.empty() || archive_name.size()>4096 || relative.is_absolute() ||
            relative.has_root_name() || relative.extension()!=".sg3" ||
            archive_name.find('\\')!=std::string::npos)
            throw std::runtime_error("road archive must be a relative .sg3 path");
        for (const auto& part:relative)
            if (part=="." || part=="..") throw std::runtime_error("road archive traversal rejected");
        entry.id.archive_relative_path=relative.lexically_normal();
        const auto index=item.at("image_index").get<std::uint64_t>();
        if (index==0 || index>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("road physical image_index invalid");
        entry.id.image_index=static_cast<std::uint32_t>(index);
        entry.ground_x=anchor(item.at("ground_anchor").at(0));
        entry.ground_y=anchor(item.at("ground_anchor").at(1));
        entry.evidence=item.at("evidence").get<std::string>();
        if (entry.evidence.empty() || entry.evidence.size()>512)
            throw std::runtime_error("road evidence length invalid");
        const auto found=std::find(decoded_ids.begin(),decoded_ids.end(),entry.id);
        if (found!=decoded_ids.end()) {
            entry.image_index=static_cast<std::size_t>(found-decoded_ids.begin());
            result.tiles[static_cast<std::size_t>(mask)]=std::move(entry);
            continue;
        }
        const auto archive_path=checked_file(root,root/relative);
        const auto archive=read_sg3_archive(archive_path);
        if (index>=archive.images.size()) throw std::runtime_error("road record outside SG3 table");
        const auto& record=archive.images[static_cast<std::size_t>(index)];
        if (record.image_type!=30 || record.width<=0 || record.height<=0 ||
            record.data_length==0 || record.horizontal_mirror_offset!=0 ||
            record.alpha_length!=0 || record.animation_sprites!=0)
            throw std::runtime_error("road requires static unmirrored Type-30 without alpha");
        const auto bytes=static_cast<std::uint64_t>(record.width)*
            static_cast<std::uint64_t>(record.height)*4U;
        if (bytes>max_rgba-total_rgba) throw std::runtime_error("road RGBA budget exceeded");
        const auto bitmap=resolve_sg3_image_bitmap(archive_path,archive,record);
        if (bitmap.status!=Sg3BitmapStatus::Resolved)
            throw std::runtime_error("road .555 source unresolved or unsafe");
        checked_file(root,bitmap.path);
        auto image=load_sg3_image({archive_path,entry.id.image_index});
        if (image.width!=record.width || image.height!=record.height || image.pixels.size()!=bytes)
            throw std::runtime_error("road decoded image differs from metadata");
        entry.image_index=result.unique_images.size();
        decoded_ids.push_back(entry.id);
        result.unique_images.push_back(std::move(image));
        result.tiles[static_cast<std::size_t>(mask)]=std::move(entry);
        total_rgba+=bytes;
    }
    return result;
}
}
