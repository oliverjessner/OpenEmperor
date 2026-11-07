#pragma once

#include "simulation/original/ModelData.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace openemperor::original {

inline constexpr std::uint32_t model_schema_version = 1;

enum class EvidenceStatus : std::uint8_t {
    VerifiedOriginal = 0, ReverseEngineered = 1, Inferred = 2,
    AuthoredFallback = 3, Unknown = 4
};

// These identify records in the original model file, not World building IDs.
enum class ModelBuilding : std::uint16_t {
    ClayPit = 35,
    Kiln = 43,
    Warehouse = 54,
    CommonMarketSquare = 59,
    Well = 72,
    Farmhouse = 193,
    HerbalistsStall = 207,
    AcupuncturistsClinic = 208,
    MusicSchool = 211,
    AcrobatSchool = 212,
    DramaSchool = 213,
    AncestralShrine = 214,
    DaoistShrine = 215,
    BuddhistShrine = 217,
    ConfucianAcademy = 219,
    HempFarm = 192,
    HempField = 194,
    WheatField = 195,
    MilletField = 196,
    LoggingShed = 38,
    BronzeSmelter = 39,
    BronzewareMaker = 42,
    LacquerwareMaker = 44,
    PaperMaker = 45,
    Weaver = 47,
    TeaCuringShed = 237,
    LacquerRefinery = 238,
    SilkwormShed = 239,
    TeaBush = 26,
    LacquerTree = 27,
    MulberryTree = 28,
    TheatrePavilion = 75
};

// Ordered ALL HOUSES records. Vacant House has no separate house-table row.
enum class ModelHouse : std::uint8_t {
    Shelter = 0,
    Hut = 1,
    PlainCottage = 2,
    AttractiveCottage = 3,
    SpaciousDwelling = 4,
    ElegantDwelling = 5,
    OrnateApartment = 6,
    LuxuriousApartment = 7,
    EliteUnoccupied = 8,
    EliteAbandoned = 9,
    ModestSiheyuan = 10,
    LavishSiheyuan = 11,
    HumbleCompound = 12,
    ImpressiveCompound = 13,
    HeavenlyCompound = 14
};

[[nodiscard]] ModelBuildingId model_id(ModelBuilding building);
[[nodiscard]] HouseRowId model_row(ModelHouse house);
[[nodiscard]] std::string_view model_label(ModelBuilding building);
[[nodiscard]] std::string_view model_label(ModelHouse house);
[[nodiscard]] ModelBuildingId house_building_type(ModelHouse house);
[[nodiscard]] std::optional<ModelHouse> house_model_for_building(ModelBuildingId id);

// Named access also checks the source record's saved label. Generic raw access
// through ModelData remains available for syntactically valid modified tables.
[[nodiscard]] std::int32_t raw_building_value(const ModelData& model,
                                            ModelBuilding building,
                                            BuildingField field);
[[nodiscard]] std::int32_t raw_house_value(const ModelData& model,
                                         ModelHouse house, HouseField field);

// Values prepared by the original model loader: signed percentage arithmetic
// for buildings; additive modifiers clamped to [-99, 100] for house fields.
// Both use checked int32 intermediates: a product or sum outside that original
// arithmetic domain throws std::overflow_error before division or clamping.
// These are source values, not a complete construction-price calculation or a
// conversion to OpenEmperor ticks, resources, geometry or World authority.
[[nodiscard]] std::int32_t building_value(const ModelData& model,
                                        ModelBuilding building,
                                        BuildingField field,
                                        Difficulty difficulty);
[[nodiscard]] std::int32_t house_value(const ModelData& model,
                                     ModelHouse house, HouseField field,
                                     Difficulty difficulty);

} // namespace openemperor::original
