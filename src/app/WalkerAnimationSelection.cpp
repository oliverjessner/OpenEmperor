#include "app/WalkerAnimationSelection.h"

#include "assets/Sg3ImageLoader.h"
#include "maps/ResourceGroupLookup.h"

#include <array>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

namespace openemperor {
namespace {
using Role=assets::WalkerVisualRole;
struct Family {
    Role role;
    std::uint32_t key;
    bool supplier_foot=false;
};
// These are separately documented source-group registrations, not physical
// record admission lists. Clay/Pottery/Household/Inspector/Market remain curated.
constexpr std::array<Family,12> families{{
    {Role::Clay,0x4c03},{Role::Pottery,0x4c05},{Role::Household,0x4c09},
    {Role::FireInspector,0x4c09},{Role::Supplier,0x4e60,true},
    {Role::Distributor,0x4e75},{Role::Service,0x4c03},
    {Role::HealthWorker,0x4c1e},{Role::Music,0x4c67},
    {Role::Acrobat,0x4c94},{Role::Drama,0x4c6b},{Role::VenueMusic,0x4ca0}
}};
[[noreturn]] void invalid(Role role,const char* reason) {
    throw std::runtime_error(std::string("Complete walker source qualification failed for ")+
        assets::walker_role_name(role)+": "+reason);
}
std::int32_t mirror_offset(std::uint32_t raw) {
    return raw<=0x7fffffffU ? static_cast<std::int32_t>(raw):
        static_cast<std::int32_t>(static_cast<std::int64_t>(raw)-0x100000000LL);
}
}

void validate_builtin_walker_animations(const std::filesystem::path& data_root,
                                      const assets::WalkerVisualProfile& profile) {
    if (profile.schema_version!=7 || profile.roles.size()!=families.size())
        throw std::runtime_error("Complete walker source qualification requires schema 7");
    const std::array<assets::Sg3Archive,2> archives{
        assets::read_sg3_archive(data_root/"DATA/SprMain.sg3"),
        assets::read_sg3_archive(data_root/"DATA/SprMain2.sg3")};
    std::array<maps::RuntimeArchiveLayout,2> layouts;
    std::map<std::uint32_t,maps::GroupRegistration> registrations;
    for (std::uint32_t index=0;index<2;++index) {
        const auto layout=maps::build_runtime_archive_layout(38U+index,archives[index],
            maps::RuntimeLayoutEvidence::WalkerSprites38And39);
        if (!layout) throw std::runtime_error("Complete walker source registration is unsupported");
        layouts[index]=*layout;registrations.emplace(38U+index,maps::GroupRegistration{&layouts[index]});
    }
    constexpr std::array<std::uint32_t,4> columns{2,0,2,0};
    constexpr std::array<bool,4> flips{false,true,true,false};
    for (const auto& family:families) {
        const auto* visual=profile.find(family.role);
        if (!visual || visual->frames.size()!=24)
            invalid(family.role,"all 24 native aliases are required");
        const auto index=family.key/512U-38U;
        const auto& archive=archives.at(index);const auto& layout=layouts.at(index);
        const auto selected=maps::resolve_resource_group({family.key},registrations);
        if (selected.status!=maps::GroupLookupStatus::Resolved || !selected.local_base)
            invalid(family.role,"resource group is unresolved");
        const auto local=*selected.local_base;
        const auto base=layout.physical_record_for_local(local);
        if (!base || local>layout.runtime_image_count || layout.runtime_image_count-local<96U)
            invalid(family.role,"walking sequence exceeds its registration");
        std::uint32_t group_end=layout.runtime_image_count;
        for (const auto& group:layout.groups)
            if (group.local_base>local && group.local_base<group_end) group_end=group.local_base;
        if (group_end-local<96U) invalid(family.role,"walking sequence crosses a source group");
        const auto& first=archive.images.at(*base);
        if (first.animation_sprites!=12 || first.raw[32]!=8 || first.raw[33]!=0)
            invalid(family.role,"source count/stride differs from twelve/eight");
        const auto cadence=assets::walker_role_index(family.role)<assets::walker_core_role_count ? 4U:2U;
        if (visual->ticks_per_frame!=cadence) invalid(family.role,"authored cadence differs");
        const auto expected_archive=index==0 ? "DATA/SprMain.sg3":"DATA/SprMain2.sg3";
        std::set<std::uint32_t> expected,actual;
        for (std::uint32_t phase=0;phase<12;++phase) for (const auto column:{0U,2U}) {
            const auto physical=layout.physical_record_for_local(local+8U*phase+column);
            const auto opposite=layout.physical_record_for_local(local+8U*phase+(column==0 ? 6U:4U));
            if (!physical || !opposite) invalid(family.role,"source frame is outside its runtime table");
            const auto& native=archive.images.at(*physical);const auto& mirror=archive.images.at(*opposite);
            if (native.width<=0 || native.height<=0 || native.image_type!=256 ||
                native.external_flag!=0 || native.data_length==0 || native.horizontal_mirror_offset!=0 ||
                native.alpha_length!=0 || native.shadow_marker_flag!=1)
                invalid(family.role,"native source layout differs");
            if (mirror.image_type!=256 || mirror.external_flag!=0 || mirror.shadow_marker_flag!=1 ||
                mirror.data_length!=0 || mirror.alpha_length!=0 ||
                mirror_offset(mirror.horizontal_mirror_offset)!=(column==0 ? -6:-2) ||
                mirror.width!=native.width || mirror.height!=native.height ||
                mirror.animation_y_offset!=native.animation_y_offset ||
                mirror.animation_x_offset!=native.width-native.animation_x_offset)
                invalid(family.role,"opposite source mirror relation differs");
            expected.insert(*physical);
        }
        for (const auto& frame:visual->frames) {
            if (frame.id.archive_relative_path.generic_string()!=expected_archive ||
                !expected.contains(frame.id.image_index) || !actual.insert(frame.id.image_index).second)
                invalid(family.role,"native frame identity differs or is duplicated");
        }
        const auto verify_frame=[&](std::size_t frame_index,std::uint32_t physical,bool clip_flip,bool flip) {
            if (frame_index>=visual->frames.size()) invalid(family.role,"clip references an invalid alias");
            const auto& frame=visual->frames[frame_index];
            if (frame.id.image_index!=physical || frame.image_index>=profile.unique_images.size())
                invalid(family.role,"clip phase or image reference differs");
            const auto& native=archive.images.at(physical);const auto& image=profile.unique_images[frame.image_index];
            if (image.width!=static_cast<std::uint32_t>(native.width) ||
                image.height!=static_cast<std::uint32_t>(native.height) ||
                image.pixels.size()!=static_cast<std::uint64_t>(image.width)*image.height*4U)
                invalid(family.role,"prepared image dimensions differ");
            const auto transform=assets::walker_frame_transform(frame,image.width,clip_flip);
            const double x=flip ? native.width-native.animation_x_offset:native.animation_x_offset;
            const double y=native.animation_y_offset-(family.supplier_foot ? 0:8);
            if (transform.flip_x!=flip || transform.foot_x!=x || transform.foot_y!=y)
                invalid(family.role,"display reflection or authored foot differs");
        };
        for (std::size_t direction=0;direction<4;++direction) {
            const auto& clip=visual->clips[direction];
            if (clip.size()!=12) invalid(family.role,"a complete ordered twelve-phase clip is required");
            for (std::uint32_t phase=0;phase<12;++phase)
                verify_frame(clip[phase],*layout.physical_record_for_local(local+8U*phase+columns[direction]),
                    visual->clip_flip_x[direction],flips[direction]);
        }
        const bool idle_flip=family.role!=Role::Supplier && family.role!=Role::Distributor;
        verify_frame(visual->idle_frame,*base,visual->idle_flip_x,idle_flip);
    }
}

} // namespace openemperor
