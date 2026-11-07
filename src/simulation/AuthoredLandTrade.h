#pragma once

#include "simulation/MapPermissions.h"
#include "simulation/OriginalEconomyState.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace openemperor::simulation {
enum class CourierId : std::uint32_t;
enum class LandTradeOrderId : std::uint32_t {};
enum class LandTradeDirection : std::uint8_t { Import=0, Export=1 };
inline constexpr std::uint32_t kAuthoredLandTradeRuleVersion=21;
inline constexpr const char* authored_land_trade_contract="authored-land-trade-v1";
inline constexpr int land_trade_portion=4,land_trade_external_supply=8,land_trade_external_demand=8;
inline constexpr std::int64_t land_trade_import_price=6,land_trade_export_price=3,land_trade_license_cost=40;
inline constexpr std::size_t land_trade_order_limit=8;

// Derived solely from the common source MapGeometry candidate, never road or
// terrain appearance. Production callers bind the complete unchanged map SHA.
// This supplies geometry facts, not permission, a route, or original traders.
class LandTradeBoundaryFacts {
public:
    LandTradeBoundaryFacts(int width,int height,std::span<const std::uint8_t> geometry,
                          std::string source_map_sha256);
    bool contains(Cell) const noexcept;
    bool boundary_footprint(Cell origin,int width,int height) const noexcept;
    int width() const noexcept {return width_;}
    int height() const noexcept {return height_;}
    const std::string& source_map_sha256() const noexcept {return source_map_sha256_;}
private:
    int width_,height_;
    std::vector<std::uint8_t> geometry_;
    std::string source_map_sha256_;
};

struct LandTradeBinding {
    BuildingId warehouse=static_cast<BuildingId>(0),gateway=static_cast<BuildingId>(0);
    CourierId courier=static_cast<CourierId>(0);
    std::uint64_t configured_tick=0;
    bool operator==(const LandTradeBinding&) const = default;
};
// Amount/prices/partner offers are immutable contract constants. These bounded
// rows retain real transaction witnesses, never another cargo or stock.
struct LandTradeOrder {
    LandTradeOrderId id=static_cast<LandTradeOrderId>(0);
    LandTradeDirection direction=LandTradeDirection::Import;
    std::uint64_t ordered_tick=0;
    std::optional<std::uint64_t> boundary_tick,completed_tick;
    bool operator==(const LandTradeOrder&) const = default;
};
struct AuthoredLandTradeState {
    OriginalModelSource model_source;
    std::string source_map_sha256;
    std::optional<LandTradeBinding> binding;
    std::vector<LandTradeOrder> orders;
    std::uint32_t next_order_id=1;
    bool operator==(const AuthoredLandTradeState&) const = default;
};
struct LandTradeBalance {
    std::uint64_t imports_paid=0,exports_received=0,imported=0,exported=0;
    int import_offer_remaining=land_trade_external_supply,export_offer_remaining=land_trade_external_demand;
    bool busy=false;
};
LandTradeBalance inspect_land_trade(const AuthoredLandTradeState&);
} // namespace openemperor::simulation
