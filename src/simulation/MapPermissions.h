#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace openemperor::simulation {

struct Cell {
    int x=0;
    int y=0;
    bool operator==(const Cell&) const = default;
};
struct FixedGateId {
    std::uint32_t value=0;
    auto operator<=>(const FixedGateId&) const = default;
};
inline constexpr std::uint32_t kMapPermissionsPolicyVersion=1;
enum class BuildBlocker : std::uint8_t {
    None, OutsideMap, UnsupportedTerrain, OriginalStructure, GateSolidPart,
    GateSideEntry, SandboxOccupied, InsufficientFunds, LegacyBuildabilityRestriction,
    UnsupportedHeightTransition, ProtectedGatePassage, InvalidEdge, CounterExhausted
};
const char* build_blocker_name(BuildBlocker blocker);
const char* build_blocker_reason(BuildBlocker blocker);
struct TransportEdge {
    Cell from{},to{};
    bool operator==(const TransportEdge&) const = default;
};
struct BuildDiagnostic {
    BuildBlocker blocker=BuildBlocker::None;
    std::optional<Cell> cell;
    std::optional<TransportEdge> edge;
    bool operator==(const BuildDiagnostic&) const = default;
};
struct RoadCellDiagnostic {
    Cell cell{};
    bool allowed=false;
    bool existing_road=false;
    bool fixed_passage=false;
    BuildBlocker blocker=BuildBlocker::None;
};
struct MapCellPermission {
    bool road_allowed=false;
    bool building_allowed=false;
    bool protected_original=false;
    std::int8_t height=0;
    BuildBlocker road_blocker=BuildBlocker::UnsupportedTerrain;
    BuildBlocker building_blocker=BuildBlocker::LegacyBuildabilityRestriction;
};
struct FixedGatePassage {
    FixedGateId id{};
    std::vector<Cell> protected_footprint;
    std::vector<Cell> corridor;
    std::array<Cell,2> openings{};
};

// Prepared immutable map facts, independent of SDL, graphics and mutable World
// objects. Gate corridors are the bounded OpenEmperor-authored open passage.
class MapPermissions {
public:
    MapPermissions(int width,int height,std::uint32_t policy_version,
                   std::vector<MapCellPermission> cells,std::vector<FixedGatePassage> gates);
    MapPermissions(const MapPermissions&)=default;
    MapPermissions(MapPermissions&&)=default;
    MapPermissions& operator=(const MapPermissions&)=delete;
    MapPermissions& operator=(MapPermissions&&)=delete;
    int width() const { return width_; }
    int height() const { return height_; }
    std::uint32_t policy_version() const { return policy_version_; }
    bool in_bounds(Cell cell) const;
    const MapCellPermission* cell_permission(Cell cell) const;
    bool road_allowed(Cell cell) const;
    bool building_allowed(Cell cell) const;
    bool protected_original(Cell cell) const;
    std::int8_t cell_height(Cell cell) const;
    bool fixed_passage(Cell cell) const;
    const FixedGatePassage* fixed_gate(Cell cell) const;
    std::optional<FixedGateId> fixed_gate_at(Cell cell) const;
    BuildBlocker road_blocker(Cell cell) const;
    BuildBlocker building_blocker(Cell cell) const;
    BuildBlocker transport_edge_blocker(Cell from,Cell to) const;
    bool transport_edge_allowed(Cell from,Cell to) const {
        return transport_edge_blocker(from,to)==BuildBlocker::None;
    }
    const std::vector<MapCellPermission>& cells() const { return cells_; }
    const std::vector<FixedGatePassage>& gates() const { return gates_; }
    const std::vector<std::uint8_t>& road_mask() const { return road_mask_; }
    const std::vector<std::uint8_t>& building_mask() const { return building_mask_; }
    std::string canonical_state() const;
private:
    std::size_t index(Cell cell) const;
    int width_,height_;
    std::uint32_t policy_version_;
    std::vector<MapCellPermission> cells_;
    std::vector<FixedGatePassage> gates_;
    std::vector<std::uint8_t> road_mask_,building_mask_;
    std::vector<int> gate_index_,corridor_index_;
};

// Explicit authored fixture policy: the supplied mask permits roads/buildings,
// all heights are zero and no original structures or gates are introduced.
std::shared_ptr<const MapPermissions> authored_simple_permissions(
    int width,int height,const std::vector<std::uint8_t>& mask);

} // namespace openemperor::simulation
