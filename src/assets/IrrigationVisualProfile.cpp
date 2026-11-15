#include "assets/IrrigationVisualProfile.h"
#include "assets/OriginalVisualProfileInternal.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/ResourceGroupLookup.h"
#include <map>

namespace openemperor::assets {
void append_irrigation_visual_profile(const std::filesystem::path& data_root,
        const std::filesystem::path& manifest,BuildingVisualProfile& profile,std::uint64_t max_final_rgba) {
    namespace fs=std::filesystem;
    original_visual_detail::Reader r{"Irrigation visual profile: "};
    if (profile.irrigation_prepared) r.fail("remove existing irrigation supplement before replacement");
    max_final_rgba=std::min(max_final_rgba,building_max_rgba_bytes);
    std::uint64_t total=0;
    for (const auto& image:profile.unique_images) {
        const auto bytes=r.image_bytes(image);
        if (bytes>max_final_rgba-total) r.fail("combined building RGBA budget exceeded");
        total+=bytes;
    }
    std::error_code error;const auto root=fs::canonical(data_root,error);
    if (error || !fs::is_directory(root)) r.fail("invalid data root");
    if (fs::is_symlink(fs::symlink_status(manifest,error)) || error) r.fail("metadata missing or linked");
    const auto json=r.read_manifest(manifest);
    r.keys(json,{"schema_version","mode","selection","anchor_policy","evidence","files","ditches","pumps"});
    if (r.number(json,"schema_version",1)!=1 || r.string(json,"mode")!="irrigation_static" ||
        r.string(json,"selection")!=irrigation_visual_selection_contract || r.string(json,"anchor_policy")!="type30_front_cell")
        r.fail("unsupported schema, selection or anchor policy");
    const auto evidence=r.string(json,"evidence",4096);
    if (!json.contains("files") || !json.at("files").is_array() || json.at("files").size()!=2)
        r.fail("exactly two General fingerprints required");
    const fs::path archive_name="DATA/China_General.sg3",pixels_name="DATA/China_General.555";
    std::map<fs::path,fs::path> verified;
    for (const auto& row:json.at("files")) {
        r.keys(row,{"path","sha256"});const auto name=r.relative(r.string(row,"path",4096));
        if (name!=archive_name && name!=pixels_name) r.fail("unsupported dependency");
        const auto hash=r.string(row,"sha256",64);
        if (hash.size()!=64 || !std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');})) r.fail("invalid SHA256");
        const auto path=r.checked_file(root,name);
        if (!verified.emplace(name,path).second || r.sha256(path)!=hash) r.fail("duplicate or mismatching fingerprint");
    }
    if (!json.contains("ditches") || !json.at("ditches").is_array() || json.at("ditches").size()!=36 ||
        !json.contains("pumps") || !json.at("pumps").is_array() || json.at("pumps").size()!=4)
        r.fail("complete36 Ditch and four Pump static records required");
    const auto archive=read_sg3_archive(verified.at(archive_name));
    const auto layout=maps::build_runtime_archive_layout(2,archive,maps::RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
    if (!layout) r.fail("General registration unsupported");
    auto candidate=profile;auto ids=r.physical_ids(candidate);
    const auto image=[&](const original_visual_detail::Json& row,std::uint32_t group,std::uint32_t offset,std::uint8_t side,bool pump) {
        const auto physical=r.number(row,"image_index",UINT32_MAX);
        const auto width=r.number(row,"width",4096),height=r.number(row,"height",4096),base=r.number(row,"base_bytes",12800);
        const auto resolved=maps::resolve_resource_group({group},{{2,{&*layout}}});
        if (resolved.status!=maps::GroupLookupStatus::Resolved || !resolved.packed_base) r.fail("source group unavailable");
        // This verified atlas is wider than the historical three-bit map variation.
        // Keep group_variant's independent saved-image contract unchanged.
        const auto packed=resolved.packed_base->value;
        if ((packed&0x80000000U) || (packed&0x3fffU)>0x3fffU-offset || packed>UINT32_MAX-offset)
            r.fail("source atlas offset overflow");
        const auto expected=layout->physical_record_for_local((packed+offset)&0x3fffU);
        if (!expected || *expected!=physical || physical>=archive.images.size() || width!=80U*side-2U || height<40U*side || base!=3200U*side*side)
            r.fail("record differs from native source geometry/selector");
        const auto& record=archive.images[physical];
        if (record.external_flag || !original_visual_detail::native_type30_matches(record,width,height,side,base,pump))
            r.fail("native source metadata differs");
        if (pump && (record.animation_sprites!=15 || record.animation_speed_id!=4)) r.fail("Pump base association differs");
        if (!row.contains("ground_anchor") || row.at("ground_anchor")!=original_visual_detail::Json::array({40U*side-1U,height-20U}))
            r.fail("anchor differs from authored front-cell geometry");
        const auto bitmap=resolve_sg3_image_bitmap(verified.at(archive_name),archive,record);
        if (bitmap.status!=Sg3BitmapStatus::Resolved || bitmap.path!=verified.at(pixels_name)) r.fail("bitmap lacks contained fingerprint");
        BuildingVisualEntry entry;entry.id={archive_name,static_cast<std::uint32_t>(physical)};
        entry.ground_x=40U*side-1U;entry.ground_y=static_cast<double>(height-20U);entry.footprint_side=side;entry.evidence=evidence;
        const auto old=std::find_if(ids.begin(),ids.end(),[&](const auto& id){return id && *id==entry.id;});
        if (old!=ids.end()) {
            entry.image_index=static_cast<std::size_t>(old-ids.begin());
            if (candidate.unique_images[entry.image_index].width!=width || candidate.unique_images[entry.image_index].height!=height) r.fail("shared physical geometry differs");
        } else {
            const auto bytes=width*height*4U;
            if (bytes>max_final_rgba-total) r.fail("combined building RGBA budget exceeded");
            auto rgba=load_sg3_image({verified.at(archive_name),entry.id.image_index});
            if (rgba.width!=width || rgba.height!=height || r.image_bytes(rgba)!=bytes) r.fail("decoded source geometry differs");
            entry.image_index=candidate.unique_images.size();candidate.unique_images.push_back(std::move(rgba));ids.push_back(entry.id);total+=bytes;
        }
        return entry;
    };
    for (std::size_t i=0;i<36;++i) {
        const auto& row=json.at("ditches")[i];r.keys(row,{"offset","image_index","width","height","base_bytes","ground_anchor"});
        if (r.number(row,"offset",35)!=i) r.fail("Ditch atlas requires ordered offsets0..35");
        candidate.irrigation_ditch_entries[i]=image(row,0x435,static_cast<std::uint32_t>(i),1,false);
    }
    constexpr std::array<std::uint32_t,4> groups{0x439,0x4b6,0x4b7,0x436};
    for (std::size_t i=0;i<4;++i) {
        const auto& row=json.at("pumps")[i];r.keys(row,{"orientation","group_key","image_index","width","height","base_bytes","ground_anchor"});
        if (r.number(row,"orientation",3)!=i || r.number(row,"group_key",UINT32_MAX)!=groups[i]) r.fail("Pump orientation differs from source selector");
        candidate.irrigation_pump_entries[i]=image(row,groups[i],0,2,true);
    }
    candidate.irrigation_prepared=true;profile=std::move(candidate);
}
}
