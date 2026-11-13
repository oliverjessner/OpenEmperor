#include "assets/PlantationVisualProfile.h"
#include "assets/OriginalVisualProfileInternal.h"
#include "assets/Sg3ImageLoader.h"
#include "maps/ResourceGroupLookup.h"
#include <map>

namespace openemperor::assets {
void append_plantation_visual_profile(const std::filesystem::path& data_root,
        const std::filesystem::path& manifest,BuildingVisualProfile& profile,std::uint64_t max_final_rgba) {
    namespace fs=std::filesystem;
    original_visual_detail::Reader r{"Plantation visual profile: "};
    if (profile.plantation_prepared) r.fail("remove existing plantation supplement before replacement");
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
    r.keys(json,{"schema_version","mode","selection","anchor_policy","files","plants"});
    if (r.number(json,"schema_version",1)!=1 || r.string(json,"mode")!="plantation_bases" ||
        r.string(json,"selection")!=plantation_visual_selection_contract || r.string(json,"anchor_policy")!="type30_front_cell")
        r.fail("unsupported schema, selection or anchor policy");
    if (!json.contains("files") || !json.at("files").is_array() || json.at("files").size()!=2)
        r.fail("exactly two General fingerprints required");
    std::map<fs::path,fs::path> verified;
    const fs::path archive_name="DATA/China_General.sg3",pixels_name="DATA/China_General.555";
    for (const auto& row:json.at("files")) {
        r.keys(row,{"path","sha256"});const auto name=r.relative(r.string(row,"path",4096));
        if (name!=archive_name && name!=pixels_name) r.fail("unsupported dependency");
        const auto hash=r.string(row,"sha256",64);
        if (hash.size()!=64 || !std::all_of(hash.begin(),hash.end(),[](char c){return (c>='0' && c<='9') || (c>='a' && c<='f');}))
            r.fail("invalid SHA256");
        const auto path=r.checked_file(root,name);
        if (!verified.emplace(name,path).second || r.sha256(path)!=hash) r.fail("duplicate or mismatching fingerprint");
    }
    if (!json.contains("plants") || !json.at("plants").is_array() || json.at("plants").size()!=3)
        r.fail("exactly three plant identities required");
    const auto archive=read_sg3_archive(verified.at(archive_name));
    const auto layout=maps::build_runtime_archive_layout(2,archive,maps::RuntimeLayoutEvidence::TerrainElevationAndGeneral2);
    if (!layout) r.fail("General registration unsupported");
    auto candidate=profile;auto ids=r.physical_ids(candidate);std::array<bool,3> seen{};
    for (const auto& row:json.at("plants")) {
        r.keys(row,{"model_type","group_key","evidence","stages"});
        const auto model=r.number(row,"model_type",UINT16_MAX);
        const auto index=plantation_visual_index(static_cast<std::uint16_t>(model));
        if (!index || seen[*index]) r.fail("unsupported or duplicate plant identity");
        seen[*index]=true;const std::uint32_t group=model==26 ? 0x432U:model==27 ? 0x434U:0x433U;
        if (r.number(row,"group_key",UINT32_MAX)!=group) r.fail("plant group differs from source selector");
        const auto evidence=r.string(row,"evidence",4096);const std::size_t count=model==26 ? 4:6;
        if (!row.contains("stages") || !row.at("stages").is_array() || row.at("stages").size()!=count)
            r.fail("complete canonical plant stage bank required");
        const auto resolved=maps::resolve_resource_group({group},{{2,{&*layout}}});
        if (resolved.status!=maps::GroupLookupStatus::Resolved || !resolved.packed_base) r.fail("plant group unavailable");
        for (std::size_t stage=0;stage<count;++stage) {
            const auto& image=row.at("stages")[stage];
            r.keys(image,{"image_index","width","height","base_bytes","ground_anchor"});
            const auto physical=r.number(image,"image_index",UINT32_MAX);
            const auto width=r.number(image,"width",4096),height=r.number(image,"height",4096);
            const auto base=r.number(image,"base_bytes",3200);
            const auto packed=maps::group_variant(*resolved.packed_base,static_cast<std::uint32_t>(stage));
            const auto expected=packed ? layout->physical_record_for_local(packed->value&0x3fffU):std::nullopt;
            if (!expected || *expected!=physical || physical>=archive.images.size() || width!=78 || height<40 || base!=3200)
                r.fail("plant stage differs from native source geometry/selector");
            const auto& record=archive.images[physical];
            // The pinned Lacquer offset4 has a zero size hint. Its full
            // 78x86/3200-byte Type30 geometry and independently proved1x1
            // plant identity remain mandatory. All other hints stay exactly1.
            const bool zero_hint=model==27 && stage==4 && width==78 && height==86 && record.isometric_size_flag==0;
            if (record.external_flag || (!zero_hint && record.isometric_size_flag!=1) ||
                !original_visual_detail::native_type30_matches(record,width,height,zero_hint ? 0:1,base,false))
                r.fail("plant stage native source metadata differs");
            if (!image.contains("ground_anchor") || image.at("ground_anchor")!=original_visual_detail::Json::array({39,height-20}))
                r.fail("plant anchor differs from authored front-cell geometry");
            const auto bitmap=resolve_sg3_image_bitmap(verified.at(archive_name),archive,archive.images[physical]);
            if (bitmap.status!=Sg3BitmapStatus::Resolved || bitmap.path!=verified.at(pixels_name))
                r.fail("plant bitmap lacks contained fingerprint");
            BuildingVisualEntry entry;entry.id={archive_name,static_cast<std::uint32_t>(physical)};
            entry.ground_x=39;entry.ground_y=static_cast<double>(height-20);entry.footprint_side=1;entry.evidence=evidence;
            const auto old=std::find_if(ids.begin(),ids.end(),[&](const auto& id){return id && *id==entry.id;});
            if (old!=ids.end()) {
                entry.image_index=static_cast<std::size_t>(old-ids.begin());
                if (candidate.unique_images[entry.image_index].width!=width || candidate.unique_images[entry.image_index].height!=height)
                    r.fail("shared physical image geometry differs");
            } else {
                const auto bytes=width*height*4;
                if (bytes>max_final_rgba-total) r.fail("combined building RGBA budget exceeded");
                auto rgba=load_sg3_image({verified.at(archive_name),entry.id.image_index});
                if (rgba.width!=width || rgba.height!=height || r.image_bytes(rgba)!=bytes) r.fail("decoded plant geometry differs");
                entry.image_index=candidate.unique_images.size();candidate.unique_images.push_back(std::move(rgba));ids.push_back(entry.id);total+=bytes;
            }
            candidate.plantation_entries[*index][stage]=std::move(entry);
        }
    }
    candidate.plantation_prepared=true;profile=std::move(candidate);
}
}
