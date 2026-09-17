#include "maps/ResourceGroupLookup.h"
#include "maps/GraphicsIdHypothesis.h"

#include <cassert>
#include <cstdint>
#include <map>
#include <vector>

namespace assets = openemperor::assets;
namespace maps = openemperor::maps;

int main() {
    // Independently constructed SG3 metadata bytes; no original asset bytes.
    std::vector<std::uint8_t> bytes(40680 + 32 * 64,0);
    bytes[4] = 213; // v213
    bytes[12] = 32; // image capacity
    bytes[16] = 20; // in-use count
    // File-order entries 1, 3, 5 become runtime positions 2, 1, 0.
    // Zero and signed-negative words do not enter the studied runtime list.
    bytes[82] = 4;
    bytes[84] = 0; bytes[85] = 0x80;
    bytes[86] = 9;
    bytes[90] = 12;
    assets::Sg3Archive archive = assets::parse_sg3(bytes,bytes.size());
    std::map<std::uint32_t,maps::GroupRegistration> terrain{{3,{&archive}}};
    const auto first = maps::resolve_resource_group({0x601},terrain);
    const auto middle = maps::resolve_resource_group({0x602},terrain);
    const auto last = maps::resolve_resource_group({0x603},terrain);
    assert(first.status == maps::GroupLookupStatus::Resolved &&
           first.sg3_index_position == 5 && first.sg3_file_offset == 90 &&
           first.sg3_raw_value == 12 && first.local_base == 11 &&
           first.packed_base->value == 0xc00b);
    assert(middle.sg3_index_position == 3 && middle.sg3_raw_value == 9 &&
           middle.packed_base->value == 0xc008);
    assert(last.sg3_index_position == 1 && last.sg3_raw_value == 4 &&
           last.packed_base->value == 0xc003);
    assert(maps::resolve_resource_group({0x604},terrain).status ==
           maps::GroupLookupStatus::GroupOutOfRange);
    assert(maps::resolve_resource_group({0x600},terrain).status ==
           maps::GroupLookupStatus::NoGroupPosition);
    assert(maps::resolve_resource_group({0x801},terrain).status ==
           maps::GroupLookupStatus::UnregisteredSlot);
    assert(maps::resolve_resource_group({0x80000601},terrain).status ==
           maps::GroupLookupStatus::UnsupportedHighBit);

    assets::Sg3Archive other = archive;
    other.index[5] = 15;
    assert(maps::resolve_resource_group({0x601},{{3,{&other}}}).packed_base->value == 0xc00e);
    other.header.version = 214;
    assert(maps::resolve_resource_group({0x601},{{3,{&other}}}).status ==
           maps::GroupLookupStatus::UnverifiedRegistration);
    other = archive;
    other.index[5] = 21;
    assert(maps::resolve_resource_group({0x601},{{3,{&other}}}).status ==
           maps::GroupLookupStatus::ImageOutOfRange);

    for (std::uint32_t i=0;i<8;++i) {
        const auto variant = maps::group_variant(*first.packed_base,i);
        assert(variant && variant->value == 0xc00b + i);
    }
    assert(!maps::group_variant(*first.packed_base,8));
    assert(!maps::group_variant({0xffff},1)); // No accidental slot carry.

    // The separate, already-established image resolver applies the v213
    // dummy-record translation exactly once.
    assets::AssetCatalog catalog;
    catalog.records.resize(32);
    for (std::uint32_t i=0;i<32;++i) catalog.records[i].id.image_index=i;
    const maps::GraphicsArchiveRegistration registration{&catalog,213,32,20};
    const auto image = maps::resolve_graphics_id_hypothesis(
        first.packed_base->value,{{3,registration}});
    assert(image.local_index == 11 && image.physical_record_index == 12);
}
