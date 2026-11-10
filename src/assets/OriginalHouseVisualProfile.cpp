#include "assets/BuildingVisualProfile.h"
#include "assets/OriginalVisualProfileInternal.h"
#include "assets/Sg3ImageLoader.h"
#include "core/PerformanceDiagnostics.h"
#include "maps/ResourceGroupLookup.h"

#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace openemperor::assets {
namespace {
namespace fs=std::filesystem;
using Json=nlohmann::json;
original_visual_detail::Reader reader{"Original House profile: "};
[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error("Original House profile: "+message);
}
std::optional<std::uint32_t> evidenced_group(std::uint64_t type) {
    // Pinned fresh Type2 purchase publisher4b28d0 and House setter518de0.
    // Keep the semantic selector guard independent of the metadata's physical
    // records. Callers must restrict Type2 to its evidenced fresh Common state.
    if (type==2) return 0x47aU;
    if (type>=3 && type<=10) return 0x471U+static_cast<std::uint32_t>(type-3);
    switch (type) {
    case 11:return 0x479U;
    case 12:return 0x47bU;
    case 13:return 0x47cU;
    case 14:return 0x4acU;
    default:if (type>=15 && type<=17) return 0x47eU+static_cast<std::uint32_t>(type-15);
    }
    return std::nullopt;
}
}

