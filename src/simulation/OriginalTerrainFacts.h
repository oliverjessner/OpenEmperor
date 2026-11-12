#pragma once

#include "simulation/MapPermissions.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace openemperor::simulation {

// Prepared immutable source facts, not permissions, saved flags or an emitter.
// The caller binds these raw words to the complete source-map SHA-256 through
// the existing map loader. World never reads or hashes files.
class OriginalTerrainFacts {
public:
    static constexpr int width=228,height=228;
    static constexpr std::size_t cell_count=std::size_t(width)*height;
    OriginalTerrainFacts(std::span<const std::uint32_t> raw_terrain,
                         std::string source_map_sha256);
    OriginalTerrainFacts(std::span<const std::uint32_t> raw_terrain,
                         std::span<const std::uint8_t> fertility,std::string source_map_sha256);
    bool fertility_complete() const noexcept { return fertility_complete_; }
    std::uint8_t fertility(Cell cell) const;
    const std::string& source_map_sha256() const noexcept { return source_map_sha256_; }
    bool supports_cardinal_footprint(Cell origin,int side) const noexcept;
    int cardinal_water_bonus(Cell origin,int side) const;
    bool operator==(const OriginalTerrainFacts&) const = default;
private:
    std::string source_map_sha256_;
    bool fertility_complete_=false;
    std::array<std::uint8_t,cell_count> fertility_{};
    // Separate recipient queries; neither mask changes the shared DES map.
    std::array<std::uint8_t,cell_count> side2_{},side4_{};
};

} // namespace openemperor::simulation
