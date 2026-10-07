#include "assets/WalkerVisualProfile.h"
#include "assets/Sg3ImageLoader.h"
#include "assets/Sg3ShadowComposition.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
constexpr std::uintmax_t max_manifest=1024U*1024U;
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
struct ParseState {
    fs::path root;
    WalkerVisualProfile profile;
    std::map<std::pair<std::string,std::uint32_t>,std::size_t> unique;
    std::map<std::string,Sg3Archive> archives;
    std::uint64_t rgba_bytes=0;
    std::size_t aliases_total=0;
    const WalkerVisualProfile* existing=nullptr;
    std::size_t image_base=0;
    bool canonical_identity=false;
    const RgbaImage& image(std::size_t index) const {
        return index<image_base ? existing->unique_images.at(index):
            profile.unique_images.at(index-image_base);
    }
};
WalkerRoleVisual parse_role(const nlohmann::json& json,ParseState& state,
                            bool inspector=false,bool require_internal=false) {
    if (!json.is_object() || !json.contains("ticks_per_frame") ||
        !json.at("ticks_per_frame").is_number_unsigned())
        throw std::runtime_error("walker ticks_per_frame must be positive");
    const auto tick_count=json.at("ticks_per_frame").get<std::uint64_t>();
    if (tick_count==0 || tick_count>1000)
        throw std::runtime_error("walker ticks_per_frame outside 1..1000");
    WalkerRoleVisual role;
    role.ticks_per_frame=static_cast<std::uint32_t>(tick_count);
    role.evidence=bounded_string(json,"evidence",512);
    if (inspector) role.clip_id=bounded_string(json,"clip_id",64);
    if (!json.contains("frames") || !json.at("frames").is_array() ||
        json.at("frames").empty() ||
        json.at("frames").size()>walker_max_frame_aliases-state.aliases_total)
        throw std::runtime_error("walker total frames must contain 1..256 aliases");
    state.aliases_total+=json.at("frames").size();
    std::map<std::string,std::size_t> aliases; // Scoped to this role only.
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
        if (!aliases.emplace(frame.alias,role.frames.size()).second)
            throw std::runtime_error("duplicate walker frame alias: "+frame.alias);
        const auto name=frame.id.archive_relative_path.generic_string();
        auto found=state.archives.find(name);
        if (found==state.archives.end()) {
            const auto path=checked_file(state.root,state.root/frame.id.archive_relative_path);
            found=state.archives.emplace(name,read_sg3_archive(path)).first;
        }
        const auto& archive=found->second;
        if (frame.id.image_index>=archive.images.size())
            throw std::runtime_error("walker physical image_index outside SG3 table");
        const auto& metadata=archive.images[frame.id.image_index];
        if (require_internal && metadata.external_flag!=0)
            throw std::runtime_error("FireInspector automatic supplement requires internal bitmap dependencies");
        if (classify_sg3_image_type(metadata.image_type)!=Sg3ImageKind::Sprite ||
            metadata.data_length==0 || metadata.horizontal_mirror_offset!=0 ||
            metadata.width<=0 || metadata.height<=0)
            throw std::runtime_error("walker frame has unsupported sprite/mirror layout");
        const auto identity=state.canonical_identity ?
            checked_file(state.root,state.root/frame.id.archive_relative_path).generic_string():name;
        if (state.canonical_identity) {
            const auto bitmap=resolve_sg3_image_bitmap(fs::path(identity),archive,metadata);
            if (bitmap.status!=Sg3BitmapStatus::Resolved)
                throw std::runtime_error("walker .555 source unresolved or unsafe");
            checked_file(state.root,bitmap.path);
        }
        const auto key=std::make_pair(identity,frame.id.image_index);
        if (const auto existing=state.unique.find(key);existing!=state.unique.end())
            frame.image_index=existing->second;
        else {
            if (state.image_base+state.profile.unique_images.size()>=walker_max_unique_assets)
                throw std::runtime_error("walker unique asset budget exceeded");
            const auto bytes=static_cast<std::uint64_t>(metadata.width)*
                static_cast<std::uint64_t>(metadata.height)*4U;
            if (bytes>walker_max_rgba_bytes-state.rgba_bytes)
                throw std::runtime_error("walker RGBA budget exceeded");
            const auto archive_path=checked_file(state.root,state.root/frame.id.archive_relative_path);
            const auto bitmap=resolve_sg3_image_bitmap(archive_path,archive,metadata);
            if (bitmap.status!=Sg3BitmapStatus::Resolved)
                throw std::runtime_error("walker .555 source unresolved or unsafe");
            checked_file(state.root,bitmap.path);
            auto rgba=load_sg3_image({archive_path,frame.id.image_index});
            if (rgba.width!=metadata.width || rgba.height!=metadata.height ||
                rgba.pixels.size()!=bytes)
                throw std::runtime_error("walker decoded dimensions differ from SG3 metadata");
            prepare_omega_shadow_composition(metadata,rgba);
            state.rgba_bytes+=bytes;
            frame.image_index=state.image_base+state.profile.unique_images.size();
            state.unique.emplace(key,frame.image_index);
            state.profile.unique_images.push_back(std::move(rgba));
        }
        role.frames.push_back(std::move(frame));
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
            role.clips[direction].push_back(found->second);
        }
    }
    if (role.clips[0].empty() && role.clips[1].empty() &&
        role.clips[2].empty() && role.clips[3].empty())
        throw std::runtime_error("walker profile has no mapped moving direction");
    const auto idle=bounded_string(json,"idle",64);
    const auto found=aliases.find(idle);
    if (found==aliases.end()) throw std::runtime_error("walker idle frame alias missing");
    role.idle_frame=found->second;
    return role;
}
std::optional<WalkerVisualRole> named_role(const std::string& name,bool inspector) {
    if (name=="clay") return WalkerVisualRole::Clay;
    if (name=="pottery") return WalkerVisualRole::Pottery;
    if (name=="household") return WalkerVisualRole::Household;
    if (inspector && name=="fire_inspector") return WalkerVisualRole::FireInspector;
    return std::nullopt;
}
fs::path checked_root(const fs::path& data_root) {
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) throw std::runtime_error("walker data root invalid");
    return root;
}
nlohmann::json read_manifest(const fs::path& manifest) {
    if (!fs::is_regular_file(manifest) || fs::file_size(manifest)>max_manifest)
        throw std::runtime_error("walker manifest missing or exceeds 1 MiB");
    std::ifstream input(manifest,std::ios::binary);
    if (!input) throw std::runtime_error("walker manifest cannot be read");
    std::vector<std::set<std::string>> object_keys;
    const auto reject_duplicates=[&](int,nlohmann::json::parse_event_t event,
                                     nlohmann::json& value) {
        if (event==nlohmann::json::parse_event_t::object_start) object_keys.emplace_back();
        else if (event==nlohmann::json::parse_event_t::object_end) object_keys.pop_back();
        else if (event==nlohmann::json::parse_event_t::key &&
                 !object_keys.back().insert(value.get<std::string>()).second)
            throw std::runtime_error("duplicate walker manifest key");
        return true;
    };
    auto json=nlohmann::json::parse(input,reject_duplicates);
    if (!json.is_object() || !json.contains("schema_version") ||
        !json.at("schema_version").is_number_integer() ||
        json.value("mode",std::string{})!="curated_walker_preview")
        throw std::runtime_error("unsupported walker visual profile schema/mode");
    const auto version=json.at("schema_version").get<std::int64_t>();
    if (version!=1 && version!=2 && version!=3)
        throw std::runtime_error("unsupported walker visual profile schema version");
    return json;
}
void require_animated_inspector(const WalkerRoleVisual& role,const ParseState& state,
                                bool complete) {
    for (const auto& frame:role.frames) {
        const auto& image=state.image(frame.image_index);
        bool visible=false;
        for (std::size_t offset=3;offset<image.pixels.size();offset+=4)
            if (image.pixels[offset]) { visible=true; break; }
        if (!visible) throw std::runtime_error("FireInspector frame has no visible decoded pixels");
    }
    for (const auto& clip:role.clips) {
        if (clip.empty() && !complete) continue;
        if (clip.empty())
            throw std::runtime_error("FireInspector supplement requires all four animated directions");
        if (clip.size()<2)
            throw std::runtime_error("FireInspector mapped direction requires at least two frames");
        const auto& first=state.image(role.frames.at(clip.front()).image_index);
        bool different=false;
        for (const auto frame:clip) {
            const auto& image=state.image(role.frames.at(frame).image_index);
            if (image.width!=first.width || image.height!=first.height ||
                image.pixels!=first.pixels) different=true;
        }
        if (!different)
            throw std::runtime_error("FireInspector supplement direction has no different decoded frames");
    }
}
} // namespace

