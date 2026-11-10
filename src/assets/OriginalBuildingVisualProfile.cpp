#include "assets/BuildingVisualProfile.h"
#include "assets/OriginalVisualProfileInternal.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/ResourceGroupLookup.h"
#include <map>

namespace openemperor::assets {
namespace fs=std::filesystem;
namespace {
constexpr const char* general_archive="DATA/China_General.sg3";
constexpr const char* general_pixels="DATA/China_General.555";
}

const char* original_building_role_name(OriginalBuildingVisualRole role) {
    switch (role) {
    case OriginalBuildingVisualRole::WaterWell:return "Water Well";
    case OriginalBuildingVisualRole::Acupuncture:return "Acupuncture";
    case OriginalBuildingVisualRole::MusicSchool:return "Music School";
    case OriginalBuildingVisualRole::AcrobatSchool:return "Acrobat School";
    case OriginalBuildingVisualRole::DramaSchool:return "Drama School";
    case OriginalBuildingVisualRole::DaoistShrine:return "Daoist Shrine";
    case OriginalBuildingVisualRole::BuddhistShrine:return "Buddhist Shrine";
    case OriginalBuildingVisualRole::ConfucianAcademy:return "Confucian Academy";
    case OriginalBuildingVisualRole::PaperMaker:return "Paper Maker";
    case OriginalBuildingVisualRole::Weaver:return "Weaver";
    case OriginalBuildingVisualRole::HempFarm:return "Hemp Farm";
    case OriginalBuildingVisualRole::Farmhouse:return "Farmhouse";
    case OriginalBuildingVisualRole::HempField:return "Hemp Field";
    case OriginalBuildingVisualRole::WheatField:return "Wheat Field";
    case OriginalBuildingVisualRole::MilletField:return "Millet Field";
    }
    return "Unsupported Original building";
}

void append_original_building_visual_profile(const fs::path& data_root,const fs::path& manifest,
    BuildingVisualProfile& profile,std::uint64_t max_final_rgba) {
    original_visual_detail::Reader reader{"Original building profile: "};
    if (profile.original_building_prepared) reader.fail("remove an existing extension before replacement");
    max_final_rgba=std::min(max_final_rgba,building_max_rgba_bytes);
    std::uint64_t total=0;
    for (const auto& image:profile.unique_images) {
        const auto bytes=reader.image_bytes(image);
        if (bytes>max_final_rgba-total) reader.fail("combined building RGBA budget exceeded");
        total+=bytes;
    }
    std::error_code error;
    const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) reader.fail("invalid data root");
    // A compatibility resource must be a regular metadata file, not a linked
    // replacement. Original source containment is checked independently below.
    if (fs::is_symlink(fs::symlink_status(manifest,error)) || error)
        reader.fail("metadata missing or linked");
    const auto json=reader.read_manifest(manifest);
    reader.keys(json,{"schema_version","mode","selection","anchor_policy","evidence","files","buildings"});
    const auto schema=reader.number(json,"schema_version",2);
    if ((schema!=1 && schema!=2) || reader.string(json,"mode")!="original_building_bases" ||
        reader.string(json,"selection")!="canonical_static_base" || reader.string(json,"anchor_policy")!="type30_front_cell")
        reader.fail("unsupported schema, selection or anchor policy");
    (void)reader.string(json,"evidence");
    if (!json.contains("files") || !json.at("files").is_array() || json.at("files").size()!=2)
        reader.fail("exactly two General asset fingerprints required");
    std::map<fs::path,fs::path> verified;
    for (const auto& file:json.at("files")) {
        reader.keys(file,{"path","sha256"});
        const auto path=reader.relative(reader.string(file,"path",4096));
        if (path!=general_archive && path!=general_pixels) reader.fail("unsupported General dependency");
        const auto hash=reader.string(file,"sha256",64);
        if (hash.size()!=64 || !std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
            reader.fail("invalid lowercase SHA256");
        const auto actual=reader.checked_file(root,path);
        if (!verified.emplace(path,actual).second) reader.fail("duplicate fingerprint path");
        if (reader.sha256(actual)!=hash) reader.fail("fingerprint mismatch for "+path.generic_string());
    }
    const auto role_count=schema==1 ? original_building_schema1_role_count:original_building_role_count;
    if (!json.contains("buildings") || !json.at("buildings").is_array() || json.at("buildings").size()!=role_count)
        reader.fail("schema requires exactly "+std::to_string(role_count)+" explicit Original building roles");
    const auto archive=read_sg3_archive(verified.at(general_archive));
    const auto layout=maps::build_runtime_archive_layout(2,archive,maps::RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
    if (!layout) reader.fail("General archive registration unsupported");
    auto candidate=profile;
    auto ids=reader.physical_ids(candidate);
    candidate.original_building_entries={};
    candidate.original_building_fallbacks={};
    std::array<bool,original_building_role_count> seen{};
    for (const auto& row:json.at("buildings")) {
        bool native=true;
        if (schema==2) {
            if (!row.contains("native") || !row.at("native").is_boolean()) reader.fail("explicit native/fallback selection required");
            native=row.at("native").get<bool>();
            if (native) reader.keys(row,{"model_type","original_side","native","fallback","archive","image_index","width","height","base_bytes","ground_anchor","group_key","variant","evidence"});
            else reader.keys(row,{"model_type","original_side","native","fallback","evidence"});
        } else reader.keys(row,{"model_type","original_side","fallback","archive","image_index","width","height","base_bytes","ground_anchor","group_key","variant","evidence"});
        const auto model=reader.number(row,"model_type",UINT16_MAX);
        const auto role=original_building_visual_role(static_cast<std::uint16_t>(model));
        if (!role) reader.fail("unsupported Original model registration");
        const auto index=static_cast<std::size_t>(*role);
        if (index>=role_count) reader.fail("Original model is not registered in this schema");
        if (seen[index]) reader.fail("duplicate Original building model");
        seen[index]=true;
        const auto& identity=*original_building_visual_identity(*role);
        const auto side=reader.number(row,"original_side",3);
        if (side!=identity.footprint_side) reader.fail("Original model/side mismatch");
        candidate.original_building_fallbacks[index]=reader.string(row,"fallback");
        if (!native) {
            (void)reader.string(row,"evidence");
            continue;
        }
        if (!identity.static_base_supported) reader.fail("Original field state selector is unsupported; explicit fallback required");
        const auto group=reader.number(row,"group_key",0x7fffffffU);
        const auto variant=reader.number(row,"variant",1);
        if (side!=identity.footprint_side || group!=identity.group_key || variant!=0)
            reader.fail("Original model/group/side/static variant mismatch");
        BuildingVisualEntry entry;
        entry.id.archive_relative_path=reader.relative(reader.string(row,"archive",4096));
        if (entry.id.archive_relative_path!=general_archive) reader.fail("unsupported Original building archive");
        const auto physical=reader.number(row,"image_index",UINT32_MAX);
        if (!physical) reader.fail("physical record zero is not an Original building base");
        entry.id.image_index=static_cast<std::uint32_t>(physical);
        const auto width=reader.number(row,"width",4096),height=reader.number(row,"height",4096);
        const auto base=reader.number(row,"base_bytes",28800);
        if (width!=80U*side-2U || height<40U*side || base!=3200U*side*side)
            reader.fail("Original building native Type30 geometry mismatch");
        if (!row.contains("ground_anchor") || !row.at("ground_anchor").is_array() || row.at("ground_anchor").size()!=2)
            reader.fail("Original building anchor missing");
        const auto& anchor=row.at("ground_anchor");
        if (!anchor[0].is_number_integer() || !anchor[1].is_number_integer() ||
            anchor[0].get<std::int64_t>()!=static_cast<std::int64_t>(40U*side-1U) ||
            anchor[1].get<std::int64_t>()!=static_cast<std::int64_t>(height-20U))
            reader.fail("Original building anchor differs from native front cell");
        entry.ground_x=static_cast<double>(40U*side-1U);entry.ground_y=static_cast<double>(height-20U);
        entry.footprint_side=static_cast<std::uint8_t>(side);entry.evidence=reader.string(row,"evidence");
        const auto resolution=maps::resolve_resource_group({static_cast<std::uint32_t>(group)},{{2,{&*layout}}});
        const auto packed=resolution.packed_base ? maps::group_variant(*resolution.packed_base,0):std::nullopt;
        const auto resolved=packed ? layout->physical_record_for_local(packed->value & 0x3fffU):std::nullopt;
        if (resolution.status!=maps::GroupLookupStatus::Resolved || !resolved || *resolved!=physical)
            reader.fail("Original building physical record differs from registered group/static base");
        if (physical>=archive.images.size()) reader.fail("Original building physical record outside SG3 table");
        const auto& record=archive.images[static_cast<std::size_t>(physical)];
        if (!original_visual_detail::native_type30_matches(record,width,height,side,base,true))
            reader.fail("Original building SG3 metadata mismatch");
        const auto bitmap=resolve_sg3_image_bitmap(verified.at(general_archive),archive,record);
        if (bitmap.status!=Sg3BitmapStatus::Resolved) reader.fail("Original building bitmap unavailable or unsafe");
        const auto source=fs::relative(bitmap.path,root,error);
        if (error || verified.find(source)==verified.end() || reader.checked_file(root,source)!=bitmap.path)
            reader.fail("Original building bitmap lacks contained fingerprint");
        const auto existing=std::find_if(ids.begin(),ids.end(),[&](const auto& id){return id && *id==entry.id;});
        if (existing!=ids.end()) {
            entry.image_index=static_cast<std::size_t>(existing-ids.begin());
            const auto& image=candidate.unique_images.at(entry.image_index);
            if (image.width!=width || image.height!=height) reader.fail("shared Original building image metadata mismatch");
        } else {
            const auto bytes=width*height*4U;
            if (bytes>max_final_rgba-total) reader.fail("combined building RGBA budget exceeded");
            auto image=load_sg3_image({verified.at(general_archive),entry.id.image_index});
            if (image.width!=width || image.height!=height || reader.image_bytes(image)!=bytes)
                reader.fail("Original building decoded image metadata mismatch");
            entry.image_index=candidate.unique_images.size();ids.push_back(entry.id);
            candidate.unique_images.push_back(std::move(image));total+=bytes;
        }
        candidate.original_building_entries[index]=std::move(entry);
    }
    const auto expected_end=seen.begin()+static_cast<std::ptrdiff_t>(role_count);
    if (std::find(seen.begin(),expected_end,false)!=expected_end)
        reader.fail("missing Original building role");
    // Historical schema1 carries no claim about subsequently added roles.
    for (std::size_t index=role_count;index<original_building_role_count;++index)
        candidate.original_building_fallbacks[index]=std::string(original_building_role_name(original_building_visual_identities[index].role))+
            ": absent from historical schema1; authored fallback";
    candidate.original_building_prepared=true;
    profile=std::move(candidate);
}
} // namespace openemperor::assets