void append_original_house_visual_profile(const fs::path& data_root,const fs::path& manifest,
    BuildingVisualProfile& profile,std::uint64_t max_final_rgba) {
    if (profile.original_house_prepared) fail("remove an existing extension before replacement");
    max_final_rgba=std::min(max_final_rgba,building_max_rgba_bytes);
    std::uint64_t total=0;
    for (const auto& image:profile.unique_images) {
        const auto bytes=reader.image_bytes(image);
        if (bytes>max_final_rgba-total) fail("combined building RGBA budget exceeded");
        total+=bytes;
    }
    std::error_code error;const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) fail("invalid data root");
    const auto json=reader.read_manifest(manifest);
    reader.keys(json,{"schema_version","mode","selection","anchor_policy","evidence","files","stages"});
    if (reader.number(json,"schema_version",1)!=1 || reader.string(json,"mode")!="original_house_stages" ||
        reader.string(json,"selection")!="canonical_static_variant_0" || reader.string(json,"anchor_policy")!="type30_front_cell")
        fail("unsupported schema, selection or anchor policy");
    (void)reader.string(json,"evidence");
    if (!json.contains("files") || !json.at("files").is_array() || json.at("files").empty() || json.at("files").size()>16)
        fail("bounded asset fingerprints required");
    std::map<fs::path,fs::path> verified;
    for (const auto& file:json.at("files")) {
        reader.keys(file,{"path","sha256"});const auto path=reader.relative(reader.string(file,"path",4096));
        const auto hash=reader.string(file,"sha256",64);
        if (hash.size()!=64 || !std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
            fail("invalid lowercase SHA256");
        const auto actual=reader.checked_file(root,path);
        if (!verified.emplace(path,actual).second) fail("duplicate fingerprint path");
        if (reader.sha256(actual)!=hash) fail("fingerprint mismatch for "+path.generic_string());
    }
    if (!json.contains("stages") || !json.at("stages").is_array() || json.at("stages").size()!=original_house_type_count)
        fail("exactly16 explicit House stages required");
    std::array<bool,original_house_type_count> seen{};
    auto candidate=profile;
    candidate.original_house_entries.fill(std::nullopt);
    candidate.original_house_fallbacks.fill({});
    std::map<fs::path,Sg3Archive> archives;
    std::map<fs::path,maps::RuntimeArchiveLayout> layouts;
    auto ids=reader.physical_ids(candidate);
    for (const auto& stage:json.at("stages")) {
        reader.keys(stage,{"house_type","original_side","fallback","archive","image_index","width","height","base_bytes","ground_anchor","group_key","variant","evidence"});
        const auto type=reader.number(stage,"house_type",17);
        if (type<original_house_first_type) fail("House type outside2..17");
        const auto index=static_cast<std::size_t>(type-original_house_first_type);
        if (seen[index]) fail("duplicate House type");seen[index]=true;
        const auto side=reader.number(stage,"original_side",4);
        if (side!=(type<11 ? 2U:4U)) fail("House type/family side mismatch");
        candidate.original_house_fallbacks[index]=reader.string(stage,"fallback");
        if (!stage.contains("archive")) {
            if (stage.size()!=3) fail("fallback stage must not declare partial sprite metadata");
            continue;
        }
        BuildingVisualEntry entry;entry.id.archive_relative_path=reader.relative(reader.string(stage,"archive",4096));
        if (entry.id.archive_relative_path.extension()!=".sg3") fail("House archive must be.sg3");
        const auto physical=reader.number(stage,"image_index",UINT32_MAX);
        if (!physical) fail("physical record zero is not a House body");
        entry.id.image_index=static_cast<std::uint32_t>(physical);
        const auto width=reader.number(stage,"width",4096),height=reader.number(stage,"height",4096);
        const auto base=reader.number(stage,"base_bytes",51200);
        if (width!=80U*side-2U || height<40U*side || base!=3200U*side*side)
            fail("House native Type30 geometry mismatch");
        if (!stage.contains("ground_anchor") || !stage.at("ground_anchor").is_array() || stage.at("ground_anchor").size()!=2)
            fail("House anchor missing");
        const auto& anchor=stage.at("ground_anchor");
        if (!anchor[0].is_number_integer() || !anchor[1].is_number_integer() ||
            anchor[0].get<std::int64_t>()!=static_cast<std::int64_t>(40U*side-1U) ||
            anchor[1].get<std::int64_t>()!=static_cast<std::int64_t>(height-20U))
            fail("House anchor differs from evidenced native front cell");
        entry.ground_x=static_cast<double>(40U*side-1U);entry.ground_y=static_cast<double>(height-20U);
        entry.footprint_side=static_cast<std::uint8_t>(side);entry.evidence=reader.string(stage,"evidence");
        const auto group=reader.number(stage,"group_key",0x7fffffffU);
        const auto variant=reader.number(stage,"variant",1);
        const auto expected=evidenced_group(type);
        if (!expected || group!=*expected || variant!=0)
            fail("unsupported House registration/static variant");
        const auto verified_archive=verified.find(entry.id.archive_relative_path);
        if (verified_archive==verified.end()) fail("House archive lacks fingerprint");
        auto found_archive=archives.find(entry.id.archive_relative_path);
        if (found_archive==archives.end()) found_archive=archives.emplace(entry.id.archive_relative_path,
            read_sg3_archive(verified_archive->second)).first;
        const auto& archive=found_archive->second;
        auto layout=layouts.find(entry.id.archive_relative_path);
        if (layout==layouts.end()) {
            auto prepared=maps::build_runtime_archive_layout(2,archive,maps::RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
            if (!prepared) fail("House archive registration unsupported");
            layout=layouts.emplace(entry.id.archive_relative_path,std::move(*prepared)).first;
        }
        const auto resolution=maps::resolve_resource_group({static_cast<std::uint32_t>(group)},{{2,{&layout->second}}});
        const auto packed=resolution.packed_base ? maps::group_variant(*resolution.packed_base,static_cast<std::uint32_t>(variant)):std::nullopt;
        const auto resolved=packed ? layout->second.physical_record_for_local(packed->value & 0x3fffU):std::nullopt;
        if (resolution.status!=maps::GroupLookupStatus::Resolved || !resolved || *resolved!=physical)
            fail("House physical record differs from registered group/variant");
        if (physical>=archive.images.size()) fail("House physical record outside SG3 table");
        const auto& record=archive.images[static_cast<std::size_t>(physical)];
        if (!original_visual_detail::native_type30_matches(record,width,height,side,base,false))
            fail("House SG3 metadata mismatch");
        const auto bitmap=resolve_sg3_image_bitmap(verified_archive->second,archive,record);
        if (bitmap.status!=Sg3BitmapStatus::Resolved) fail("House bitmap unavailable or unsafe");
        const auto source=fs::relative(bitmap.path,root,error);
        if (error || verified.find(source)==verified.end() || reader.checked_file(root,source)!=bitmap.path)
            fail("House bitmap lacks contained fingerprint");
        const auto existing=std::find_if(ids.begin(),ids.end(),[&](const auto& id){return id && *id==entry.id;});
        if (existing!=ids.end()) {
            entry.image_index=static_cast<std::size_t>(existing-ids.begin());
            const auto& image=candidate.unique_images.at(entry.image_index);
            if (image.width!=width || image.height!=height) fail("shared House image metadata mismatch");
        } else {
            const auto bytes=width*height*4U;
            if (bytes>max_final_rgba-total) fail("combined building RGBA budget exceeded");
            auto image=load_sg3_image({verified_archive->second,entry.id.image_index});
            if (image.width!=width || image.height!=height || reader.image_bytes(image)!=bytes)
                fail("House decoded image metadata mismatch");
            entry.image_index=candidate.unique_images.size();ids.push_back(entry.id);
            candidate.unique_images.push_back(std::move(image));total+=bytes;
        }
        candidate.original_house_entries[index]=std::move(entry);
    }
    if (std::find(seen.begin(),seen.end(),false)!=seen.end()) fail("missing House type");
    candidate.original_house_prepared=true;
    profile=std::move(candidate);
}
} // namespace openemperor::assets
