#include "persistence/SandboxSave.h"

#include "maps/EmperorContainer.h"
#include "maps/EmperorMap.h"
#include "maps/MapCatalog.h"
#include "maps/MapGeometry.h"
#include "maps/MapRulesCheck.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>

namespace openemperor::persistence {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> bytes,std::size_t at) {
    if (at>bytes.size() || bytes.size()-at<2)
        throw maps::UnsupportedMapProfile("Grower source operand exceeds map header");
    return static_cast<std::uint16_t>(bytes[at]) |
        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[at+1])<<8U);
}
int signed16(std::uint16_t value) {
    return value<0x8000U ? static_cast<int>(value):static_cast<int>(value)-0x10000;
}
std::uint32_t le32(std::span<const std::uint8_t> bytes,std::size_t at) {
    return static_cast<std::uint32_t>(le16(bytes,at)) |
        (static_cast<std::uint32_t>(le16(bytes,at+2))<<16U);
}
} // namespace

std::shared_ptr<const simulation::GrowerSourceFacts> prepare_original_grower_source_facts(
        const std::filesystem::path& root,const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute())
        throw std::invalid_argument("Grower source facts require a relative map path");
    for (const auto& part:relative)
        if (part==".." || part==".") throw std::invalid_argument("Grower source map is not a canonical relative path");
    const auto path=maps::resolve_map_path(root,relative);
    const auto container=maps::EmperorContainer::open(path);
    const auto source_hash=maps::map_input_sha256(container);
    if (container.multipart() || container.parts().size()!=1)
        throw maps::UnsupportedMapProfile("Grower source facts require one standalone map part");
    const auto probe=maps::probe_map_part(container,0);
    if (probe.profile!=maps::PartProfile::Map)
        throw maps::UnsupportedMapProfile("Grower source facts require the supported original map profile");
    const maps::MapGeometry geometry{probe.declared_map_size};
    if (!geometry.supported)
        throw maps::UnsupportedMapProfile("Grower source facts require supported map geometry");

    // EXE_OBSERVED: outer Map5 WORD, cafe DWORD, Header2 WORD and48-byte
    // body, then the MapProperties WORD. Properties3/6 select the same
    // packed read branch. Native owner+31c/+31e are logical850/852; native
    // structure padding is not part of the archive. Read only those bounded
    // operands through the existing container, never a second map parser.
    const auto prefix=container.read_range(0,0,854);
    const auto properties_schema=le16(prefix,56);
    if (le16(prefix,0)!=5 || le16(prefix,6)!=2 ||
        (properties_schema!=3 && properties_schema!=6))
        throw maps::UnsupportedMapProfile("Grower entry requires Map5/Header2/Properties3or6");
    // Native53ce60 takes the coordinate base from Header+4, not the separately
    // serialized MapProperties size. Require agreement before using the shared
    // geometry's border; inconsistent source operands are not repaired here.
    constexpr std::uint32_t native_context_sizes[]{56,84,112,140,170,226};
    const auto context_index=le32(prefix,8);
    if (context_index>=std::size(native_context_sizes) ||
        native_context_sizes[context_index]!=geometry.declared_size)
        throw maps::UnsupportedMapProfile("Grower map header and declared geometry disagree");
    const int x=signed16(le16(prefix,850)),y=signed16(le16(prefix,852));
    if (x<0 || y<0 || x>=static_cast<int>(geometry.declared_size) ||
        y>=static_cast<int>(geometry.declared_size))
        throw maps::UnsupportedMapProfile("Grower source entry is outside native map bounds");
    const maps::GridCell storage{static_cast<std::uint32_t>(x)+geometry.border,
                                 static_cast<std::uint32_t>(y)+geometry.border};
    if (!geometry.contains(storage))
        throw maps::UnsupportedMapProfile("Grower source entry is outside playable map geometry");

    // Hash the retained bytes that supplied every operand, then independently
    // recheck the live source through the same bounded shared input owner.
    if (maps::map_input_sha256(maps::EmperorContainer::open(path))!=source_hash)
        throw std::runtime_error("original map changed while preparing Grower source facts");
    return std::shared_ptr<const simulation::GrowerSourceFacts>(new simulation::GrowerSourceFacts(source_hash,
        simulation::Cell{static_cast<int>(storage.x),static_cast<int>(storage.y)}));
}

} // namespace openemperor::persistence
