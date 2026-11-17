#pragma once

#include "simulation/MapPermissions.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace openemperor::simulation { class GrowerSourceFacts; }
namespace openemperor::persistence {
std::shared_ptr<const simulation::GrowerSourceFacts> prepare_original_grower_source_facts(
    const std::filesystem::path&,const std::filesystem::path&);
}
namespace openemperor::simulation {

// Immutable source operand for the explicitly selected transient Grower
// session. The entry is a storage-grid cell, not a Farm spawn grant, road,
// permission, prepared distance or saved authority.
class GrowerSourceFacts {
public:
    const std::string& source_map_sha256() const noexcept { return source_map_sha256_; }
    Cell storage_entry_cell() const noexcept { return storage_entry_cell_; }
    std::int16_t source_region_operand() const noexcept { return source_region_operand_; }
    bool operator==(const GrowerSourceFacts&) const = default;
private:
    friend std::shared_ptr<const GrowerSourceFacts> persistence::prepare_original_grower_source_facts(
        const std::filesystem::path&,const std::filesystem::path&);
    GrowerSourceFacts(std::string source_map_sha256,Cell storage_entry_cell,std::int16_t source_region_operand)
        : source_map_sha256_(std::move(source_map_sha256)),
          storage_entry_cell_(storage_entry_cell),source_region_operand_(source_region_operand) {
        if (source_map_sha256_.size()!=64 ||
            !std::all_of(source_map_sha256_.begin(),source_map_sha256_.end(),[](char c) {
                return (c>='0' && c<='9') || (c>='a' && c<='f');
            }) || storage_entry_cell.x<0 || storage_entry_cell.y<0 ||
            storage_entry_cell.x>=228 || storage_entry_cell.y>=228)
            throw std::invalid_argument("invalid immutable Grower source facts");
    }
    std::string source_map_sha256_;
    Cell storage_entry_cell_{};
    // Source Header+8 low WORD, separate from the geometry-context DWORD.
    // Its live region lifetime and crop factor remain admission obligations.
    std::int16_t source_region_operand_=0;
};

} // namespace openemperor::simulation
