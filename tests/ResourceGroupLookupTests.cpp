#include "maps/ResourceGroupLookup.h"
#include "maps/GraphicsIdHypothesis.h"

#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace assets = openemperor::assets;
namespace maps = openemperor::maps;

namespace {
void check(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string{message});
}
void u16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value) {
    bytes.at(offset) = static_cast<std::uint8_t>(value);
    bytes.at(offset+1) = static_cast<std::uint8_t>(value >> 8U);
}
void u32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value >> (8U*i));
}
assets::Sg3Archive archive(std::uint32_t capacity, std::uint32_t in_use,
                            std::string_view first_group,
                            const std::vector<std::pair<std::uint32_t,std::uint16_t>>& indices) {
    std::vector<std::uint8_t> bytes(40680ULL+64ULL*capacity,0);
    u32(bytes,4,213); u32(bytes,12,capacity); u32(bytes,16,in_use);
    if (!first_group.empty()) {
        u32(bytes,20,1);
        check(first_group.size()<65,"synthetic group name too long");
        for (std::size_t i=0;i<first_group.size();++i)
            bytes[680+i]=static_cast<std::uint8_t>(first_group[i]);
    }
    for (const auto& [position,value] : indices)
        u16(bytes,80+2*position,value);
    return assets::parse_sg3(bytes,bytes.size());
}
} // namespace

int main() {
    try {
        // Value sorting would yield 12,9,4,4; unique increasing list keys
        // preserve the file order 9,4,12,4, including a duplicate.
        const auto plain=archive(32,20,"Other.bmp",
                                 {{1,9},{2,0},{3,4},{4,0x8000},{5,12},{7,4}});
        const auto layout=maps::build_runtime_archive_layout(3,plain);
        check(layout && !layout->system_branch_activated && layout->system_record_skip==0 &&
              layout->runtime_image_count==20 && layout->first_physical_record==1,
              "ordinary v213 layout and dummy skip");
        check(layout->groups.size()==4 && layout->groups[0].raw_value==9 &&
              layout->groups[1].raw_value==4 && layout->groups[2].raw_value==12 &&
              layout->groups[3].raw_value==4,
              "file order, zero/sign filtering, and duplicate values");
        const std::map<std::uint32_t,maps::GroupRegistration> context{{3,{&*layout}}};
        const auto first=maps::resolve_resource_group({0x601},context);
        const auto last=maps::resolve_resource_group({0x604},context);
        check(first.status==maps::GroupLookupStatus::Resolved &&
              first.sg3_index_position==1 && first.sg3_file_offset==82 &&
              first.sg3_raw_value==9 && first.local_base==8 &&
              first.packed_base && first.packed_base->value==0xc008,
              "first file-order group and exact raw-minus-one base");
        check(last.status==maps::GroupLookupStatus::Resolved &&
              last.sg3_index_position==7 && last.packed_base->value==0xc003,
              "last file-order group");
        check(maps::resolve_resource_group({0x605},context).status==
                  maps::GroupLookupStatus::GroupOutOfRange &&
              maps::resolve_resource_group({0x600},context).status==
                  maps::GroupLookupStatus::NoGroupPosition &&
              maps::resolve_resource_group({0x801},context).status==
                  maps::GroupLookupStatus::UnregisteredSlot &&
              maps::resolve_resource_group({0x80000601},context).status==
                  maps::GroupLookupStatus::UnsupportedHighBit,
              "unsupported keys and positions reject without fallback");
        const auto permuted=archive(32,20,"Other.bmp",{{1,4},{3,12},{5,9}});
        const auto permuted_layout=maps::build_runtime_archive_layout(3,permuted);
        check(permuted_layout && permuted_layout->groups[0].raw_value==4 &&
              permuted_layout->groups[1].raw_value==12 &&
              permuted_layout->groups[2].raw_value==9,
              "a second permutation retains positions, not value order");

        const auto system=archive(512,400,"Zeus_system.bmp",
                                  {{1,199},{2,201},{4,204},{5,212},{6,0x8000}});
        const auto system_layout=maps::build_runtime_archive_layout(3,system);
        check(system_layout && system_layout->system_branch_activated &&
              system_layout->system_record_skip==200 &&
              system_layout->runtime_image_count==200 &&
              system_layout->first_physical_record==201 &&
              system_layout->physical_record_for_local(0)==201 &&
              system_layout->physical_record_for_local(199)==400 &&
              !system_layout->physical_record_for_local(200),
              "first internal group activates literal 200-record skip and bounded image count");
        check(system_layout->groups.size()==3 &&
              system_layout->groups[0].raw_value==201 &&
              system_layout->groups[1].raw_value==204 &&
              system_layout->groups[2].raw_value==212 &&
              system_layout->groups[1].local_base==3,
              "system skip filters low index words and shifts group bases once");
        const std::map<std::uint32_t,maps::GroupRegistration> system_context{{3,{&*system_layout}}};
        const auto group=maps::resolve_resource_group({0x602},system_context);
        check(group.status==maps::GroupLookupStatus::Resolved &&
              group.sg3_index_position==4 && group.sg3_file_offset==88 &&
              group.sg3_raw_value==204 && group.packed_base->value==0xc003,
              "group lookup uses shared system layout");
        assets::AssetCatalog catalog;
        catalog.records.resize(512);
        for (std::uint32_t i=0;i<512;++i) catalog.records[i].id.image_index=i;
        const std::map<std::uint32_t,maps::GraphicsArchiveRegistration> graphics{
            {3,{&catalog,&*system_layout}}};
        const auto image=maps::resolve_graphics_id_hypothesis(group.packed_base->value,graphics);
        check(image.local_index==3 && image.physical_record_index==204 &&
              catalog.records[204].id.image_index==204,
              "group base and image resolver agree without a second skip");
        for (std::uint32_t i=0;i<8;++i) {
            const auto variant=maps::group_variant(*group.packed_base,i);
            check(variant && variant->value==0xc003+i,
                  "all eight supplied variants preserve packed slot");
        }
        check(!maps::group_variant(*group.packed_base,8) &&
              !maps::group_variant({0xffff},1),
              "variant budget and packed-slot carry rejected");

        auto bad=system;
        bad.header.version=214;
        check(!maps::build_runtime_archive_layout(3,bad) &&
              !maps::build_runtime_archive_layout(4,system),
              "unverified version and registration rejected");
        bad=system;
        bad.header.reported_images_in_use=199;
        check(!maps::build_runtime_archive_layout(3,bad),"skip beyond reported count rejected");
        const auto bad_group=archive(512,400,"Zeus_system.bmp",{{1,500}});
        const auto bad_group_layout=maps::build_runtime_archive_layout(3,bad_group);
        check(bad_group_layout && maps::resolve_resource_group(
              {0x601},{{3,{&*bad_group_layout}}}).status==maps::GroupLookupStatus::ImageOutOfRange,
              "out-of-range group base rejected");
        check(maps::resolve_graphics_id_hypothesis(0xc000,{{3,{&catalog,nullptr}}}).status==
              maps::GraphicsIdStatus::UnverifiedRegistration,
              "missing shared layout rejected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Runtime group test failed: " << error.what() << '\n';
        return 1;
    }
}