const char* walker_role_name(WalkerVisualRole role) {
    switch (role) {
    case WalkerVisualRole::Clay: return "clay";
    case WalkerVisualRole::Pottery: return "pottery";
    case WalkerVisualRole::Household: return "household";
    case WalkerVisualRole::FireInspector: return "fire_inspector";
    }
    return "unknown";
}

std::uint64_t walker_rgba_bytes(const WalkerVisualProfile& profile) {
    if (profile.unique_images.size()>walker_max_unique_assets)
        throw std::runtime_error("walker unique asset budget exceeded");
    std::uint64_t total=0;
    for (const auto& image:profile.unique_images) {
        const auto bytes=static_cast<std::uint64_t>(image.width)*image.height*4U;
        if (!image.width || !image.height || image.pixels.size()!=bytes)
            throw std::runtime_error("walker prepared image dimensions do not match its pixels");
        if (bytes>walker_max_rgba_bytes-total)
            throw std::runtime_error("walker RGBA budget exceeded");
        total+=bytes;
    }
    return total;
}

WalkerVisualProfile load_walker_visual_profile(const fs::path& data_root,
                                               const fs::path& manifest) {
    const auto root=checked_root(data_root);
    const auto json=read_manifest(manifest);
    const auto version=json.at("schema_version").get<std::int64_t>();
    ParseState state;
    state.root=root;
    state.canonical_identity=version==3;
    state.profile.schema_version=static_cast<std::uint32_t>(version);
    if (version==1) {
        if (json.value("role",std::string{})!="clay")
            throw std::runtime_error("schema-1 walker role must be clay");
        state.profile.roles[walker_role_index(WalkerVisualRole::Clay)]=parse_role(json,state);
    } else {
        if (!json.contains("roles") || !json.at("roles").is_object() ||
            json.at("roles").empty() || json.at("roles").size()>
                (version==2 ? walker_core_role_count:walker_visual_role_count))
            throw std::runtime_error("walker roles exceed this schema's role limit");
        for (const auto& [name,value]:json.at("roles").items()) {
            const auto role=named_role(name,version==3);
            if (!role) throw std::runtime_error("unknown walker visual role: "+name);
            state.profile.roles[walker_role_index(*role)]=parse_role(value,state,
                *role==WalkerVisualRole::FireInspector);
            if (*role==WalkerVisualRole::FireInspector)
                require_animated_inspector(*state.profile.find(*role),state,false);
        }
    }
    return std::move(state.profile);
}

