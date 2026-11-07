#include "simulation/original/ModelSchema.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace openemperor::original {
namespace {

constexpr std::array<std::string_view, house_row_count> house_labels{
    "1: Shelter", "2: Hut", "3: Plain Cottage", "4: Attractive Cottage",
    "5: Spacious Dwelling", "6: Elegant Dwelling", "7: Ornate Apartment",
    "8: Lux. Apartment", "Elite: Unoccupied", "Elite 0: Abandoned",
    "Elite 1: Modest Siheyuan", "Elite 2: Lavish Siheyuan",
    "Elite 3: Humble Compound", "Elite 4: Impressive Compound",
    "Elite 5: Heavenly Compound"
};

void validate_field(BuildingField field) {
    if (static_cast<std::size_t>(field) >= building_field_count)
        throw std::out_of_range("invalid original building field");
}

void validate_field(HouseField field) {
    if (static_cast<std::size_t>(field) >= house_field_count)
        throw std::out_of_range("invalid original house field");
}

void validate_difficulty(Difficulty difficulty) {
    if (static_cast<std::size_t>(difficulty) >= difficulty_count)
        throw std::out_of_range("invalid original model difficulty");
}

} // namespace

ModelBuildingId model_id(ModelBuilding building) {
    // Reject undefined enum values even when a modified table contains the ID.
    (void)model_label(building);
    return {static_cast<std::uint16_t>(building)};
}

HouseRowId model_row(ModelHouse house) {
    const auto row = static_cast<std::uint8_t>(house);
    if (row >= house_row_count)
        throw std::out_of_range("invalid named original house row");
    return {row};
}

std::string_view model_label(ModelBuilding building) {
    switch (building) {
    case ModelBuilding::LoggingShed: return "Logging Shed";
    case ModelBuilding::BronzeSmelter: return "Bronze Smelter";
    case ModelBuilding::BronzewareMaker: return "Bronzeware Maker";
    case ModelBuilding::LacquerwareMaker: return "Lacquerware Maker";
    case ModelBuilding::PaperMaker: return "Paper Maker";
    case ModelBuilding::Weaver: return "Weaver";
    case ModelBuilding::TeaCuringShed: return "Tea Curing Shed";
    case ModelBuilding::LacquerRefinery: return "Lacquer Refinery";
    case ModelBuilding::SilkwormShed: return "Silkworm Shed";
    case ModelBuilding::TeaBush: return "Tea Bush";
    case ModelBuilding::LacquerTree: return "Lacquer Tree";
    case ModelBuilding::MulberryTree: return "Mulberry Tree";
    case ModelBuilding::TheatrePavilion: return "Theatre Pavilion";
    case ModelBuilding::ClayPit: return "Clay Pit";
    case ModelBuilding::Kiln: return "Kiln";
    case ModelBuilding::Warehouse: return "Warehouse";
    case ModelBuilding::CommonMarketSquare: return "Common Market Square";
    case ModelBuilding::Well: return "Well";
    case ModelBuilding::Farmhouse: return "Farmhouse";
    case ModelBuilding::HerbalistsStall: return "Herbalist's Stall";
    case ModelBuilding::AcupuncturistsClinic: return "Acupuncturist's Clinic";
    case ModelBuilding::MusicSchool: return "Music  School";
    case ModelBuilding::AcrobatSchool: return "Acrobat School";
    case ModelBuilding::DramaSchool: return "Drama School";
    case ModelBuilding::AncestralShrine: return "Ancestral Shrine";
    case ModelBuilding::DaoistShrine: return "Daoist Shrine";
    case ModelBuilding::BuddhistShrine: return "Buddhist Shrine";
    case ModelBuilding::ConfucianAcademy: return "Confucian Academny";
    case ModelBuilding::HempFarm: return "Hemp Farm";
    case ModelBuilding::HempField: return "Hemp Field";
    case ModelBuilding::WheatField: return "Wheat Field";
    case ModelBuilding::MilletField: return "Millet Field";
    }
    throw std::out_of_range("invalid named original building ID");
}

std::string_view model_label(ModelHouse house) {
    return house_labels.at(model_row(house).value);
}

ModelBuildingId house_building_type(ModelHouse house) {
    const auto row = model_row(house).value;
    return {static_cast<std::uint16_t>(row + 3U)};
}

std::optional<ModelHouse> house_model_for_building(ModelBuildingId id) {
    if (id.value > max_building_id)
        throw std::out_of_range("original building ID exceeds model bound");
    if (id.value < 3 || id.value > 17) return std::nullopt;
    return static_cast<ModelHouse>(id.value - 3U);
}

std::int32_t raw_building_value(const ModelData& model, ModelBuilding building,
                                BuildingField field) {
    validate_field(field);
    const auto id = model_id(building);
    if (model.building_label(id) != model_label(building))
        throw std::invalid_argument("original named building source label does not match schema");
    return model.building(id, field);
}

std::int32_t raw_house_value(const ModelData& model, ModelHouse house,
                            HouseField field) {
    validate_field(field);
    const auto row = model_row(house);
    if (model.house_label(row) != model_label(house))
        throw std::invalid_argument("original named house source label does not match schema");
    return model.house(row, field);
}

std::int32_t building_value(const ModelData& model, ModelBuilding building,
                            BuildingField field, Difficulty difficulty) {
    validate_difficulty(difficulty);
    const auto raw = raw_building_value(model, building, field);
    const auto modifier = model.building_modifier(difficulty, field);
    const auto product = static_cast<std::int64_t>(raw) * modifier;
    if (product < std::numeric_limits<std::int32_t>::min() ||
        product > std::numeric_limits<std::int32_t>::max())
        throw std::overflow_error("original building modifier product exceeds int32");
    return static_cast<std::int32_t>(product / 100);
}

std::int32_t house_value(const ModelData& model, ModelHouse house,
                         HouseField field, Difficulty difficulty) {
    validate_difficulty(difficulty);
    const auto raw = raw_house_value(model, house, field);
    const auto modifier = model.house_modifier(difficulty, field);
    const auto value = static_cast<std::int64_t>(raw) + modifier;
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max())
        throw std::overflow_error("original house modifier sum exceeds int32");
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(value, -99, 100));
}

} // namespace openemperor::original
