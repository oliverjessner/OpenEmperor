#include "assets/BuildingVisualProfile.h"
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
        throw std::runtime_error("building asset missing or escapes data root");
    return resolved;
}
double checked_anchor(const nlohmann::json& value) {
    if (!value.is_number()) throw std::runtime_error("building anchor must be numeric");
    const double result=value.get<double>();
    if (!std::isfinite(result) || std::abs(result)>4096)
        throw std::runtime_error("building anchor outside finite limit");
    return result;
}
std::optional<BuildingVisualRole> parse_role(const std::string& name) {
    for (const auto role:building_roles) if (name==building_role_name(role)) return role;
    return std::nullopt;
}
void check_footprint(BuildingVisualRole role,std::uint8_t side,std::uint16_t width) {
    if (requires_one_cell(role) && side!=1)
        throw std::runtime_error("building visual footprint does not match the 1x1 role");
    if ((role==BuildingVisualRole::Well || role==BuildingVisualRole::HealthPost) && (side!=1 || width!=78))
        throw std::runtime_error(role==BuildingVisualRole::Well ?
            "Well requires an Emperor 1x1 Type-30 base (78 wide, 3200 bytes)":
            "Health Post requires an Emperor 1x1 Type-30 base (78 wide, 3200 bytes)");
    if (is_household_stage(role) && (side!=2 || width!=158))
        throw std::runtime_error("household stage requires an Emperor 2x2 Type-30 base (158 wide, 12800 bytes)");
}
}
const char* building_role_name(BuildingVisualRole role) {
    switch (role) {
    case BuildingVisualRole::ClaySource: return "clay_source";
    case BuildingVisualRole::Pottery: return "pottery";
    case BuildingVisualRole::Warehouse: return "warehouse";
    case BuildingVisualRole::Household: return "household";
    case BuildingVisualRole::Farm: return "farm";
    case BuildingVisualRole::ServicePost: return "service_post";
    case BuildingVisualRole::Market: return "market";
    case BuildingVisualRole::Well: return "well";
    case BuildingVisualRole::HealthPost: return "health_post";
    case BuildingVisualRole::FireWatch: return "fire_watch";
    case BuildingVisualRole::HouseholdLevel0: return "household_level_0";
    case BuildingVisualRole::HouseholdLevel1: return "household_level_1";
    case BuildingVisualRole::HouseholdLevel2: return "household_level_2";
    }
    return "unknown";
}
BuildingVisualProfile load_building_visual_profile(const fs::path& data_root,
                                                   const fs::path& manifest) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) throw std::runtime_error("building data root invalid");
    if (!fs::is_regular_file(manifest,error) || error ||
        fs::file_size(manifest,error)>1024U*1024U || error)
        throw std::runtime_error("building manifest missing or exceeds 1 MiB");
    std::ifstream input(manifest,std::ios::binary);
    if (!input) throw std::runtime_error("building manifest cannot be read");
    std::vector<std::set<std::string>> object_keys;
    const auto reject_duplicates=[&](int,nlohmann::json::parse_event_t event,
                                      nlohmann::json& value) {
        if (event==nlohmann::json::parse_event_t::object_start) object_keys.emplace_back();
        else if (event==nlohmann::json::parse_event_t::key &&
                 !object_keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("duplicate building manifest key");
        else if (event==nlohmann::json::parse_event_t::object_end) object_keys.pop_back();
        return true;
    };
    const auto json=nlohmann::json::parse(input,reject_duplicates);
    if (!json.is_object() || !json.contains("schema_version") ||
        !json.at("schema_version").is_number_integer() || json.at("schema_version")!=1 ||
        !json.contains("mode") || json.at("mode")!="curated_building_preview" ||
        !json.contains("buildings") || !json.at("buildings").is_object() ||
        json.at("buildings").empty() || json.at("buildings").size()>building_role_count)
        throw std::runtime_error("unsupported building visual schema/mode/roles");
    for (const auto& [key,value]:json.items()) {
        (void)value;
        if (key!="schema_version" && key!="mode" && key!="buildings")
            throw std::runtime_error("unknown building manifest key: "+key);
    }
    BuildingVisualProfile result;
    std::vector<AssetId> decoded_ids;
    std::vector<std::uint8_t> decoded_footprint_sides;
    std::uint64_t total_rgba=0;
    for (auto it=json.at("buildings").begin();it!=json.at("buildings").end();++it) {
        const auto role=parse_role(it.key());
        if (!role) throw std::runtime_error("unknown building visual role: "+it.key());
        const auto& item=it.value();
        if (!item.is_object() || !item.contains("archive") || !item.at("archive").is_string() ||
            !item.contains("image_index") || !item.at("image_index").is_number_unsigned() ||
            !item.contains("ground_anchor") || !item.at("ground_anchor").is_array() ||
            item.at("ground_anchor").size()!=2 ||
            !item.contains("evidence") || !item.at("evidence").is_string())
            throw std::runtime_error("building visual entry malformed");
        for (const auto& [key,value]:item.items()) {
            (void)value;
            if (key!="archive" && key!="image_index" && key!="ground_anchor" && key!="evidence")
                throw std::runtime_error("unknown building visual entry key: "+key);
        }
        BuildingVisualEntry entry;
        const auto archive_name=item.at("archive").get<std::string>();
        entry.id.archive_relative_path=fs::path(archive_name);
        const auto& relative=entry.id.archive_relative_path;
        if (archive_name.empty() || archive_name.size()>4096 || relative.is_absolute() ||
            relative.has_root_name() || relative.extension()!=".sg3" ||
            archive_name.find('\\')!=std::string::npos)
            throw std::runtime_error("building archive must be a relative .sg3 path");
        for (const auto& part:relative)
            if (part=="." || part=="..") throw std::runtime_error("building archive traversal rejected");
        entry.id.archive_relative_path=relative.lexically_normal();
        const auto index=item.at("image_index").get<std::uint64_t>();
        if (index==0 || index>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("building physical image_index invalid");
        entry.id.image_index=static_cast<std::uint32_t>(index);
        entry.ground_x=checked_anchor(item.at("ground_anchor").at(0));
        entry.ground_y=checked_anchor(item.at("ground_anchor").at(1));
        entry.evidence=item.at("evidence").get<std::string>();
        if (entry.evidence.empty() || entry.evidence.size()>512)
            throw std::runtime_error("building evidence length invalid");
        const auto found=std::find(decoded_ids.begin(),decoded_ids.end(),entry.id);
        if (found!=decoded_ids.end()) {
            entry.image_index=static_cast<std::size_t>(found-decoded_ids.begin());
            entry.footprint_side=decoded_footprint_sides.at(entry.image_index);
            check_footprint(*role,entry.footprint_side,result.unique_images.at(entry.image_index).width);
            result.entries[role_index(*role)]=std::move(entry);
            continue;
        }
        const auto archive_path=checked_file(root,root/relative);
        const auto archive=read_sg3_archive(archive_path);
        if (index>=archive.images.size()) throw std::runtime_error("building record outside SG3 table");
        const auto& record=archive.images[static_cast<std::size_t>(index)];
        if (record.image_type!=30 || record.width<=0 || record.height<=0 ||
            record.data_length==0 || record.horizontal_mirror_offset!=0)
            throw std::runtime_error("building requires supported unmirrored Type-30 image");
        const auto side=static_cast<std::uint64_t>(record.isometric_size_flag);
        const bool valid_side=side>=1 && side<=5;
        const bool emperor_geometry=valid_side &&
            static_cast<std::uint64_t>(record.width)==80U*side-2U &&
            static_cast<std::uint64_t>(record.uncompressed_length)==3200U*side*side;
        const bool classic_geometry=valid_side &&
            static_cast<std::uint64_t>(record.width)==60U*side-2U &&
            static_cast<std::uint64_t>(record.uncompressed_length)==1800U*side*side;
        if (!emperor_geometry && !classic_geometry)
            throw std::runtime_error("building Type-30 footprint geometry is unsupported");
        entry.footprint_side=static_cast<std::uint8_t>(side);
        check_footprint(*role,entry.footprint_side,static_cast<std::uint16_t>(record.width));
        const auto bytes=static_cast<std::uint64_t>(record.width)*
            static_cast<std::uint64_t>(record.height)*4U;
        if (bytes>max_rgba-total_rgba) throw std::runtime_error("building RGBA budget exceeded");
        const auto bitmap=resolve_sg3_image_bitmap(archive_path,archive,record);
        if (bitmap.status!=Sg3BitmapStatus::Resolved)
            throw std::runtime_error("building .555 source unresolved or unsafe");
        checked_file(root,bitmap.path);
        auto image=load_sg3_image({archive_path,entry.id.image_index});
        if (image.width!=record.width || image.height!=record.height ||
            image.pixels.size()!=bytes)
            throw std::runtime_error("building decoded image differs from metadata");
        entry.image_index=result.unique_images.size();
        decoded_ids.push_back(entry.id);
        decoded_footprint_sides.push_back(entry.footprint_side);
        result.unique_images.push_back(std::move(image));
        result.entries[role_index(*role)]=std::move(entry);
        total_rgba+=bytes;
    }
    return result;
}
}