void append_fire_inspector_visual_profile(const fs::path& data_root,
                                         const fs::path& manifest,
                                         WalkerVisualProfile& profile) {
    const auto root=checked_root(data_root);
    const auto json=read_manifest(manifest);
    if (json.at("schema_version").get<std::int64_t>()!=3 ||
        !json.contains("roles") || !json.at("roles").is_object() ||
        json.at("roles").size()!=1 || !json.at("roles").contains("fire_inspector"))
        throw std::runtime_error("FireInspector supplement must contain only its schema-3 role");
    if (profile.find(WalkerVisualRole::FireInspector))
        throw std::runtime_error("FireInspector role is already configured");
    ParseState state;
    state.root=root;
    state.existing=&profile;
    state.image_base=profile.unique_images.size();
    state.canonical_identity=true;
    state.rgba_bytes=walker_rgba_bytes(profile);
    for (const auto& visual:profile.roles) if (visual) {
        if (visual->frames.size()>walker_max_frame_aliases-state.aliases_total)
            throw std::runtime_error("walker total frame alias budget exceeded");
        state.aliases_total+=visual->frames.size();
        for (const auto& frame:visual->frames) {
            if (frame.image_index>=state.image_base)
                throw std::runtime_error("walker core frame references an absent prepared image");
            const auto path=checked_file(root,root/relative_archive(frame.id.archive_relative_path.generic_string()));
            state.unique.try_emplace(std::make_pair(path.generic_string(),frame.id.image_index),
                                     frame.image_index);
        }
    }
    auto role=parse_role(json.at("roles").at("fire_inspector"),state,true,true);
    require_animated_inspector(role,state,true);
    // No core pixel copies: all fallible preparation precedes committing new tails.
    static_assert(std::is_nothrow_move_constructible_v<RgbaImage>);
    static_assert(std::is_nothrow_move_constructible_v<WalkerRoleVisual>);
    profile.unique_images.reserve(state.image_base+state.profile.unique_images.size());
    for (auto& image:state.profile.unique_images) profile.unique_images.push_back(std::move(image));
    profile.roles[walker_role_index(WalkerVisualRole::FireInspector)].emplace(std::move(role));
    profile.schema_version=3;
}
}
